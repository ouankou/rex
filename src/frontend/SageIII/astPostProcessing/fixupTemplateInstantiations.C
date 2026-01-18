// tps (01/14/2010) : Switching from rose.h to sage3.
#include "astPostProcessing/fixupTemplateInstantiations.h"
#include "markCompilerGenerated.h"
#include "sage3basic.h"
void fixupTemplateInstantiations(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Fixup template specializations:");

  // This simplifies how the traversal is called!
  FixupTemplateInstantiations declarationFixupTraversal;

  // I think the default should be preorder so that the interfaces would be more
  // uniform
  declarationFixupTraversal.traverse(node, preorder);
}

void FixupTemplateInstantiations::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  // Take care of marking the whole subtree of any declarations
  // that the legacy frontend/Sage connection marked as compiler generated.
  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);

  // DQ (1/18/2014): Testcode test2012_75.c demonstrates why we need to force
  // the function parameters to be marked as compiler generated.  Else there are
  // errors in how comments are woven back into the AST. DQ (1/18/2014): Skip
  // function parameter lists, since they are always marked as compiler
  // generated (because we don't have source position information for them).
  // Perhaps marking it as frontend specific would be more appropriate).
  // if (declaration != NULL)
  // if (declaration != NULL && isSgFunctionParameterList(declaration) == NULL)
  if (declaration != NULL) {
    if (declaration->get_file_info() == NULL) {
      printf("Error: (declaration->get_file_info() == NULL) declaration = %p = "
             "%s = %s \n",
             declaration, declaration->class_name().c_str(),
             SageInterface::get_name(declaration).c_str());
    }
    ROSE_ASSERT(declaration->get_file_info() != NULL);

    // DQ (6/17/2005): compiler generated does not imply that it will be
    // output by the unparser (anymore) Some declarations are marked as
    // compiler generated in the legacy frontend/Sage III translation, but
    // the whole subtree is never marked at that point.  This step marks
    // the whole subtree as compiler generated when just the declaration
    // is detected as having been marked in the legacy frontend/Sage III
    // translation.
    if (declaration->get_file_info()->isCompilerGenerated() == true) {

      // DQ (8/10/2005): We should never mark a template declaration as compiler
      // generated (though perhaps partial specializations could be marked as
      // such later). ROSE_ASSERT(isSgTemplateDeclaration(node) == NULL);
      if (isSgTemplateDeclaration(node) == NULL) {
        // Mark the whole declaration as compiler generated since we
        // could not do so in the legacy frontend/Sage III translation
        markAsCompilerGenerated(declaration);
      }
    }
  }
}
