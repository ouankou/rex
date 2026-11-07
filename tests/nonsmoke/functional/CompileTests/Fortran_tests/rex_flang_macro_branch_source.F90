#define REX_BRANCH_BODY(flag) if(flag)then; value=7; else; value=-1; endif
#define REX_BRANCH_ALIAS REX_BRANCH_BODY
program rex_flang_macro_branch_source
  implicit none
  integer :: value
  logical :: flag
  flag = .true.
  value = 0
  REX_BRANCH_ALIAS(flag)
  print '(I0)', value
end program
