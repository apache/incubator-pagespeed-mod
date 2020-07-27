def pagespeed_cc_test(
        name,
        srcs = [],
        data = [],
        # List of pairs (Bazel shell script target, shell script args)
        repository = "",
        external_deps = [],
        deps = [],
        tags = [],
        args = [],
        copts = [],
        shard_count = None,
        coverage = True,
        local = False,  
        size = "medium"):
    test_lib_tags = []
    native.cc_test(
        name = name,
        copts = copts,
        linkstatic=True,
        srcs = srcs,
        deps = [
            repository + "//test:main",
        ] + deps,
        local = local,
        shard_count = shard_count,
        size = size,
        data = data
    )


def _pagespeed_cc_test_infrastructure_library(
        name,
        srcs = [],
        hdrs = [],
        data = [],
        external_deps = [],
        deps = [],
        repository = "",
        tags = [],
        include_prefix = None,
        copts = [],
        **kargs):
    native.cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        data = data,
        copts = copts,
        testonly = 1,
        deps = deps,
        tags = tags,
        include_prefix = include_prefix,
        alwayslink = 1,
        linkstatic = True,
        **kargs
    )

def pagespeed_cc_test_library(
        name,
        srcs = [],
        hdrs = [],
        data = [],
        external_deps = [],
        deps = [],
        repository = "",
        tags = [],
        include_prefix = None,
        copts = [],
        **kargs):
    deps = deps 
    _pagespeed_cc_test_infrastructure_library(
        name,
        srcs,
        hdrs,
        data,
        external_deps,
        deps,
        repository,
        tags,
        include_prefix,
        copts,
        visibility = ["//visibility:public"],
        **kargs
    )