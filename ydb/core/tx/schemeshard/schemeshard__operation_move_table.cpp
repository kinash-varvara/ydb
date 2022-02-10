#include "schemeshard__operation_part.h"
#include "schemeshard__operation_common.h"
#include "schemeshard_impl.h"

namespace {

using namespace NKikimr;
using namespace NSchemeShard;

class TConfigureParts: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override {
        return TStringBuilder()
            << "TMoveTable TConfigureParts"
            << ", operationId: " << OperationId;
    }

public:
    TConfigureParts(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {});
    }

    bool HandleReply(TEvDataShard::TEvProposeTransactionResult::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvProposeTransactionResult"
                               << " at tabletId# " << ssId);
        LOG_DEBUG_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    DebugHint() << " HandleReply TEvProposeTransactionResult"
                                << " message# " << ev->Get()->Record.ShortDebugString());

        if (!NTableState::CollectProposeTransactionResults(OperationId, ev, context)) {
            return false;
        }

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxMoveTable);
        Y_VERIFY(txState->MinStep); // we have to have right minstep

        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at tablet" << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxMoveTable);
        Y_VERIFY(txState->SourcePathId);

        TPath dstPath = TPath::Init(txState->TargetPathId, context.SS);
        Y_VERIFY(dstPath.IsResolved());
        TPath srcPath = TPath::Init(txState->SourcePathId, context.SS);
        Y_VERIFY(srcPath.IsResolved());
        TTableInfo::TPtr srcTable = context.SS->Tables.at(srcPath->PathId);

        NIceDb::TNiceDb db(context.Txc.DB);

        // txState catches table shards
        if (!txState->Shards) {
            txState->Shards.reserve(srcTable->GetPartitions().size());
            for (const auto& shard : srcTable->GetPartitions()) {
                auto shardIdx = shard.ShardIdx;
                TShardInfo& shardInfo = context.SS->ShardInfos[shardIdx];

                txState->Shards.emplace_back(shardIdx, ETabletType::DataShard, TTxState::ConfigureParts);

                shardInfo.CurrentTxId = OperationId.GetTxId();
                context.SS->PersistShardTx(db, shardIdx, OperationId.GetTxId());
            }
            context.SS->PersistTxState(db, OperationId);
        }
        Y_VERIFY(txState->Shards.size());

        TString txBody;
        {
            auto seqNo = context.SS->StartRound(*txState);

            NKikimrTxDataShard::TFlatSchemeTransaction tx;
            context.SS->FillSeqNo(tx, seqNo);
            auto move = tx.MutableMoveTable();
            PathIdFromPathId(srcPath->PathId, move->MutablePathId()); 
            move->SetTableSchemaVersion(srcTable->AlterVersion+1);

            PathIdFromPathId(dstPath->PathId, move->MutableDstPathId()); 
            move->SetDstPath(TPath::Init(dstPath->PathId, context.SS).PathString());

            for (const auto& child: srcPath->GetChildren()) {
                auto name = child.first;

                TPath srcIndexPath = srcPath.Child(name);
                Y_VERIFY(srcIndexPath.IsResolved());

                if (srcIndexPath.IsDeleted()) {
                    continue;
                }

                TPath dstIndexPath = dstPath.Child(name);
                Y_VERIFY(dstIndexPath.IsResolved());

                auto remap = move->AddReMapIndexes();
                PathIdFromPathId(srcIndexPath->PathId, remap->MutablePathId()); 
                PathIdFromPathId(dstIndexPath->PathId, remap->MutableDstPathId()); 
            }

            Y_PROTOBUF_SUPPRESS_NODISCARD tx.SerializeToString(&txBody);
        }

        // send messages
        txState->ClearShardsInProgress();
        for (ui32 i = 0; i < txState->Shards.size(); ++i) {
            auto idx = txState->Shards[i].Idx;
            auto datashardId = context.SS->ShardInfos[idx].TabletID;

            auto event = MakeHolder<TEvDataShard::TEvProposeTransaction>(NKikimrTxDataShard::TX_KIND_SCHEME,
                                                                         context.SS->TabletID(),
                                                                         context.Ctx.SelfID,
                                                                         ui64(OperationId.GetTxId()),
                                                                         txBody,
                                                                         context.SS->SelectProcessingPrarams(txState->TargetPathId));

            context.OnComplete.BindMsgToPipe(OperationId, datashardId, idx, event.Release());
        }
        txState->UpdateShardsInProgress(TTxState::ConfigureParts);

        return false;
    }
};

