#include "rose.h"

#include "UnparseHeadersTransformVisitor.h"

const string UnparseHeadersTransformVisitor::matchEnding = "_rename_me";
const size_t UnparseHeadersTransformVisitor::matchEndingSize =
    matchEnding.size();
const string UnparseHeadersTransformVisitor::renameEnding = "_renamed";
const string UnparseHeadersTransformVisitor::rewriteAssignmentMarker =
    "_rewrite_assignment";

void UnparseHeadersTransformVisitor::visit(SgNode *node) {
  // Use a pointer to a constant SgVariableDeclaration to be able to call the
  // constant getter variableDeclaration -> get_variables(), which does not mark
  // the node as modified.
  const SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(node);
  if (variableDeclaration != NULL) {
    const SgInitializedNamePtrList &nameList =
        variableDeclaration->get_variables();
    for (SgInitializedNamePtrList::const_iterator nameListIterator =
             nameList.begin();
         nameListIterator != nameList.end(); nameListIterator++) {
      string originalName = ((*nameListIterator)->get_name()).getString();

      // Rename any variable, whose name ends with matchEnding.
      if (originalName.size() >= matchEndingSize &&
          originalName.compare(originalName.size() - matchEndingSize,
                               matchEndingSize, matchEnding) == 0) {
        SageInterface::set_name(*nameListIterator, originalName + renameEnding);
      }
    }
  }

  // DQ (9/20/2018): If we are using the token based unparsing, then any change
  // to the SgInitializedName must also touch the associated variable reference
  // expressions.  I don't think there is a good way to automate this except to
  // put this support into the SageInterface::set_name() function (which we
  // could do later).

  SgVarRefExp *varRefExp = isSgVarRefExp(node);
  if (varRefExp != NULL) {
    const string originalName = varRefExp->get_symbol()->get_name().getString();
    if (originalName.find(rewriteAssignmentMarker) != string::npos) {
      SgAssignOp *assignOp = isSgAssignOp(varRefExp->get_parent());
      if (assignOp != NULL && assignOp->get_lhs_operand() == varRefExp) {
        SgIntVal *intVal = isSgIntVal(assignOp->get_rhs_operand());
        if (intVal != NULL) {
          SageInterface::replaceExpression(intVal, SageBuilder::buildIntVal(2),
                                           false);
        }

        SgStatement *enclosingStatement =
            SageInterface::getEnclosingStatement(varRefExp);
        if (enclosingStatement != NULL) {
          enclosingStatement->setTransformation();
        }
      }
    }
  }
}
