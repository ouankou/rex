struct RexObjectHolder {
  int value;
};

struct RexNestedLeaf {
  int member;
};

struct RexNestedHolder {
  RexNestedLeaf embedded;
};

static RexObjectHolder rex_object;

int rex_invoke(int value, int offset);
int rex_direct_invoke(int value);

#define value rex_object.value
#define member embedded.member
#define rex_invoke(argument) rex_invoke((argument), 7)
#define REX_DIRECT_INVOKE rex_direct_invoke
#define rex_guarded_declaration(argument) ((argument) + 1)

int(rex_guarded_declaration)(int input);

int rex_object_macro_surface() { return value; }

int rex_nested_member_macro_surface(RexNestedHolder &object) {
  return object.member;
}

int rex_function_macro_surface() { return rex_invoke(5); }

int rex_function_callee_macro_surface() { return REX_DIRECT_INVOKE(11); }
