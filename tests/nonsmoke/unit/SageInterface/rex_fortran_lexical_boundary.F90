#define JOINED 8
#define REX_EMPTY(x)
#define REX_FUNCTION(x) x
#define REX_ALIAS REX_FUNCTION
#define REX_FILENAME __FILE__
#define REX_WARN_CALL(message,file,line) call record(message,file,line)
#define REX_WARNING(message) REX_WARN_CALL(message,REX_FILENAME,__LINE__)
#define REX_WARNING_IF(condition,message) if (condition) REX_WARNING(message)
#define REX_IGNORED_WARNING_IF(condition,message) CONTINUE !message
#define REX_STRINGIFY_INNER(x) #x
#define REX_STRINGIFY(x) REX_STRINGIFY_INNER(x)
#define REX_CAPTURE(x) call capture(REX_STRINGIFY(x))
#define REX_VARIADIC(first,...) call values(first,__VA_ARGS__)
#define REX_LEXICAL_D REX_LEXICAL_D
#define REX_LEXICAL_COMMENT !REX_LEXICAL_D
subroutine p(a,b)
  integer :: a, b, n, jo
  integer, parameter :: rex_omp_value = _OPENMP
  integer, parameter :: rex_builtin_line = __LINE__
  character(len=*), parameter :: rex_builtin_file = __FILE__
  n=JO&
!$&INED&
  & + 0
!$ n=n+1
  n=n+2 REX_EMPTY("!") ! lexical terminal macro comment
  n=n+REX_ALIAS(2)
  REX_WARNING_IF(n > 0, "rex warning\npayload")
  REX_WARNING_IF(n > 0, "rex warning\npayload")
  REX_IGNORED_WARNING_IF(n < 0, "rex ignored\npayload");
  REX_CAPTURE(n + (1))
  REX_CAPTURE()
  REX_VARIADIC(1, 2, 3)
  REX_LEXICAL_COMMENT
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
