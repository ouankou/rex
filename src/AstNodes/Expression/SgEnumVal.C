#include "sage3basic.h"

SgType *SgEnumVal::get_type() const {
  SgEnumDeclaration *enumDeclaration = this->get_declaration();
  ROSE_ASSERT(enumDeclaration != NULL);

  SgEnumType *enumType = isSgEnumType(enumDeclaration->get_type());
  if (enumType == NULL ||
      !SageInterface::isExactTagTypeIdentity(enumType, enumDeclaration)) {
    fprintf(stderr,
            "REX_AST_INVARIANT[enum-literal-semantic-type]: declaration has "
            "no exact constructed enum type\n");
    ROSE_ABORT();
  }
  if (get_literal_type() != NULL && get_literal_type() != enumType) {
    fprintf(stderr,
            "REX_AST_INVARIANT[enum-literal-semantic-type]: explicit literal "
            "type does not name its exact enum declaration\n");
    ROSE_ABORT();
  }
  return get_literal_type() != NULL ? get_literal_type() : enumType;
}

// Qualification metadata must not mark source tokens as modified.
int SgEnumVal::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgEnumVal::set_name_qualification_length(int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgEnumVal::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgEnumVal::set_type_elaboration_required(bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgEnumVal::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgEnumVal::set_global_qualification_required(
    bool global_qualification_required) {

  p_global_qualification_required = global_qualification_required;
}

int SgEnumVal::get_explicit_name_qualification_length() const {
  return p_explicit_name_qualification_length;
}

void SgEnumVal::set_explicit_name_qualification_length(
    int explicit_name_qualification_length) {
  p_explicit_name_qualification_length = explicit_name_qualification_length;
}

bool SgEnumVal::get_explicit_global_qualification() const {
  return p_explicit_global_qualification;
}

void SgEnumVal::set_explicit_global_qualification(
    bool explicit_global_qualification) {
  p_explicit_global_qualification = explicit_global_qualification;
}

const SgStringList &SgEnumVal::get_explicit_name_qualification_tokens() const {
  return p_explicit_name_qualification_tokens;
}

SgStringList &SgEnumVal::get_explicit_name_qualification_tokens() {
  return p_explicit_name_qualification_tokens;
}

void SgEnumVal::set_explicit_name_qualification_tokens(
    const SgStringList &explicit_name_qualification_tokens) {
  p_explicit_name_qualification_tokens = explicit_name_qualification_tokens;
}
