UNITTEST_FOR(ydb/core/tx/schemeshard) 
 
OWNER( 
    ilnaz 
    g:kikimr 
) 
 
FORK_SUBTESTS() 
 
SPLIT_FACTOR(2) 
 
IF (SANITIZER_TYPE OR WITH_VALGRIND) 
    TIMEOUT(3600) 
    SIZE(LARGE) 
    TAG(ya:fat) 
ELSE() 
    TIMEOUT(600) 
    SIZE(MEDIUM) 
ENDIF() 
 
PEERDIR( 
    ydb/core/tx/schemeshard/ut_helpers 
) 
 
SRCS( 
    ut_replication_reboots.cpp 
) 
 
YQL_LAST_ABI_VERSION() 
 
END() 
