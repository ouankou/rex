#include "rose.h"

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgTypedefDeclaration *vec_typedef = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *decl = isSgTypedefDeclaration(node);
    if (decl != nullptr && decl->get_name() == "Vec") {
      vec_typedef = decl;
      break;
    }
  }
  ROSE_ASSERT(vec_typedef != nullptr);

  SgClassDeclaration *class_decl =
      isSgClassDeclaration(vec_typedef->get_declaration());
  ROSE_ASSERT(class_decl != nullptr);
  ROSE_ASSERT(class_decl->get_parent() == vec_typedef);
  SgClassType *class_type = class_decl->get_type();
  ROSE_ASSERT(class_type != nullptr);

  Rose_STL_Container<SgNode *> mappers =
      NodeQuery::querySubTree(project, V_SgOmpDeclareMapperStatement);
  ROSE_ASSERT(mappers.size() == 1);
  SgOmpDeclareMapperStatement *mapper =
      isSgOmpDeclareMapperStatement(mappers.front());
  ROSE_ASSERT(mapper != nullptr);

  SgTypeExpression *class_type_expr =
      SageBuilder::buildTypeExpression(class_type);
  ROSE_ASSERT(class_type_expr != nullptr);
  SageInterface::setSourcePosition(class_type_expr);
  mapper->set_mapper_type(class_type_expr);
  class_type_expr->set_parent(mapper);

  SgVarRefExp *mapper_variable = isSgVarRefExp(mapper->get_mapper_variable());
  ROSE_ASSERT(mapper_variable != nullptr);
  SgVariableSymbol *mapper_symbol =
      isSgVariableSymbol(mapper_variable->get_symbol());
  ROSE_ASSERT(mapper_symbol != nullptr);
  SgInitializedName *mapper_declaration = mapper_symbol->get_declaration();
  ROSE_ASSERT(mapper_declaration != nullptr);
  mapper_declaration->set_type(class_type);
  ROSE_ASSERT(mapper_variable->get_type() == class_type);

  return backend(project);
}
