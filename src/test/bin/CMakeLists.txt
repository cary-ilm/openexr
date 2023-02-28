
if(BUILD_TESTING)

  find_package(Python COMPONENTS Interpreter)
  
  message(STATUS "adding python tests")
  add_test(NAME OpenEXR.python.exrinfo
           COMMAND ${Python_EXECUTABLE}
           ${CMAKE_CURRENT_SOURCE_DIR}/test_exrinfo.py ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR})
else()
  message(STATUS "NOT adding python tests")
endif()


