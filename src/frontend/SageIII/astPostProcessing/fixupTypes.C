// tps (01/14/2010) : Switching from rose.h to sage3.
#include "fixupTypes.h"

#include "sage3basic.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

void resetTypesInAST() {
  ResetTypes t;
  t.traverseMemoryPool();
}

void ResetTypes::visit(SgNode *node) {
  // The purpose of this traversal is to fixup the AST to share types more
  // uniformally shared. It appears that in the legacy frontend/Sage translation
  // some types are not shared properly, particularly SgNamedTypes.

  SgNamedType *namedType = isSgNamedType(node);
  if (namedType != NULL) {
    SgDeclarationStatement *declaration = namedType->get_declaration();
    ROSE_ASSERT(declaration != NULL);
    SgDeclarationStatement *definingDeclaration =
        declaration->get_definingDeclaration();
    SgDeclarationStatement *nondefiningDeclaration =
        declaration->get_firstNondefiningDeclaration();
    if ((definingDeclaration != NULL) && (nondefiningDeclaration != NULL)) {
      // printf ("ResetTypes::visit(): resetting the definition used in
      // namedType = %p = %s \n",namedType,namedType->class_name().c_str());
#if DEBUG_SAGE_ACCESS_FUNCTIONS || 0
      // DQ (6/12/2007): New access function tests using
      // DEBUG_SAGE_ACCESS_FUNCTIONS and DEBUG_SAGE_ACCESS_FUNCTIONS_ASSERTION
      // in sage3.h indicate this is required.
      if (namedType->get_declaration() != NULL) {
        printf("Note in ResetTypes::visit(): overwriting "
               "namedType->get_declaration() = %p with NULL before assignment "
               "to nondefiningDeclaration = %p \n",
               namedType->get_declaration(), nondefiningDeclaration);
        namedType->set_declaration(NULL);
      }
#endif
      namedType->set_declaration(nondefiningDeclaration);
    }
  }

  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
  if (declaration != NULL) {
    switch (declaration->variantT()) {
    case V_SgClassDeclaration: {
      SgClassDeclaration *declaration = isSgClassDeclaration(node);
      SgClassDeclaration *definingDeclaration =
          isSgClassDeclaration(declaration->get_definingDeclaration());
      SgClassDeclaration *nondefiningDeclaration =
          isSgClassDeclaration(declaration->get_firstNondefiningDeclaration());
      // SgType* declarationType = declaration->get_type();
      bool resetType = false;
      if (definingDeclaration != NULL) {
        // Set the name to that in the defining declaration
        // DQ (6/12/2007): When this is executed we have a bug in ROSE!
        // DQ (6/12/2007): New access function tests using
        // DEBUG_SAGE_ACCESS_FUNCTIONS and DEBUG_SAGE_ACCESS_FUNCTIONS_ASSERTION
        // in sage3.h indicate this is required. Avoid calling
        // definingDeclaration->get_type() multiple times. printf
        // ("definingDeclaration = %p = %s
        // \n",definingDeclaration,definingDeclaration->class_name().c_str());
        SgClassType *newType = definingDeclaration->get_type();
#if DEBUG_SAGE_ACCESS_FUNCTIONS
        if (declaration->get_type() != NULL && newType != NULL) {
          printf("Note in ResetTypes::visit(): overwriting "
                 "declaration->get_type() = %p with NULL before assignment to "
                 "definingDeclaration->get_type() = %p \n",
                 declaration->get_type(), newType);
          declaration->set_type(NULL);
        }
#endif
        declaration->set_type(newType);

        // printf ("ResetTypes::visit(): resetType == true: Can we delete
        // declarationType = %p \n",declarationType); delete declarationType;
        ROSE_ASSERT(declaration->get_type() == definingDeclaration->get_type());

        resetType = true;
      }

      if ((resetType == false) && (nondefiningDeclaration != NULL)) {
        // Set the name to that in the defining declaration
        SgClassType *newType = nondefiningDeclaration->get_type();
#if DEBUG_SAGE_ACCESS_FUNCTIONS
        if (declaration->get_type() != NULL && newType != NULL) {
          printf("Note in ResetTypes::visit(): overwriting "
                 "declaration->get_type() = %p with NULL before assignment to "
                 "nondefiningDeclaration->get_type() = %p \n",
                 declaration->get_type(), newType);
          declaration->set_type(NULL);
        }
#endif
        // declaration->set_type(nondefiningDeclaration->get_type());
        declaration->set_type(newType);
        // printf ("ResetTypes::visit(): resetType == false: Can we delete
        // declarationType = %p \n",declarationType); delete declarationType;
        // ROSE_ASSERT(declaration->get_type() ==
        // definingDeclaration->get_type());
        ROSE_ASSERT(declaration->get_type() ==
                    nondefiningDeclaration->get_type());
        resetType = true;
      }

      break;
    }

    case V_SgFunctionDeclaration: {
      SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
      SgFunctionDeclaration *definingDeclaration =
          isSgFunctionDeclaration(declaration->get_definingDeclaration());
      SgFunctionDeclaration *nondefiningDeclaration = isSgFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration());
      if (definingDeclaration != NULL) {
        if (declaration->get_type() != definingDeclaration->get_type()) {
          // DQ (3/3/2009): Make this conditional upon it being a
          // transformation, since they cause types to be build redundantly in
          // the SageBuilder function (I think).
          if (declaration->get_file_info()->isTransformation() == false) {
            printf("Error: types in declaration = %p = %s and "
                   "definingDeclaration = %p not shared \n",
                   declaration, declaration->class_name().c_str(),
                   definingDeclaration);
            declaration->get_file_info()->display(
                "Error: types in declaration and definingDeclaration are not "
                "shared");
            printf("definingDeclaration->unparseToString() = %s \n",
                   definingDeclaration->unparseToString().c_str());
          }

          if (definingDeclaration->get_type() != NULL) {
            // Use the type held in the defining declaration
            // delete declaration->get_type();
            declaration->set_type(definingDeclaration->get_type());
          }
        }
        ROSE_ASSERT(declaration->get_type() == definingDeclaration->get_type());
      }

      if (nondefiningDeclaration != NULL && definingDeclaration != NULL) {
        if (declaration->get_type() != nondefiningDeclaration->get_type()) {
          if (nondefiningDeclaration->get_type() != NULL) {
            // delete nondefiningDeclaration->get_type();
            nondefiningDeclaration->set_type(definingDeclaration->get_type());
          } else {
            if (definingDeclaration->get_type() != NULL) {
              // delete definingDeclaration->get_type();
              definingDeclaration->set_type(nondefiningDeclaration->get_type());
            } else {
              printf("Error: ResetTypes::visit(node = %p = %s) \n", node,
                     node->class_name().c_str());
              printf("Warning: get_type() == NULL for both defining and "
                     "nondefining declarations \n");
              ROSE_ABORT();
            }
          }
        }
        ROSE_ASSERT(declaration->get_type() ==
                    nondefiningDeclaration->get_type());
      }

      if ((definingDeclaration != NULL) && (nondefiningDeclaration != NULL)) {
        if (definingDeclaration->get_type() != NULL) {
          // Use the type held in the defining declaration
          if (nondefiningDeclaration->get_type() != NULL) {
            // delete nondefiningDeclaration->get_type();
          }
          nondefiningDeclaration->set_type(definingDeclaration->get_type());
        }
        ROSE_ASSERT(definingDeclaration->get_type() ==
                    nondefiningDeclaration->get_type());
      }
      break;
    }

    default: {
      break;
    }
    }
  }

  // DQ (10/10/2007): Moved to after any possible use above (reported by
  // Valentin). Partial function types are used to build function types, but
  // after we have used them then can be removed!
  if (isSgPartialFunctionType(node) != NULL) {
    delete node;
  }
}
