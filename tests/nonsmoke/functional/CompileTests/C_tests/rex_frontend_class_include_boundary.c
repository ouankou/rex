struct rex_class_include_contract {
  int owner_head;
#include "rex_frontend_class_include_members.def"
  int owner_tail;
};

int main(void) {
  struct rex_class_include_contract value = {1, 2, 3, 4};
  return value.included_first != 2 || value.owner_tail != 4;
}
