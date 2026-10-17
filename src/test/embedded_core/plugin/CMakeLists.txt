# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

# The plugin half of the OPENEXR_FORCE_EMBEDDED_CORE test: built
# against the custom (embedded core, custom namespace) OpenEXR
# installation. See share/ci/scripts/test_embedded_core.sh.

cmake_minimum_required(VERSION 3.14)
project(exrplugin CXX)
find_package(OpenEXR CONFIG REQUIRED)

add_library(exrplugin MODULE plugin.cpp)
target_link_libraries(exrplugin PRIVATE OpenEXR::OpenEXR)
set_target_properties(exrplugin PROPERTIES CXX_VISIBILITY_PRESET hidden)
