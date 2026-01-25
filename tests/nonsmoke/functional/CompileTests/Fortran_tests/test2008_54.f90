! This file shows the three ways in which a function can specify its return type
! (not counting the variations of these that would use implicit type rules).

function foo1() result(ret)
    integer ret
    ret = 1
end function

integer function foo2() result(ret)
  ! Error checking with -rose:fortran_std=f95 causes this to be an error
  ! integer ret
    ret = 1
end function

! Older F77 style syntax
function foo3()
    integer foo3
    foo3 = 1
end function
