module rex_unparser_safety
  implicit none

  type :: rex_type
    integer :: value
  end type rex_type

  integer, target :: target = 7
  integer, pointer :: ptr => target

contains

  subroutine reset(item)
    class(rex_type), intent(out) :: item
    item%value = 0
  end subroutine reset
end module rex_unparser_safety

block data rex_data
  integer :: shared
  common /rex_common/ shared
  data shared / 3 /
end block data rex_data
