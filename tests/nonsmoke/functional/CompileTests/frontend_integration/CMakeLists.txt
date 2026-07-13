include(frontend_integration_tests.cmake)

add_executable(frontend_test frontend_test.C)
target_link_libraries(frontend_test ROSE_DLL ${link_with_libraries})

set(_frontend_c_tests_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests)
set(_frontend_cxx_tests_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(_frontend_flags -w -rose:verbose 0)
set(_frontend_additional_c_tests
  test2004_126.c
  test2007_70.c
  test2007_95.c
  test2009_05.c
  test2009_06.c
  test2009_19.c
  test2010_16.c
  zpagccp.c
)
list(APPEND FRONTEND_INTEGRATION_C_TESTS ${_frontend_additional_c_tests})
list(REMOVE_DUPLICATES FRONTEND_INTEGRATION_C_TESTS)

rex_get_registered_compile_source_set(_frontend_negative_c_sources
  REX_C_TESTS_NEGATIVE_SOURCES)
rex_get_registered_compile_source_set(_frontend_c89_sources
  REX_C89_TESTS_SOURCES)
set(_frontend_negative_c_tests)
foreach(source_rel IN LISTS _frontend_negative_c_sources)
  get_filename_component(_negative_name "${source_rel}" NAME)
  list(APPEND _frontend_negative_c_tests "${_negative_name}")
endforeach()
if(_frontend_negative_c_tests)
  list(REMOVE_ITEM FRONTEND_INTEGRATION_C_TESTS ${_frontend_negative_c_tests})
endif()

set(_frontend_c89_tests)
foreach(source_rel IN LISTS _frontend_c89_sources)
  get_filename_component(_c89_name "${source_rel}" NAME)
  list(APPEND _frontend_c89_tests "${_c89_name}")
endforeach()
list(REMOVE_DUPLICATES _frontend_c89_tests)

# Map of specimen file names to their extra include paths via aligned key/value lists.
set(_specimen_include_keys
  "test2005_168.c"
  "test2005_168.c"
)
set(_specimen_include_values
  "-I${CMAKE_SOURCE_DIR}/src/util/commandlineProcessing"
  "-I${CMAKE_SOURCE_DIR}/src/frontend/SageIII"
)

# Verify that the key and value lists are synchronized.
list(LENGTH _specimen_include_keys _keys_len)
list(LENGTH _specimen_include_values _values_len)
if(NOT _keys_len EQUAL _values_len)
  message(FATAL_ERROR "Mismatch between _specimen_include_keys and _specimen_include_values lists size.")
endif()

foreach(specimen IN LISTS FRONTEND_INTEGRATION_C_TESTS)
  set(_frontend_c_flags ${_frontend_flags})
  set(_extra_includes -I${_frontend_c_tests_dir})
  if(specimen IN_LIST _frontend_c89_tests)
    list(APPEND _frontend_c_flags -std=gnu89)
  endif()
  if(_keys_len GREATER 0)
    math(EXPR _last_pair_index "${_keys_len} - 1")
    foreach(_pair_index RANGE ${_last_pair_index})
      list(GET _specimen_include_keys ${_pair_index} _specimen_key)
      if("${specimen}" STREQUAL "${_specimen_key}")
        list(GET _specimen_include_values ${_pair_index} _specimen_include)
        list(APPEND _extra_includes ${_specimen_include})
      endif()
    endforeach()
  endif()
  add_test(
    NAME frontend_integration_c_${specimen}
    COMMAND frontend_test ${_frontend_c_flags} ${_extra_includes}
      -c ${_frontend_c_tests_dir}/${specimen}
  )
endforeach()

foreach(specimen IN LISTS FRONTEND_INTEGRATION_CXX_TESTS)
  add_test(
    NAME frontend_integration_cxx_${specimen}
    COMMAND frontend_test ${_frontend_flags}
      -I${_frontend_cxx_tests_dir}
      -c ${_frontend_cxx_tests_dir}/${specimen}
  )
endforeach()
