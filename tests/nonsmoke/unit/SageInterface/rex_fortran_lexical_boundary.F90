#define JOINED 8
#define REX_EMPTY(x)
#define REX_FUNCTION(x) x
#define REX_ALIAS REX_FUNCTION
subroutine p(a,b)
  integer :: a, b, n, jo
  integer, parameter :: rex_omp_value = _OPENMP
  n=JO&
!$&INED&
  & + 0
!$ n=n+1
  n=n+2 REX_EMPTY("!") ! lexical terminal macro comment
  n=n+REX_ALIAS(2)
  call sub( &
!$ a, &
  b)
!$omp parallel &
!$omp& num_threads(2)
  a = a + 1
!$omp end parallel
!$acc parallel
!$acc end parallel
!$cuf kernel do (1) <<<*, *>>>
#if 0
!$omp rex_fortran_inactive_marker
#endif
end
