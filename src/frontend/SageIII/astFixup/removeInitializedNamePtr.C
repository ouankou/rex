// tps (01/14/2010) : Switching from rose.h to sage3.
#include "AstFixup.h"
#include "removeInitializedNamePtr.h"
#include "sage3basic.h"

// tps : Added this as it is defined somewhere in rose.h
#include "AstDiagnostics.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

// [DQ]
// Allocation of space for listOfTraversedTypes declared in traversal
list<SgNode *> RemoveInitializedNamePtr::listOfTraversedNodes;

// [MS]
RemoveInitializedNamePtrInheritedAttribute
RemoveInitializedNamePtr::evaluateInheritedAttribute(
    SgNode *node, RemoveInitializedNamePtrInheritedAttribute ia) {
  ROSE_ASSERT(node);

  // printf ("In RemoveInitializedNamePtr::evaluateInheritedAttribute node = %p
  // = %s \n",node,node->sage_class_name());

  switch (node->variantT()) {
    // [MS]: FIX 1 : remove cycle in SgInitializedName Node Pair
  case V_SgInitializedName: {
    // SgInitializedName* initializedName =
    // static_cast<SgInitializedName*>(node); printf ("Found SgInitializedName
    // name = %s \n",initializedName->get_name().str());
    if (!ia.firstInitializedNameVisited) {
      // printf ("Found first SgInitializedName \n");
      ia.firstInitializedNameVisited = true;
    } else {
      // printf ("Found additional SgInitializedName: calling set_itemptr(NULL)
      // \n");
    }
    break;
  }

    // [MS]: FIX 2 : remove redundant pointer to definition from a
    // SgMemberFunctionDeclaration node if the implementation of the function is
    // also reachable from SgGlobal->SgMemberFunctionDeclaration (the test
    // implemented here  is structural knowledge and not explicitely tested
    // here!)
  case V_SgMemberFunctionDeclaration: {
    SgMemberFunctionDeclaration *mfdnode =
        static_cast<SgMemberFunctionDeclaration *>(node);

    // DQ (11/28/2003): fixed pointer to definition to make it valid only
    //                  if their is a function definition plus body
    ROSE_ASSERT(mfdnode != NULL);
    // ROSE_ASSERT (mfdnode->get_definition() != NULL);

    // DQ (11/28/2003): Added conditional test so that appies only if the
    // mfdnode->get_definition() != NULL
    if (mfdnode->get_definition() != NULL) {
      // DQ (9/24/2004): Make this test also dependent upon
      // (mfdnode->get_definition()->get_parent() != NULL) This is
      // part of a attempt to remove the use of parents in the legacy
      // frontend/Sage interface (so that they will only be set in
      // postprocessing (AST Fixup).
      // ROSE_ASSERT(mfdnode->get_definition()->get_parent() !=
      // NULL); if ( mfdnode->get_definition()->get_parent() != node
      // /* cut off in class */
      //      && !(mfdnode->get_parent()->variantT() == V_SgGlobal)
      //      /* but do not cut off at nodes ref.to by SgGlobal */)

      // DQ (9/13/2011): Reported as possible NULL value in static analysis of
      // ROSE code.
      if (mfdnode->get_definition()->get_parent() != NULL) {
        if (mfdnode->get_definition()->get_parent() !=
            node) { /* cut off in class */
          SgNode *parentNode = mfdnode->get_parent();
          ROSE_ASSERT(parentNode != NULL);
          if (!(parentNode->variantT() ==
                V_SgGlobal)) { /* but do not cut off at nodes ref.to by SgGlobal
                                */
            printf("mfdnode->get_parent() = %p = %s \n", parentNode,
                   parentNode->sage_class_name());
            ROSE_ASSERT(mfdnode->get_definition()->get_parent() == node);
            ROSE_ASSERT(parentNode->variantT() != V_SgGlobal);

            // DQ (12/5/2003): This case is now an error and
            // should no longer be required.  The fix was made
            // in the legacy frontend/SAGE interface code so to
            // test that fix we make it an error here.
            printf("In AstFixes.C: Eliminated this fix of the AST (now an "
                   "error, fix is not required) \n");
            ROSE_ABORT();

            // DQ (5/18/2005): Removed definition_ref since it is redundant with
            // definingDeclaration and firstNondefiningDeclaration. DQ This code
            // is not reached but left as documentation about how the problem
            // was previously fixed
            // mfdnode->set_definition_ref(mfdnode->get_definition()); // copy
            // definition pointer
            mfdnode->set_definition(
                NULL); // nullify pointer to definition because it is
                       // reachable from SgGlobal->SgMemberFunctionDeclaration
                       // as well
          }
        }
      }
    }
    break;
  }

  case V_SgClassDeclaration: {
    // printf ("Found a SgClassDeclaration \n");
    // SgClassDeclaration* classDeclaration = isSgClassDeclaration(node);
    // printf ("classDeclaration->isForward() = %s
    // \n",classDeclaration->isForward() ? "true" : "false");
    break;
  }

    // DQ (1/19/2004): Required for DataStructure example to work! But causes
    // the test2004_05.C file to fail!

    // DQ (1/18/2004): Island Hunting (fixup isolated not-type AST fragments
    // hiding in in typedefs)
  case V_SgTypedefDeclaration: {
    // printf ("Found a SgTypedefDeclaration \n");
    SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(node);
    ROSE_ASSERT(typedefDeclaration != NULL);

    SgTypedefType *typedefType =
        isSgTypedefType(typedefDeclaration->get_type());
    ROSE_ASSERT(typedefType != NULL);

    // printf ("typedefType->get_name() = %s \n",typedefType->get_name().str());

#ifndef NDEBUG
    SgDeclarationStatement *declaration = typedefType->get_declaration();
    ROSE_ASSERT(declaration != NULL);
#endif
    // printf ("declaration->sage_class_name() = %s
    // \n",declaration->sage_class_name());

    SgType *type = typedefDeclaration->get_base_type();
    ROSE_ASSERT(type != NULL);

    // jumps over DEF2TYPE traversal barrier
    SgClassType *classType = isSgClassType(type); // traversal succeeds
    if (classType != NULL) {

      // printf ("classType->get_autonomous_declaration() = %s
      // \n",classType->get_autonomous_declaration() ? "true" : "false");

      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(classType->get_declaration());
      ROSE_ASSERT(classDeclaration != NULL);
      // printf ("In Typedef: SgClassDeclaration = %p \n",classDeclaration);
      // printf ("classDeclaration->isForward() = %s
      // \n",classDeclaration->isForward() ? "true" : "false");

      // SgClassDefinition* classDefinition =
      // isSgClassDefinition(classDeclaration->get_definition()); ROSE_ASSERT
      // (classDefinition != NULL);

      // DQ (1/19/2004): Required for DataStructure example to work! But causes
      // the test2004_05.C file to fail! Because in test2004_05 the same class
      // declaration will be visited twice (if this it ON)!!!

      // A specific test code forces a bug fix (test code derived from code in
      // stdio.h). test2004_07.C demonstrates that we if we want to traverse
      // typedefs to eliminate islands of unfixed AST then we have to both visit
      // the typedef statements AND only traverse the types used in them
      // (base_type) the first time it is seen.  So we keep track of all types
      // seen in typedefs, so that we will only traverse them once.  Not doing
      // so results in infinte recursion!

      // printf ("checking for classType = %p \n",classType);
      list<SgNode *>::iterator previouslyTraversedNode = find(
          listOfTraversedNodes.begin(), listOfTraversedNodes.end(), classType);
      bool traverseNode =
          (previouslyTraversedNode == listOfTraversedNodes.end());

      if (traverseNode == true) {
        // Add to list of traversed types
        listOfTraversedNodes.push_back(classType);

        // AstCycleTest cycTest3;
        // cycTest3.traverse(classDeclaration);
        // subTemporaryAstFixes(classDeclaration);

        RemoveInitializedNamePtrInheritedAttribute ia;
        RemoveInitializedNamePtr temporaryfix;
        temporaryfix.traverse(classDeclaration, ia);
      }
    }
    break;
  }
    // DQ (8/25/2004): Added default statement to eliminate warning about switch
    // not having default (good idea anyway)
  default: {
    // g++ needs a block here
  }
  }

  return ia;
}

