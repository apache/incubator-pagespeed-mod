licenses(["notice"])  # Apache 2

cc_library(
    name = "htmlparse",
    srcs = [
        "file_driver.cc",
        "file_statistics_log.cc",
        "logging_html_filter.cc",
        "statistics_log.cc",
    ],
    hdrs = [
        "public/file_driver.h",
        "public/file_statistics_log.h",
        "public/logging_html_filter.h",
        "public/statistics_log.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/html:html",
    ],
)
