# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

cmake_minimum_required(VERSION 3.12)

set (Python_ADDITIONAL_VERSIONS 3)
find_package (PythonLibs REQUIRED)
find_package (PythonInterp REQUIRED)

include_directories ("${PYTHON_INCLUDE_DIRS}")

add_library (PyOpenEXR SHARED OpenEXR.cpp)

set_target_properties (PyOpenEXR PROPERTIES PREFIX "")
set_target_properties (PyOpenEXR PROPERTIES OUTPUT_NAME "OpenEXR")
set_target_properties (PyOpenEXR PROPERTIES SUFFIX ".so")

target_link_libraries (PyOpenEXR "${PYTHON_LIBRARIES}" OpenEXR::OpenEXR)