// MS
// void subTemporaryAstFixes(SgNode* node)
void removeInitializedNamePtr(SgNode *node) {
  // DQ (9/12/2004): This function was updateded some time ago to only test for
  // the conditions that used to be fixed.  An error is now reported if the
  // condition is found and execution terminates. The conditions previously
  // fixed here are not fixed in the legacy frontend AST. Different fixups
  // developed by Markus to correct problems in the legacy frontend/SAGE
  // connection

  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("SubTemporaryAstFixes:");

  if (SgProject::get_verbose() >= AST_FIXES_VERBOSE_LEVEL)
    cout << "/* AST Fixes fixup initializers */" << endl;

  RemoveInitializedNamePtrInheritedAttribute ia;
  RemoveInitializedNamePtr temporaryfix;
  temporaryfix.traverse(node, ia);

  if (SgProject::get_verbose() >= AST_FIXES_VERBOSE_LEVEL)
    cout << "/* AST Fixes empty operator nodes */" << endl;

  DeleteEmptyOperatorNodes delOpNodes;
  delOpNodes.traverse(node);
}

// [DQ]
// Allocation of space for listOfTraversedTypes declared in traversal
list<SgNode *> DeleteEmptyOperatorNodes::listOfTraversedTypes;

void DeleteEmptyOperatorNodes::visitWithAstNodePointersList(
    SgNode * /*node*/, AstNodePointersList l) {
  for (AstNodePointersList::iterator i = l.begin(); i != l.end(); i++) {
    // DQ (4/8/2011): This is an Insure++ issue...fixed by using the
    // isSgDotExp() function which handles NULL pointers more cleanly. if
    // (SgDotExp* dotNode=dynamic_cast<SgDotExp*>(*i))
    if (SgDotExp *dotNode = isSgDotExp(*i)) {
      // cout << "AST TEST: SgDotExp(" << dotNode->get_lhs_operand_i() << ", "
      //      << dotNode->get_rhs_operand_i() << ") found." << endl;
      if (dotNode->get_lhs_operand_i() == NULL &&
          dotNode->get_rhs_operand_i() == NULL) {
        cout << "AST TEST: WARNING: SgDotExp node has no successors." << endl;
      }
    }
  }

  // MS: FIX 4:
}
