#include "rose.h"

class SimpleInstrumentation : public SgSimpleProcessing {
  SgFunctionSymbol *functionSymbol;

public:
  explicit SimpleInstrumentation(SgGlobal *scope) {
    ROSE_ASSERT(scope != nullptr);
    auto *parameter_type = SageBuilder::buildPointerType(
        SageBuilder::buildConstType(SageBuilder::buildCharType()));
    auto *parameters = SageBuilder::buildFunctionParameterList();
    auto *parameter =
        SageBuilder::buildInitializedName("textString", parameter_type);
    SageInterface::appendArg(parameters, parameter);
    auto *declaration = SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexicalAtTop(scope),
        "coverageTraceFunc1", SageBuilder::buildVoidType(), parameters, scope);
    declaration->get_declarationModifier().get_storageModifier().setExtern();
    declaration->set_linkage("C");
    functionSymbol =
        isSgFunctionSymbol(declaration->get_symbol_from_symbol_table());
    ROSE_ASSERT(functionSymbol != nullptr);
  }

  void visit(SgNode *node) override {
    auto *block = isSgBasicBlock(node);
    if (block == nullptr)
      return;
    auto *position = block->get_file_info();
    ROSE_ASSERT(position != nullptr);
    const std::string location = position->get_filenameString() + " " +
                                 std::to_string(position->get_line());
    auto *arguments =
        SageBuilder::buildExprListExp(SageBuilder::buildStringVal(location));
    auto *call = SageBuilder::buildFunctionCallExp(functionSymbol, arguments);
    SageInterface::prependStatement(SageBuilder::buildExprStatement(call),
                                    block);
  }
};

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  const int frontend_status = project->get_frontendErrorCode();
  if (frontend_status != 0) {
    SageInterface::deleteAST(project);
    return frontend_status;
  }

  for (SgFile *file : project->get_fileList()) {
    auto *source = isSgSourceFile(file);
    ROSE_ASSERT(source != nullptr);
    SimpleInstrumentation traversal(source->get_globalScope());
    traversal.traverseWithinFile(source, preorder);
  }
  AstTests::runAllTests(project);
  const int backend_status = backend(project);
  SageInterface::deleteAST(project);
  return backend_status;
}
