#include "sage3basic.h"

#include "nonrealQualificationSupport.h"

namespace SageInterface {

bool nonrealTypeCarriesWrittenQualification(const SgNonrealType *nonreal_type) {
  if (nonreal_type == nullptr) {
    return false;
  }

  const SgNonrealDecl *nrdecl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  if (nrdecl == nullptr) {
    return false;
  }

  // A nested declaration-scope chain is semantic identity, not source
  // spelling. SgNonrealDecl instances are shared, so treating that chain as a
  // written qualifier lets one TypeLoc occurrence change every other use of
  // the same type. Only an explicit source payload can own written syntax;
  // frontend TypeLoc producers publish that payload on the exact declaration,
  // expression, base, function-argument, or template-argument use site.
  return nrdecl->get_source_name_qualification_present() &&
         (nrdecl->get_source_name_global_qualification() ||
          !nrdecl->get_source_name_qualification_tokens().empty());
}

bool typeCarriesWrittenNonrealQualification(const SgType *type) {
  if (type == nullptr) {
    return false;
  }

  const SgType *stripped = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  return nonrealTypeCarriesWrittenQualification(isSgNonrealType(stripped));
}

bool nonrealTypeHasSemanticQualificationChain(
    const SgNonrealType *nonreal_type) {
  if (nonreal_type == nullptr) {
    return false;
  }

  const SgNonrealDecl *nrdecl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  const SgDeclarationScope *declaration_scope =
      isSgDeclarationScope(nrdecl != nullptr ? nrdecl->get_parent() : nullptr);
  return nrdecl != nullptr &&
         (nrdecl->get_has_global_qualifier() ||
          (declaration_scope != nullptr &&
           isSgNonrealDecl(declaration_scope->get_parent()) != nullptr));
}

bool typeHasSemanticNonrealQualificationChain(const SgType *type) {
  if (type == nullptr) {
    return false;
  }

  const SgType *stripped = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  return nonrealTypeHasSemanticQualificationChain(isSgNonrealType(stripped));
}

bool typeCarriesIntrinsicNonrealQualification(const SgType *type) {
  return typeCarriesWrittenNonrealQualification(type) ||
         typeHasSemanticNonrealQualificationChain(type);
}

} // namespace SageInterface
