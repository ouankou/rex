program rex_test2026_cuf_kernel_do
  implicit none
  integer :: i
  real :: a(10)
!$cuf kernel do <<<1,1>>>
  do i = 1, 10
    a(i) = i
  end do
end program rex_test2026_cuf_kernel_do
