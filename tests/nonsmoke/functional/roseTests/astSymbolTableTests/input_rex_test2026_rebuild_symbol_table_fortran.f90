module rex_test2026_rebuild_mod
contains
  subroutine rex_test2026_rebuild_worker(n)
    implicit none
    integer :: i
    integer, intent(in) :: n
!$omp parallel do
    do i = 1, n
    end do
  end subroutine rex_test2026_rebuild_worker
end module rex_test2026_rebuild_mod

program rex_test2026_rebuild_driver
  use rex_test2026_rebuild_mod
  implicit none
  call rex_test2026_rebuild_worker(4)
end program rex_test2026_rebuild_driver
