LIBRARY() 
 
OWNER( 
    va-kuznecov 
    g:kikimr 
) 
 
PEERDIR( 
    library/cpp/actors/core
    library/cpp/monlib/dynamic_counters
    util 
    ydb/core/base
    ydb/core/mon
    ydb/core/node_whiteboard
) 
 
SRCS( 
    defs.h 
    immediate_control_board_actor.cpp 
    immediate_control_board_actor.h 
    immediate_control_board_control.cpp 
    immediate_control_board_control.h 
    immediate_control_board_impl.cpp 
    immediate_control_board_impl.h 
    immediate_control_board_wrapper.h 
) 
 
END() 

RECURSE_FOR_TESTS(
    ut
)
