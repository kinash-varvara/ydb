LIBRARY() 
 
OWNER( 
    cthulhu 
    va-kuznecov 
    g:kikimr 
) 
 
SRCS( 
    blobstorage_probes.cpp 
) 
 
PEERDIR( 
    contrib/libs/grpc 
    library/cpp/lwtrace/protos
    ydb/core/base
    ydb/core/protos
) 
 
END() 
