typedef struct {
  int state;
} rex_state_type;

template <typename State> struct rex_base {
  explicit rex_base(int) {}
};

struct rex_derived : rex_base<rex_state_type> {
  rex_derived() : rex_base<rex_state_type>(0) {}
};

int main() {
  rex_derived value;
  return sizeof(value) == 0;
}
