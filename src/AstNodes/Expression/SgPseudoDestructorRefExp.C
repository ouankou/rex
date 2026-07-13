#include "sage3basic.h"

void SgPseudoDestructorRefExp::post_construction_initialization() {
  SgMemberFunctionType *callableType =
      isSgMemberFunctionType(p_expression_type);
  if (p_object_type == nullptr || isSgTypeUnknown(p_object_type) != nullptr ||
      isSgTypeDefault(p_object_type) != nullptr || callableType == nullptr ||
      isSgTypeVoid(callableType->get_return_type()) == nullptr) {
    std::cerr << "REX_AST_INVARIANT[pseudo-destructor-type-producer]: "
                 "pseudo-destructor requires an exact object type and void "
                 "member-function result type\n";
    ROSE_ABORT();
  }
}

// DQ (1/18/2020): Adding name qualification support to
// SgPseudoDestructorRefExp.
int SgPseudoDestructorRefExp::get_name_qualification_length() const {
  ROSE_ASSERT(this != NULL);
  return p_name_qualification_length;
}

// DQ (1/18/2020): Adding name qualification support to
// SgPseudoDestructorRefExp.
void SgPseudoDestructorRefExp::set_name_qualification_length(
    int name_qualification_length) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_name_qualification_length = name_qualification_length;
}

// DQ (1/18/2020): Adding name qualification support to
// SgPseudoDestructorRefExp.
bool SgPseudoDestructorRefExp::get_type_elaboration_required() const {
  ROSE_ASSERT(this != NULL);
  return p_type_elaboration_required;
}

// DQ (1/18/2020): Adding name qualification support to
// SgPseudoDestructorRefExp.
void SgPseudoDestructorRefExp::set_type_elaboration_required(
    bool type_elaboration_required) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_type_elaboration_required = type_elaboration_required;
}

// DQ (1/18/2020): Adding name qualification support to
// SgPseudoDestructorRefExp.
bool SgPseudoDestructorRefExp::get_global_qualification_required() const {
  ROSE_ASSERT(this != NULL);
  return p_global_qualification_required;
}

// DQ (1/18/2020): Adding name qualification support to
// SgPseudoDestructorRefExp.
void SgPseudoDestructorRefExp::set_global_qualification_required(
    bool global_qualification_required) {
  ROSE_ASSERT(this != NULL);

  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_global_qualification_required = global_qualification_required;
}
