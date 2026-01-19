#include "rose.h"
//#include "FunctionInsert.h"

using namespace SageBuilder;
using namespace SageInterface;

class SimpleInstrumentation : public SgSimpleProcessing
   {
     public:void visit(SgNode* astNode);
     bool done;
   };

void SimpleInstrumentation::visit(SgNode* astNode)
   {
  // SgFile *file=isSgFile(astNode);
     SgFunctionDefinition* funcdef = isSgFunctionDefinition(astNode);
     if (funcdef != NULL && !done)
        {
          printf ("Found SgFunctionDefinition: funcdef->get_declaration()->get_name() = %s \n",funcdef->get_declaration()->get_name().str());

          SgScopeStatement *scope = getGlobalScope(funcdef);
          SgFunctionDeclaration *func_defn = buildDefiningFunctionDeclaration(
              SgName("testFunc"), buildVoidType(),
              buildFunctionParameterList(
                  buildInitializedName(SgName("param1"), buildIntType(), NULL)),
              scope);
          SgFunctionDeclaration *func_decl =
              buildNondefiningFunctionDeclaration(func_defn, scope);
          if (NULL != isSgGlobal(scope))
             {
               SgStatement *first_stmt = findFirstDefiningFunctionDecl(scope);

               printf ("Handling the global scope: first_stmt = %p \n",first_stmt);

               if (NULL == first_stmt)
                  {
                    first_stmt = getFirstStatement(scope);
                  }

               if (NULL != first_stmt)
                  {
                 printf("Calling insertStatementBefore() \n");
                 insertStatementBefore(first_stmt, func_decl);
                  }
                 else
                  {
                    printf("Calling prependStatement() \n");
                    prependStatement(func_decl, scope);
                  }
             }
            else
             {
               printf("Handling the non-global scope \n");
               prependStatement(func_decl, scope);
             }

          SgStatement *last_global_decl = findLastDeclarationStatement(scope);
          insertStatementAfter(last_global_decl, func_defn);

          SgBasicBlock *func_body = func_defn->get_definition()->get_body();

          SgVariableDeclaration *i = buildVariableDeclaration(SgName("i"),buildIntType(),buildAssignInitializer(buildIntVal(0),buildIntType()),func_body);
          appendStatement(i, func_body);

          done = true;
        }
   }

int main(int argc, char *argv[])
   {
     SgProject* project = frontend(argc,argv);
     ROSE_ASSERT(project!=NULL);

     SimpleInstrumentation treeTraversal;
     treeTraversal.done = false;
     treeTraversal.traverseInputFiles(project, preorder);

     return backend(project);
}
