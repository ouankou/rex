namespace rex_function_production {
template <class T> T selected(T value) { return value; }

template <> int selected<int>(int value);
extern template long selected<long>(long value);
template char selected<char>(char value);

#define REX_FUNCTION_PRODUCTION_NAME selected<short>
extern template short REX_FUNCTION_PRODUCTION_NAME(short value);

struct defaults {
  int owned_default(int value = 7) { return value; }
};
} // namespace rex_function_production

int rex_frontend_function_production_contract() {
  rex_function_production::defaults object;
  return object.owned_default();
}

namespace rex_function_production {
struct rex_structural_ctor_list_owner {
  int value;
  rex_structural_ctor_list_owner(int input) : value(input) {}
  int rex_structural_nonconstructor() const { return value; }
};
} // namespace rex_function_production
