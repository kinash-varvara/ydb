LIBRARY()

OWNER(
    xenoxeno
    g:kikimr 
)

SRCS(
    balancer.cpp
    balancer.h
    boot_queue.cpp
    boot_queue.h
    domain_info.h
    drain.cpp
    fill.cpp
    hive.cpp
    hive.h
    hive_domains.cpp
    hive_domains.h
    hive_events.h
    hive_impl.cpp
    hive_impl.h
    hive_log.cpp
    hive_log.h
    hive_schema.h
    hive_statics.cpp
    hive_transactions.h
    leader_tablet_info.cpp
    leader_tablet_info.h
    metrics.h
    monitoring.cpp
    node_info.cpp
    node_info.h
    sequencer.cpp
    sequencer.h
    follower_group.h
    follower_tablet_info.cpp
    follower_tablet_info.h
    storage_group_info.cpp
    storage_group_info.h
    storage_pool_info.cpp
    storage_pool_info.h
    tablet_info.cpp
    tablet_info.h
    tx__adopt_tablet.cpp
    tx__block_storage_result.cpp
    tx__configure_subdomain.cpp
    tx__create_tablet.cpp
    tx__cut_tablet_history.cpp
    tx__delete_tablet.cpp
    tx__delete_tablet_result.cpp
    tx__disconnect_node.cpp
    tx__init_scheme.cpp
    tx__kill_node.cpp
    tx__load_everything.cpp
    tx__lock_tablet.cpp
    tx__process_boot_queue.cpp
    tx__process_pending_operations.cpp
    tx__reassign_groups.cpp
    tx__register_node.cpp
    tx__release_tablets.cpp
    tx__release_tablets_reply.cpp
    tx__request_tablet_seq.cpp
    tx__response_tablet_seq.cpp
    tx__restart_tablet.cpp
    tx__seize_tablets.cpp
    tx__seize_tablets_reply.cpp
    tx__resume_tablet.cpp
    tx__start_tablet.cpp
    tx__status.cpp
    tx__stop_tablet.cpp
    tx__switch_drain.cpp
    tx__sync_tablets.cpp
    tx__unlock_tablet.cpp
    tx__update_domain.cpp
    tx__update_tablet_groups.cpp
    tx__update_tablet_metrics.cpp
    tx__update_tablet_status.cpp
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/interconnect
    library/cpp/json
    library/cpp/monlib/dynamic_counters
    ydb/core/base
    ydb/core/blobstorage/base
    ydb/core/blobstorage/crypto
    ydb/core/blobstorage/nodewarden
    ydb/core/engine/minikql
    ydb/core/node_whiteboard
    ydb/core/protos
    ydb/core/sys_view/common
    ydb/core/tablet
    ydb/core/tablet_flat
)

END()

RECURSE_FOR_TESTS(
    ut
)
