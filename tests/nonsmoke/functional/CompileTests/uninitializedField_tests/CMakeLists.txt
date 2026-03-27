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

set(_uninit_flags -w -rose:verbose 0)
set(_uninit_includes
  -isystem ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/input_codes/system_header_like
)

function(rex_uninit_test_name out_var prefix source_rel)
  set(_name ${source_rel})
  string(REPLACE "/" "_" _name "${_name}")
  string(REPLACE "." "_" _name "${_name}")
  set(${out_var} "${prefix}_${_name}" PARENT_SCOPE)
endfunction()

function(rex_add_uninit_cxx_tests property_name)
  set(_group_flags ${ARGN})
  rex_get_registered_compile_source_set(_sources ${property_name})
  foreach(source_rel IN LISTS _sources)
    set(_source_path ${REX_COMPILE_TESTS_SOURCE_DIR}/${source_rel})
    get_filename_component(_source_name ${_source_path} NAME)
    get_filename_component(_source_dir ${_source_path} DIRECTORY)
    rex_get_cxx_specimen_extra_flags(_extra_frontend_flags ${_source_name})
    rex_uninit_test_name(_test_name uninit_fields_cxx ${source_rel})
    add_test(
      NAME ${_test_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        ${_group_flags}
        ${_extra_frontend_flags}
        -DTEST_STRING_MACRO="Thu Apr 14 08:18:33 PDT 2005"
        -I${_source_dir}
        ${_uninit_includes}
        -c ${_source_path}
    )
    if(source_rel MATCHES "axpy")
      set_tests_properties(${_test_name} PROPERTIES TIMEOUT 7200)
    endif()
  endforeach()
endfunction()

function(rex_add_uninit_c_copy_tests prefix property_name)
  set(_group_flags ${ARGN})
  rex_get_registered_compile_source_set(_sources ${property_name})
  foreach(source_rel IN LISTS _sources)
    set(_source_path ${REX_COMPILE_TESTS_SOURCE_DIR}/${source_rel})
    get_filename_component(_source_dir ${_source_path} DIRECTORY)
    rex_uninit_test_name(_test_name ${prefix} ${source_rel})
    add_test(
      NAME ${_test_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${BASH_EXECUTABLE} -c
        "cp \"$1\" \"$2\" && shift 2 && \"$@\""
        dummy
        ${_source_path}
        ${_test_name}.temp.c
        $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        ${_group_flags}
        -I${_source_dir}
        ${_uninit_includes}
        -c
        ${_test_name}.temp.c
    )
  endforeach()
endfunction()

function(rex_add_uninit_fortran_tests prefix property_name)
  set(_group_flags ${ARGN})
  rex_get_registered_compile_source_set(_sources ${property_name})
  foreach(source_rel IN LISTS _sources)
    set(_source_path ${REX_COMPILE_TESTS_SOURCE_DIR}/${source_rel})
    rex_uninit_test_name(_test_name ${prefix} ${source_rel})
    rex_get_fortran_specimen_extra_flags(_extra_frontend_flags ${source_rel})
    add_test(
      NAME ${_test_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:testUninitializedFields>
        ${_uninit_flags}
        ${_group_flags}
        ${_extra_frontend_flags}
        -c ${_source_path}
    )
  endforeach()
endfunction()

if(NOT _uninit_skip_all)
  rex_add_uninit_cxx_tests(REX_CXX_TESTS_DEFAULT_SOURCES -std=gnu++14)
  rex_add_uninit_cxx_tests(REX_CXX_TESTS_GNUXX03_SOURCES -std=gnu++03)
  rex_add_uninit_cxx_tests(REX_CXX_TESTS_GNUXX17_SOURCES -std=gnu++17)
  rex_add_uninit_cxx_tests(
    REX_CXX_TESTS_DELAYED_TEMPLATE_PARSING_SOURCES
    -std=gnu++14
    -rex:clang:delayed-template-parsing)
  rex_add_uninit_cxx_tests(REX_CXX03_TESTS_SOURCES -std=c++03)
  rex_add_uninit_cxx_tests(REX_CXX11_TESTS_CXX11_SOURCES -std=c++11)
  rex_add_uninit_cxx_tests(REX_CXX11_TESTS_CXX03_SOURCES -std=c++03)
  rex_add_uninit_cxx_tests(REX_CXX14_TESTS_SOURCES -std=c++14)
  rex_add_uninit_cxx_tests(REX_CXX17_TESTS_SOURCES -std=c++17)
  rex_add_uninit_cxx_tests(REX_CXX20_TESTS_SOURCES -std=c++20)

  rex_get_registered_compile_source_set(_all_c_sources REX_C_TESTS_SOURCES)
  rex_get_registered_compile_source_set(_c89_sources REX_C89_TESTS_SOURCES)
  rex_get_registered_compile_source_set(_c89_gnu89_sources REX_C89_GNU89_SOURCES)
  if(_c89_gnu89_sources)
    list(REMOVE_ITEM _c89_sources ${_c89_gnu89_sources})
  endif()
  if(_c89_sources)
    list(REMOVE_ITEM _all_c_sources ${_c89_sources})
  endif()
  set_property(GLOBAL PROPERTY REX_UNINIT_C_TESTS_DEFAULT_SOURCES ${_all_c_sources})
  set_property(GLOBAL PROPERTY REX_UNINIT_C89_STRICT_SOURCES ${_c89_sources})

  rex_add_uninit_c_copy_tests(uninit_fields_c REX_UNINIT_C_TESTS_DEFAULT_SOURCES)
  rex_add_uninit_c_copy_tests(uninit_fields_c89 REX_UNINIT_C89_STRICT_SOURCES -std=c89)
  rex_add_uninit_c_copy_tests(uninit_fields_c89 REX_C89_GNU89_SOURCES -std=gnu89)
  rex_add_uninit_c_copy_tests(uninit_fields_c REX_C_SUBSET_OF_CXX_SOURCES)
  rex_add_uninit_c_copy_tests(uninit_fields_c99 REX_C99_TESTS_SOURCES -rose:C99)

if(enable-fortran)
    rex_add_uninit_fortran_tests(uninit_fields_f90 REX_FORTRAN_F90_SOURCES
      -rose:fortran_std=f90)
    rex_add_uninit_fortran_tests(uninit_fields_f77 REX_FORTRAN_F77_SOURCES
      -rose:fortran_std=f77)
    rex_add_uninit_fortran_tests(uninit_fields_f03 REX_FORTRAN_F03_SOURCES
      -rose:skip_syntax_check
      -rose:skipfinalCompileStep
      -rose:fortran_std=f2003)
endif()

endif()