void MarkSrcDropped(NIceDb::TNiceDb& db,
                    TOperationContext& context,
                    TOperationId operationId,
                    const TTxState& txState,
                    TPath& srcPath)
{
    srcPath->SetDropped(txState.PlanStep, operationId.GetTxId());
    context.SS->PersistDropStep(db, srcPath->PathId, txState.PlanStep, operationId);
    context.SS->Tables.at(srcPath->PathId)->DetachShardsStats();
    context.SS->PersistRemoveTable(db, srcPath->PathId, context.Ctx);
    context.SS->PersistUserAttributes(db, srcPath->PathId, srcPath->UserAttrs, nullptr);

    srcPath.Parent()->DecAliveChildren();
    srcPath.DomainInfo()->DecPathsInside();

    IncParentDirAlterVersionWithRepublish(operationId, srcPath, context);
}

class TPropose: public TSubOperationState {
private:
    TOperationId OperationId;
    TTxState::ETxState& NextState;

    TString DebugHint() const override {
        return TStringBuilder()
            << "TMoveTable TPropose"
            << ", operationId: " << OperationId;
    }
public:
    TPropose(TOperationId id, TTxState::ETxState& nextState)
        : OperationId(id)
        , NextState(nextState)
    {
        IgnoreMessages(DebugHint(), {TEvHive::TEvCreateTabletReply::EventType, TEvDataShard::TEvProposeTransactionResult::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        const auto& evRecord = ev->Get()->Record;

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     DebugHint() << " HandleReply TEvSchemaChanged"
                     << " at tablet: " << ssId);
        LOG_DEBUG_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    DebugHint() << " HandleReply TEvSchemaChanged"
                     << " triggered early"
                     << ", message: " << evRecord.ShortDebugString());

        NTableState::CollectSchemaChanged(OperationId, ev, context);
        return false;
    }

    bool HandleReply(TEvPrivate::TEvOperationPlan::TPtr& ev, TOperationContext& context) override {
        TStepId step = TStepId(ev->Get()->StepId);
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvOperationPlan"
                               << ", step: " << step
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxMoveTable);

        auto srcPath = TPath::Init(txState->SourcePathId, context.SS);
        auto dstPath = TPath::Init(txState->TargetPathId, context.SS);

        NIceDb::TNiceDb db(context.Txc.DB);

        txState->PlanStep = step;
        context.SS->PersistTxPlanStep(db, OperationId, step);

        // move shards
        for (const auto& shard : txState->Shards) {
            auto shardIdx = shard.Idx;
            TShardInfo& shardInfo = context.SS->ShardInfos[shardIdx];

            shardInfo.PathId = dstPath->PathId;
            context.SS->DecrementPathDbRefCount(srcPath.Base()->PathId, "move shard");
            context.SS->IncrementPathDbRefCount(dstPath.Base()->PathId, "move shard");
            context.SS->PersistShardPathId(db, shardIdx, dstPath.Base()->PathId);

            srcPath.Base()->DecShardsInside();
            dstPath.Base()->IncShardsInside();
        }

        Y_VERIFY(!context.SS->Tables.contains(dstPath.Base()->PathId));
        Y_VERIFY(context.SS->Tables.contains(srcPath.Base()->PathId));

        TTableInfo::TPtr tableInfo = new TTableInfo(*context.SS->Tables.at(srcPath.Base()->PathId));
        tableInfo->AlterVersion += 1;

        // copy table info
        context.SS->Tables[dstPath.Base()->PathId] = tableInfo;
        context.SS->PersistTable(db, dstPath.Base()->PathId);
        context.SS->PersistTablePartitioning(db, dstPath.Base()->PathId, tableInfo);
        context.SS->PersistTablePartitionStats(db, dstPath.Base()->PathId, tableInfo);
        context.SS->IncrementPathDbRefCount(dstPath.Base()->PathId, "move table info");

        dstPath->StepCreated = step;
        context.SS->PersistCreateStep(db, dstPath.Base()->PathId, step);
        dstPath.DomainInfo()->IncPathsInside();

