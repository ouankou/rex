module rex_test2026_openacc_module
contains
!$acc routine seq
  subroutine acc_routine(n, x)
    implicit none
    integer, intent(in) :: n
    real :: x(n)
    x = 0.0
  end subroutine acc_routine
end module rex_test2026_openacc_module

program rex_test2026_openacc_constructs
  use rex_test2026_openacc_module
  implicit none
  integer :: i
  real :: a(10)
!$acc parallel loop
  do i = 1, 10
    a(i) = i
  end do
!$acc end parallel loop
!$acc cache(a)
!$acc wait
!$acc atomic update
  a(1) = a(1) + 1.0
end program rex_test2026_openacc_constructs
