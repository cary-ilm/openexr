# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

# The application half of the OPENEXR_FORCE_EMBEDDED_CORE test: built
# against a standard OpenEXR installation. See
# share/ci/scripts/test_embedded_core.sh.

cmake_minimum_required(VERSION 3.14)
project(exrapp CXX)
find_package(OpenEXR CONFIG REQUIRED)

add_executable(exrapp app.cpp)
target_link_libraries(exrapp PRIVATE OpenEXR::OpenEXR ${CMAKE_DL_LIBS})
