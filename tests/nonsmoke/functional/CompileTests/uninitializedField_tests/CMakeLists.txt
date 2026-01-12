include_directories(${ROSE_INCLUDES})

find_path(VALGRIND_INCLUDE_DIR valgrind/valgrind.h)
set(_uninit_disable_all FALSE)
set(_uninit_disable_reason "")
if(NOT VALGRIND_INCLUDE_DIR)
  set(_uninit_disable_all TRUE)
  set(_uninit_disable_reason "valgrind headers not found")
endif()

file(READ ${CMAKE_BINARY_DIR}/rose_config.h _rose_config)
string(FIND "${_rose_config}" "#define ROSE_USE_VALGRIND" _valgrind_define)
if(_valgrind_define EQUAL -1)
  set(_uninit_disable_all TRUE)
  if(_uninit_disable_reason)
    set(_uninit_disable_reason "${_uninit_disable_reason}; ROSE_USE_VALGRIND disabled")
  else()
    set(_uninit_disable_reason "ROSE_USE_VALGRIND disabled")
  endif()
endif()

if(_uninit_disable_all)
  message(STATUS "Registering uninitializedField_tests as disabled (${_uninit_disable_reason})")
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
set(_uninit_c99_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C99_tests)
set(_uninit_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests)

function(rex_mark_uninit_disabled test_name)
  if(NOT _uninit_disable_all)
    return()
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS disabled)
  if(NOT REX_ENABLE_DISABLED_TESTS)
    set_tests_properties(${test_name} PROPERTIES DISABLED TRUE)
  endif()
endfunction()

foreach(testcode IN LISTS UNINIT_CXX_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_cxx_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:testUninitializedFields>
      ${_uninit_flags}
      -DTEST_STRING_MACRO="Thu Apr 14 08:18:33 PDT 2005"
      -I${_uninit_cxx_dir}
      ${_uninit_includes}
      -c ${_uninit_cxx_dir}/${testcode}
  )
  rex_mark_uninit_disabled(uninit_fields_cxx_${_uninit_name})
endforeach()

foreach(testcode IN LISTS UNINIT_C_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_c_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "cp ${_uninit_cxx_dir}/${testcode} ${testcode}.temp.c && $<TARGET_FILE:testUninitializedFields> ${_uninit_flags} -I${_uninit_cxx_dir} ${_uninit_includes} -c ${testcode}.temp.c"
  )
  rex_mark_uninit_disabled(uninit_fields_c_${_uninit_name})
endforeach()

foreach(testcode IN LISTS UNINIT_C99_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_c99_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "cp ${_uninit_c99_dir}/${testcode} ${testcode}.temp.c && $<TARGET_FILE:testUninitializedFields> ${_uninit_flags} -I${_uninit_c99_dir} ${_uninit_includes} -c ${testcode}.temp.c"
  )
  rex_mark_uninit_disabled(uninit_fields_c99_${_uninit_name})
endforeach()

if(enable-fortran)
  foreach(testcode IN LISTS UNINIT_F90_TEST_SOURCES)
    string(REPLACE "." "_" _uninit_name ${testcode})
    add_test(
      NAME uninit_fields_f90_${_uninit_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        -rose:f90
        -c ${_uninit_fortran_dir}/${testcode}
    )
    rex_mark_uninit_disabled(uninit_fields_f90_${_uninit_name})
  endforeach()

  foreach(testcode IN LISTS UNINIT_F77_TEST_SOURCES)
    string(REPLACE "." "_" _uninit_name ${testcode})
    add_test(
      NAME uninit_fields_f77_${_uninit_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        -rose:f77
        -c ${_uninit_fortran_dir}/${testcode}
    )
    rex_mark_uninit_disabled(uninit_fields_f77_${_uninit_name})
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
        -rose:f2003
        -c ${_uninit_fortran_dir}/${testcode}
    )
    rex_mark_uninit_disabled(uninit_fields_f03_${_uninit_name})
  endforeach()
endif()

# Makefile check targets.
set(_uninit_cxx_tests)
foreach(testcode IN LISTS UNINIT_CXX_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  list(APPEND _uninit_cxx_tests "uninit_fields_cxx_${_uninit_name}")
endforeach()

set(_uninit_c_tests)
foreach(testcode IN LISTS UNINIT_C_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  list(APPEND _uninit_c_tests "uninit_fields_c_${_uninit_name}")
endforeach()

set(_uninit_c99_tests)
foreach(testcode IN LISTS UNINIT_C99_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  list(APPEND _uninit_c99_tests "uninit_fields_c99_${_uninit_name}")
endforeach()

set(_uninit_f90_tests)
foreach(testcode IN LISTS UNINIT_F90_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  list(APPEND _uninit_f90_tests "uninit_fields_f90_${_uninit_name}")
endforeach()

set(_uninit_f77_tests)
foreach(testcode IN LISTS UNINIT_F77_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  list(APPEND _uninit_f77_tests "uninit_fields_f77_${_uninit_name}")
endforeach()

set(_uninit_f03_tests)
foreach(testcode IN LISTS UNINIT_F03_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  list(APPEND _uninit_f03_tests "uninit_fields_f03_${_uninit_name}")
endforeach()

function(rex_add_uninit_group group_name)
  set(_tests ${ARGN})
  if(NOT _tests)
    return()
  endif()
  set(_group "${_tests}")
  string(REPLACE ";" "\;" _group "${_group}")
  add_test(
    NAME ${group_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND}
      -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
      -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
      -DREX_CTEST_GROUP_TESTS=${_group}
      -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
  )
  set_tests_properties(${group_name} PROPERTIES LABELS disabled DEPENDS "${_tests}")
  if(NOT REX_ENABLE_DISABLED_TESTS)
    set_tests_properties(${group_name} PROPERTIES DISABLED TRUE)
  endif()
endfunction()

rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-c ${_uninit_c_tests})
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-c99 ${_uninit_c99_tests})
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-cxx ${_uninit_cxx_tests})
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-f03 ${_uninit_f03_tests})
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-f77 ${_uninit_f77_tests})
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-f90 ${_uninit_f90_tests})

set(_uninit_fortran_groups
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-f90
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-f77
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-f03
)
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_check-fortran ${_uninit_fortran_groups})

set(_uninit_conditional_groups
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-cxx
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-c
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-c99
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-fortran
)
rex_add_uninit_group(nonsmoke_functional_CompileTests_uninitializedField_tests_conditional-check-local ${_uninit_conditional_groups})

rex_add_uninit_group(
  nonsmoke_functional_CompileTests_uninitializedField_tests_check-local
  nonsmoke_functional_CompileTests_uninitializedField_tests_conditional-check-local
)
