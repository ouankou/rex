#include <utility>

struct RexAstJsonBindingValue {
  RexAstJsonBindingValue();
  ~RexAstJsonBindingValue();
};
struct RexAstJsonBindingTuple {};
namespace std {
template <> struct tuple_size<RexAstJsonBindingTuple> {
  static constexpr size_t value = 1;
};
template <> struct tuple_element<0, RexAstJsonBindingTuple> {
  using type = RexAstJsonBindingValue;
};
} // namespace std
template <std::size_t Index> RexAstJsonBindingValue get(RexAstJsonBindingTuple);

struct RexAstJsonCanonicalOwner {
  int value;
  int rex_canonical_member() const { return value; }
};

namespace RexAstJsonCanonicalNamespace {
template <typename T> struct RexAstJsonCanonicalTemplate;
}

namespace RexAstJsonCanonicalNamespace {
template <typename T> struct RexAstJsonCanonicalTemplate {
  T value;
};
} // namespace RexAstJsonCanonicalNamespace

RexAstJsonCanonicalOwner rex_ast_json_canonical_owner{17};
RexAstJsonCanonicalNamespace::RexAstJsonCanonicalTemplate<int>
    rex_ast_json_canonical_template_owner{23};

void rex_scope_enum_embedded_move() {
  {
    enum RexEmbeddedEnum { rex_embedded_value = 1 } value = rex_embedded_value;
    const volatile RexEmbeddedEnum &reference = value;
    (void)reference;
  }
  {
  }
}

void rex_scope_enum_typedef_move() {
  {
    typedef enum RexAliasedEnum { rex_aliased_value = 2 } Alias;
    Alias value = rex_aliased_value;
    (void)value;
  }
  {
  }
}

void rex_scope_enum_forward_move() {
  {
    enum class RexForwardEnum : int;
    enum class RexForwardEnum : int { value = 3 } first{}, second{};
    (void)first;
    (void)second;
  }
  {
  }
}

void rex_scope_class_embedded_move() {
  {
    struct RexEmbeddedClass {
      int value;
    } first{1};
    const volatile RexEmbeddedClass &reference = first;
    RexAstJsonCanonicalOwner &borrowed = rex_ast_json_canonical_owner;
    (void)reference;
    (void)borrowed;
  }
  {
  }
}

void rex_scope_class_typedef_move() {
  {
    typedef struct RexAliasedClass {
      int value;
    } Alias;
    Alias first{2};
    (void)first;
  }
  {
  }
}

void rex_scope_class_forward_move() {
  {
    struct RexForwardClass;
    struct RexForwardClass {
      int value;
    } first{3}, second{4};
    (void)first;
    (void)second;
  }
  {
  }
}

namespace RexScopeFunctionSource {
int rex_scope_function_generated(int value) { return value; }
double rex_scope_function_written(double);
double rex_scope_function_written(double value) { return value; }
template <class T> T rex_scope_function_template(T value) { return value; }
int rex_scope_function_grouped(int), rex_scope_function_data;
} // namespace RexScopeFunctionSource
namespace RexScopeFunctionTarget {}

int main() {
  auto [bound_value] = rex_ast_json_canonical_owner;
  auto [bound_temporary] = RexAstJsonBindingTuple{};
  return rex_ast_json_canonical_owner.rex_canonical_member() == 17 &&
                 bound_value == 17 &&
                 rex_ast_json_canonical_template_owner.value == 23
             ? 0
             : 1;
}
