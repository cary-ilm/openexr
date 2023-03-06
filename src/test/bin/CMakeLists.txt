
if(BUILD_TESTING)

  find_package(Python COMPONENTS Interpreter)
  
  message(STATUS "adding bin tests")

  add_test(NAME OpenEXR.python.exrinfo
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrinfo.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME OpenEXR.python.exrheader
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrheader.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME OpenEXR.python.exrstdattr
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrstdattr.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR})

  set(images "https://raw.githubusercontent.com/AcademySoftwareFoundation/openexr-images/main")
  file(DOWNLOAD ${images}/Beachball/singlepart.0001.exr ${CMAKE_CURRENT_BINARY_DIR}/singlepart.0001.exr)
  file(DOWNLOAD ${images}/Beachball/multipart.0001.exr ${CMAKE_CURRENT_BINARY_DIR}/multipart.0001.exr)
  
  add_test(NAME OpenEXR.python.exrcheck
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrcheck.py
           ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR}
           singlepart.0001.exr multipart.0001.exr)

else()
  message(STATUS "NOT adding bin tests")
endif()


