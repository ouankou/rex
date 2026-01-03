include(frontend_integration_tests.cmake)

add_executable(frontend_test frontend_test.C)
target_link_libraries(frontend_test ROSE_DLL ${link_with_libraries})

set(_frontend_c_tests_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests)
set(_frontend_cxx_tests_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(_frontend_flags -w -rose:verbose 0)
set(_frontend_disabled_c_tests
  test2004_126.c
  test2007_70.c
  test2007_95.c
  test2009_05.c
  test2009_06.c
  test2009_19.c
  test2010_16.c
  zpagccp.c
)

foreach(specimen IN LISTS FRONTEND_INTEGRATION_C_TESTS)
  set(_extra_includes -I${_frontend_c_tests_dir})
  if(specimen STREQUAL "test2005_168.c")
    list(APPEND _extra_includes
      -I${CMAKE_SOURCE_DIR}/src/util/commandlineProcessing)
  endif()
  add_test(
    NAME frontend_integration_c_${specimen}
    COMMAND frontend_test ${_frontend_flags} ${_extra_includes}
      -c ${_frontend_c_tests_dir}/${specimen}
  )
  if(specimen IN_LIST _frontend_disabled_c_tests)
    set_tests_properties(frontend_integration_c_${specimen} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

foreach(specimen IN LISTS FRONTEND_INTEGRATION_CXX_TESTS)
  add_test(
    NAME frontend_integration_cxx_${specimen}
    COMMAND frontend_test ${_frontend_flags}
      -I${_frontend_cxx_tests_dir}
      -c ${_frontend_cxx_tests_dir}/${specimen}
  )
endforeach()
