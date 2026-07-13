module rex_unparser_fortran_procedure_pointer
  implicit none

  abstract interface
    subroutine rex_callback()
    end subroutine rex_callback
  end interface

  procedure(rex_callback), pointer :: procedure_pointer => null()

contains

  subroutine rex_target()
  end subroutine rex_target

  subroutine rex_assign_target()
    procedure_pointer => rex_target
    call procedure_pointer()
  end subroutine rex_assign_target
end module rex_unparser_fortran_procedure_pointer
