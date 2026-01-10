include_directories(${ROSE_INCLUDES})

set(_aplus_include_flags ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR})
string(JOIN " " _aplus_include_flags_str ${_aplus_include_flags})

set(_aplus_testcodes
  test_bool.C
  data_hash.C
  abstract_op.C
  error_checking.C
  index_operator_notemplates.C
  lazy_statement.C
  new_operators_notemplates.C
  descriptor_cache.C
  lazy_aggregate.C
  lazy_task.C
  static_initialization.C
  comm_man.C
  descriptor_notemplates.C
  lazy_expression.C
  lazy_taskset.C
  index.C
  lazy_operand.C
  lazyarray.C
  where.C
  partitioning.C
  domain.C
  array_notemplates.C
)

set(_aplus_disabled_testcodes
  testUnparser.C
  f_op_all.C
  i_op_all.C
  d_op_all.C
)

foreach(testcode IN LISTS _aplus_testcodes)
  get_filename_component(_aplus_name ${testcode} NAME_WE)
  add_test(
    NAME compiler_options_aplus_${_aplus_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "${ROSE_TEST_TRANSLATOR} -DHAVE_CONFIG_H -rose:skip_transformation -E ${_aplus_include_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode} > ${testcode} && ${CMAKE_CXX_COMPILER} -DHAVE_CONFIG_H ${_aplus_include_flags_str} -c ${testcode}"
  )
endforeach()

foreach(testcode IN LISTS _aplus_disabled_testcodes)
  get_filename_component(_aplus_name ${testcode} NAME_WE)
  add_test(
    NAME compiler_options_aplus_${_aplus_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "${ROSE_TEST_TRANSLATOR} -DHAVE_CONFIG_H -rose:skip_transformation -E ${_aplus_include_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode} > ${testcode} && ${CMAKE_CXX_COMPILER} -DHAVE_CONFIG_H ${_aplus_include_flags_str} -c ${testcode}"
  )
  set_tests_properties(compiler_options_aplus_${_aplus_name} PROPERTIES DISABLED TRUE)
endforeach()

add_test(
  NAME compiler_options_aplus_quiet_output
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f outputFile; ${ROSE_TEST_TRANSLATOR} -DHAVE_CONFIG_H -rose:skip_transformation ${_aplus_include_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/test_bool.C > outputFile 2>&1; outputLines=$(wc -l < outputFile); if [ \"$outputLines\" -eq 0 ]; then exit 0; else cat outputFile; exit 1; fi"
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_A++Code_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_aplus_abstract_op\;compiler_options_aplus_array_notemplates\;compiler_options_aplus_comm_man\;compiler_options_aplus_data_hash\;compiler_options_aplus_descriptor_cache\;compiler_options_aplus_descriptor_notemplates\;compiler_options_aplus_domain\;compiler_options_aplus_error_checking\;compiler_options_aplus_index\;compiler_options_aplus_index_operator_notemplates\;compiler_options_aplus_lazy_aggregate\;compiler_options_aplus_lazy_expression\;compiler_options_aplus_lazy_operand\;compiler_options_aplus_lazy_statement\;compiler_options_aplus_lazy_task\;compiler_options_aplus_lazy_taskset\;compiler_options_aplus_lazyarray\;compiler_options_aplus_new_operators_notemplates\;compiler_options_aplus_partitioning\;compiler_options_aplus_static_initialization\;compiler_options_aplus_test_bool\;compiler_options_aplus_where
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_A++Code_check-local PROPERTIES DEPENDS "compiler_options_aplus_abstract_op;compiler_options_aplus_array_notemplates;compiler_options_aplus_comm_man;compiler_options_aplus_data_hash;compiler_options_aplus_descriptor_cache;compiler_options_aplus_descriptor_notemplates;compiler_options_aplus_domain;compiler_options_aplus_error_checking;compiler_options_aplus_index;compiler_options_aplus_index_operator_notemplates;compiler_options_aplus_lazy_aggregate;compiler_options_aplus_lazy_expression;compiler_options_aplus_lazy_operand;compiler_options_aplus_lazy_statement;compiler_options_aplus_lazy_task;compiler_options_aplus_lazy_taskset;compiler_options_aplus_lazyarray;compiler_options_aplus_new_operators_notemplates;compiler_options_aplus_partitioning;compiler_options_aplus_static_initialization;compiler_options_aplus_test_bool;compiler_options_aplus_where" )
