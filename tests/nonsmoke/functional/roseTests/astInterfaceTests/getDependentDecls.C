/*
test code for C++
Any reference to class or template class from a namespace will treated as a reference 
to the enclosing namespace: In this example, several classes are dependent but their 
enclosing namespace will be reported as dependent declarations.
by Liao, 5/8/2009
*/
#include "rose.h"
#include <iostream>
#include <vector>
using namespace std;

int main(int argc, char * argv[])

{
  SgProject *project = frontend (argc, argv);

  // DEBUG: Check all function declarations and their scopes
  Rose_STL_Container<SgNode*> funcs = NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  cout << "DEBUG: Found " << funcs.size() << " function declarations:\n";
  for (Rose_STL_Container<SgNode*>::iterator it = funcs.begin(); it != funcs.end(); ++it) {
    SgFunctionDeclaration* fd = isSgFunctionDeclaration(*it);
    if (fd) {
      SgScopeStatement* scope = fd->get_scope();
      cout << "  Function: " << fd->get_name().getString()
           << ", Scope: " << (scope ? scope->class_name() : "NULL")
           << ", isGlobal: " << (isSgGlobal(scope) ? "YES" : "NO")
           << ", defining: " << (fd->get_definingDeclaration() == fd ? "YES" : "NO")
           << "\n";
    }
  }

  SgFunctionDeclaration* func = SageInterface::findMain(project);
  cout << "DEBUG: findMain returned: " << (func ? func->get_name().getString() : "NULL") << "\n";
  ROSE_ASSERT(func != NULL);
  SgBasicBlock* body = func->get_definition()->get_body();
  ROSE_ASSERT(body!= NULL);
  SgStatement* stmt = SageInterface::getFirstStatement(body);
  ROSE_ASSERT(stmt != NULL);

  if (isSgForStatement(stmt))
  {
    vector<SgDeclarationStatement* > decls;

    decls = SageInterface::getDependentDeclarations(stmt);
    for (vector<SgDeclarationStatement* >::iterator iter = decls.begin(); iter !=decls.end(); iter++)
    cout<<"Dependent Declaration:"<<*iter<<" "<<(*iter)->class_name()<<" "<<(*iter)->unparseToString()<<SageInterface::get_name(*iter)<<endl;
  }
  else
    ROSE_ASSERT(false);

  // run all tests
  AstTests::runAllTests(project);

  // Generate source code from AST and call the vendor's compiler
  return backend(project);
}

