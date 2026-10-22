#include "sage3basic.h"

void SgMemberFunctionRefExp::post_construction_initialization() {
  (void)getAssociatedMemberFunctionDeclaration();
}

SgMemberFunctionSymbol *SgMemberFunctionRefExp::get_symbol() const {
  return get_symbol_i();
}

void SgMemberFunctionRefExp::set_symbol(SgMemberFunctionSymbol *symbol) {
  set_symbol_i(symbol);
}

SgMemberFunctionDeclaration *
SgMemberFunctionRefExp::getAssociatedMemberFunctionDeclaration() const {
  SgMemberFunctionSymbol *memberFunctionSymbol = this->get_symbol();
  ROSE_ASSERT(memberFunctionSymbol != nullptr);
  SgMemberFunctionDeclaration *declaration =
      memberFunctionSymbol->get_declaration();
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(isSgMemberFunctionType(declaration->get_type()) != nullptr);
  return declaration;
}

int SgMemberFunctionRefExp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgMemberFunctionRefExp::set_name_qualification_length(
    int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgMemberFunctionRefExp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgMemberFunctionRefExp::set_type_elaboration_required(
    bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgMemberFunctionRefExp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgMemberFunctionRefExp::set_global_qualification_required(
    bool global_qualification_required) {
  p_global_qualification_required = global_qualification_required;
}

int SgMemberFunctionRefExp::get_explicit_name_qualification_length() const {
  return p_explicit_name_qualification_length;
}

void SgMemberFunctionRefExp::set_explicit_name_qualification_length(
    int explicit_name_qualification_length) {
  p_explicit_name_qualification_length = explicit_name_qualification_length;
}

bool SgMemberFunctionRefExp::get_explicit_global_qualification() const {
  return p_explicit_global_qualification;
}

void SgMemberFunctionRefExp::set_explicit_global_qualification(
    bool explicit_global_qualification) {
  p_explicit_global_qualification = explicit_global_qualification;
}

const SgStringList &
SgMemberFunctionRefExp::get_explicit_name_qualification_tokens() const {
  return p_explicit_name_qualification_tokens;
}

SgStringList &SgMemberFunctionRefExp::get_explicit_name_qualification_tokens() {
  return p_explicit_name_qualification_tokens;
}

void SgMemberFunctionRefExp::set_explicit_name_qualification_tokens(
    const SgStringList &explicit_name_qualification_tokens) {
  p_explicit_name_qualification_tokens = explicit_name_qualification_tokens;
}
