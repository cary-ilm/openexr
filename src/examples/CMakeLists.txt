# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

if("${CMAKE_PROJECT_NAME}" STREQUAL "")
  cmake_minimum_required(VERSION 3.12)

  message(STATUS "Configuring OpenEXRExamples")

  project(OpenEXRExamples)
  find_package(OpenEXR)
else()
  message(STATUS "OpenEXRExamples is a sub-project")
endif()

add_executable(OpenEXRExamples
  drawImage.cpp
  generalInterfaceExamples.cpp
  generalInterfaceTiledExamples.cpp
  lowLevelIoExamples.cpp
  main.cpp
  previewImageExamples.cpp
  rgbaInterfaceExamples.cpp
  rgbaInterfaceTiledExamples.cpp
)
target_link_libraries(OpenEXRExamples OpenEXR::OpenEXR)

# Examples
install(
  FILES
    main.cpp
    drawImage.cpp
    rgbaInterfaceExamples.cpp
    rgbaInterfaceTiledExamples.cpp
    generalInterfaceExamples.cpp
    lowLevelIoExamples.cpp
    previewImageExamples.cpp
    generalInterfaceTiledExamples.cpp
    generalInterfaceTiledExamples.h
    drawImage.h
    rgbaInterfaceExamples.h
    generalInterfaceExamples.h
    rgbaInterfaceTiledExamples.h
    lowLevelIoExamples.h
    previewImageExamples.h
    namespaceAlias.h
  DESTINATION
    ${CMAKE_INSTALL_DOCDIR}/examples
  )
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(OpenEXRExamples PRIVATE OPENEXR_DLL)
endif()
