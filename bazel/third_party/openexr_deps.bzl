# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

"""External dependencies for openexr."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def openexr_deps():
    """Fetches dependencies (zlib and Imath) of OpenEXR."""

    maybe(
        http_archive,
        name = "net_zlib_zlib",
        build_file = "@openexr//:bazel/third_party/zlib.BUILD",
        sha256 = "91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9",
        strip_prefix = "zlib-1.2.12",
        urls = [
            "https://zlib.net/zlib-1.2.12.tar.gz",
            "https://mirror.bazel.build/zlib.net/zlib-1.2.12.tar.gz",
        ],
    )

    # sha256 was determined using:
    # curl -sL https://github.com/AcademySoftwareFoundation/Imath/archive/refs/tags/v3.1.5.tar.gz --output Imath-3.1.5.tar.gz
    # sha256sum Imath-3.1.5.tar.gz
    # If the hash is incorrect Bazel will report an error and show the actual hash of the file.
    maybe(
        http_archive,
        name = "Imath",
        build_file = "@com_openexr//:bazel/third_party/Imath.BUILD",
        strip_prefix = "Imath-3.1.7",
        sha256 = "bff1fa140f4af0e7f02c6cb78d41b9a7d5508e6bcdfda3a583e35460eb6d4b47",
        urls = ["https://github.com/AcademySoftwareFoundation/Imath/archive/refs/tags/v3.1.7.tar.gz"],
    )

    http_archive(
        name = "bazel_skylib",
        sha256 = "b8a1527901774180afc798aeb28c4634bdccf19c4d98e7bdd1ce79d1fe9aaad7",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.4.1/bazel-skylib-1.4.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.4.1/bazel-skylib-1.4.1.tar.gz",
        ],
    )
