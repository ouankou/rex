#include "sage3basic.h"

void SgFunctionRefExp::post_construction_initialization() {
  set_function_type(p_function_type);
}

void SgFunctionRefExp::set_function_type(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(isSgFunctionType(type->stripTypedefsAndModifiers()));
  p_function_type = type;
}

SgType *SgFunctionRefExp::get_type() const {
  ROSE_ASSERT(p_symbol_i != nullptr);
  ROSE_ASSERT(p_function_type != nullptr);
  ROSE_ASSERT(isSgFunctionType(p_function_type->stripTypedefsAndModifiers()));
  return p_function_type;
}

SgFunctionSymbol *SgFunctionRefExp::get_symbol() const {
  return get_symbol_i();
}

void SgFunctionRefExp::set_symbol(SgFunctionSymbol *symbol) {
  ROSE_ASSERT(symbol != nullptr && symbol->get_type() != nullptr);
  set_symbol_i(symbol);
  set_function_type(symbol->get_type());
}

SgFunctionDeclaration *
SgFunctionRefExp::getAssociatedFunctionDeclaration() const {
  SgFunctionSymbol *functionSymbol = this->get_symbol();
  ROSE_ASSERT(functionSymbol != NULL);
  return functionSymbol->get_declaration();
}

int SgFunctionRefExp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgFunctionRefExp::set_name_qualification_length(
    int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgFunctionRefExp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgFunctionRefExp::set_type_elaboration_required(
    bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgFunctionRefExp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgFunctionRefExp::set_global_qualification_required(
    bool global_qualification_required) {
  p_global_qualification_required = global_qualification_required;
}

int SgFunctionRefExp::get_explicit_name_qualification_length() const {
  return p_explicit_name_qualification_length;
}

void SgFunctionRefExp::set_explicit_name_qualification_length(
    int explicit_name_qualification_length) {
  p_explicit_name_qualification_length = explicit_name_qualification_length;
}

bool SgFunctionRefExp::get_explicit_global_qualification() const {
  return p_explicit_global_qualification;
}

void SgFunctionRefExp::set_explicit_global_qualification(
    bool explicit_global_qualification) {
  p_explicit_global_qualification = explicit_global_qualification;
}

const SgStringList &
SgFunctionRefExp::get_explicit_name_qualification_tokens() const {
  return p_explicit_name_qualification_tokens;
}

SgStringList &SgFunctionRefExp::get_explicit_name_qualification_tokens() {
  return p_explicit_name_qualification_tokens;
}

void SgFunctionRefExp::set_explicit_name_qualification_tokens(
    const SgStringList &explicit_name_qualification_tokens) {
  p_explicit_name_qualification_tokens = explicit_name_qualification_tokens;
}
