#include "sage3basic.h"

SgType *SgLambdaRefExp::get_type() const {

  return SageBuilder::buildVoidType();
}

void SgLambdaRefExp::post_construction_initialization() {}

SgFunctionParameterList *SgLambdaRefExp::get_parameterList() {
  ROSE_ASSERT(this != NULL);

  SgFunctionDeclaration *func_decl = this->get_functionDeclaration();
  ROSE_ASSERT(func_decl != NULL);

  return func_decl->get_parameterList();
}

SgStatement *SgLambdaRefExp::get_body() {
  ROSE_ASSERT(this != NULL);

  SgFunctionDeclaration *func_decl = this->get_functionDeclaration();
  ROSE_ASSERT(func_decl != NULL);

  SgFunctionDefinition *func_def = func_decl->get_definition();
  ROSE_ASSERT(func_def != NULL);

  return func_def->get_body();
}
