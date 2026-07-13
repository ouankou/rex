#ifndef RESET_PARENT_POINTERS_H
#define RESET_PARENT_POINTERS_H

/*! \brief Validate exact parent ownership for a published AST.

    Interface for resetting parent pointers (called by temporaryAstFixes()
    function, but also required to reset parent pointers after any addition
    of new AST fragments to the AST).

    \internal This function can be called directly as well.
 */

void validateParentPointers(SgNode *node, SgNode *parent = nullptr);

/*! \brief Validate a fresh detached construction transaction.

    The transaction root must already be owned by the supplied detached
    boundary.  No parent or ownership field is changed.
 */
void validateFreshSubtreeOwnership(SgNode *node, SgNode *boundary);

// *******************************************************************************************
// DQ (3/5/2003): Need to have this in the header file so that the static member
// data modifiedNodeInformationList can be accessed within ROSE translators
// (this might change later).
// *******************************************************************************************
#include "AstNodeVisitMapping.h"

//! Inherited attribute required for ResetParentPointers class.
class ValidateParentPointersInheritedAttribute {
public:
  //! Default constructor
  ValidateParentPointersInheritedAttribute() : parentNode(nullptr) {}

  // DQ (8/1/2019): Copy constructor.
  ValidateParentPointersInheritedAttribute(
      const ValidateParentPointersInheritedAttribute &X) {
    parentNode = X.parentNode;
  }
  ValidateParentPointersInheritedAttribute &
  operator=(const ValidateParentPointersInheritedAttribute &) = default;

  //! Store previous node for reference and internal testing and output of
  //! debugging information
  SgNode *parentNode;
};

/*! \brief This traversal implements the mechanism to reset all parent pointers
   (back edges in the AST graph)

    This traversal traverses the whole AST except types and symbols (future
   versions of the traversal will traverse type and symbols)

    \internal This traversal is a demonstration of how to traverse the full AST
   including islands of code not yet handled in the default traversal.  Nested
   traversals are used to implement the traversal of source code (typically
   class definitions) hidden in types.
 */
class ValidateParentPointers
    : public SgTopDownProcessing<ValidateParentPointersInheritedAttribute> {
public:
  //! Required traversal function
  ValidateParentPointersInheritedAttribute evaluateInheritedAttribute(
      SgNode *node,
      ValidateParentPointersInheritedAttribute inheritedAttribute);

  //! Test function to test parent pointers (from any point back to the root)
  void traceBackToRoot(SgNode *node);

  //! resets pointers in islands of AST code not currently traversed (hidden in
  //! types or arrays of types)
  void validateParentPointersInType(SgType *typeNode, SgNode *previousNode);

  /*! \brief Reset parents of referenced defining and first non-defining
     declaration.

      SgDeclarationStatement objects contain references to the defining and
     first non-defining declarations, both should have their parents set.  In
     general this will be sufficient to set all the associated declarations used
     internally is they are shared.  Declaration statements that are explicit
     forward declarations are however not all referencing the first non-defining
     declaration (since this would violate the rule of uniqueness of statements
     (only enforced within a single scope).
   */
  void validateParentPointersInDeclaration(SgDeclarationStatement *declaration,
                                           SgNode *inputParent);

  /*! \brief Reset parent pointers appearing in subtrees represnting the
     template arguments

      This function traverses the list of template arguments and looks for
     SgNameTypes and reset the parents in their associated declarations.

      \internal This could be eliminated if we were to traverse the template
     arguments (not clear if that is a good idea).
   */
  void validateParentPointersInTemplateArgumentList(
      const SgTemplateArgumentPtrList &templateArgList);
};

/*! \brief This is a top level function not called recursively.

   This function calls resetParentPointer() which is a recursively
   called function which would be difficult to obtain performance
   information from directly.
 */
void topLevelValidateParentPointers(SgNode *node);

/*! \brief This is a top level function not called recursively.

   This function call the traversal to reset the parent pointers of
   data members in class definitions, namespace definitions and global
   scope.
 */
void validateParentPointersOfClassOrNamespaceDeclarations(SgNode *node);

/*! \brief This traversal implements the mechanism to reset all parent pointers
   (back edges in the AST graph)

    This traversal traverses the more of the AST (but is not a memory pool
   traversal) except types and symbols (future versions of the traversal will
   traverse type and symbols).

    \internal This traversal is a demonstration of how to traverse the full AST
   including islands of code not yet handled in the default traversal.  Nested
   traversals are used to implement the traversal of source code (typically
   class definitions) hidden in types.
 */
class ValidateParentPointersOfClassAndNamespaceDeclarations
    : public SgSimpleProcessing {
public:
  //! Required traversal function
  void visit(SgNode *node);
};

// endif for RESET_PARENT_POINTERS_H
#endif
