include_directories(${ROSE_INCLUDES})

find_path(VALGRIND_INCLUDE_DIR valgrind/valgrind.h)
set(_uninit_skip_all FALSE)
set(_uninit_skip_reason "")
if(NOT VALGRIND_INCLUDE_DIR)
  set(_uninit_skip_all TRUE)
  set(_uninit_skip_reason "valgrind headers not found")
endif()

file(READ ${CMAKE_BINARY_DIR}/rose_config.h _rose_config)
string(FIND "${_rose_config}" "#define ROSE_USE_VALGRIND" _valgrind_define)
if(_valgrind_define EQUAL -1)
  set(_uninit_skip_all TRUE)
  if(_uninit_skip_reason)
    set(_uninit_skip_reason "${_uninit_skip_reason}; ROSE_USE_VALGRIND unavailable")
  else()
    set(_uninit_skip_reason "ROSE_USE_VALGRIND unavailable")
  endif()
endif()

if(_uninit_skip_all)
  message(STATUS "Not registering uninitializedField_tests (${_uninit_skip_reason})")
  add_executable(testUninitializedFields EXCLUDE_FROM_ALL testUninitializedFields.C)
else()
  add_executable(testUninitializedFields testUninitializedFields.C)
endif()
target_link_libraries(testUninitializedFields ROSE_DLL ${link_with_libraries})
if(VALGRIND_INCLUDE_DIR)
  target_include_directories(testUninitializedFields PRIVATE ${VALGRIND_INCLUDE_DIR})
endif()

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; uninitializedField_tests requires bash")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/uninitializedField_tests_lists.cmake)

set(_uninit_flags -w -rose:verbose 0)
set(_uninit_includes
  -isystem ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/input_codes/system_header_like
)

set(_uninit_cxx_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_uninit_cxx_dirs
  ${_uninit_cxx_dir}
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx03_tests
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx11_tests
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx14_tests
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx17_tests
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx20_tests
)
set(_uninit_c99_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C99_tests)
set(_uninit_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests)

function(_resolve_uninit_source out_var testcode)
  foreach(_uninit_dir IN LISTS ARGN)
    set(_uninit_candidate ${_uninit_dir}/${testcode})
    if(EXISTS ${_uninit_candidate})
      set(${out_var} ${_uninit_candidate} PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${out_var} "" PARENT_SCOPE)
endfunction()

if(NOT _uninit_skip_all)
set(_missing_uninit_cxx_sources)
foreach(testcode IN LISTS UNINIT_CXX_TEST_SOURCES)
  _resolve_uninit_source(_uninit_source_path ${testcode} ${_uninit_cxx_dirs})
  if(_uninit_source_path STREQUAL "")
    list(APPEND _missing_uninit_cxx_sources ${testcode})
    continue()
  endif()
  get_filename_component(_uninit_source_dir ${_uninit_source_path} DIRECTORY)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_cxx_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:testUninitializedFields>
      ${_uninit_flags}
      -DTEST_STRING_MACRO="Thu Apr 14 08:18:33 PDT 2005"
      -I${_uninit_source_dir}
      ${_uninit_includes}
      -c ${_uninit_source_path}
  )
  if(testcode MATCHES "axpy")
    # Valgrind amplifies Clang frontend cost for heavy STL inputs; allow enough time.
    set_tests_properties(uninit_fields_cxx_${_uninit_name} PROPERTIES TIMEOUT 7200)
  endif()
endforeach()
if(_missing_uninit_cxx_sources)
  list(JOIN _missing_uninit_cxx_sources ", " _missing_uninit_cxx_sources_msg)
  message(STATUS
    "Skipping uninitializedField_tests sources missing from CompileTests tree: "
    "${_missing_uninit_cxx_sources_msg}")
endif()

foreach(testcode IN LISTS UNINIT_C_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  if(testcode MATCHES "\\.c$")
    set(_uninit_source_dir
        ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests)
  else()
    set(_uninit_source_dir ${_uninit_cxx_dir})
  endif()
  add_test(
    NAME uninit_fields_c_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "cp \"$1\" \"$2\" && shift 2 && \"$@\""
      dummy
      ${_uninit_source_dir}/${testcode}
      ${testcode}.temp.c
      $<TARGET_FILE:testUninitializedFields>
      ${_uninit_flags}
      -I${_uninit_source_dir}
      ${_uninit_includes}
      -c
      ${testcode}.temp.c
  )
endforeach()

foreach(testcode IN LISTS UNINIT_C99_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_c99_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "cp \"$1\" \"$2\" && shift 2 && \"$@\""
      dummy
      ${_uninit_c99_dir}/${testcode}
      ${testcode}.temp.c
      $<TARGET_FILE:testUninitializedFields>
      ${_uninit_flags}
      -I${_uninit_c99_dir}
      ${_uninit_includes}
      -c
      ${testcode}.temp.c
  )
endforeach()

if(enable-fortran)
  foreach(testcode IN LISTS UNINIT_F90_TEST_SOURCES)
    string(REPLACE "." "_" _uninit_name ${testcode})
    add_test(
      NAME uninit_fields_f90_${_uninit_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        -rose:fortran_std=f90
        -c ${_uninit_fortran_dir}/${testcode}
    )
  endforeach()

  foreach(testcode IN LISTS UNINIT_F77_TEST_SOURCES)
    string(REPLACE "." "_" _uninit_name ${testcode})
    add_test(
      NAME uninit_fields_f77_${_uninit_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        -rose:fortran_std=f77
        -c ${_uninit_fortran_dir}/${testcode}
    )
  endforeach()

  foreach(testcode IN LISTS UNINIT_F03_TEST_SOURCES)
    string(REPLACE "." "_" _uninit_name ${testcode})
    add_test(
      NAME uninit_fields_f03_${_uninit_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        -rose:skip_syntax_check
        -rose:skipfinalCompileStep
        -rose:fortran_std=f2003
        -c ${_uninit_fortran_dir}/${testcode}
    )
  endforeach()
endif()

endif()
