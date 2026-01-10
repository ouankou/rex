set(files_to_test
  test_bool.C
  data_hash.C
  error_checking.C
  lazy_statement.C
  new_operators_notemplates.C
  descriptor_cache.C
  lazy_aggregate.C
  lazy_task.C
  static_initialization.C
  comm_man.C
  descriptor_notemplates.C
  lazy_taskset.C
  where.C
  abstract_op.C
  index_operator_notemplates.C
  index.C
  lazy_operand.C
  domain.C
  partitioning.C
  lazy_expression.C
  lazyarray.C
  array_notemplates.C
  f_op_all.C
  i_op_all.C
  d_op_all.C
)

set(ROSE_DISABLED_TESTCODES
  f_op_all.C
  i_op_all.C
  d_op_all.C
)
if(NOT with-ROSE_LONG_MAKE_CHECK_RULE)
  list(APPEND ROSE_DISABLED_TESTCODES
    error_checking.C
    lazy_statement.C
    new_operators_notemplates.C
    descriptor_cache.C
    lazy_aggregate.C
    lazy_task.C
    static_initialization.C
    comm_man.C
    descriptor_notemplates.C
    lazy_taskset.C
    where.C
    abstract_op.C
    index_operator_notemplates.C
    index.C
    lazy_operand.C
    domain.C
    partitioning.C
    lazy_expression.C
    lazyarray.C
    array_notemplates.C
  )
endif()

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} AXXTEST)
endforeach()

# Makefile check targets.
set(_axx_group_sources ${files_to_test})
list(REMOVE_ITEM _axx_group_sources ${ROSE_DISABLED_TESTCODES})
set(_axx_group_tests)
foreach(file_to_test ${_axx_group_sources})
  string(REPLACE "/" "_" _test_key "${file_to_test}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _axx_group_tests "AXXTEST_${_test_key}")
endforeach()
add_test(
  NAME nonsmoke_functional_CompileTests_A++Code_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_axx_group_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_A++Code_check-local PROPERTIES DEPENDS "${_axx_group_tests}" )
