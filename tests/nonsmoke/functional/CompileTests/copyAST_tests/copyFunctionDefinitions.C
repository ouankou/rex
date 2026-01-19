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
       SgTreeCopy tc;
       std::string functionDefinitionString =
           functionDefinition->unparseToString();
       printf("(before copy) functionDefinitionString = %s \n",
              functionDefinitionString.c_str());
       SgFunctionDefinition *copy = isSgFunctionDefinition(n->copy(tc));
       ROSE_ASSERT(copy != NULL);
       functionDefinitionString = functionDefinition->unparseToString();
       printf("(after copy) functionDefinitionString = %s \n",
              functionDefinitionString.c_str());
       SgFunctionDeclaration *parentScope =
           isSgFunctionDeclaration(functionDefinition->get_parent());
       ROSE_ASSERT(parentScope);
       copy->set_parent(parentScope);
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
