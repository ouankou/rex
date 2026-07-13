#include <vector>

enum RexGlobalAccessEnum { rex_global_access_value };
typedef int RexGlobalAccessTypedef;
using RexGlobalAccessAlias = int;
template <class T> using RexGlobalAccessAliasTemplate = T;
void rex_global_access_function();
int rex_global_access_variable;
std::vector<bool> rex_access_vector_bool;

namespace RexAccessNamespace {
using RexNamespaceAccessAlias = int;
void rex_namespace_access_function();
} // namespace RexAccessNamespace

struct RexAccessBase {
  static void rex_protected_using_target();
};

class RexAccessLabelClass : RexAccessBase {
  int implicit_private;

public:
  int explicit_public;
  void force_complete_class_lookup() { rex_protected_using_target(); }

protected:
  using RexAccessBase::rex_protected_using_target;
  int explicit_protected;

private:
  int explicit_private;

  using member_alias = int;
  template <class T> using member_alias_template = T;
  static_assert(true, "member access is semantic, not syntax repair");
};

struct RexAccessLabelStruct {
  int implicit_public;

private:
  int explicit_private;

public:
  int explicit_public;
};

void rex_binding_access() {
  int values[2] = {1, 2};
  auto [rex_binding_first, rex_binding_second] = values;
  (void)rex_binding_first;
  (void)rex_binding_second;
}
