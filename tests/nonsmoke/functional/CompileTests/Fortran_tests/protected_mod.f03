module protected_mod
  implicit none
  integer, protected :: pval = 0
  integer :: qval = 0
contains
  subroutine set_pval(v)
    integer, intent(in) :: v
    pval = v
  end subroutine set_pval
end module protected_mod
