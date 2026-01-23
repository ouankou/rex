set(_experimental_fortran_tests
  binary_expressions.f90
  case_1.f90
  common_1.f90
  continue.f90
  dowhile.f90
  expressions.f90
  fail_image.f90
  ifstmt1.f90
  ifstmt2.f90
  ifstmt3.f90
  ifstmt4.f90
  ifstmt5.f90
  implicit_none.f90
  implicit_none_external.f90
  implicit_none_external_type.f90
  implicit_none_type.f90
  negative_values.f90
  print1.f90
  type_decls_1.f90
  type_decls_2.f90
  type_decls_3.f90
  Expr_IntLiteralConstant.f90
)

set(_experimental_fortran_20xx_tests
  stop.f90
  tiny.f90
)

set(_experimental_fortran_todo_tests
  common_2.f90
  function_2.f90
  function_3.f90
  function_4.f90
  subroutine_1.f90
  type_def_1.f90
)

function(add_experimental_fortran_test specimen extra_flags)
  string(REPLACE "." "_" _name "${specimen}")
  add_test(
    NAME experimental_fortran_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND sh -c
      "$<TARGET_FILE:testTranslator> -rose:experimental_flang_frontend ${extra_flags} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen} && test -s rose_${specimen}"
  )
  set_tests_properties(experimental_fortran_${_name} PROPERTIES LABELS experimental_fortran)
endfunction()

foreach(specimen IN LISTS _experimental_fortran_tests)
  add_experimental_fortran_test(${specimen} "")
endforeach()

foreach(specimen IN LISTS _experimental_fortran_20xx_tests)
  add_experimental_fortran_test(${specimen} "-rose:skipfinalCompileStep")
endforeach()

foreach(specimen IN LISTS _experimental_fortran_todo_tests)
  add_experimental_fortran_test(${specimen} "")
endforeach()
