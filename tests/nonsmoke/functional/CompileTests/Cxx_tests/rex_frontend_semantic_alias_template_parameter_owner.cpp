#include <rex_frontend_semantic_alias_template_parameter_owner.hpp>

RexSemanticAliasResult rex_semantic_alias_template_parameter_owner_result = 5;

template <class T = int> using RexLocalAlias = T;

using RexAliasInt = int;
RexLocalAlias<> *rex_alias_default = nullptr;
RexLocalAlias<int> *rex_alias_direct = nullptr;
RexLocalAlias<RexAliasInt> *rex_alias_synonym = nullptr;
RexLocalAlias<RexLocalAlias<int>> *rex_alias_nested = nullptr;

void rex_local_alias_left() {
  struct RexAliasLocal {
    int left;
  };
  RexLocalAlias<RexAliasLocal> *rex_alias_object = nullptr;
  (void)rex_alias_object;
}

void rex_local_alias_right() {
  struct RexAliasLocal {
    double right;
  };
  RexLocalAlias<RexAliasLocal> *rex_alias_object = nullptr;
  (void)rex_alias_object;
}
