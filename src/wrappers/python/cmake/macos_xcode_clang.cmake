# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.
#
# Used only for cibuildwheel macOS wheels. GitHub runners often put Homebrew
# LLVM ahead of Xcode on PATH; linking that LLVM pulls in libunwind (min
# macOS 14), which delocate then bundles and rejects vs older wheel tags.

set (CMAKE_C_COMPILER "/usr/bin/clang" CACHE FILEPATH "" FORCE)
set (CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE FILEPATH "" FORCE)
