#include "rose.h"

class Visitor: public AstSimpleProcessing
   {
     public:
         virtual void visit(SgNode* n);
};

void
Visitor::visit(SgNode* n)
   {
     SgForStatement* forStatement = isSgForStatement(n);
     if (forStatement != NULL)
        {
       SgTreeCopy tc;
       SgForStatement *copy = isSgForStatement(n->copy(tc));
       ROSE_ASSERT(copy != NULL);
       SgScopeStatement *parentScope =
           isSgScopeStatement(forStatement->get_parent());
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
