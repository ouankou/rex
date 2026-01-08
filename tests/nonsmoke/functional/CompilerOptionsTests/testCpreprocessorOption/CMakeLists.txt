include_directories(${ROSE_INCLUDES})

set(_cpre_include_flags ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR})
string(JOIN " " _cpre_include_flags_str ${_cpre_include_flags})

set(_cpre_testcodes data_hash.C)

set(_cpre_disabled_testcodes
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
  testUnparser.C
  f_op_all.C
  i_op_all.C
  d_op_all.C
)

foreach(testcode IN LISTS _cpre_testcodes)
  get_filename_component(_cpre_name ${testcode} NAME_WE)
  add_test(
    NAME compiler_options_cpreprocessor_${_cpre_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -E -DHAVE_CONFIG_H ${_cpre_include_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode} > ${_cpre_name}.out.C && ${CMAKE_CXX_COMPILER} -DHAVE_CONFIG_H -c ${_cpre_name}.out.C"
  )
endforeach()

foreach(testcode IN LISTS _cpre_disabled_testcodes)
  get_filename_component(_cpre_name ${testcode} NAME_WE)
  add_test(
    NAME compiler_options_cpreprocessor_${_cpre_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -E -DHAVE_CONFIG_H ${_cpre_include_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${testcode} > ${_cpre_name}.out.C && ${CMAKE_CXX_COMPILER} -DHAVE_CONFIG_H -c ${_cpre_name}.out.C"
  )
  set_tests_properties(compiler_options_cpreprocessor_${_cpre_name}
    PROPERTIES DISABLED TRUE)
endforeach()

add_test(
  NAME compiler_options_cpreprocessor_sys_include_cc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_C_COMPILER} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe
    -I${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testSysIncludeOptionOrder.c
)
set_tests_properties(compiler_options_cpreprocessor_sys_include_cc
  PROPERTIES WILL_FAIL TRUE)

add_test(
  NAME compiler_options_cpreprocessor_sys_include_translator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe
    -I${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe
    -c ${CMAKE_CURRENT_SOURCE_DIR}/testSysIncludeOptionOrder.c
)
set_tests_properties(compiler_options_cpreprocessor_sys_include_translator
  PROPERTIES WILL_FAIL TRUE)

add_test(
  NAME compiler_options_cpreprocessor_include_order
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "${CMAKE_C_COMPILER} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe -I${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe -c ${CMAKE_CURRENT_SOURCE_DIR}/testIncludeOptionOrder.c > testIncludeOptionOrder.out && ${ROSE_TEST_TRANSLATOR} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/doNotChooseMe -I${CMAKE_CURRENT_SOURCE_DIR}/doChooseMe -c ${CMAKE_CURRENT_SOURCE_DIR}/testIncludeOptionOrder.c > testIncludeOptionOrder.out"
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testCpreprocessorOption_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_cpreprocessor_data_hash\;compiler_options_cpreprocessor_include_order\;compiler_options_cpreprocessor_sys_include_translator
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testCpreprocessorOption_check-local PROPERTIES
  DEPENDS "compiler_options_cpreprocessor_data_hash;compiler_options_cpreprocessor_include_order;compiler_options_cpreprocessor_sys_include_translator"
)
