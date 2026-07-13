module rex_fortran_enum_bind_c_mod
  implicit none

  enum, bind(c)
    enumerator :: rex_zero
    enumerator :: rex_five = 5, rex_six
  end enum

contains

  subroutine rex_verify_enum()
    enum, bind(c)
      enumerator :: rex_local = rex_five, rex_local_next
    end enum

    if (rex_zero /= 0) error stop
    if (rex_six /= 6) error stop
    if (rex_local_next /= 6) error stop
  end subroutine rex_verify_enum
end module rex_fortran_enum_bind_c_mod

program rex_fortran_enum_bind_c
  use rex_fortran_enum_bind_c_mod
  implicit none

  call rex_verify_enum()
  if (rex_five /= 5) error stop
end program rex_fortran_enum_bind_c
