OWNER(
    alexvru
    g:kikimr 
)

LIBRARY()

SRCS(
    basic_test.cpp
    objectwithstate.cpp
)

PEERDIR(
    library/cpp/actors/protos
    ydb/core/base
    ydb/core/blobstorage/pdisk
    ydb/library/pdisk_io
)

END()
