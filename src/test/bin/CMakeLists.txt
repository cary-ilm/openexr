# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

if(BUILD_TESTING)

  set(images "https://raw.githubusercontent.com/AcademySoftwareFoundation/openexr-images/main")
  file(DOWNLOAD ${images}/TestImages/GrayRampsHorizontal.exr ${CMAKE_CURRENT_BINARY_DIR}/GrayRampsHorizontal.exr)
  file(DOWNLOAD ${images}/Beachball/singlepart.0001.exr ${CMAKE_CURRENT_BINARY_DIR}/singlepart.0001.exr)
  file(DOWNLOAD ${images}/Beachball/multipart.0001.exr ${CMAKE_CURRENT_BINARY_DIR}/multipart.0001.exr)
  
  find_package(Python COMPONENTS Interpreter)
  
  add_test(NAME OpenEXR.python.exrinfo
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrinfo.py ${CMAKE_BINARY_DIR}/bin)

  add_test(NAME OpenEXR.python.exrheader
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrheader.py ${CMAKE_BINARY_DIR}/bin)

  add_test(NAME OpenEXR.python.exrstdattr
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrstdattr.py ${CMAKE_BINARY_DIR}/bin)

  add_test(NAME OpenEXR.python.exrcheck
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrcheck.py
           ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR}
           singlepart.0001.exr multipart.0001.exr)

endif()


