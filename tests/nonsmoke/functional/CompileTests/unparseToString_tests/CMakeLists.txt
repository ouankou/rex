add_executable(unparseToString unparseToString.C )
target_link_libraries(unparseToString ROSE_DLL ${link_with_libraries} )

add_executable(unparseProject unparseProject.C)
target_link_libraries(unparseProject ROSE_DLL ${link_with_libraries})

set(ROSE_FLAGS  -w )

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(TESTCODE_INCLUDES -I${TESTCODE_DIR})
set(rex_test2025_issue148_system_header_dir
  ${CMAKE_CURRENT_SOURCE_DIR}/../../input_codes/system_header_like)

# This populates the list EXAMPLE_TESTCODES_REQUIRED_TO_PASS

set(CXX_TESTCODES_TO_UNPARSE ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(APPEND CXX_TESTCODES_TO_UNPARSE test2003_24.C test2004_82.C)
list(REMOVE_DUPLICATES CXX_TESTCODES_TO_UNPARSE)
list(REMOVE_ITEM CXX_TESTCODES_TO_UNPARSE ${TESTCODE_CURRENTLY_FAILING})
set(REX_UNPARSE_ALIAS_FAILING
  math.C
  test2004_117.C
  test2004_121.C
  test2004_35.C
  test2004_77.C
  test2004_85.C
  test2005_06.C
  test2005_42.C
  test2005_50.C
  test2006_08.C
  test2006_20.C
  test2006_37.C
  test2007_141.C
  test2007_51.C
  test2007_52.C
  test2007_62.C
  test2009_21.C
  test2011_01.C
  test2011_02.C
  test2011_05.C
  test2011_06.C
  test2011_07.C
  test2011_141.C
  test2011_142.C
)

function(rex_lock_test_for_input test_name input_file)
  get_filename_component(lock_base ${input_file} NAME_WE)
  set_tests_properties(${test_name} PROPERTIES RESOURCE_LOCK rose_output_${lock_base})
endfunction()

foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
  set(_labels UNPARSETOSTRING_FAILING known_fail)
  if(NOT REX_ENABLE_FAILING_TESTS)
    list(APPEND _labels disabled)
  endif()

  if(file_to_test IN_LIST REX_UNPARSE_ALIAS_FAILING)
    add_test(
      NAME ut_${file_to_test}
      COMMAND unparseToString ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
              -c ${TESTCODE_DIR}/${file_to_test})
    add_test(
      NAME ua_${file_to_test}
      COMMAND unparseToString --all ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
              -c ${TESTCODE_DIR}/${file_to_test})

    rex_lock_test_for_input(ut_${file_to_test} ${file_to_test})
    rex_lock_test_for_input(ua_${file_to_test} ${file_to_test})

    if(NOT REX_ENABLE_FAILING_TESTS)
      set_tests_properties(ut_${file_to_test} PROPERTIES DISABLED TRUE)
      set_tests_properties(ua_${file_to_test} PROPERTIES DISABLED TRUE)
    endif()
    set_tests_properties(ut_${file_to_test} PROPERTIES LABELS "${_labels}")
    set_tests_properties(ua_${file_to_test} PROPERTIES DEPENDS ut_${file_to_test} LABELS "${_labels}")
  else()
    add_test(
      NAME ut_failing_${file_to_test}
      COMMAND unparseToString ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
              -c ${TESTCODE_DIR}/${file_to_test})
    add_test(
      NAME ua_failing_${file_to_test}
      COMMAND unparseToString --all ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
              -c ${TESTCODE_DIR}/${file_to_test})

    rex_lock_test_for_input(ut_failing_${file_to_test} ${file_to_test})
    rex_lock_test_for_input(ua_failing_${file_to_test} ${file_to_test})

    if(NOT REX_ENABLE_FAILING_TESTS)
      set_tests_properties(ut_failing_${file_to_test} PROPERTIES DISABLED TRUE)
      set_tests_properties(ua_failing_${file_to_test} PROPERTIES DISABLED TRUE)
    endif()
    set_tests_properties(ut_failing_${file_to_test} PROPERTIES LABELS "${_labels}")
    set_tests_properties(ua_failing_${file_to_test} PROPERTIES DEPENDS ut_failing_${file_to_test} LABELS "${_labels}")
  endif()
endforeach()

foreach(file_to_test ${CXX_TESTCODES_TO_UNPARSE})
  set(extra_frontend_flags)
  if (file_to_test IN_LIST REX_FRONTEND_ERROR_TESTS)
    list(APPEND extra_frontend_flags -rex:clang:continue-on-error)
  endif()
  if (file_to_test STREQUAL "rex_test2025_issue148_system_header_mutation.cpp")
    list(APPEND extra_frontend_flags
      -isystem ${rex_test2025_issue148_system_header_dir})
  endif()

  add_test(
    NAME ut_${file_to_test}
    COMMAND unparseToString ${ROSE_FLAGS} ${TESTCODE_INCLUDES} ${extra_frontend_flags}
            -c ${TESTCODE_DIR}/${file_to_test})
  add_test(
    NAME ua_${file_to_test}
    COMMAND unparseToString --all ${ROSE_FLAGS} ${TESTCODE_INCLUDES} ${extra_frontend_flags}
            -c ${TESTCODE_DIR}/${file_to_test})

  rex_lock_test_for_input(ut_${file_to_test} ${file_to_test})
  rex_lock_test_for_input(ua_${file_to_test} ${file_to_test})

  set_tests_properties(ua_${file_to_test} PROPERTIES DEPENDS ut_${file_to_test} LABELS UNPARSETOSTRING)
endforeach()

add_test(
  NAME rex_issue481_typedef_const_no_double_const
  COMMAND sh -c "out=$(\"$@\") || exit $?; echo \"$out\" | grep -q \"const constint\" && exit 1; echo \"$out\" | grep -q \"\\bconstint\\b\""
    dummy
    $<TARGET_FILE:unparseToString> ${ROSE_FLAGS} ${TESTCODE_INCLUDES}
      -c ${TESTCODE_DIR}/test2006_130.C
)
set_tests_properties(rex_issue481_typedef_const_no_double_const PROPERTIES LABELS rex)

add_test(
  NAME unparseProject_test2014_26
  COMMAND unparseProject ${CMAKE_CURRENT_SOURCE_DIR}/test2014_26.C
)

add_test(
  NAME unparseProject_test2017_01
  COMMAND unparseProject ${CMAKE_CURRENT_SOURCE_DIR}/test2017_01.C
)

# Makefile check targets.
set(_unparse_all_tests)
set(_unparse_type_tests)
foreach(file_to_test ${CXX_TESTCODES_TO_UNPARSE})
  list(APPEND _unparse_all_tests "ua_${file_to_test}")
  list(APPEND _unparse_type_tests "ut_${file_to_test}")
endforeach()
set(_unparse_check_tests ${_unparse_all_tests} ${_unparse_type_tests})

add_test(
  NAME nonsmoke_functional_CompileTests_unparseToString_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_unparse_check_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_unparseToString_tests_check-local PROPERTIES DEPENDS "${_unparse_check_tests}" )

add_test(
  NAME nonsmoke_functional_CompileTests_unparseToString_tests_check-unparseAll
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_unparse_all_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_unparseToString_tests_check-unparseAll PROPERTIES LABELS disabled DEPENDS "${_unparse_all_tests}" )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_unparseToString_tests_check-unparseAll PROPERTIES DISABLED TRUE)
endif()

add_test(
  NAME nonsmoke_functional_CompileTests_unparseToString_tests_check-unparseType
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_unparse_type_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_unparseToString_tests_check-unparseType PROPERTIES LABELS disabled DEPENDS "${_unparse_type_tests}" )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_unparseToString_tests_check-unparseType PROPERTIES DISABLED TRUE)
endif()
