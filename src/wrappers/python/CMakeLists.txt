# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

if(NOT "${CMAKE_PROJECT_NAME}" STREQUAL "OpenEXR")
  cmake_minimum_required(VERSION 3.12)
  project(PyOpenEXR)
endif()

find_package(Python REQUIRED COMPONENTS Development.Module)
#if(NOT TARGET Python::Python)
#  message(WARNING "Unable to find any python libraries")
#  return()
#else()
#  message(STATUS "Found python libraries: ${Python_LIBRARIES}")
#endif()

if(NOT "${CMAKE_PROJECT_NAME}" STREQUAL "OpenEXR")
  find_package(OpenEXR)
endif()

python_add_library (PyOpenEXR MODULE OpenEXR.cpp WITH_SOABI)

target_link_libraries (PyOpenEXR PRIVATE "${Python_LIBRARIES}" OpenEXR::OpenEXR)

configure_file(Imath.py ${CMAKE_CURRENT_BINARY_DIR}/Imath.py COPYONLY)

set_target_properties (PyOpenEXR PROPERTIES OUTPUT_NAME "OpenEXR")

if(BUILD_TESTING)

  add_test(OpenEXR.PyOpenEXR pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests)

  set_tests_properties(OpenEXR.PyOpenEXR PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )

endif()
