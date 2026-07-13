! This file when compiled after the test2010_78.f90
! (which contains the m178 module) will not unparse
! the "i1 = 1" statement.
program prog1
use m178
i1 = 1
end program
