#define REX_SOURCE_TOKEN_VALUE 7
#define REX_SOURCE_TOKEN_TEXT "macro bang ! remains preprocessing text"
#define REX_SOURCE_TOKEN_SUM(lhs, rhs) \
  ((lhs) + (rhs))
#if 0
this inactive source is intentionally not valid Fortran
#define REX_SOURCE_TOKEN_INACTIVE 1
#endif
program rex_fortran_source_token_provenance
  implicit none
  integer, parameter :: value = REX_SOURCE_TOKEN_SUM(REX_SOURCE_TOKEN_VALUE, 0)
  character(len=*), parameter :: text = "literal bang ! remains character data"

! standalone source-token comment
!$ompx rex_source_token_opaque
!$OMP ERROR AT(EXECUTION) SEVERITY(WARNING) MESSAGE(REX_SOURCE_TOKEN_TEXT)
  print *, value, text ! trailing source-token comment
end program rex_fortran_source_token_provenance