        dstPath.Activate();
        IncParentDirAlterVersionWithRepublish(OperationId, dstPath, context);

        if (context.SS->EnableSchemeTransactionsAtSchemeShard) {
            // only persist step, but do not set drop step for the src path, wait for replacement publication first
            NextState = TTxState::WaitShadowPathPublication;
            context.SS->ChangeTxState(db, OperationId, TTxState::WaitShadowPathPublication);
            return true;
        }

        MarkSrcDropped(db, context, OperationId, *txState, srcPath);

        Y_VERIFY(!context.SS->TablesWithSnaphots.contains(srcPath.Base()->PathId));

        NextState = TTxState::ProposedWaitParts;
        context.SS->ChangeTxState(db, OperationId, TTxState::ProposedWaitParts);
        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxMoveTable);
        Y_VERIFY(txState->SourcePathId);
        Y_VERIFY(txState->MinStep);

        TSet<TTabletId> shardSet;
        for (const auto& shard : txState->Shards) {
            TShardIdx idx = shard.Idx;
            TTabletId tablet = context.SS->ShardInfos.at(idx).TabletID;
            shardSet.insert(tablet);
        }

        context.OnComplete.ProposeToCoordinator(OperationId, txState->TargetPathId, txState->MinStep, std::move(shardSet));
        return false;
    }
};

class TWaitRenamedPathPublication: public TSubOperationState {
private:
    TOperationId OperationId;

    TPathId ActivePathId;

    TString DebugHint() const override {
        return TStringBuilder()
                << "TMoveTable TWaitRenamedPathPublication"
                << " operationId: " << OperationId;
    }

public:
    TWaitRenamedPathPublication(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {TEvHive::TEvCreateTabletReply::EventType, TEvDataShard::TEvProposeTransactionResult::EventType, TEvPrivate::TEvOperationPlan::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvDataShard::TEvSchemaChanged"
                               << ", save it"
                               << ", at schemeshard: " << ssId);

        NTableState::CollectSchemaChanged(OperationId, ev, context);
        return false;
    }

    bool HandleReply(TEvPrivate::TEvCompletePublication::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvPrivate::TEvCompletePublication"
                               << ", msg: " << ev->Get()->ToString()
                               << ", at tablet" << ssId);

        Y_VERIFY(ActivePathId == ev->Get()->PathId);

        NIceDb::TNiceDb db(context.Txc.DB);
        context.SS->ChangeTxState(db, OperationId, TTxState::DeletePathBarrier);
        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        context.OnComplete.RouteByTabletsFromOperation(OperationId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", operation type: " << TTxState::TypeName(txState->TxType)
                               << ", at tablet" << ssId);

        TPath srcPath = TPath::Init(txState->SourcePathId, context.SS);

        if (srcPath.IsActive()) {
            LOG_DEBUG_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                        DebugHint() << " ProgressState"
                                    << ", no renaming has been detected for this operation");

            NIceDb::TNiceDb db(context.Txc.DB);
            context.SS->ChangeTxState(db, OperationId, TTxState::DeletePathBarrier);
            return true;
        }

        auto activePath = TPath::Resolve(srcPath.PathString(), context.SS);
        Y_VERIFY(activePath.IsResolved());

        Y_VERIFY(activePath != srcPath);

        ActivePathId = activePath->PathId;
        context.OnComplete.PublishAndWaitPublication(OperationId, activePath->PathId);

        return false;
    }
};


class TDeleteTableBarrier: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override {
        return TStringBuilder()
                << "TMoveTable TDeleteTableBarrier"
                << " operationId: " << OperationId;
    }

public:
    TDeleteTableBarrier(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {TEvHive::TEvCreateTabletReply::EventType, TEvDataShard::TEvProposeTransactionResult::EventType, TEvPrivate::TEvOperationPlan::EventType});
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvDataShard::TEvSchemaChanged"
                               << ", save it"
                               << ", at schemeshard: " << ssId);

        NTableState::CollectSchemaChanged(OperationId, ev, context);
        return false;
    }

    bool HandleReply(TEvPrivate::TEvCompleteBarrier::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvPrivate:TEvCompleteBarrier"
                               << ", msg: " << ev->Get()->ToString()
                               << ", at tablet" << ssId);

        NIceDb::TNiceDb db(context.Txc.DB);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        TPath srcPath = TPath::Init(txState->SourcePathId, context.SS);

        Y_VERIFY(txState->PlanStep);

        MarkSrcDropped(db, context, OperationId, *txState, srcPath);

        context.SS->ChangeTxState(db, OperationId, TTxState::ProposedWaitParts);
        return true;
    }
    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        context.OnComplete.RouteByTabletsFromOperation(OperationId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", operation type: " << TTxState::TypeName(txState->TxType)
                               << ", at tablet" << ssId);

        context.OnComplete.Barrier(OperationId, "RenamePathBarrier");
        return false;
    }
};


