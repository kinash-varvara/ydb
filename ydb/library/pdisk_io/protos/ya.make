PROTO_LIBRARY() 
 
OWNER(
    va-kuznecov
    g:kikimr
)
 
IF (OS_WINDOWS) 
    NO_OPTIMIZE_PY_PROTOS() 
ENDIF() 
 
SRCS( 
    sector_map.proto 
) 
 
EXCLUDE_TAGS(GO_PROTO) 
 
END() 
