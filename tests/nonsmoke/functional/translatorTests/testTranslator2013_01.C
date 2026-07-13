#include "rose.h"
// #include "FunctionInsert.h"

using namespace SageBuilder;
using namespace SageInterface;

class SimpleInstrumentation : public SgSimpleProcessing {
public:
  void visit(SgNode *astNode);
  bool done;
};

void SimpleInstrumentation::visit(SgNode *astNode) {
  // SgFile *file=isSgFile(astNode);
  SgFunctionDefinition *funcdef = isSgFunctionDefinition(astNode);
  if (funcdef != NULL && !done) {
    printf("Found SgFunctionDefinition: funcdef->get_declaration()->get_name() "
           "= %s \n",
           funcdef->get_declaration()->get_name().str());

    SgScopeStatement *scope = getGlobalScope(funcdef);
    SgFunctionDeclaration *func_defn = buildDefiningFunctionDeclaration(
        function_declaration_ownership::sourceLexical(), SgName("testFunc"),
        buildVoidType(),
        buildFunctionParameterList(
            buildInitializedName(SgName("param1"), buildIntType(), NULL)),
        scope);
    SgFunctionDeclaration *func_decl = buildNondefiningFunctionDeclaration(
        function_declaration_ownership::sourceLexicalAtTop(scope), func_defn,
        scope);
    ROSE_ASSERT(func_decl->get_parent() == scope);
    ROSE_ASSERT(func_defn->get_parent() == scope);

    SgBasicBlock *func_body = func_defn->get_definition()->get_body();

    SgVariableDeclaration *i = buildVariableDeclaration(
        SgName("i"), buildIntType(),
        buildAssignInitializer(buildIntVal(0), buildIntType()), func_body);
    appendStatement(i, func_body);

    done = true;
  }
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SimpleInstrumentation treeTraversal;
  treeTraversal.done = false;
  treeTraversal.traverseInputFiles(project, preorder);

  return backend(project);
}
