#define REX_MOVE_DECLARATION_VALUE(index) (values[(index)])

struct RexMoveDeclarationAnonymousTag {
  union {
    int integer_value;
    long long_value;
  };
};

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
  RexMoveDeclarationAnonymousTag tagged_value;

  for (index = 0; index < 4; ++index) {
    if (true) {
      tagged_value.integer_value = index;
      result = REX_MOVE_DECLARATION_VALUE(index) + tagged_value.integer_value;
    }
  }
  return result;
#endif
}
