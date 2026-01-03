include_directories(${ROSE_INCLUDES})

find_path(VALGRIND_INCLUDE_DIR valgrind/valgrind.h)
if(NOT VALGRIND_INCLUDE_DIR)
  message(STATUS "Skipping uninitializedField_tests (valgrind headers not found)")
  return()
endif()

file(READ ${CMAKE_BINARY_DIR}/rose_config.h _rose_config)
string(FIND "${_rose_config}" "#define ROSE_USE_VALGRIND" _valgrind_define)
if(_valgrind_define EQUAL -1)
  message(STATUS "Skipping uninitializedField_tests (ROSE_USE_VALGRIND disabled in rose_config.h)")
  return()
endif()

add_executable(testUninitializedFields testUninitializedFields.C)
target_link_libraries(testUninitializedFields ROSE_DLL ${link_with_libraries})
target_include_directories(testUninitializedFields PRIVATE ${VALGRIND_INCLUDE_DIR})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; uninitializedField_tests requires bash")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/uninitializedField_tests_lists.cmake)

set(_uninit_flags -w -rose:verbose 0)
set(_uninit_includes
  -I${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/A++Code)

set(_uninit_cxx_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_uninit_c99_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C99_tests)
set(_uninit_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests)

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
endforeach()

foreach(testcode IN LISTS UNINIT_C_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_c_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "cp ${_uninit_cxx_dir}/${testcode} ${testcode}.temp.c && $<TARGET_FILE:testUninitializedFields> ${_uninit_flags} -I${_uninit_cxx_dir} ${_uninit_includes} -c ${testcode}.temp.c"
  )
endforeach()

foreach(testcode IN LISTS UNINIT_C99_TEST_SOURCES)
  string(REPLACE "." "_" _uninit_name ${testcode})
  add_test(
    NAME uninit_fields_c99_${_uninit_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "cp ${_uninit_c99_dir}/${testcode} ${testcode}.temp.c && $<TARGET_FILE:testUninitializedFields> ${_uninit_flags} -I${_uninit_c99_dir} ${_uninit_includes} -c ${testcode}.temp.c"
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
        -rose:f90
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
        -rose:f77
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
        -rose:f2003
        -c ${_uninit_fortran_dir}/${testcode}
    )
  endforeach()
endif()
