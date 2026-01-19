
#include "rose.h"

// DQ (2/26/2009): The copyGraph.[hC] files have been moved to src/midend/astDump.
// directory and are not included in librose.so.
// #include "copyGraph.C"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

SgNode*
copyAST ( SgNode* node )
   {
     ROSE_ASSERT(node != NULL);

  // This is a better implementation using a derived class from SgCopyHelp to control the 
  // copying process (skipping the copy of any function definition).  This is a variable 
  // declaration with an explicitly declared class type.
     class RestrictedCopyType : public SgCopyHelp
        {
       // DQ (9/26/2005): This class demonstrates the use of the copy mechanism 
       // within Sage III (originally designed and implemented by Qing Yi).
       // One problem with it is that there is no context information permitted.

          public:
               virtual SgNode *copyAst(const SgNode *n)
                  {
                 // This is the simpliest possible version of a deep copy
                 // SgCopyHelp::copyAst() member function.
                 SgNode *returnValue = n->copy(*this);
                 return returnValue;
                  }
        } restrictedCopyType;

  // This triggers a bug with test2005_152.C (the unparsed code fails for g++ 4.1.2, but not 3.5.6)
        SgNode *copyOfNode = node->copy(restrictedCopyType);
        ROSE_ASSERT(copyOfNode != NULL);

        // std::vector<SgNode*> intersectionNodeList_early =
        // SageInterface::astIntersection(node,copyOfNode,&restrictedCopyType);

        // DQ (10/19/2007): This might really be required inorder to pass our
        // strict tests of the AST.
        if (SgProject::get_verbose() > 0)
          printf ("Running the AST Post Processing Phase on the new copy of the AST! \n");

     AstPostProcessing(copyOfNode);

     if (SgProject::get_verbose() > 0)
          printf ("DONE: Running the AST Post Processing Phase on the new copy of the AST! \n");

     if (SgProject::get_verbose() > 0)
          printf ("\n\nCompare two generated ASTs ... \n");

     std::vector<SgNode*> intersectionNodeList = SageInterface::astIntersection(node,copyOfNode,&restrictedCopyType);

     if (SgProject::get_verbose() > 0)
          printf ("DONE: Compare two generated ASTs ... \n");

  // DQ (11/2/2007): Make this a stricter test!
     ROSE_ASSERT(intersectionNodeList.size() == 0);

     ROSE_ASSERT(copyOfNode != NULL);
     return copyOfNode;
   }



void
printOutTemplateDeclarations ()
   {
  // Debugging support

     class TraverseMemoryPool : public ROSE_VisitTraversal
        {
          public:
            // Required traversal function
               void visit (SgNode* node)
                  {
                    SgTemplateDeclaration* derivedDeclaration = isSgTemplateDeclaration(node);
                    if (derivedDeclaration != NULL)
                       {
                         printf ("case V_SgTemplateDeclaration: derivedDeclaration               = %p \n",derivedDeclaration);
                         printf ("case V_SgTemplateDeclaration: derivedDeclaration->get_parent() = %p = %s \n",derivedDeclaration->get_parent(),derivedDeclaration->get_parent()->class_name().c_str());
                         printf ("case V_SgTemplateDeclaration: derivedDeclaration->get_scope()  = %p = %s \n",derivedDeclaration->get_scope(),derivedDeclaration->get_scope()->class_name().c_str());
                         printf ("case V_SgTemplateDeclaration: derivedDeclaration->get_declarationModifier().isFriend() = %s \n",derivedDeclaration->get_declarationModifier().isFriend() ? "true" : "false");
                         printf ("case V_SgTemplateDeclaration: derivedDeclaration->get_name()   = %s \n",derivedDeclaration->get_name().str());
                         printf ("case V_SgTemplateDeclaration: derivedDeclaration->get_string() = %s \n",derivedDeclaration->get_string().str());
                       }
                  }

            // This avoids a warning by g++
               virtual ~TraverseMemoryPool() {};         
        };

     printf ("Friend template function declarations are not properly marked as friends \n");

  // This will traverse the whole memory pool
     TraverseMemoryPool traversal;
     traversal.traverseMemoryPool();
   }






int
main ( int argc, char* argv[] )
   {
  // Main Function for default example ROSE Preprocessor
  // This is an example of a preprocessor that can be built with ROSE
  // This example can be used to test the ROSE infrastructure

     ios::sync_with_stdio();     // Syncs C++ and C I/O subsystems!

     if (SgProject::get_verbose() > 0)
          printf ("In preprocessor.C: main() \n");

     SgProject* project = frontend(argc,argv);
     ROSE_ASSERT (project != NULL);

     if (project->get_verbose() > 0)
          printf ("\n\nRunning tests on the original AST (before copying) \n");

  // DQ (2/6/2004): These tests fail in Coco for test2004_14.C
  // AstTests::runAllTests(const_cast<SgProject*>(project));
     AstTests::runAllTests(project);

     if (project->get_verbose() > 0)
          printf ("Calling the AST copy mechanism \n");

  // printf ("\n\nCalling the AST copy mechanism \n");

     set<SgNode*> oldNodes;
     // if (numberOfNodes() < 2000)
        {
          oldNodes = getAllNodes();
        }

        // Use this setting to control if we make a copy or not!
        // Demonstrate the copying of the whole AST
        SgProject *newProject = static_cast<SgProject *>(copyAST(project));
        ROSE_ASSERT(newProject != NULL);

        // Debugging support
        // Output the declaration so that we can investigate friend template
        // function declarations (these are not properly marked as friend
        // functions). printOutTemplateDeclarations();

        // printf ("\n\nRunning tests on the original AST \n");
        AstTests::runAllTests(project);
        // DQ (10/19/2007): Turning this off allows for a lot of things to work
        // great, but it is cheating :-).
        if (project->get_verbose() > 0)
          printf ("\n\nRunning tests on the copy of the AST \n");

        AstTests::runAllTests(newProject);

        if (project->get_verbose() > 0)
          printf ("Calling the backend() \n");

     int errorCode = 0;
     errorCode = backend(project);

  // DQ (7/7/2005): Only output the performance report if verbose is set (greater than zero)
     if (project->get_verbose() > 0)
        {
       // Output any saved performance data (see ROSE/src/astDiagnostics/AstPerformance.h)
          AstPerformance::generateReport();
        }

  // printf ("Exiting with errorCode = %d \n",errorCode);
     return errorCode;
   }
