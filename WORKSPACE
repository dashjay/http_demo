load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "libsockpp",
    urls = ["https://github.com/fpagliughi/sockpp/archive/refs/tags/v0.7.tar.gz"],
    sha256 = "5cbf593f534fef5e12a4aff97498f0917bbfcd67d71c7b376a50c92b9478a86b",
    strip_prefix = "sockpp-0.7",
    build_file_content = """
cc_library(
    name = "sockpp", 
    srcs = glob(["src/*.cpp"]), 
    hdrs = glob(["include/sockpp/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"], 
)
"""
)