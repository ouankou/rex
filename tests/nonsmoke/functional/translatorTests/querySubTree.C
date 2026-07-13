/*
 * Verify that AST queries expose both the source declaration and its exact
 * typed semantic-auxiliary declaration without conflating their ownership
 * roles.
 */
#include "rose.h"

#include <iostream>
using namespace std;

int main(int argc, char *argv[]) {
  int lexicalCounter = 0;
  int auxiliaryCounter = 0;
  SgProject *project = frontend(argc, argv);
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(project, V_SgDeclarationStatement);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgFunctionDeclaration *decl = isSgFunctionDeclaration((*i));
    // cout<<"decl: "<<decl<< " "<< decl->unparseToString()<<endl;
    if (decl && decl->get_name() == string("foo")) {
      if (SageInterface::hasExactSemanticAuxiliaryOwnership(decl)) {
        auxiliaryCounter++;
      } else {
        ROSE_ASSERT(decl->get_definition() != nullptr);
        lexicalCounter++;
      }
    }
  }

  if (lexicalCounter != 1 || auxiliaryCounter != 1) {
    cerr << "Error. Expected one source function and one exact semantic "
            "auxiliary declaration, but found source="
         << lexicalCounter << " auxiliary=" << auxiliaryCounter << endl;
    ROSE_ASSERT(false);
  }

  return backend(project);
}
