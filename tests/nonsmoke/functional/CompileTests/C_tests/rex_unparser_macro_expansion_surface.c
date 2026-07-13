struct rex_object_holder {
  int value;
};

struct rex_nested_leaf {
  int member;
};

struct rex_nested_holder {
  struct rex_nested_leaf embedded;
};

static struct rex_object_holder rex_object;

int rex_invoke(int value, int offset);
int rex_direct_invoke(int value);

#define value rex_object.value
#define member embedded.member
#define rex_invoke(argument) rex_invoke((argument), 7)
#define REX_DIRECT_INVOKE rex_direct_invoke

int rex_object_macro_surface(void) { return value; }

int rex_nested_member_macro_surface(struct rex_nested_holder *object) {
  return object->member;
}

int rex_function_macro_surface(void) { return rex_invoke(5); }

int rex_function_callee_macro_surface(void) { return REX_DIRECT_INVOKE(11); }
