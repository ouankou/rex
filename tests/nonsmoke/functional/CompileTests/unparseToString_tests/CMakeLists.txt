add_executable(unparseToString unparseToString.C )
target_link_libraries(unparseToString ROSE_DLL ${link_with_libraries} )

add_executable(unparseProject unparseProject.C)
target_link_libraries(unparseProject ROSE_DLL ${link_with_libraries})

set(ROSE_FLAGS  -w )

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(TESTCODE_INCLUDES -I${TESTCODE_DIR})

# This populates the list EXAMPLE_TESTCODES_REQUIRED_TO_PASS

set(CXX_TESTCODES_TO_UNPARSE ${CXX_TESTCODES_STANDARD_TO_RUN})
list(APPEND CXX_TESTCODES_TO_UNPARSE test2003_24.C test2004_82.C)
list(REMOVE_DUPLICATES CXX_TESTCODES_TO_UNPARSE)

foreach(file_to_test ${CXX_TESTCODES_TO_UNPARSE})
  rex_get_cxx_specimen_extra_flags(extra_frontend_flags ${file_to_test})

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
