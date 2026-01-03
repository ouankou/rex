add_executable(unparseToString unparseToString.C )
target_link_libraries(unparseToString ROSE_DLL ${link_with_libraries} )

add_executable(unparseProject unparseProject.C)
target_link_libraries(unparseProject ROSE_DLL ${link_with_libraries})

set(ROSE_FLAGS  -w )

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(TESTCODE_INCLUDES -I${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/A++Code
                      -I${TESTCODE_DIR})
set(rex_test2025_issue148_system_header_dir
  ${CMAKE_CURRENT_SOURCE_DIR}/../../input_codes/system_header_like)

# This populates the list EXAMPLE_TESTCODES_REQUIRED_TO_PASS

set(CXX_TESTCODES_TO_UNPARSE ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM CXX_TESTCODES_TO_UNPARSE ${TESTCODE_CURRENTLY_FAILING})

function(rex_lock_test_for_input test_name input_file)
  get_filename_component(lock_base ${input_file} NAME_WE)
  set_tests_properties(${test_name}
    PROPERTIES RESOURCE_LOCK "rose_output_${lock_base}")
endfunction()

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
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

    set_tests_properties(ua_failing_${file_to_test}
      PROPERTIES DEPENDS ut_failing_${file_to_test} 
      LABELS UNPARSETOSTRING_FAILING)
  endforeach()
endif()

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

  set_tests_properties(ua_${file_to_test}
    PROPERTIES DEPENDS ut_${file_to_test} 
    LABELS UNPARSETOSTRING)
endforeach()

add_test(
  NAME unparseProject_test2014_26
  COMMAND unparseProject ${CMAKE_CURRENT_SOURCE_DIR}/test2014_26.C
)

add_test(
  NAME unparseProject_test2017_01
  COMMAND unparseProject ${CMAKE_CURRENT_SOURCE_DIR}/test2017_01.C
)
