#include "sage3basic.h"

void SgPseudoDestructorRefExp::post_construction_initialization() {
  SgMemberFunctionType *callableType =
      isSgMemberFunctionType(p_expression_type);
  if (p_object_type == nullptr || isSgTypeUnknown(p_object_type) != nullptr ||
      isSgTypeDefault(p_object_type) != nullptr || callableType == nullptr ||
      callableType->get_class_type() != p_object_type ||
      callableType->get_has_ellipses() ||
      !callableType->get_arguments().empty() ||
      isSgTypeVoid(callableType->get_return_type()) == nullptr) {
    std::cerr << "REX_AST_INVARIANT[pseudo-destructor-type-producer]: "
                 "pseudo-destructor requires a nullary, non-variadic void "
                 "member-function type bound to its exact object type\n";
    ROSE_ABORT();
  }
}

int SgPseudoDestructorRefExp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgPseudoDestructorRefExp::set_name_qualification_length(
    int name_qualification_length) {
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_name_qualification_length = name_qualification_length;
}

bool SgPseudoDestructorRefExp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgPseudoDestructorRefExp::set_type_elaboration_required(
    bool type_elaboration_required) {
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_type_elaboration_required = type_elaboration_required;
}

bool SgPseudoDestructorRefExp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgPseudoDestructorRefExp::set_global_qualification_required(
    bool global_qualification_required) {

  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_global_qualification_required = global_qualification_required;
}
