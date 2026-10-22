module rex_namelist_owner
  implicit none
  integer :: hidden = 5
  namelist /imported_group/ hidden
contains
  subroutine rex_set_hidden(value)
    integer, intent(in) :: value
    hidden = value
  end subroutine
  integer function rex_get_hidden()
    rex_get_hidden = hidden
  end function
end module rex_namelist_owner

module rex_namelist_bridge
  use rex_namelist_owner, only: exported_group => imported_group, &
                              rex_set_hidden, rex_get_hidden
end module rex_namelist_bridge

program rex_namelist_object_identity
  use rex_namelist_bridge, only: renamed_group => exported_group, &
                              rex_set_hidden, rex_get_hidden
  implicit none
  integer :: first, second
  character(128) :: buffer
  namelist /rex_group/ first
  namelist /rex_group/ second
  first = 7
  second = 11
  write(buffer, nml=rex_group)
  first = 0
  second = 0
  read(buffer, nml=rex_group)
  if (first + second /= 18) stop 1
  write(buffer, nml=renamed_group)
  call rex_set_hidden(23)
  read(buffer, nml=renamed_group)
  if (rex_get_hidden() /= 5) stop 2
end program rex_namelist_object_identity
