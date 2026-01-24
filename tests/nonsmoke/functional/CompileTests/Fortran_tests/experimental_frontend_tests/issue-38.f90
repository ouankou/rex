subroutine issue_38_sub(x, y)
  integer :: x
  integer, optional :: y
  interface
    subroutine issue_38_helper(a, b)
      integer :: a
      integer, optional :: b
    end subroutine issue_38_helper
  end interface

  if (present(y)) then
    x = x + y
  endif
  call issue_38_helper(x, y)
end subroutine issue_38_sub

subroutine issue_38_helper(a, b)
  integer :: a
  integer, optional :: b

  if (present(b)) then
    a = a + b
  else
    a = a * 2
  endif
end subroutine issue_38_helper
