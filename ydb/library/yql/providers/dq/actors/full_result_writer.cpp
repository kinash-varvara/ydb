#include "proto_builder.h"
#include "full_result_writer.h"

#include <ydb/library/yql/providers/dq/actors/actor_helpers.h>
#include <ydb/library/yql/providers/dq/actors/events.h>
#include <ydb/library/yql/providers/dq/api/protos/service.pb.h>

#include <ydb/library/yql/core/issue/yql_issue.h>

#include <ydb/library/yql/utils/log/log.h>
#include <ydb/library/yql/utils/failure_injector/failure_injector.h>
#include <ydb/library/yql/utils/yql_panic.h>

#include <library/cpp/actors/core/actor.h>

#include <util/generic/size_literals.h>
#include <util/system/env.h>

#include <utility>

namespace NYql::NDqs {

using namespace NKikimr::NMiniKQL;

class TFullResultWriterActor : public NActors::TActor<TFullResultWriterActor> {
public:
    static constexpr char ActorName[] = "YQL_DQ_FULL_RESULT_WRITER";

    explicit TFullResultWriterActor(const TString& traceId,
        const TString& resultType,
        THolder<IDqFullResultWriter>&& writer,
        const NActors::TActorId& aggregatorId)
        : NActors::TActor<TFullResultWriterActor>(&TFullResultWriterActor::Handler)
        , TraceID(traceId)
        , ResultBuilder(MakeHolder<TProtoBuilder>(resultType, TVector<TString>()))
        , FullResultWriter(std::move(writer))
        , AggregatorID(aggregatorId)
    {
    }

private:
    STRICT_STFUNC(Handler, {
        cFunc(NActors::TEvents::TEvPoison::EventType, PassAway)
        HFunc(TEvFullResultWriterWriteRequest, OnWriteRequest)
        HFunc(TEvFullResultWriterStatusRequest, OnStatusRequest)  // legacy
    })

    void PassAway() override {
        YQL_LOG_CTX_SCOPE(TraceID);
        YQL_LOG(DEBUG) << __FUNCTION__;
        try {
            FullResultWriter->Abort();
        } catch (...) {
            YQL_LOG(WARN) << "FullResultWriter->Abort(): " << CurrentExceptionMessage();
        }
        ResultBuilder.Reset();
        FullResultWriter.Reset();

        Send(AggregatorID, MakeHolder<NActors::TEvents::TEvGone>());

        NActors::TActor<TThis>::PassAway();
    }

    void OnStatusRequest(TEvFullResultWriterStatusRequest::TPtr&, const NActors::TActorContext&) {
        YQL_LOG_CTX_SCOPE(TraceID);
        NDqProto::TFullResultWriterStatusResponse response;
        response.SetBytesReceived(BytesReceived);
        if (ErrorMessage) {
            response.SetErrorMessage(*ErrorMessage);
        }
        Send(AggregatorID, new TEvFullResultWriterStatusResponse(response));
    }

    void OnWriteRequest(TEvFullResultWriterWriteRequest::TPtr& ev, const NActors::TActorContext&) {
        YQL_LOG_CTX_SCOPE(TraceID);
        auto& request = ev->Get()->Record;
        if (request.GetFinish()) {
            Finish();
        } else {
            Continue(request);
        }
    }

    void Finish() {
        YQL_LOG(DEBUG) << __FUNCTION__;
        try {
            TFailureInjector::Reach("full_result_fail_on_finish", [] { throw yexception() << "full_result_fail_on_finish"; });
            FullResultWriter->Finish();
            if (ErrorMessage) {
                Send(AggregatorID, MakeHolder<TEvDqFailure>(NYql::NDqProto::StatusIds::INTERNAL_ERROR, *ErrorMessage, false, true));
            } else {
                Send(AggregatorID, MakeHolder<TEvDqFailure>().Release());
            }
        } catch (...) {
            TIssue issue(CurrentExceptionMessage());
            issue.SetCode(TIssuesIds::DQ_GATEWAY_NEED_FALLBACK_ERROR, TSeverityIds::S_ERROR);
            if (ErrorMessage) {
                issue.AddSubIssue(MakeIntrusive<TIssue>(*ErrorMessage));
            }
            Send(AggregatorID, MakeHolder<TEvDqFailure>(NYql::NDqProto::StatusIds::INTERNAL_ERROR, issue, false, false).Release());
        }
        Send(SelfId(), MakeHolder<NActors::TEvents::TEvPoison>());
    }

    void Continue(NDqProto::TFullResultWriterWriteRequest& request) {
        YQL_LOG(DEBUG) << "Continue -- RowCount = " << FullResultWriter->GetRowCount();
        ui64 reqSize = request.ByteSizeLong();
        WriteToFullResultTable(request);
        BytesReceived += reqSize;
    }

    void WriteToFullResultTable(NDqProto::TFullResultWriterWriteRequest& request) {
        if (ErrorMessage) {
            YQL_LOG(DEBUG) << "Failed to write previous chunk, aborting";
            return;
        }

        try {
            TFailureInjector::Reach("full_result_fail_on_write", [] { throw yexception() << "full_result_fail_on_write"; });
            ResultBuilder->WriteData(request.GetData(), [writer = FullResultWriter.Get()](const NUdf::TUnboxedValuePod& value) {
                writer->AddRow(value);
                return true;
            });
            NDqProto::TFullResultWriterAck ackRecord; 
            ackRecord.SetMessageId(request.GetMessageId());
            Send(AggregatorID, MakeHolder<TEvFullResultWriterAck>(ackRecord));
        } catch (...) {
            ErrorMessage = CurrentExceptionMessage();
            Send(AggregatorID, MakeHolder<TEvDqFailure>(NYql::NDqProto::StatusIds::INTERNAL_ERROR, *ErrorMessage, false, true));
        }

        if (ErrorMessage) {
            YQL_LOG(DEBUG) << "An error occurred: " << *ErrorMessage;
        }
    }

private:
    const TString TraceID;
    THolder<TProtoBuilder> ResultBuilder;
    THolder<IDqFullResultWriter> FullResultWriter;
    NActors::TActorId AggregatorID;

    ui64 BytesReceived{0};
    TMaybe<TString> ErrorMessage;
};


THolder<NActors::IActor> MakeFullResultWriterActor(
    const TString& traceId,
    const TString& resultType,
    THolder<IDqFullResultWriter>&& writer,
    const NActors::TActorId& aggregatorId)
{
    return MakeHolder<TFullResultWriterActor>(traceId, resultType, std::move(writer), aggregatorId);
}

} // namespace NYql::NDqs
