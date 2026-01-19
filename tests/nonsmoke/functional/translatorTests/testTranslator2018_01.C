// Example of how to do a specific transformation (adding a function parameter to existing functions).

#include "rose.h"

using namespace SageBuilder;
using namespace SageInterface;

class SimpleTransformation : public SgSimpleProcessing
   {
     public:
          void visit(SgNode* astNode);
   };

void SimpleTransformation::visit(SgNode* astNode)
   {
  // Visit function to iterate over the AST and transform all functions.
     SgFunctionDeclaration* functionDeclaration = isSgFunctionDeclaration(astNode);
     if (functionDeclaration != NULL)
        {
        }
   }

int main(int argc, char *argv[])
   {
     SgProject* project = frontend(argc,argv);
     ROSE_ASSERT(project!=NULL);

     SimpleTransformation treeTraversal;
     treeTraversal.traverseInputFiles(project, preorder);

  // AST consistency tests (optional for users, but this enforces more of our tests)
     AstTests::runAllTests(project);

     return backend(project);
   }




