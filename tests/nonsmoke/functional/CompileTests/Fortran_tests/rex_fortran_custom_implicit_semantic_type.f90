subroutine rex_custom_implicit_dummy_before_statement(alpha, beta, zeta)
  implicit complex*008(a-c,e-e), logical(z)

  alpha = cmplx(1.0, 2.0)
  beta = cmplx(3.0, 4.0)
  echo = cmplx(5.0, 6.0)
  if (aimag(echo) /= 6.0) error stop 1
  zeta = .true.
end subroutine rex_custom_implicit_dummy_before_statement

subroutine rex_custom_implicit_local_after_statement()
  implicit complex(a-c), logical(z)

  cobalt = cmplx(5.0, 6.0)
  zenith = .false.
  if (aimag(cobalt) /= 6.0 .or. zenith) error stop 2
end subroutine rex_custom_implicit_local_after_statement

function rex_custom_implicit_function_result(alpha) result(beta)
  implicit complex(a-c), logical(z)

  beta = alpha + cmplx(1.0, -1.0)
end function rex_custom_implicit_function_result

subroutine rex_data_initialization()
  implicit none
  integer, parameter :: copies = 2
  integer repeated(3) /2*2,1/
  integer named(3) /copies*3,4/
  integer scalar /7/
  integer standalone(3)
  data standalone /copies*3,4/
  if (any(repeated /= [2,2,1])) error stop 4
  if (any(named /= [3,3,4])) error stop 5
  if (scalar /= 7 .or. any(standalone /= named)) error stop 6
end subroutine rex_data_initialization

program rex_custom_implicit_semantic_type
  implicit none

  complex :: value
  complex :: rex_custom_implicit_function_result

  call rex_custom_implicit_dummy_before_statement(value, value, .true.)
  call rex_custom_implicit_local_after_statement()
  call rex_data_initialization()
  value = rex_custom_implicit_function_result(cmplx(2.0, 3.0))
  if (real(value) /= 3.0 .or. aimag(value) /= 2.0) error stop 3
end program rex_custom_implicit_semantic_type
