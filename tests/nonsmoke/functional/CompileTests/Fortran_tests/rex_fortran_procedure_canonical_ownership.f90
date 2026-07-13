program rex_procedure_canonical_ownership
  implicit none

  interface
    subroutine rex_interface_subroutine(value)
      integer, intent(in) :: value
    end subroutine rex_interface_subroutine

    integer function rex_interface_function(value)
      integer, intent(in) :: value
    end function rex_interface_function
  end interface

  integer :: input
  integer :: rex_statement_function

  rex_statement_function(input) = input + 1

  input = rex_statement_function(1)
  call rex_implicit_external(input)
  call rex_internal_worker(input)

contains

  subroutine rex_internal_worker(value)
    integer, intent(in) :: value
  end subroutine rex_internal_worker

end program rex_procedure_canonical_ownership
