load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

def pagespeed_cc_benchmark(
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
        shard_count = 1,
        coverage = True,
        local = False,
        size = "medium"):
    test_lib_tags = []
    cc_test(
        name = name,
        copts = copts,
        linkstatic = True,
        srcs = srcs,
        deps = deps + ["//test/pagespeed/kernel/base:pagespeed_gtest"],
        local = local,
        shard_count = 1,
        size = size,
        data = data,
    )


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
        shard_count = 10,
        coverage = True,
        local = False,
        size = "medium"):
    test_lib_tags = []
    cc_test(
        name = name,
        copts = copts,
        linkstatic = True,
        srcs = srcs,
        deps = [
            repository + "//test:main",
        ] + deps + ["//test/pagespeed/kernel/base:pagespeed_gtest"],
        local = local,
        shard_count = shard_count,
        size = size,
        data = data,
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
    cc_library(
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
