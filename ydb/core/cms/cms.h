#pragma once

#include "defs.h"
#include "cluster_info.h"
#include "cms_state.h"

#include <library/cpp/actors/interconnect/events_local.h>
#include <library/cpp/actors/core/actor.h>
#include <ydb/core/protos/cms.pb.h>

/**
 * Here we declare interface for CMS (Cluster Management System) tablet whose intention
 * is to help with cluster maintenance. Primary CMS functionality includes:
 *   - Grant permissions for maintenance actions (such as restart service or server,
 *     replace disk etc.). Permission is given when requested action doesn't break
 *     cluster availability. Current cluster status is collected from NodeWhiteboard
 *     services at request processing time
 *   - Manage issued permissions. Users may examine own permissions, extend and
 *     reject them. Issued permissions affect cluster state and are taken into
 *     account when other permission requests are processed
 *   - Manage permission requests. If required action is temporarily disallowed
 *     then user may put its request into requests queue and track its status.
 *
 * Currently CMS functionality doesn't include any manipulations with cluster. Thusly
 * it doesn't affect cluster state and configuration. Such functionality is planned
 * for the future though.
 *
 * More info about CMS may be found at:
 *     https://wiki.yandex-team.ru/users/ienkovich/docs/ydb/cms/
 */

namespace NKikimr {
namespace NCms {

struct TEvCms {
    enum EEv {
        EvClusterStateRequest = EventSpaceBegin(TKikimrEvents::ES_CMS),
        EvClusterStateResponse,
        EvPermissionRequest,
        EvCheckRequest,
        EvConditionalPermissionRequest,
        EvPermissionResponse,
        EvManageRequestRequest,
        EvManageRequestResponse,
        EvManagePermissionRequest,
        EvManagePermissionResponse,
        EvNotification,
        EvNotificationResponse,
        EvManageNotificationRequest,
        EvManageNotificationResponse,
        EvGetConfigRequest,
        EvSetConfigRequest,
        EvSetMarkerRequest,
        EvResetMarkerRequest,
        EvGetLogTailRequest,
        EvGetLogTailResponse,

        EvWalleCreateTaskRequest = EvClusterStateRequest + 512,
        EvWalleCreateTaskResponse,
        EvWalleListTasksRequest,
        EvWalleListTasksResponse,
        EvWalleCheckTaskRequest,
        EvWalleCheckTaskResponse,
        EvWalleRemoveTaskRequest,
        EvWalleRemoveTaskResponse,
        EvStoreWalleTask,
        EvWalleTaskStored,
        EvRemoveWalleTask,
        EvWalleTaskRemoved,
        EvGetConfigResponse,
        EvSetConfigResponse,
        EvSetMarkerResponse,
        EvResetMarkerResponse,

        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(TKikimrEvents::ES_CMS), "expect EvEnd < EventSpaceEnd(TKikimrEvents::ES_CMS)");

    template <typename TEv, typename TRecord, ui32 TEventType>
    using TEventPB = TEventShortDebugPB<TEv, TRecord, TEventType>;

    struct TEvClusterStateRequest : public TEventPB<TEvClusterStateRequest,
                                                    NKikimrCms::TClusterStateRequest,
                                                    EvClusterStateRequest> {
    };

    struct TEvClusterStateResponse : public TEventPB<TEvClusterStateResponse,
                                                     NKikimrCms::TClusterStateResponse,
                                                     EvClusterStateResponse> {
    };

    struct TEvPermissionRequest : public TEventPB<TEvPermissionRequest,
                                                  NKikimrCms::TPermissionRequest,
                                                  EvPermissionRequest> {
    };

    struct TEvCheckRequest : public TEventPB<TEvCheckRequest,
                                             NKikimrCms::TCheckRequest,
                                             EvCheckRequest> {
    };

    struct TEvConditionalPermissionRequest : public TEventPB<TEvConditionalPermissionRequest,
                                                             NKikimrCms::TConditionalPermissionRequest,
                                                             EvConditionalPermissionRequest> {
    };

    struct TEvPermissionResponse : public TEventPB<TEvPermissionResponse,
                                                   NKikimrCms::TPermissionResponse,
                                                   EvPermissionResponse> {
    };

    struct TEvManageRequestRequest : public TEventPB<TEvManageRequestRequest,
                                                     NKikimrCms::TManageRequestRequest,
                                                     EvManageRequestRequest> {
    };

    struct TEvManageRequestResponse : public TEventPB<TEvManageRequestResponse,
                                                      NKikimrCms::TManageRequestResponse,
                                                      EvManageRequestResponse> {
    };

    struct TEvManagePermissionRequest : public TEventPB<TEvManagePermissionRequest,
                                                        NKikimrCms::TManagePermissionRequest,
                                                        EvManagePermissionRequest> {
    };

    struct TEvManagePermissionResponse : public TEventPB<TEvManagePermissionResponse,
                                                         NKikimrCms::TManagePermissionResponse,
                                                         EvManagePermissionResponse> {
    };

    struct TEvNotification : public TEventPB<TEvNotification,
                                             NKikimrCms::TNotification,
                                             EvNotification> {
    };

    struct TEvNotificationResponse : public TEventPB<TEvNotificationResponse,
                                                     NKikimrCms::TNotificationResponse,
                                                     EvNotificationResponse> {
    };

    struct TEvManageNotificationRequest : public TEventPB<TEvManageNotificationRequest,
                                                          NKikimrCms::TManageNotificationRequest,
                                                          EvManageNotificationRequest> {
    };

