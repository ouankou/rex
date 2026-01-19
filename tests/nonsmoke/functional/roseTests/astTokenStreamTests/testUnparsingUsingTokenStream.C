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
          printf ("In visit(): Found SgFunctionDefinition: funcdef->get_declaration()->get_name() = %s \n",funcdef->get_declaration()->get_name().str());

          SgScopeStatement *scope = getGlobalScope(funcdef);
          // DQ (11/9/2015): Unless this is marked to be output in code
          // generaton, this function will not show up in the output.
          SgFunctionDeclaration *func_defn = buildDefiningFunctionDeclaration(
              SgName("testFunc"), buildVoidType(),
              buildFunctionParameterList(
                  buildInitializedName(SgName("param1"), buildIntType(), NULL)),
              scope);

          SgFunctionDeclaration *func_decl =
              buildNondefiningFunctionDeclaration(func_defn, scope);
          // if (NULL != isSgGlobal(scope))
          if (isSgGlobal(scope) != NULL)
             {
            SgStatement *first_stmt = findFirstDefiningFunctionDecl(scope);
            if (NULL == first_stmt) {
              first_stmt = getFirstStatement(scope);
            }

            if (NULL != first_stmt) {
              // DQ (11/9/2015): Unless this is marked to be output in code
              // generaton, this function will not show up in the output.
              insertStatementBefore(first_stmt, func_decl);
            } else {
              // DQ (11/9/2015): Unless this is marked to be output in code
              // generaton, this function will not show up in the output.
              prependStatement(func_decl, scope);
            }
          } else {
            // DQ (11/9/2015): Unless this is marked to be output in code generaton, this function will not show up in the output.
               prependStatement(func_decl, scope);
          }

       // DQ (11/9/2015): Unless this is marked to be output in code generaton, this function will not show up in the output.
          insertStatementAfter(funcdef->get_declaration(), func_defn);
          SgBasicBlock *func_body = func_defn->get_definition()->get_body();
          SgVariableDeclaration *i = buildVariableDeclaration(SgName("i"),buildIntType(),buildAssignInitializer(buildIntVal(0),buildIntType()),func_body);

          // DQ (11/9/2015): Unless this is marked to be output in code
          // generaton, this function will not show up in the output.
          appendStatement(i, func_body);
          // DQ (5/19/2021): Mark the subtree as being from same file as the
          // scope.
          int physical_file_id = scope->get_file_info()->get_physical_file_id();
          // DQ (5/20/2021): We need to set the physical file id for boththe
          // defining and firstnondefining declarations.
          SageInterface::markSubtreeToBeUnparsed(func_defn,physical_file_id);
          SageInterface::markSubtreeToBeUnparsed(func_decl, physical_file_id);
          done = true;
        }
   }

int main(int argc, char *argv[])
   {
     SgProject* project = frontend(argc,argv);
     ROSE_ASSERT(project!=NULL);

  // SgProject::set_verbose(3);

     SimpleInstrumentation treeTraversal;
     treeTraversal.done = false;
     treeTraversal.traverseInputFiles(project, preorder);

  // SgProject::set_verbose(0);

  // DQ (4/15/2015): We should reset the isModified flags as part of the transformation 
  // because we have added statements explicitly marked as transformations.
  // checkIsModifiedFlag(project);

  // Output an optional graph of the AST (just the tree, when active)
     printf ("Generating a dot file... (ROSE Release Note: turn off output of dot files before committing code) \n");
  // DQ (12/22/2019): Call multi-file version (instead of generateDOT() function).
  // generateAstGraph(project, 2000);
  // generateDOT ( *project );
     generateDOTforMultipleFile(*project);

     return backend(project);
}
