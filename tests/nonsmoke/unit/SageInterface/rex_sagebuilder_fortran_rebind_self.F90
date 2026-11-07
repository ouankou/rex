#ifndef REX_REBIND_MAIN
#define REX_REBIND_MAIN
program rex_rebind_self
implicit none
integer :: total
total = 0
#define REX_REBIND_END 0
#include "rex_sagebuilder_fortran_rebind_self.F90"
#include "rex_sagebuilder_fortran_rebind_self.F90"
#undef REX_REBIND_END
#define REX_REBIND_END 1
total = total + 1
#include "rex_sagebuilder_fortran_rebind_self.F90"
#else
#if REX_REBIND_END
! REX_REBIND_SELF_END
print '(I0)', total
end program rex_rebind_self
#else
! REX_REBIND_SELF_INCREMENT
total = total + 3
#endif
#endif
