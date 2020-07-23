drp_build_rule = """
genrule(
    name = "registry_tables_generator",
    tools = [
        "src/registry_tables_generator/registry_tables_generator.py",
        "src/third_party/effective_tld_names/effective_tld_names.dat",
    ],
    outs = [
        "registry_tables_genfiles/registry_tables.h"
    ],
    cmd = ("python2 ./$(location src/registry_tables_generator/registry_tables_generator.py) ./$(location src/third_party/effective_tld_names/effective_tld_names.dat) $@ $@_test")
)

cc_library(
    name = "drp",
    srcs = [
        "src/domain_registry/private/init_registry_tables.c",
        "src/domain_registry/private/trie_search.c",
        "src/domain_registry/private/registry_search.c",
        "src/domain_registry/private/assert.c",
    ],
    hdrs = [
        "src/domain_registry/domain_registry.h",
        "src/domain_registry/private/registry_types.h",
        "src/domain_registry/private/trie_node.h",
        "src/domain_registry/private/trie_search.h",
        "src/domain_registry/private/assert.h",
        "src/domain_registry/private/string_util.h",
        ":registry_tables_generator",
    ],
    visibility = ["//visibility:public"],
    copts = ["-Iexternal/drp/src"]
)
"""