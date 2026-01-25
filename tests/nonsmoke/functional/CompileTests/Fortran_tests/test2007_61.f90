subroutine foo()
  integer :: a, b, c
  ! solve(a,z=0)
  a = 0
  call solve(a, b, c)
contains
  subroutine solve(x, y, z)
    integer, optional :: x, y, z
    x = 1
  end subroutine solve
end subroutine foo
