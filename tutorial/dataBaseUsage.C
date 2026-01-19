// Example ROSE Translator: used for testing ROSE infrastructure

#include "rose.h"

using namespace std;
using namespace Rose;

// DQ (9/9/2005): Don't include the database by default
// TPS (01Dec2008): Enabled mysql and this fails.
// seems like it is not supposed to be included

int main(int argc, char *argv[]) {
  // TPS (01Dec2008): Enabled mysql and this fails.
  // seems like it is not supposed to be included

  // Build the AST used by ROSE
  SgProject *project = frontend(argc, argv);

  // Run internal consistency tests on AST
  AstTests::runAllTests(project);

  // Build a list of functions within the AST
  Rose_STL_Container<SgNode *> functionDeclarationList =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);

  int counter = 0;
  for (Rose_STL_Container<SgNode *>::iterator i =
           functionDeclarationList.begin();
       i != functionDeclarationList.end(); i++) {
    // Build a pointer to the current type so that we can call
    // the get_name() member function.
    SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(*i);
    ROSE_ASSERT(functionDeclaration != NULL);

    SgName func_name = functionDeclaration->get_name();
    // Skip builtin functions for shorter output, Liao 4/28/2008
    if (func_name.getString().find("__builtin", 0) == 0)
      continue;

    // output the function number and the name of the function
    printf("function name #%d is %s at line %d \n", counter++, func_name.str(),
           functionDeclaration->get_file_info()->get_line());

    string functionName = functionDeclaration->get_qualified_name().str();

    // TPS (01Dec2008): Enabled mysql and this fails.
    // seems like it is not supposed to be included
  }

  // TPS (01Dec2008): Enabled mysql and this fails.
  // seems like it is not supposed to be included
  printf("Program compiled without data base connection support (add using "
         "ROSE configure option) \n");

  return 0;
}
