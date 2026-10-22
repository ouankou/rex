module rex_parent_module
  implicit none
  type :: parent
    integer :: value
  end type
end module

module rex_extension_module
  use rex_parent_module, only: renamed_parent => parent
  implicit none
  type :: parent
    real :: decoy
  end type
  type, extends(renamed_parent) :: child
    integer :: own_value
  end type
  type, extends(child) :: grandchild
    integer :: last_value
  end type
end module

program rex_fortran_extends_semantic_binding
  use rex_parent_module, only: renamed_parent => parent
  use rex_extension_module, only: grandchild
  implicit none
  type :: parent
    real :: decoy
  end type
  type, extends(renamed_parent) :: local_child
    integer :: own_value
  end type
  type(grandchild) :: first
  type(local_child) :: second
  first%value = 7
  second%value = 9
  if (first%value + second%value /= 16) error stop
end program
