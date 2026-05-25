# setup ROSE_INCLUDE_FLAGS variable (used by some compile tests)
foreach(include ${ROSE_INCLUDES})
  list(APPEND ROSE_INCLUDE_FLAGS "-I${include}")
endforeach()

# default values for our compile tests
set(ROSE_FLAGS -w -rose:verbose 0)
set(translator testTranslator)

# define compile_test CMake function
function(compile_test input_file label)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(_test_name_prefix "${label}")
  if(DEFINED REX_COMPILE_TEST_NAME_PREFIX)
    set(_test_name_prefix "${REX_COMPILE_TEST_NAME_PREFIX}")
  endif()
  set(test_id ${_test_name_prefix}_${_test_key})
  set(_rose_flags ${ROSE_FLAGS})
  set(_test_output_dir "${ROSE_TEST_OUTPUT_DIR}")
  if(_test_output_dir STREQUAL "")
    set(_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
  endif()
  file(MAKE_DIRECTORY "${_test_output_dir}")
  if(DEFINED ROSE_GNU89_TESTCODES AND input_file IN_LIST ROSE_GNU89_TESTCODES)
    # Allow per-test GNU89-based C89 dialect classification without changing global defaults.
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags "-std=gnu89")
  endif()
  rex_get_cxx_specimen_extra_flags(_extra_rose_flags ${input_file})
  list(APPEND _rose_flags ${_extra_rose_flags})
  if(DEFINED ROSE_OUTPUT_TO_BUILD_TESTCODES AND
     input_file IN_LIST ROSE_OUTPUT_TO_BUILD_TESTCODES)
    set(_output_base "${input_file}")
    string(REPLACE "/" "_" _output_base "${_output_base}")
    list(APPEND _rose_flags "-rose:output"
         "${_test_output_dir}/rose_${_output_base}")
  endif()

  # optional argument to omit include directories
  if(ARGV2)
    add_test(
      NAME ${test_id}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${translator}
        ${_rose_flags} -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
    )
  else()
    add_test(
      NAME ${test_id}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${translator}
        ${_rose_flags} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
        ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
    )
  endif()
  set_tests_properties(${test_id} PROPERTIES LABELS "${label}")
  set_property(TEST ${test_id} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
endfunction()

function(compile_negative_test input_file label)
  compile_test(${input_file} ${label} ${ARGN})
  rex_compile_test_name(_test_name ${label} ${input_file})
  set_tests_properties(${_test_name} PROPERTIES WILL_FAIL TRUE)
endfunction()

# Prevent race conditions when multiple tests process the same input file.
# Sets a CTest RESOURCE_LOCK so tests sharing the same base input name are serialized.
function(rex_lock_test_for_input test_name input_file)
  get_filename_component(lock_base ${input_file} NAME_WE)
  set_tests_properties(${test_name} PROPERTIES RESOURCE_LOCK rose_output_${lock_base})
endfunction()

# Compute the CTest test name that compile_test would generate for a given
# input file and label, so callers can reference it without hardcoding.
function(rex_compile_test_name out_var label input_file)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(_test_name_prefix "${label}")
  if(DEFINED REX_COMPILE_TEST_NAME_PREFIX)
    set(_test_name_prefix "${REX_COMPILE_TEST_NAME_PREFIX}")
  endif()
  set(${out_var} "${_test_name_prefix}_${_test_key}" PARENT_SCOPE)
endfunction()

set(REX_COMPILE_TESTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

function(rex_register_compile_source_set property_name)
  foreach(source_rel IN LISTS ARGN)
    get_filename_component(_source_abs "${source_rel}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_source_abs}")
      message(FATAL_ERROR
        "CompileTests source '${source_rel}' registered from "
        "${CMAKE_CURRENT_SOURCE_DIR} does not exist")
    endif()
    file(RELATIVE_PATH _source_from_root "${REX_COMPILE_TESTS_SOURCE_DIR}"
         "${_source_abs}")
    set_property(GLOBAL APPEND PROPERTY ${property_name} "${_source_from_root}")
  endforeach()
endfunction()

function(rex_get_registered_compile_source_set out_var property_name)
  get_property(_sources GLOBAL PROPERTY ${property_name})
  if(NOT _sources)
    set(_sources)
  endif()
  list(REMOVE_DUPLICATES _sources)
  list(SORT _sources)
  set(${out_var} "${_sources}" PARENT_SCOPE)
endfunction()

function(rex_verify_compile_test_source_coverage)
  set(options)
  set(oneValueArgs DIRECTORY PATTERN)
  set(multiValueArgs PROPERTIES IGNORE)
  cmake_parse_arguments(REX_VERIFY "${options}" "${oneValueArgs}"
                        "${multiValueArgs}" ${ARGN})

  if(NOT REX_VERIFY_DIRECTORY)
    set(REX_VERIFY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  if(NOT REX_VERIFY_PATTERN)
    set(REX_VERIFY_PATTERN "*.c")
  endif()
  if(NOT REX_VERIFY_PROPERTIES)
    message(FATAL_ERROR "rex_verify_compile_test_source_coverage requires PROPERTIES")
  endif()

  file(GLOB _actual_local_files RELATIVE "${REX_VERIFY_DIRECTORY}"
       "${REX_VERIFY_DIRECTORY}/${REX_VERIFY_PATTERN}")
  file(RELATIVE_PATH _directory_from_root "${REX_COMPILE_TESTS_SOURCE_DIR}"
       "${REX_VERIFY_DIRECTORY}")

  set(_actual_root_relative_files)
  foreach(_file IN LISTS _actual_local_files)
    if(_directory_from_root STREQUAL ".")
      set(_root_relative "${_file}")
    else()
      set(_root_relative "${_directory_from_root}/${_file}")
    endif()
    list(APPEND _actual_root_relative_files "${_root_relative}")
  endforeach()

  set(_registered_root_relative_files)
  foreach(_property IN LISTS REX_VERIFY_PROPERTIES)
    get_property(_registered GLOBAL PROPERTY ${_property})
    if(_registered)
      list(APPEND _registered_root_relative_files ${_registered})
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _registered_root_relative_files)

  set(_ignored_root_relative_files)
  foreach(_ignore IN LISTS REX_VERIFY_IGNORE)
    get_filename_component(_ignore_abs "${_ignore}" ABSOLUTE
                           BASE_DIR "${REX_VERIFY_DIRECTORY}")
    if(NOT EXISTS "${_ignore_abs}")
      message(FATAL_ERROR
        "Ignored compile-test source '${_ignore}' does not exist in "
        "${REX_VERIFY_DIRECTORY}")
    endif()
    file(RELATIVE_PATH _ignore_root "${REX_COMPILE_TESTS_SOURCE_DIR}"
         "${_ignore_abs}")
    list(APPEND _ignored_root_relative_files "${_ignore_root}")
  endforeach()
  list(REMOVE_DUPLICATES _ignored_root_relative_files)

  set(_unregistered_files)
  foreach(_file IN LISTS _actual_root_relative_files)
    if(NOT _file IN_LIST _registered_root_relative_files AND
       NOT _file IN_LIST _ignored_root_relative_files)
      list(APPEND _unregistered_files "${_file}")
    endif()
  endforeach()

  set(_stale_registrations)
  foreach(_file IN LISTS _registered_root_relative_files)
    if(_directory_from_root STREQUAL ".")
      set(_belongs_to_directory TRUE)
    else()
      string(FIND "${_file}" "${_directory_from_root}/" _prefix_pos)
      if(_prefix_pos EQUAL 0)
        set(_belongs_to_directory TRUE)
      else()
        set(_belongs_to_directory FALSE)
      endif()
    endif()

    if(_belongs_to_directory AND
       NOT _file IN_LIST _actual_root_relative_files)
      list(APPEND _stale_registrations "${_file}")
    endif()
  endforeach()

  if(_unregistered_files OR _stale_registrations)
    list(SORT _unregistered_files)
    list(SORT _stale_registrations)

    set(_message "CompileTests source coverage mismatch in ${REX_VERIFY_DIRECTORY}")
    if(_unregistered_files)
      string(APPEND _message "\nUnregistered sources:")
      foreach(_file IN LISTS _unregistered_files)
        string(APPEND _message "\n  ${_file}")
      endforeach()
    endif()
    if(_stale_registrations)
      string(APPEND _message "\nStale registrations:")
      foreach(_file IN LISTS _stale_registrations)
        string(APPEND _message "\n  ${_file}")
      endforeach()
    endif()
    message(FATAL_ERROR "${_message}")
  endif()
endfunction()

# determine what subdirectories to enter based on our configuration settings.

add_subdirectory(CudaTests)

# REX: Fortran tests are independent of which C/C++ compiler is used to build REX itself
# Move Fortran test registration outside the Clang-specific if/else block
if(ENABLE-FORTRAN)
  add_subdirectory(Fortran_tests)
  add_subdirectory(experimental_fortran_tests)
endif()

# REX: Enable all tests regardless of compiler (Clang frontend is now standard)
if(ENABLE-C)
  add_subdirectory(C_tests)
  add_subdirectory(C89_std_c89_tests)
  add_subdirectory(C11_tests)
  add_subdirectory(C_subset_of_Cxx_tests)
  add_subdirectory(OpenClTests)
  add_subdirectory(Cxx_tests)
  add_subdirectory(Cxx03_tests)
  add_subdirectory(Cxx11_tests)
  add_subdirectory(Cxx14_tests)
  add_subdirectory(Cxx17_tests)
  add_subdirectory(Cxx20_tests)
  add_subdirectory(C99_tests)
  add_subdirectory(OpenACC_tests)

  add_subdirectory(C++Code)
  add_subdirectory(copyAST_tests)
  # ROSE-1738
  add_subdirectory(RoseExample_tests)
  add_subdirectory(unparseToString_tests)
  add_subdirectory(sourcePosition_tests)
  # requires Valgrind
  add_subdirectory(uninitializedField_tests)
  add_subdirectory(OvertureCode)
  add_subdirectory(ExpressionTemplateExample_tests)
  add_subdirectory(hiddenTypeAndDeclarationListTests)
  add_subdirectory(sizeofOperation_tests)

  add_subdirectory(nameQualificationAndTypeElaboration_tests)
  add_subdirectory(unparse_template_from_ast)
  add_subdirectory(STL_tests)

  # Legacy frontend interface tests are obsolete
  # if(enable-legacy-frontend-interface)
  #   add_subdirectory(LegacyFrontendInterface_C_tests)
  # endif()

  add_subdirectory(UnparseHeadersTests)
  add_subdirectory(UnparseHeadersUsingTokenStream_tests)
  add_subdirectory(frontend_integration)

  add_subdirectory(mergeAST_tests)
  add_subdirectory(ElsaTestCases)
  add_subdirectory(virtualCFG_tests)
  add_subdirectory(mixLanguage_tests)
  add_subdirectory(systemc_tests)
  add_subdirectory(staticCFG_tests)
endif()

if(ENABLE-C OR ENABLE-FORTRAN)
  add_subdirectory(OpenMP_tests)
  if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/OpenMP_VV/tests")
    add_subdirectory(OpenMP_VV_tests)
  else()
    message(STATUS "Skipping OpenMP_VV_tests: third_party/OpenMP_VV/tests not found")
  endif()
endif()
