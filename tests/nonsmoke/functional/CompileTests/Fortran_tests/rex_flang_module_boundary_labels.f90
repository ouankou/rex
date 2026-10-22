10 module rex_flang_module_boundary_labels_m
  implicit none
20 enum, bind(c)
    enumerator :: red = 1
30 end enum
40 type :: item
    integer :: value
50 end type item
60 interface
    subroutine rex_declared()
    end subroutine rex_declared
70 end interface
80 end module rex_flang_module_boundary_labels_m

program rex_flang_module_boundary_labels
  use rex_flang_module_boundary_labels_m
  implicit none
  if (red /= 1) error stop
end program rex_flang_module_boundary_labels
