#include "rose.h"

class Visitor: public AstSimpleProcessing
   {
     public:
         virtual void visit(SgNode* n);
};

void
Visitor::visit(SgNode* n)
   {
     SgBasicBlock* basicBlock = isSgBasicBlock(n);
     if (basicBlock != NULL)
        {
       SgTreeCopy tc;
       std::string basicBlockString = basicBlock->unparseToString();
       printf("(before copy) forStatementString = %s \n",
              basicBlockString.c_str());
       SgBasicBlock *copy = isSgBasicBlock(n->copy(tc));
       ROSE_ASSERT(copy != NULL);
       basicBlockString = basicBlock->unparseToString();
       printf("(after copy) forStatementString = %s \n",
              basicBlockString.c_str());
       SgScopeStatement *parentScope =
           isSgScopeStatement(basicBlock->get_parent());
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
