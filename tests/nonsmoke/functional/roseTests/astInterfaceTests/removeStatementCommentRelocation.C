// DQ (9/16/2010): This test code demonstrate how to insert a statement
// before and after a function in a file.  Important to this test code
// is that we correctly handle the and CPP directives that might be
// attached to the first function declaration.

#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  SgGlobal *globalScope = getFirstGlobalScope(project);

  // Comments and CPP directives are not (cannot be) attached to the
  // SgSourceFile IR node. If there are no statements in the AST, then any
  // comments and CPP directives are attached to the global scope IR node.
  AttachedPreprocessingInfoType *comments =
      globalScope->getAttachedPreprocessingInfo();
  // printf ("Global scope comments = %p \n",comments);
  if (comments != NULL) {
    printf("comments found in SgGlobal size = %zu \n", comments->size());
  }

  Rose_STL_Container<SgNode *> functionDefinitionList =
      NodeQuery::querySubTree(project, V_SgFunctionDefinition);

  Rose_STL_Container<SgNode *>::iterator i = functionDefinitionList.begin();
  while (i != functionDefinitionList.end()) {
    SgFunctionDefinition *functionDefinition = isSgFunctionDefinition(*i);
    ROSE_ASSERT(functionDefinition != NULL);
    SgFunctionDeclaration *functionDeclaration =
        functionDefinition->get_declaration();
    ROSE_ASSERT(functionDeclaration != NULL);
    ROSE_ASSERT(functionDeclaration->get_definingDeclaration() ==
                functionDeclaration);
    ROSE_ASSERT(functionDeclaration->get_definition() == functionDefinition);
    SgName functionName = functionDeclaration->get_name();

    if (functionName == "removeThisFunctionToTestAttachedInfoBeforeStatement") {
      SageInterface::removeStatement(functionDeclaration);
      ROSE_ASSERT(functionDeclaration->get_parent() == NULL);
    }
    if (functionName == "removeThisFunctionToTestAttachedInfoAfterStatement") {
      SageInterface::removeStatement(functionDeclaration);
      ROSE_ASSERT(functionDeclaration->get_parent() == NULL);
    }

    i++;
  }

  return backend(project);
}
