subroutine rex_custom_implicit_dummy_before_statement(alpha, beta, zeta)
  implicit complex(a-c), logical(z)

  alpha = cmplx(1.0, 2.0)
  beta = cmplx(3.0, 4.0)
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

program rex_custom_implicit_semantic_type
  implicit none

  complex :: value
  complex :: rex_custom_implicit_function_result

  call rex_custom_implicit_dummy_before_statement(value, value, .true.)
  call rex_custom_implicit_local_after_statement()
  value = rex_custom_implicit_function_result(cmplx(2.0, 3.0))
  if (real(value) /= 3.0 .or. aimag(value) /= 2.0) error stop 3
end program rex_custom_implicit_semantic_type
