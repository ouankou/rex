#include "rose.h"

class Visitor: public AstSimpleProcessing
   {
     public:
         virtual void visit(SgNode* n);
};

void
Visitor::visit(SgNode* n)
   {
     SgFunctionDefinition* functionDefinition = isSgFunctionDefinition(n);
     if (functionDefinition != NULL)
        {
       SgFunctionDeclaration *functionDeclaration =
           functionDefinition->get_declaration();
       ROSE_ASSERT(functionDeclaration != NULL);
       ROSE_ASSERT(functionDeclaration->get_definition() == functionDefinition);
       SgTreeCopy tc;
       std::string functionDefinitionString =
           functionDefinition->unparseToString();
       printf("(before copy) functionDefinitionString = %s \n",
              functionDefinitionString.c_str());
       SgFunctionDeclaration *declarationCopy =
           isSgFunctionDeclaration(functionDeclaration->copy(tc));
       ROSE_ASSERT(declarationCopy != NULL);
       SgFunctionDefinition *copy = declarationCopy->get_definition();
       ROSE_ASSERT(copy != NULL);
       ROSE_ASSERT(copy != functionDefinition);
       ROSE_ASSERT(copy->get_declaration() == declarationCopy);
       ROSE_ASSERT(copy->get_parent() == declarationCopy);
       // A copied declaration family is deliberately detached until a producer
       // inserts it into an exact source-file context.  Verify the copy above,
       // but do not ask the context-sensitive definition subtree to unparse as
       // though it were already published.
       functionDefinitionString = functionDefinition->unparseToString();
       printf("(after copy) functionDefinitionString = %s \n",
              functionDefinitionString.c_str());
     }
   }

int main( int argc, char * argv[] )
   {
     SgProject* sageProject = frontend(argc,argv);
     AstTests::runAllTests(sageProject);
     Visitor v;
     v.traverse(sageProject, postorder);

     generateAstGraph(sageProject, 4000);

     return 0;
   }
