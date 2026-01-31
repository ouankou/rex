#include "rose.h"

#include <string>

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  project->skipfinalCompileStep(true);

  SgFunctionDeclaration *main_decl = SageInterface::findMain(project);
  ROSE_ASSERT(main_decl != nullptr);
  SgBasicBlock *body = main_decl->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);

  SgType *int_type = SageBuilder::buildIntType();
  SgNullExpression *null_expr = SageBuilder::buildNullExpression_nfi();
  ROSE_ASSERT(null_expr != nullptr);

  SgAssignInitializer *init =
      SageBuilder::buildAssignInitializer(null_expr, int_type);
  ROSE_ASSERT(init != nullptr);

  SgVariableDeclaration *decl =
      SageBuilder::buildVariableDeclaration("x", int_type, init, body);
  ROSE_ASSERT(decl != nullptr);
  SageInterface::appendStatement(decl, body);

  SgSourceFile *source_file = isSgSourceFile(project->get_fileList()[0]);
  ROSE_ASSERT(source_file != nullptr);

  std::string unparsed = globalUnparseToString(source_file);

  bool has_plain_zero = unparsed.find("x = 0") != std::string::npos ||
                        unparsed.find("x=0") != std::string::npos;
  bool has_typed_zero = unparsed.find("x = (int)0") != std::string::npos ||
                        unparsed.find("x=(int)0") != std::string::npos;

  ROSE_ASSERT(has_plain_zero || has_typed_zero);

  return backend(project);
}
