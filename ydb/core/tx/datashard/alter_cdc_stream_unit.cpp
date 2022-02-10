#include "datashard_impl.h" 
#include "datashard_pipeline.h" 
#include "execution_unit_ctors.h" 
 
namespace NKikimr { 
namespace NDataShard {
 
class TAlterCdcStreamUnit : public TExecutionUnit { 
public: 
    TAlterCdcStreamUnit(TDataShard& self, TPipeline& pipeline)
        : TExecutionUnit(EExecutionUnitKind::AlterCdcStream, false, self, pipeline) 
    { 
    } 
 
    bool IsReadyToExecute(TOperation::TPtr) const override { 
        return true; 
    } 
 
    EExecutionStatus Execute(TOperation::TPtr op, TTransactionContext& txc, const TActorContext& ctx) override { 
        Y_VERIFY(op->IsSchemeTx()); 
 
        TActiveTransaction* tx = dynamic_cast<TActiveTransaction*>(op.Get()); 
        Y_VERIFY_S(tx, "cannot cast operation of kind " << op->GetKind()); 
 
        auto& schemeTx = tx->GetSchemeTx(); 
        if (!schemeTx.HasAlterCdcStreamNotice()) { 
            return EExecutionStatus::Executed; 
        } 
 
        const auto& params = schemeTx.GetAlterCdcStreamNotice(); 
        const auto& streamDesc = params.GetStreamDescription(); 
 
        const auto pathId = TPathId(params.GetPathId().GetOwnerId(), params.GetPathId().GetLocalId()); 
        Y_VERIFY(pathId.OwnerId == DataShard.GetPathOwnerId()); 
 
        const auto streamPathId = TPathId(streamDesc.GetPathId().GetOwnerId(), streamDesc.GetPathId().GetLocalId()); 
        Y_VERIFY(streamPathId.OwnerId == DataShard.GetPathOwnerId()); 
 
        Y_VERIFY_S(streamDesc.GetState() == NKikimrSchemeOp::ECdcStreamStateDisabled, "Unexpected alter cdc stream"
            << ": desc# " << streamDesc.ShortDebugString()); 
        auto tableInfo = DataShard.AlterTableDisableCdcStream(ctx, txc, pathId, params.GetTableSchemaVersion(), streamPathId); 
        DataShard.AddUserTable(pathId, tableInfo); 
 
        BuildResult(op, NKikimrTxDataShard::TEvProposeTransactionResult::COMPLETE); 
        op->Result()->SetStepOrderId(op->GetStepOrder().ToPair()); 
 
        return EExecutionStatus::DelayCompleteNoMoreRestarts; 
    } 
 
    void Complete(TOperation::TPtr, const TActorContext&) override { 
    } 
}; 
 
THolder<TExecutionUnit> CreateAlterCdcStreamUnit(TDataShard& self, TPipeline& pipeline) {
    return THolder(new TAlterCdcStreamUnit(self, pipeline)); 
} 
 
} // namespace NDataShard
} // namespace NKikimr 
