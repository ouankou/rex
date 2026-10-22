// Construct a function with an owned parameter, body, and canonical declaration
// chain. The builders publish these relationships during construction.
#include "rose.h"

class SimpleInstrumentation : public SgSimpleProcessing {
public:
  void visit(SgNode *node) override {
    auto *scope = isSgGlobal(node);
    if (scope == nullptr)
      return;

    auto *parameter = SageBuilder::buildInitializedName(
        "var_name",
        SageBuilder::buildReferenceType(SageBuilder::buildIntType()));
    auto *parameters = SageBuilder::buildFunctionParameterList();
    SageInterface::appendArg(parameters, parameter);
    auto *function = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexicalAtTop(scope),
        "my_function", SageBuilder::buildIntType(), parameters, scope);
    auto *body = function->get_definition()->get_body();
    auto *symbol =
        isSgVariableSymbol(parameter->get_symbol_from_symbol_table());
    ROSE_ASSERT(symbol != nullptr);
    auto *reference = SageBuilder::buildVarRefExp(symbol);
    auto *increment = SageBuilder::buildPlusPlusOp(
        reference, reference->get_type(), SgUnaryOp::prefix);
    SageInterface::appendStatement(SageBuilder::buildReturnStmt(increment),
                                   body);
  }
};

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SimpleInstrumentation traversal;
  traversal.traverseInputFiles(project, preorder);
  AstTests::runAllTests(project);
  return backend(project);
}
