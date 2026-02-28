# setup ROSE_INCLUDE_FLAGS variable (used by some compile tests)
foreach(include ${ROSE_INCLUDES})
  list(APPEND ROSE_INCLUDE_FLAGS "-I${include}")
endforeach()

# default values for our compile tests
set(ROSE_FLAGS -w -rose:verbose 0)
set(translator testTranslator)

option(REX_ENABLE_FAILING_TESTS "Enable tests known to fail" OFF)

# define compile_test CMake function
function(compile_test input_file label)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(test_id ${label}_${_test_key})
  set(_rose_flags ${ROSE_FLAGS})
  if(DEFINED ROSE_GNU89_TESTCODES AND input_file IN_LIST ROSE_GNU89_TESTCODES)
    # Allow per-test GNU89-based C89 dialect classification without changing global defaults.
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags "-std=gnu89")
  endif()
  if(DEFINED ROSE_OUTPUT_TO_BUILD_TESTCODES AND
     input_file IN_LIST ROSE_OUTPUT_TO_BUILD_TESTCODES)
    set(_output_dir "${ROSE_TEST_OUTPUT_DIR}")
    if(_output_dir STREQUAL "")
      set(_output_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    file(MAKE_DIRECTORY "${_output_dir}")
    set(_output_base "${input_file}")
    string(REPLACE "/" "_" _output_base "${_output_base}")
    list(APPEND _rose_flags "-rose:output"
         "${_output_dir}/rose_${_output_base}")
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
  set(_labels ${label})
  set(_disable_test FALSE)
  set(_expected_fail FALSE)
  if(DEFINED ROSE_DISABLED_TESTCODES AND input_file IN_LIST ROSE_DISABLED_TESTCODES)
    list(APPEND _labels disabled)
    if(NOT REX_ENABLE_DISABLED_TESTS)
      set(_disable_test TRUE)
    endif()
  endif()
  if(DEFINED ROSE_FAILING_TESTCODES AND input_file IN_LIST ROSE_FAILING_TESTCODES)
    list(APPEND _labels known_fail)
    if(NOT REX_ENABLE_FAILING_TESTS)
      list(APPEND _labels disabled)
      set(_disable_test TRUE)
    endif()
  endif()
  if(DEFINED ROSE_EXPECTED_FAIL_TESTCODES AND input_file IN_LIST ROSE_EXPECTED_FAIL_TESTCODES)
    list(APPEND _labels expected_fail)
    set(_expected_fail TRUE)
  endif()
  list(REMOVE_DUPLICATES _labels)
  set_tests_properties(${test_id} PROPERTIES LABELS "${_labels}")

  if(_expected_fail)
    set_tests_properties(${test_id} PROPERTIES WILL_FAIL TRUE)
  endif()
  if(_disable_test)
    set_tests_properties(${test_id} PROPERTIES DISABLED TRUE)
  endif()
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
  set(${out_var} "${label}_${_test_key}" PARENT_SCOPE)
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

  if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Xtensa")
    add_subdirectory(mergeAST_tests)
    add_subdirectory(ElsaTestCases)
    add_subdirectory(virtualCFG_tests)
    add_subdirectory(mixLanguage_tests)
    add_subdirectory(systemc_tests)
    add_subdirectory(staticCFG_tests)
  endif()
endif()

if(ENABLE-C OR ENABLE-FORTRAN)
  add_subdirectory(OpenMP_tests)
  if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/OpenMP_VV/tests")
    add_subdirectory(OpenMP_VV_tests)
  else()
    message(STATUS "Skipping OpenMP_VV_tests: third_party/OpenMP_VV/tests not found")
  endif()
endif()
