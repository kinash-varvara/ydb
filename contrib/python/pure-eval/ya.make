# Generated by devtools/yamaker (pypi).

PY3_LIBRARY()

VERSION(0.2.2)

LICENSE(MIT)

NO_LINT()

PY_SRCS(
    TOP_LEVEL
    pure_eval/__init__.py
    pure_eval/core.py
    pure_eval/my_getattr_static.py
    pure_eval/utils.py
    pure_eval/version.py
)

RESOURCE_FILES(
    PREFIX contrib/python/pure-eval/
    .dist-info/METADATA
    .dist-info/top_level.txt
    pure_eval/py.typed
)

END()