class TDone: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override {
        return TStringBuilder()
            << "TMoveTable TDone"
            << ", operationId: " << OperationId;
    }
public:
    TDone(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), AllIncomingEvents());
    }

    bool HandleReply(TEvDataShard::TEvSchemaChanged::TPtr& ev, TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();
        const TActorId& ackTo = ev->Sender;

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TProposedDeletePart"
                               << " repeated message, ack it anyway"
                               << " at tablet: " << ssId);

        THolder<TEvDataShard::TEvSchemaChangedResult> event = MakeHolder<TEvDataShard::TEvSchemaChangedResult>();
        event->Record.SetTxId(ui64(OperationId.GetTxId()));

        context.OnComplete.Send(ackTo, std::move(event));
        return false;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxMoveTable);

        LOG_DEBUG_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", SourcePathId: " << txState->SourcePathId
                               << ", TargetPathId: " << txState->TargetPathId
                               << ", at schemeshard: " << ssId);

        // clear resources on src
        NIceDb::TNiceDb db(context.Txc.DB);
        TPathElement::TPtr srcPath = context.SS->PathsById.at(txState->SourcePathId);
        context.OnComplete.ReleasePathState(OperationId, srcPath->PathId, TPathElement::EPathState::EPathStateNotExist);

        TPathElement::TPtr dstPath = context.SS->PathsById.at(txState->TargetPathId);
        context.OnComplete.ReleasePathState(OperationId, dstPath->PathId, TPathElement::EPathState::EPathStateNoChanges);

        context.OnComplete.DoneOperation(OperationId);
        return true;
    }
};


class TMoveTable: public TSubOperation {
    const TOperationId OperationId;
    const TTxTransaction Transaction;
    TTxState::ETxState State = TTxState::Invalid;
    TTxState::ETxState AfterPropose = TTxState::Invalid;

    TTxState::ETxState NextState() {
        return TTxState::ConfigureParts;
    }

    TTxState::ETxState NextState(TTxState::ETxState state) {
        switch(state) {
        case TTxState::Waiting:
        case TTxState::ConfigureParts:
            return TTxState::Propose;
        case TTxState::Propose:
            return AfterPropose;

        case TTxState::WaitShadowPathPublication:
            return TTxState::DeletePathBarrier;
        case TTxState::DeletePathBarrier:
            return TTxState::ProposedWaitParts;

        case TTxState::ProposedWaitParts:
            return TTxState::Done;
        default:
            return TTxState::Invalid;
        }
        return TTxState::Invalid;
    }

    TSubOperationState::TPtr SelectStateFunc(TTxState::ETxState state) {
        switch(state) {
        case TTxState::Waiting:
        case TTxState::ConfigureParts:
            return MakeHolder<TConfigureParts>(OperationId);
        case TTxState::Propose:
            return MakeHolder<TPropose>(OperationId, AfterPropose);
        case TTxState::WaitShadowPathPublication:
            return MakeHolder<TWaitRenamedPathPublication>(OperationId);
        case TTxState::DeletePathBarrier:
            return MakeHolder<TDeleteTableBarrier>(OperationId);
        case TTxState::ProposedWaitParts:
            return MakeHolder<NTableState::TProposedWaitParts>(OperationId);
        case TTxState::Done:
            return MakeHolder<TDone>(OperationId);
        default:
            return nullptr;
        }
    }

    void StateDone(TOperationContext& context) override {
        State = NextState(State);

        if (State != TTxState::Invalid) {
            SetState(SelectStateFunc(State));
            context.OnComplete.ActivateTx(OperationId);
        }
    }

public:
    TMoveTable(TOperationId id, const TTxTransaction& tx)
        : OperationId(id)
        , Transaction(tx)
    {
    }

