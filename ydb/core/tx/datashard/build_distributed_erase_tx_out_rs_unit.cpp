#include "datashard_active_transaction.h" 
#include "datashard_distributed_erase.h" 
#include "datashard_impl.h" 
#include "datashard_pipeline.h" 
#include "erase_rows_condition.h" 
#include "execution_unit_ctors.h" 
 
#include <util/generic/bitmap.h> 
#include <util/generic/hash.h> 
#include <util/generic/hash_set.h> 
#include <util/generic/ptr.h> 
 
namespace NKikimr { 
namespace NDataShard {
 
class TBuildDistributedEraseTxOutRSUnit : public TExecutionUnit { 
    static TVector<NTable::TTag> MakeTags(const TVector<NTable::TTag>& conditionTags, 
            const google::protobuf::RepeatedField<ui32>& indexColumnIds) { 
 
        Y_VERIFY(conditionTags.size() == 1, "Multi-column conditions are not supported"); 
        TVector<NTable::TTag> tags = conditionTags; 
 
        THashSet<NTable::TTag> uniqTags(tags.begin(), tags.end()); 
        for (const auto columnId : indexColumnIds) { 
            if (uniqTags.insert(columnId).second) { 
                tags.push_back(columnId); 
            } 
        } 
 
        return tags; 
    } 
 
    static TVector<TCell> ExtractIndexCells(const NTable::TRowState& row, const TVector<NTable::TTag>& tags, 
            const google::protobuf::RepeatedField<ui32>& indexColumnIds) { 
 
        THashMap<ui32, ui32> tagToPos; 
        for (ui32 pos = 0; pos < tags.size(); ++pos) { 
            const auto tag = tags.at(pos); 
 
            Y_VERIFY_DEBUG(!tagToPos.contains(tag)); 
            tagToPos.emplace(tag, pos); 
        } 
 
        TVector<TCell> result; 
        for (const auto columnId : indexColumnIds) { 
            auto it = tagToPos.find(columnId); 
            Y_VERIFY(it != tagToPos.end()); 
 
            const auto pos = it->second; 
            Y_VERIFY(pos < row.Size()); 
 
            result.push_back(row.Get(pos)); 
        } 
 
        return result; 
    } 
 
    static bool CompareCells(const TVector<TRawTypeValue>& expectedValue, const TVector<TCell>& actualValue) { 
        Y_VERIFY(expectedValue.size() == actualValue.size()); 
 
        for (ui32 pos = 0; pos < expectedValue.size(); ++pos) { 
            const auto& expected = expectedValue.at(pos); 
            const auto& actual = actualValue.at(pos); 
 
            if (0 != CompareTypedCells(actual, expected.AsRef(), expected.Type())) { 
                return false; 
            } 
        } 
 
        return true; 
    } 
 
public: 
    TBuildDistributedEraseTxOutRSUnit(TDataShard& self, TPipeline& pipeline)
        : TExecutionUnit(EExecutionUnitKind::BuildDistributedEraseTxOutRS, true, self, pipeline) 
    { 
    } 
 
    bool IsReadyToExecute(TOperation::TPtr) const override { 
        return true; 
    } 
 
