#include "sage3basic.h"

SgType *SgLambdaExp::get_type() const {
  SgClassDeclaration *closure = get_lambda_closure_class();
  SgClassType *closure_type =
      closure != nullptr ? isSgClassType(closure->get_type()) : nullptr;
  if (closure_type == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[lambda-type]: lambda expression has no exact "
            "closure class type\n");
    ROSE_ABORT();
  }
  SgDeclarationStatement *type_declaration = closure_type->get_declaration();
  SgDeclarationStatement *closure_first =
      closure->get_firstNondefiningDeclaration();
  SgDeclarationStatement *closure_definition =
      closure->get_definingDeclaration();
  if (type_declaration != closure && type_declaration != closure_first &&
      type_declaration != closure_definition) {
    fprintf(stderr,
            "REX_AST_INVARIANT[lambda-type]: closure class type belongs to a "
            "different declaration family\n");
    ROSE_ABORT();
  }
  return closure_type;
}