    TMoveTable(TOperationId id, TTxState::ETxState state)
        : OperationId(id)
        , State(state)
    {
        SetState(SelectStateFunc(state));
    }

    THolder<TProposeResponse> Propose(const TString&, TOperationContext& context) override {
        const TTabletId ssId = context.SS->SelfTabletId();

        const auto acceptExisted = !Transaction.GetFailOnExist();
        const auto& opDescr = Transaction.GetMoveTable();

        const TString& srcPathStr = opDescr.GetSrcPath();
        const TString& dstPathStr = opDescr.GetDstPath();

        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TMoveTable Propose"
                         << ", from: "<< srcPathStr
                         << ", to: " << dstPathStr
                         << ", opId: " << OperationId
                         << ", at schemeshard: " << ssId);

        THolder<TProposeResponse> result;
        result.Reset(new TEvSchemeShard::TEvModifySchemeTransactionResult(
            NKikimrScheme::StatusAccepted, ui64(OperationId.GetTxId()), ui64(ssId)));

        TString errStr;

        TPath srcPath = TPath::Resolve(srcPathStr, context.SS);
        {
            TPath::TChecker checks = srcPath.Check();
            checks
                .NotEmpty()
                .NotUnderDomainUpgrade()
                .IsAtLocalSchemeShard()
                .IsResolved()
                .NotDeleted()
                .IsTable()
                .NotBackupTable()
                .NotUnderTheSameOperation(OperationId.GetTxId())
                .NotUnderOperation();

            if (!checks) {
                TString explain = TStringBuilder() << "path fail checks"
                                                   << ", path: " << srcPath.PathString();
                auto status = checks.GetStatus(&explain);
                result->SetError(status, explain);
                return result;
            }
        }

        TPath dstPath = TPath::Resolve(dstPathStr, context.SS);
        TPath dstParent = dstPath.Parent();

        {
            TPath::TChecker checks = dstParent.Check();
            checks
                .NotUnderDomainUpgrade()
                .IsAtLocalSchemeShard()
                .IsResolved();

                if (dstParent.IsUnderDeleting()) {
                    checks
                        .IsUnderDeleting()
                        .IsUnderTheSameOperation(OperationId.GetTxId());
                } else if (dstParent.IsUnderMoving()) {
                    // it means that dstPath is free enough to be the move destination
                    checks
                        .IsUnderMoving()
                        .IsUnderTheSameOperation(OperationId.GetTxId());
                } else if (dstParent.IsUnderCreating()) {
                    checks
                        .IsUnderCreating()
                        .IsUnderTheSameOperation(OperationId.GetTxId());
                } else {
                    checks
                        .NotUnderOperation();
                }

            if (!checks) {
                TString explain = TStringBuilder() << "parent dst path fail checks"
                                                   << ", path: " << dstParent.PathString();
                auto status = checks.GetStatus(&explain);
                result->SetError(status, explain);
                return result;
            }
        }

        if (dstParent.IsUnderOperation()) {
            dstPath = TPath::ResolveWithInactive(OperationId, dstPathStr, context.SS);
        }

        {
            TPath::TChecker checks = dstPath.Check();
            checks.IsAtLocalSchemeShard();
            if (dstPath.IsResolved()) {
                checks
                    .IsResolved();

                if (dstPath.IsUnderDeleting()) {
                    checks
                        .IsUnderDeleting()
                        .IsUnderTheSameOperation(OperationId.GetTxId());
                } else if (dstPath.IsUnderMoving()) {
                    // it means that dstPath is free enough to be the move destination
                    checks
                        .IsUnderMoving()
                        .IsUnderTheSameOperation(OperationId.GetTxId());
                } else {
                    checks
                        .NotUnderTheSameOperation(OperationId.GetTxId())
                        .FailOnExist(TPathElement::EPathType::EPathTypeTable, acceptExisted);
                }
            } else {
                checks
                    .NotEmpty()
                    .NotResolved();
            }

            if (checks) {
                checks
                    .DepthLimit()
                    .IsValidLeafName();
            }

            if (!checks) {
                TString explain = TStringBuilder() << "dst path fail checks"
                                                   << ", path: " << dstPath.PathString();
                auto status = checks.GetStatus(&explain);
                result->SetError(status, explain);
                return result;
            }
        }

