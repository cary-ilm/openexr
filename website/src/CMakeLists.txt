# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project

# The documentation examples in this directory are standalone snippets
# (one example per file) that are compiled only to verify they have no
# compiler errors; the object output is never linked.  They are built as a
# single aggregate translation unit (all.cpp) which #includes each example
# source, renaming the few intentionally-reused symbols with the
# preprocessor.  This compiles as fast as a unity build without having to
# disable unity for the rest of the project or rename symbols in the
# example sources.  all.cpp must be kept in sync with the example files.
add_library(website_src OBJECT
    all.cpp
)

target_compile_options(website_src PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/wd4101>

    $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-variable>
    $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-but-set-variable>

    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-variable>
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-but-set-variable>
)

target_link_libraries(website_src PRIVATE OpenEXR::OpenEXR)
