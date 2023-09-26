LIBRARY()

SRCS(
    yql_dq_control.cpp
    yql_dq_control.h
    yql_dq_datasink_constraints.cpp
    yql_dq_datasink_type_ann.cpp
    yql_dq_datasink_type_ann.h
    yql_dq_datasource_constraints.cpp
    yql_dq_datasource_type_ann.cpp
    yql_dq_datasource_type_ann.h
    yql_dq_gateway.cpp
    yql_dq_gateway.h
    yql_dq_provider.cpp
    yql_dq_provider.h
    yql_dq_datasink.cpp
    yql_dq_datasink.h
    yql_dq_datasource.cpp
    yql_dq_datasource.h
    yql_dq_recapture.cpp
    yql_dq_recapture.h
    yql_dq_statistics.cpp
    yql_dq_statistics.h
    yql_dq_statistics_json.cpp
    yql_dq_statistics_json.h
    yql_dq_validate.cpp
)

PEERDIR(
    library/cpp/grpc/client
    library/cpp/threading/task_scheduler
    library/cpp/threading/future
    library/cpp/svnversion
    library/cpp/yson/node
    library/cpp/yson
    ydb/public/lib/yson_value
    ydb/public/sdk/cpp/client/ydb_driver
    ydb/library/yql/core
    ydb/library/yql/core/issue
    ydb/library/yql/utils/backtrace
    ydb/library/yql/dq/integration
    ydb/library/yql/dq/integration/transform
    ydb/library/yql/dq/transform
    ydb/library/yql/dq/tasks
    ydb/library/yql/dq/type_ann
    ydb/library/yql/providers/common/gateway
    ydb/library/yql/providers/common/metrics
    ydb/library/yql/providers/common/schema/expr
    ydb/library/yql/providers/common/transform
    ydb/library/yql/providers/common/activation
    ydb/library/yql/providers/common/proto
    ydb/library/yql/providers/dq/api/grpc
    ydb/library/yql/providers/dq/api/protos
    ydb/library/yql/providers/dq/common
    ydb/library/yql/providers/dq/config
    ydb/library/yql/providers/dq/expr_nodes
    ydb/library/yql/providers/dq/opt
    ydb/library/yql/providers/dq/planner
    ydb/library/yql/providers/result/expr_nodes
    ydb/library/yql/minikql
)

YQL_LAST_ABI_VERSION()

END()

RECURSE(
    exec
)

RECURSE_FOR_TESTS(
    ut
)
