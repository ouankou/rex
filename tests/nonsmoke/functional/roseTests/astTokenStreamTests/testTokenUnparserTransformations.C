
// This test code tests the unparsing using the token stream using any
// input code and using transformations of variables with a specific suffix.

#include "rose.h"

#include <string>

using namespace std;

class TransformVisitor : public AstSimpleProcessing {
private:
  static const string matchEnding;
  static const size_t matchEndingSize;
  static const string renameEnding;

protected:
  void visit(SgNode *astNode);
};

const string TransformVisitor::matchEnding = "_rename_me";
const size_t TransformVisitor::matchEndingSize = matchEnding.size();
const string TransformVisitor::renameEnding = "_renamed";

void TransformVisitor::visit(SgNode *node) {

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
        string new_name = originalName + renameEnding;

        // SageInterface::set_name(*nameListIterator, originalName +
        // renameEnding);
        SageInterface::set_name(*nameListIterator, new_name);
        printf("variable: new_name = %s \n", new_name.c_str());
      }
    }
  }

  // DQ (2/4/2021): Adding support to rename enum values in SgEnumDeclaration.
  const SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(node);
  if (enumDeclaration != NULL) {
    const SgInitializedNamePtrList &enumerators =
        enumDeclaration->get_enumerators();
    for (SgInitializedNamePtrList::const_iterator nameListIterator =
             enumerators.begin();
         nameListIterator != enumerators.end(); nameListIterator++) {
      string originalName = ((*nameListIterator)->get_name()).getString();
      // Rename any variable, whose name ends with matchEnding.
      if (originalName.size() >= matchEndingSize &&
          originalName.compare(originalName.size() - matchEndingSize,
                               matchEndingSize, matchEnding) == 0) {
        string new_name = originalName + renameEnding;

        // SageInterface::set_name(*nameListIterator, originalName +
        // renameEnding);
        SageInterface::set_name(*nameListIterator, new_name);
        printf("enumerator: new_name = %s \n", new_name.c_str());
      }
    }
  }

  // DQ (2/4/2021): Adding support to rename enum values in SgEnumDeclaration.
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
  if (functionDeclaration != NULL) {
    // SgFunctionDeclaration*
    // SageInterface::replaceFunctionDefinitionWithDeclaration(
    // SgFunctionDeclaration *functionDefinition)

    string originalName = functionDeclaration->get_name();

    string matchEnding = "_make_prototype";
    size_t matchEndingSize = matchEnding.size();
    // Rename any variable, whose name ends with matchEnding.
    if (originalName.size() >= matchEndingSize &&
        originalName.compare(originalName.size() - matchEndingSize,
                             matchEndingSize, matchEnding) == 0) {
      printf("Calling "
             "SageInterface::"
             "replaceFunctionDefinitionWithDeclaration(): "
             "functionDeclaration = %p \n",
             functionDeclaration);

      SgDeclarationStatement *sourceReplacement =
          SageInterface::replaceFunctionDefinitionWithDeclaration(
              functionDeclaration);

      printf("Done: calling "
             "SageInterface::"
             "replaceFunctionDefinitionWithDeclaration(): "
             "functionDeclaration = %p sourceReplacement = %p \n",
             functionDeclaration, sourceReplacement);
    }
  }

  // DQ (9/20/2018): If we are using the token based unparsing, then any change
  // to the SgInitializedName must also touch the associated variable reference
  // expressions.  I don't think there is a good way to automate this except to
  // put this support into the SageInterface::set_name() function (which we
  // could do later).

  // If we find a variable in a SgExprStatement then we want to be able to
  // outline it.
  SgVarRefExp *varRefExp = isSgVarRefExp(node);
  if (varRefExp != NULL) {
    SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
    ROSE_ASSERT(variableSymbol != NULL);
    string originalName = variableSymbol->get_name().str();
  }
}

int main(int argc, char *argv[]) {
  ROSE_ASSERT(argc > 1);

  SgProject *project = frontend(argc, argv);

  // AstTests::runAllTests(project);

  TransformVisitor transformation;
  transformation.traverse(project, preorder);

  return backend(project);
}
