#ifndef ROSE_NONREAL_QUALIFICATION_SUPPORT_H
#define ROSE_NONREAL_QUALIFICATION_SUPPORT_H

#include "sage3basic.h"

namespace SageInterface {

inline bool
nonrealTypeCarriesWrittenQualification(const SgNonrealType *nonreal_type) {
  if (nonreal_type == nullptr) {
    return false;
  }

  const SgNonrealDecl *nrdecl =
      isSgNonrealDecl(nonreal_type->get_declaration());
  if (nrdecl == nullptr) {
    return false;
  }

  if (nrdecl->get_has_global_qualifier()) {
    return true;
  }

  SgDeclarationScope *nrscope = isSgDeclarationScope(nrdecl->get_parent());
  return nrscope != nullptr &&
         isSgNonrealDecl(nrscope->get_parent()) != nullptr;
}

inline bool typeCarriesWrittenNonrealQualification(const SgType *type) {
  if (type == nullptr) {
    return false;
  }

  const SgType *stripped = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  return nonrealTypeCarriesWrittenQualification(isSgNonrealType(stripped));
}

} // namespace SageInterface

#endif