        if (!context.SS->CheckApplyIf(Transaction, errStr)) {
            result->SetError(NKikimrScheme::StatusPreconditionFailed, errStr);
            return result;
        }

        if (!context.SS->CheckLocks(srcPath.Base()->PathId, Transaction, errStr)) {
            result->SetError(NKikimrScheme::StatusMultipleModifications, errStr);
            return result;
        }

        TPathId allocatedPathId = context.SS->AllocatePathId();
        context.MemChanges.GrabNewPath(context.SS, allocatedPathId);
        context.MemChanges.GrabPath(context.SS, dstParent.Base()->PathId);
        context.MemChanges.GrabPath(context.SS, srcPath.Base()->PathId);
        context.MemChanges.GrabPath(context.SS, srcPath.Base()->ParentPathId);
        context.MemChanges.GrabNewTxState(context.SS, OperationId);
        context.MemChanges.GrabNewIndex(context.SS, allocatedPathId);

        context.DbChanges.PersistPath(allocatedPathId);
        context.DbChanges.PersistPath(dstParent.Base()->PathId);
        context.DbChanges.PersistPath(srcPath.Base()->PathId);
        context.DbChanges.PersistPath(srcPath.Base()->ParentPathId);
        context.DbChanges.PersistApplyUserAttrs(allocatedPathId);
        context.DbChanges.PersistTxState(OperationId);

        // create new path and inherit properties from src
        dstPath.MaterializeLeaf(srcPath.Base()->Owner, allocatedPathId, /*allowInactivePath*/ true);
        result->SetPathId(dstPath.Base()->PathId.LocalPathId);
        dstPath.Base()->CreateTxId = OperationId.GetTxId();
        dstPath.Base()->LastTxId = OperationId.GetTxId();
        dstPath.Base()->PathState = TPathElement::EPathState::EPathStateCreate;
        dstPath.Base()->PathType = TPathElement::EPathType::EPathTypeTable;
        dstPath.Base()->UserAttrs->AlterData = srcPath.Base()->UserAttrs;
        dstPath.Base()->ACL = srcPath.Base()->ACL;

        dstParent.Base()->IncAliveChildren();

        // create tx state, do not catch shards right now
        TTxState& txState = context.SS->CreateTx(OperationId, TTxState::TxMoveTable, dstPath.Base()->PathId, srcPath.Base()->PathId);
        txState.State = TTxState::ConfigureParts;

        srcPath->PathState = TPathElement::EPathState::EPathStateMoving;
        srcPath->LastTxId = OperationId.GetTxId();

        IncParentDirAlterVersionWithRepublish(OperationId, dstPath, context);
        IncParentDirAlterVersionWithRepublish(OperationId, srcPath, context);

        // wait splits
        TTableInfo::TPtr tableSrc = context.SS->Tables.at(srcPath.Base()->PathId);
        for (auto splitTx: tableSrc->GetSplitOpsInFlight()) {
            context.OnComplete.Dependence(splitTx.GetTxId(), OperationId.GetTxId());
        }

        context.OnComplete.ActivateTx(OperationId);
        State = NextState();
        SetState(SelectStateFunc(State));

        return result;
    }

    void AbortPropose(TOperationContext& context) override {
        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TMoveTable AbortPropose"
                         << ", opId: " << OperationId
                         << ", at schemeshard: " << context.SS->TabletID());
    }

    void AbortUnsafe(TTxId forceDropTxId, TOperationContext& context) override {
        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TMoveTable AbortUnsafe"
                         << ", opId: " << OperationId
                         << ", forceDropId: " << forceDropTxId
                         << ", at schemeshard: " << context.SS->TabletID());

        context.OnComplete.DoneOperation(OperationId);
    }
};

}

namespace NKikimr {
namespace NSchemeShard {

ISubOperationBase::TPtr CreateMoveTable(TOperationId id, const TTxTransaction& tx) {
    return new TMoveTable(id, tx);
}

ISubOperationBase::TPtr CreateMoveTable(TOperationId id, TTxState::ETxState state) {
    Y_VERIFY(state != TTxState::Invalid);
    return new TMoveTable(id, state);
}

}
}
