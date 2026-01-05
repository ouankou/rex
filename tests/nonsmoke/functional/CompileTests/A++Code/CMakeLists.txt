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
