LIBRARY()

OWNER(
    ddoarn
    g:kikimr 
)

SRCS(
    defs.h
    flat_boot_misc.cpp
    flat_comp.cpp
    flat_comp_create.cpp
    flat_comp_gen.cpp
    flat_comp_shard.cpp
    flat_cxx_database.h
    flat_database.cpp
    flat_database.h
    flat_dbase_scheme.cpp
    flat_dbase_apply.cpp
    flat_exec_broker.cpp
    flat_exec_commit.cpp
    flat_exec_commit_mgr.cpp
    flat_exec_seat.cpp
    flat_executor.cpp
    flat_executor.h
    flat_executor_bootlogic.cpp
    flat_executor_bootlogic.h
    flat_executor_borrowlogic.cpp
    flat_executor_borrowlogic.h
    flat_executor_compaction_logic.cpp
    flat_executor_compaction_logic.h
    flat_executor_counters.cpp
    flat_executor_counters.h
    flat_executor_db_mon.cpp
    flat_executor_gclogic.cpp
    flat_executor_gclogic.h
    flat_bio_actor.cpp
    flat_executor_snapshot.cpp
    flat_executor_txloglogic.cpp
    flat_executor_txloglogic.h
    flat_iterator.h
    flat_load_blob_queue.cpp
    flat_mem_warm.cpp
    flat_sausagecache.cpp
    flat_sausagecache.h
    flat_sausage_meta.cpp
    flat_page_label.cpp
    flat_part_dump.cpp
    flat_part_iter_multi.cpp
    flat_part_loader.cpp
    flat_part_overlay.cpp
    flat_part_outset.cpp
    flat_part_slice.cpp
    flat_range_cache.cpp
    flat_row_versions.cpp
    flat_stat_part.cpp
    flat_stat_part.h
    flat_stat_table.h
    flat_stat_table.cpp
    flat_store_hotdog.cpp
    flat_table.cpp
    flat_table.h
    flat_table_part.cpp
    flat_table_part.h
    flat_table_misc.cpp
    flat_update_op.h
    probes.cpp
    shared_handle.cpp
    shared_sausagecache.cpp
    shared_sausagecache.h
    tablet_flat_executor.h
    tablet_flat_executor.cpp
    tablet_flat_executed.h
    tablet_flat_executed.cpp
    flat_executor.proto
)

GENERATE_ENUM_SERIALIZATION(flat_comp_gen.h)

GENERATE_ENUM_SERIALIZATION(flat_comp_shard.h)

GENERATE_ENUM_SERIALIZATION(flat_part_loader.h)

GENERATE_ENUM_SERIALIZATION(flat_executor_compaction_logic.h)

GENERATE_ENUM_SERIALIZATION(flat_row_eggs.h)

IF (KIKIMR_TABLET_BORROW_WITHOUT_META)
    CFLAGS(
        -DKIKIMR_TABLET_BORROW_WITHOUT_META=1
    )
ENDIF()

PEERDIR(
    contrib/libs/protobuf
    library/cpp/containers/intrusive_rb_tree
    library/cpp/containers/stack_vector
    library/cpp/digest/crc32c
    library/cpp/html/pcdata
    library/cpp/lwtrace
    library/cpp/lwtrace/mon
    ydb/core/base
    ydb/core/control
    ydb/core/protos
    ydb/core/tablet
    ydb/core/tablet_flat/protos
    ydb/library/binary_json
    ydb/library/dynumber
    ydb/library/mkql_proto/protos
)

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
    ut_large
)
