#ifndef REX_FRONTEND_SEMANTIC_AUXILIARY_ACCESS_HPP
#define REX_FRONTEND_SEMANTIC_AUXILIARY_ACCESS_HPP

class RexSemanticAuxiliaryAccess {
protected:
  template <typename T>
  inline static constexpr bool rex_protected_value = sizeof(T) != 0;

public:
  template <typename T>
  inline static constexpr bool rex_public_value = rex_protected_value<T>;
};

#endif
