! rex_module_prefix
module rex_procedure_comment_module ! rex_module_header
  implicit none
  ! rex_type_prefix
  type :: &
      & rex_header_type ! rex_type_header
    integer :: value
  end type rex_header_type
end module rex_procedure_comment_module

program rex_procedure_canonical_ownership ! rex_program_header
  implicit none

  ! rex_interface_block_prefix
  interface ! rex_interface_block_header
    ! rex_interface_procedure_prefix
    subroutine rex_interface_subroutine( &
        & value) ! rex_interface_header
      integer, intent(in) :: value
    end subroutine rex_interface_subroutine

    ! rex_interface_procedure_prefix
    integer function rex_interface_function( &
        & value) ! rex_interface_header
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

  ! rex_procedure_prefix
  subroutine rex_internal_worker( &
      & value) ! rex_procedure_header
    integer, intent(in) :: value
  end subroutine rex_internal_worker

end program rex_procedure_canonical_ownership
