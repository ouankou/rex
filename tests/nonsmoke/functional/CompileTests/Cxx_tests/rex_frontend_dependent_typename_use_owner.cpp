template <typename T> struct RexDependentTypenameOwner {
  using type = T;
};

template <typename T> struct RexDependentTypedefOwner {
  typedef typename RexDependentTypenameOwner<T>::type RexDependentTypedef;
};

template <typename T>
using RexDependentFunctionType =
    void(typename RexDependentTypenameOwner<T>::type);

template <typename First, typename Second> struct RexDependentPair {};

template <typename T> struct RexDependentConstructor {
  template <typename U>
  RexDependentConstructor(
      const RexDependentPair<U, typename RexDependentTypenameOwner<T>::type>
          &value);
};

template <typename T>
typename RexDependentTypenameOwner<T>::type
rex_dependent_typename_use(typename RexDependentTypenameOwner<T>::type value) {
  typename RexDependentTypenameOwner<T>::type local = value;
  return local;
}

template <typename T> int rex_dependent_typename_expression() {
  return typename RexDependentTypenameOwner<T>::type();
}

template <typename T> int rex_dependent_typename_cast() {
  return static_cast<typename RexDependentTypenameOwner<T>::type>(2);
}

struct RexQualifiedElaboratedTag {};
struct ::RexQualifiedElaboratedTag *rex_qualified_elaborated_tag = nullptr;

template <typename T> class RexElaboratedTemplateTag {};
class RexElaboratedTemplateTag<int> *rex_elaborated_template_tag = nullptr;

struct RexTypeOperandElaboration {};
constexpr auto rex_plain_sizeof = sizeof(RexTypeOperandElaboration);
constexpr auto rex_elaborated_sizeof = sizeof(struct RexTypeOperandElaboration);
constexpr auto rex_plain_alignof = alignof(RexTypeOperandElaboration);
constexpr auto rex_elaborated_alignof =
    alignof(struct RexTypeOperandElaboration);
auto *rex_plain_cast = static_cast<RexTypeOperandElaboration *>(nullptr);
auto *rex_elaborated_cast =
    static_cast<struct RexTypeOperandElaboration *>(nullptr);

template <typename T> struct RexDependentSizeofOwner {
  struct Nested {};
  static unsigned storage[sizeof(Nested)];
};

template <typename T>
unsigned RexDependentSizeofOwner<T>::storage[sizeof(Nested)] = {};

int main() {
  return rex_dependent_typename_use<int>(1) +
         RexDependentSizeofOwner<int>::storage[0];
}
