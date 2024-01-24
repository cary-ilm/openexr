# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

if(NOT "${CMAKE_PROJECT_NAME}" STREQUAL "OpenEXR")
  cmake_minimum_required(VERSION 3.12)
  project(PyOpenEXR)
  find_package(OpenEXR)
endif()

add_library (PyOpenEXR SHARED OpenEXR.cpp)

find_package(Python COMPONENTS Interpreter Development)
if(NOT TARGET Python::Interpreter AND NOT TARGET Python::Python)
  message(WARNING "Unable to find any python interpreter or libraries")
  return()
else()
  message(STATUS "Found python libraries: ${PYTHON_LIBRARIES}")
endif()

include_directories ("${PYTHON_INCLUDE_DIRS}")

set_target_properties (PyOpenEXR PROPERTIES PREFIX "")
set_target_properties (PyOpenEXR PROPERTIES OUTPUT_NAME "OpenEXR")
set_target_properties (PyOpenEXR PROPERTIES SUFFIX ".so")
set_target_properties (PyOpenEXR PROPERTIES DEBUG_POSTFIX "")

target_link_libraries (PyOpenEXR "${PYTHON_LIBRARIES}" OpenEXR::OpenEXR)

configure_file(Imath.py ${CMAKE_CURRENT_BINARY_DIR}/Imath.py COPYONLY)

if(BUILD_TESTING)

  add_test(OpenEXR.PyOpenEXR pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests)

  set_tests_properties(OpenEXR.PyOpenEXR PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )

endif()
