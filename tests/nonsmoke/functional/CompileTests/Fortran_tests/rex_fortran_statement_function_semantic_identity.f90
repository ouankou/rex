program rex_statement_function_semantic_identity
  implicit real(a-h, o-z), integer(i-n)

  integer :: rex_statement_value
  integer :: input
  integer :: output

  rex_statement_value(input) = input + 1
  output = rex_statement_value(8)
  if (output /= 9) error stop 1
end program rex_statement_function_semantic_identity