    EExecutionStatus Execute(TOperation::TPtr op, TTransactionContext& txc, const TActorContext&) override { 
        Y_VERIFY(op->IsDistributedEraseTx()); 
 
        TActiveTransaction* tx = dynamic_cast<TActiveTransaction*>(op.Get()); 
        Y_VERIFY_S(tx, "cannot cast operation of kind " << op->GetKind()); 
 
        const auto& eraseTx = tx->GetDistributedEraseTx(); 
        if (!eraseTx->HasDependents()) { 
            return EExecutionStatus::Executed; 
        } 
 
        const auto& request = eraseTx->GetRequest(); 
        const ui64 tableId = request.GetTableId(); 
 
        Y_VERIFY(DataShard.GetUserTables().contains(tableId)); 
        const TUserTable& tableInfo = *DataShard.GetUserTables().at(tableId); 
 
        THolder<IEraseRowsCondition> condition{CreateEraseRowsCondition(request)};
        Y_VERIFY(condition.Get()); 
        condition->Prepare(txc.DB.GetRowScheme(tableInfo.LocalTid), 0); 
 
        const auto tags = MakeTags(condition->Tags(), eraseTx->GetIndexColumnIds()); 
        auto readVersion = DataShard.GetReadWriteVersions(tx).ReadVersion; 
        bool pageFault = false; 
 
        TDynBitMap confirmedRows; 
        for (ui32 i = 0; i < request.KeyColumnsSize(); ++i) { 
            TSerializedCellVec keyCells; 
            Y_VERIFY(TSerializedCellVec::TryParse(request.GetKeyColumns(i), keyCells)); 
            Y_VERIFY(keyCells.GetCells().size() == tableInfo.KeyColumnTypes.size()); 
 
            TVector<TRawTypeValue> key; 
            for (ui32 pos = 0; pos < tableInfo.KeyColumnTypes.size(); ++pos) { 
                const NScheme::TTypeId type = tableInfo.KeyColumnTypes[pos]; 
                const TCell& cell = keyCells.GetCells()[pos]; 
 
                key.emplace_back(TRawTypeValue(cell.AsRef(), type)); 
            } 
 
            TSerializedCellVec indexCells; 
            TVector<TRawTypeValue> indexTypedVals; 
            if (!eraseTx->GetIndexColumns().empty()) { 
                Y_VERIFY(i < static_cast<ui32>(eraseTx->GetIndexColumns().size())); 
                Y_VERIFY(TSerializedCellVec::TryParse(eraseTx->GetIndexColumns().at(i), indexCells)); 
                Y_VERIFY(indexCells.GetCells().size() == static_cast<ui32>(eraseTx->GetIndexColumnIds().size())); 
 
                for (ui32 pos = 0; pos < static_cast<ui32>(eraseTx->GetIndexColumnIds().size()); ++pos) { 
                    auto it = tableInfo.Columns.find(eraseTx->GetIndexColumnIds().Get(pos)); 
                    Y_VERIFY(it != tableInfo.Columns.end()); 
 
                    const NScheme::TTypeId type = it->second.Type; 
                    const TCell& cell = indexCells.GetCells()[pos]; 
 
                    indexTypedVals.emplace_back(TRawTypeValue(cell.AsRef(), type)); 
                } 
            } 
 
            NTable::TRowState row; 
            const auto ready = txc.DB.Select(tableInfo.LocalTid, key, tags, row, 0, readVersion); 
 
            if (pageFault) { 
                continue; 
            } 
 
            switch (ready) { 
            case NTable::EReady::Page: 
                pageFault = true; 
                break; 
            case NTable::EReady::Gone: 
                confirmedRows.Reset(i); 
                break; 
            case NTable::EReady::Data: 
                if (!condition->Check(row) || !CompareCells(indexTypedVals, ExtractIndexCells(row, tags, eraseTx->GetIndexColumnIds()))) { 
                    confirmedRows.Reset(i); 
                } else { 
                    confirmedRows.Set(i); 
                } 
                break; 
            } 
        } 
 
        if (pageFault) { 
            return EExecutionStatus::Restart; 
        } 
 
        NKikimrTxDataShard::TDistributedEraseRS rs; 
        rs.SetConfirmedRows(SerializeBitMap(confirmedRows)); 
        const TString rsBody = rs.SerializeAsString(); 
 
        for (const auto& dependent : eraseTx->GetDependents()) { 
            op->OutReadSets()[std::make_pair(DataShard.TabletID(), dependent.GetShardId())] = rsBody; 
        } 
 
        eraseTx->SetConfirmedRows(std::move(confirmedRows)); 
 
        return EExecutionStatus::Executed; 
    } 
 
    void Complete(TOperation::TPtr, const TActorContext&) override { 
    } 
}; 
 
THolder<TExecutionUnit> CreateBuildDistributedEraseTxOutRSUnit(TDataShard& self, TPipeline& pipeline) {
    return THolder(new TBuildDistributedEraseTxOutRSUnit(self, pipeline)); 
} 
 
} // namespace NDataShard
} // namespace NKikimr 
