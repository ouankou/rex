namespace rex_base_identity {
struct Base {};
using Alias = Base;

template <class T> struct Box {};
template <class T> using BoxAlias = Box<T>;

struct DerivedFromAlias : Alias {};
struct DerivedFromAliasTemplate : BoxAlias<int> {};

template <class T> struct DependentBase : T {};
template <class... Bases> struct PackedBases : Bases... {};
} // namespace rex_base_identity

int rex_frontend_base_specifier_identity() {
  rex_base_identity::DependentBase<rex_base_identity::Base> dependent;
  rex_base_identity::PackedBases<rex_base_identity::Base,
                                 rex_base_identity::Box<int>>
      packed;
  (void)dependent;
  (void)packed;
  return 0;
}
