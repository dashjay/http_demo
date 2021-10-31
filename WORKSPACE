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

http_archive(
    name = "spdlog",
    urls = ["https://github.com/gabime/spdlog/archive/refs/tags/v1.9.2.tar.gz"],
    strip_prefix = "spdlog-1.9.2",
    build_file_content = """
filegroup(
    name = "include",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"], 
)
"""
)


http_archive(
    name = "cpptoml",
    urls = ["https://github.com/skystrife/cpptoml/archive/refs/tags/v0.1.1.tar.gz"],
    sha256 = "23af72468cfd4040984d46a0dd2a609538579c78ddc429d6b8fd7a10a6e24403",
    strip_prefix = "cpptoml-0.1.1",
    build_file_content = """
filegroup(
    name = "include",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"], 
)
"""
)