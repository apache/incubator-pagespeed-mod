google_sparsehash_build_rule = """
cc_library(
    name = "google_sparsehash",
    hdrs = [
        #"gen/arch/linux/x64/include/google/sparsehash/sparseconfig.h",
        
    ] + glob(["src/sparsehash/*"]),
    visibility = ["//visibility:public"],
)
"""