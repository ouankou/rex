#define REX_MOVE_DECLARATION_VALUE(index) (values[(index)])

#ifdef REX_MOVE_DECLARATION_LONG_RESULT
long
#else
int
#endif
rex_move_declaration_boundary_ownership() {
#ifdef REX_MOVE_DECLARATION_INACTIVE_BRANCH
  return 7;
#else
  int index;
  int result;
  int *values;

  for (index = 0; index < 4; ++index) {
    if (true) {
      result = REX_MOVE_DECLARATION_VALUE(index);
    }
  }
  return result;
#endif
}
