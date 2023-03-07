# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) Contributors to the OpenEXR Project.

if(BUILD_TESTING)

  find_package(Python3 COMPONENTS Interpreter)
  if (NOT Python3_FOUND)
    message(STATUS "Python3 not found: skipping bin tests")
    return()
  endif()
    
  message(STATUS "adding bin tests")

  set(images_url "https://raw.githubusercontent.com/AcademySoftwareFoundation/openexr-images/main")
  set(images 
      TestImages/GrayRampsHorizontal.exr
      LuminanceChroma/Garden.exr
      MultiResolution/ColorCodedLevels.exr
      LuminanceChroma/Flowers.exr
      MultiView/Adjuster.exr
      Chromaticities/Rec709_YC.exr
      Chromaticities/Rec709.exr
      Chromaticities/XYZ_YC.exr
      Chromaticities/XYZ.exr
      TestImages/GammaChart.exr
      Beachball/singlepart.0001.exr
      v2/LeftView/Balls.exr
      v2/Stereo/Trunks.exr
      Beachball/multipart.0001.exr
  )

  foreach(image in LIST ${images})
      message(STATUS "Downloading ${images_url}/${image}")
      file(DOWNLOAD ${images_url}/${image} ${CMAKE_CURRENT_BINARY_DIR}/${image})
  endforeach()

  add_test(NAME OpenEXR.python.exrinfo
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrinfo.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME OpenEXR.python.exrheader
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrheader.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME OpenEXR.python.exrstdattr
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrstdattr.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME OpenEXR.python.exrmakepreview
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrmakepreview.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME OpenEXR.python.exr2aces
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exr2aces.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME OpenEXR.python.exrcheck
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrcheck.py
           ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR}
           ${images})

endif()