    struct TEvManageNotificationResponse : public TEventPB<TEvManageNotificationResponse,
                                                           NKikimrCms::TManageNotificationResponse,
                                                           EvManageNotificationResponse> {
    };

    struct TEvWalleCreateTaskRequest : public TEventPB<TEvWalleCreateTaskRequest,
                                                       NKikimrCms::TWalleCreateTaskRequest,
                                                       EvWalleCreateTaskRequest> {
    };

    struct TEvWalleCreateTaskResponse : public TEventPB<TEvWalleCreateTaskResponse,
                                                        NKikimrCms::TWalleCreateTaskResponse,
                                                        EvWalleCreateTaskResponse> {
    };

    struct TEvWalleListTasksRequest : public TEventPB<TEvWalleListTasksRequest,
                                                      NKikimrCms::TWalleListTasksRequest,
                                                      EvWalleListTasksRequest> {
    };

    struct TEvWalleListTasksResponse : public TEventPB<TEvWalleListTasksResponse,
                                                       NKikimrCms::TWalleListTasksResponse,
                                                       EvWalleListTasksResponse> {
    };

    struct TEvWalleCheckTaskRequest : public TEventPB<TEvWalleCheckTaskRequest,
                                                      NKikimrCms::TWalleCheckTaskRequest,
                                                      EvWalleCheckTaskRequest> {
    };

    struct TEvWalleCheckTaskResponse : public TEventPB<TEvWalleCheckTaskResponse,
                                                       NKikimrCms::TWalleCheckTaskResponse,
                                                       EvWalleCheckTaskResponse> {
    };

    struct TEvWalleRemoveTaskRequest : public TEventPB<TEvWalleRemoveTaskRequest,
                                                       NKikimrCms::TWalleRemoveTaskRequest,
                                                       EvWalleRemoveTaskRequest> {
    };

    struct TEvWalleRemoveTaskResponse : public TEventPB<TEvWalleRemoveTaskResponse,
                                                        NKikimrCms::TWalleRemoveTaskResponse,
                                                        EvWalleRemoveTaskResponse> {
    };

    struct TEvStoreWalleTask : public TEventLocal<TEvStoreWalleTask, EvStoreWalleTask> {
        TWalleTaskInfo Task;

        TString ToString() const override
        {
            return Sprintf("%s { Task: %s }", ToStringHeader().data(), Task.ToString().data());
        }
    };

    struct TEvWalleTaskStored : public TEventLocal<TEvWalleTaskStored, EvWalleTaskStored> {
        TString TaskId;

        TEvWalleTaskStored(TString id)
            : TaskId(id)
        {
        }

        TString ToString() const override
        {
            return Sprintf("%s { TaskId: %s }", ToStringHeader().data(), TaskId.data());
        }
    };

    struct TEvRemoveWalleTask : public TEventLocal<TEvRemoveWalleTask, EvRemoveWalleTask> {
        TString TaskId;

        TString ToString() const override
        {
            return Sprintf("%s { TaskId: %s }", ToStringHeader().data(), TaskId.data());
        }
    };

    struct TEvWalleTaskRemoved : public TEventLocal<TEvWalleTaskRemoved, EvWalleTaskRemoved> {
        TString TaskId;

        TEvWalleTaskRemoved(TString id)
            : TaskId(id)
        {
        }

        TString ToString() const override
        {
            return Sprintf("%s { TaskId: %s }", ToStringHeader().data(), TaskId.data());
        }
    };

    struct TEvGetConfigRequest : public TEventPB<TEvGetConfigRequest,
                                                 NKikimrCms::TGetConfigRequest,
                                                 EvGetConfigRequest> {
    };

    struct TEvGetConfigResponse : public TEventPB<TEvGetConfigResponse,
                                                  NKikimrCms::TGetConfigResponse,
                                                  EvGetConfigResponse> {
    };

    struct TEvSetConfigRequest : public TEventPB<TEvSetConfigRequest,
                                                 NKikimrCms::TSetConfigRequest,
                                                 EvSetConfigRequest> {
    };

    struct TEvSetConfigResponse : public TEventPB<TEvSetConfigResponse,
                                                  NKikimrCms::TSetConfigResponse,
                                                  EvSetConfigResponse> {
    };

    struct TEvSetMarkerRequest : public TEventPB<TEvSetMarkerRequest,
                                                 NKikimrCms::TSetMarkerRequest,
                                                 EvSetMarkerRequest> {
    };

    struct TEvSetMarkerResponse : public TEventPB<TEvSetMarkerResponse,
                                                  NKikimrCms::TSetMarkerResponse,
                                                  EvSetMarkerResponse> {
    };

    struct TEvResetMarkerRequest : public TEventPB<TEvResetMarkerRequest,
                                                   NKikimrCms::TResetMarkerRequest,
                                                   EvResetMarkerRequest> {
    };

    struct TEvResetMarkerResponse : public TEventPB<TEvResetMarkerResponse,
                                                    NKikimrCms::TResetMarkerResponse,
                                                    EvResetMarkerResponse> {
    };

    struct TEvGetLogTailRequest : public TEventPB<TEvGetLogTailRequest,
                                                  NKikimrCms::TGetLogTailRequest,
                                                  EvGetLogTailRequest> {
    };

    struct TEvGetLogTailResponse : public TEventPB<TEvGetLogTailResponse,
                                                   NKikimrCms::TGetLogTailResponse,
                                                   EvGetLogTailResponse> {
    };
};

IActor *CreateCms(const TActorId &tablet, TTabletStorageInfo *info);

} // NCms
} // NKikimr
