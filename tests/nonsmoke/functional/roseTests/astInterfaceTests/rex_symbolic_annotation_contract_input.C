unsigned long rex_symbolic_annotation_sizeof_fixed() { return sizeof(int); }
unsigned long rex_symbolic_annotation_sizeof_array_type() {
  return sizeof(int[sizeof(int)]);
}
unsigned long rex_symbolic_annotation_sizeof_vla_left(int bound) {
  int values[bound];
  return sizeof(values);
}
unsigned long rex_symbolic_annotation_sizeof_vla_right(int bound) {
  int values[bound + 1];
  return sizeof(values);
}
template <class T> unsigned long rex_symbolic_annotation_sizeof_dependent() {
  return sizeof(T);
}
unsigned long rex_symbolic_annotation_sizeof_unevaluated(int &counter) {
  return sizeof(++counter);
}
unsigned long rex_symbolic_annotation_sizeof_vla_effect(int bound, int *left,
                                                        int *right) {
  int values[bound];
  return sizeof((left = right, values));
}

int rex_symbolic_annotation_integer_decimal() { return 42; }
int rex_symbolic_annotation_integer_hex() { return 0x2a; }
int rex_symbolic_annotation_integer_octal() { return 052; }
unsigned long rex_symbolic_annotation_integer_suffix() { return 42UL; }
int rex_symbolic_annotation_integer_separator() { return 0x2'a; }
enum rex_symbolic_annotation_constant_enum {
  rex_symbolic_annotation_constant = 42
};
rex_symbolic_annotation_constant_enum rex_symbolic_annotation_integer_enum() {
  return rex_symbolic_annotation_constant;
}

struct rex_symbolic_record {
  int member;
  int get_member() const { return member; }
  static int static_member(int value) { return value; }
  template <class T> int template_member(T value) const {
    return member + value;
  }
  template <class T> static int selected_member;
};

template <class T> int rex_symbolic_record::selected_member = 5;

int rex_symbolic_annotation_static_call(int value) {
  return rex_symbolic_record::static_member(value);
}

int rex_symbolic_annotation_template_call(rex_symbolic_record object,
                                          int value) {
  return object.template_member(value);
}

namespace rex_symbolic_member {
struct Tag {};
} // namespace rex_symbolic_member

using RexSymbolicMemberAlias = rex_symbolic_member::Tag;

int rex_symbolic_annotation_static_identity(rex_symbolic_record left,
                                            rex_symbolic_record right,
                                            rex_symbolic_record *pointer) {
  return left.selected_member<rex_symbolic_member::Tag> +
         right.selected_member<RexSymbolicMemberAlias> +
         pointer->selected_member<rex_symbolic_member::Tag> +
         left.selected_member<int> +
         rex_symbolic_record::selected_member<rex_symbolic_member::Tag>;
}

rex_symbolic_record rex_symbolic_make_record(int &counter) {
  ++counter;
  return {};
}

int rex_symbolic_annotation_static_temporary(int &counter) {
  return rex_symbolic_make_record(counter)
      .selected_member<rex_symbolic_member::Tag>;
}

int rex_symbolic_annotation_member_template(rex_symbolic_record object) {
  return object.selected_member<rex_symbolic_member::Tag>;
}

int rex_symbolic_annotation_member_call(rex_symbolic_record object,
                                        int (rex_symbolic_record::*method)()
                                            const) {
  return (object.*method)();
}

int rex_symbolic_annotation_member_arrow_call(
    rex_symbolic_record *object, int (rex_symbolic_record::*method)() const) {
  return (object->*method)();
}

struct constant_record {
  int member;
};

int rex_symbolic_annotation_prototype(int value);
int rex_symbolic_annotation_prototype(double value);

int rex_symbolic_annotation_local_type_left() {
  struct RexLocalRecord {
    int member;
  };
  RexLocalRecord value{1};
  return value.member;
}

int rex_symbolic_annotation_local_type_right() {
  struct RexLocalRecord {
    int member;
  };
  RexLocalRecord value{2};
  return value.member;
}

namespace rex_symbolic_scope {
namespace rex_symbolic_scope {
struct RexScopedRecord {
  int member;
};
int rex_symbolic_annotation_scoped_type(RexScopedRecord value,
                                        const RexScopedRecord &reference) {
  return value.member;
}
} // namespace rex_symbolic_scope
} // namespace rex_symbolic_scope

int rex_symbolic_annotation_base_type(
    constant_record value, const constant_record &reference,
    constant_record *pointer, const constant_record *const &pointer_reference,
    constant_record &&rvalue_reference) {
  return value.member + reference.member + pointer->member +
         pointer_reference->member + rvalue_reference.member;
}

int rex_symbolic_annotation_member(rex_symbolic_record object) {
  return object.member;
}

template <class T> int rex_symbolic_annotation_dependent_member(T object) {
  return object.member;
}

template <class T> int rex_symbolic_annotation_dependent_call(T object) {
  return object.get_member();
}

int rex_symbolic_annotation_member_pointer(rex_symbolic_record left,
                                           rex_symbolic_record right,
                                           int rex_symbolic_record::*field) {
  return left.*field + right.*field + left.*field;
}

int rex_symbolic_annotation_member_arrow_pointer(
    rex_symbolic_record *left, rex_symbolic_record *right,
    int rex_symbolic_record::*field) {
  return left->*field + right->*field + left->*field;
}

int rex_symbolic_annotation_variable(int value) { return value; }

int rex_symbolic_annotation_pointer_min(int (*min)(int, int), int left,
                                        int right) {
  return min(left, right);
}

int rex_symbolic_annotation_pointer_max(int (*max)(int, int), int left,
                                        int right) {
  return max(left, right);
}

int min() { return 4; }
int max(int value) { return value; }

int rex_symbolic_annotation_zero_min() { return min(); }
int rex_symbolic_annotation_unary_max(int value) { return max(value); }

int value() { return 7; }

int rex_symbolic_annotation_shadow(int value) {
  int result = value;
  {
    int value = 3;
    result += value;
  }
  return result + value + ::value();
}

int *rex_symbolic_annotation_address(int *pointer) { return &*pointer; }

int rex_symbolic_annotation_dereference(int *pointer) { return *pointer; }

int *rex_symbolic_annotation_conditional_address(bool condition, int *left,
                                                 int *right) {
  return &(condition ? *left : *right);
}

int *rex_symbolic_annotation_comma_address(int &counter, int *pointer) {
  return &(counter++, *pointer);
}

int *rex_symbolic_annotation_binary_conditional_address(int *left, int *right) {
  return &(*left ?: *right);
}

double rex_symbolic_annotation_conversion(int value) { return value; }

int rex_symbolic_annotation_modulo(int left, int right) { return left % right; }

int rex_symbolic_annotation_bit_and(int left, int right) {
  return left & right;
}

int rex_symbolic_annotation_bit_or(int left, int right) { return left | right; }

int rex_symbolic_annotation_bit_complement(int value) { return ~value; }

int rex_symbolic_annotation_prefix_increment(int &value) { return ++value; }

int rex_symbolic_annotation_postfix_decrement(int &value) { return value--; }

int *rex_symbolic_annotation_allocation() { return new int(1); }

int rex_symbolic_annotation_observable_zero_product(int &value) {
  return 0 * value++;
}

int rex_symbolic_annotation_volatile_zero_product(volatile int *pointer) {
  return 0 * *pointer;
}

unsigned rex_symbolic_annotation_unsigned_minus(unsigned value) {
  return -value;
}

double rex_symbolic_annotation_floating_group(double left, double middle,
                                              double right) {
  return left + (middle + right);
}

int rex_symbolic_annotation_array_access(int (*array)[4], int row, int column) {
  return array[row][column];
}

_Complex double rex_symbolic_annotation_complex_floating_group(
    _Complex double left, _Complex double middle, _Complex double right) {
  return left + (middle + right);
}
