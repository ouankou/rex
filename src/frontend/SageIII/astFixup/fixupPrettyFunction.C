#include "fixupPrettyFunction.h"

#include "sage3basic.h"

void fixupPrettyFunctionVariables(SgNode *node) {
  // PP (7/28/22): fix RC-1370: avoid updating AST nodes that look like
  //               legacy frontend's __PRETTY_FUNCTION__ representation.
  TimingPerformance timer("Fixup Pretty Print variables:");

  // This simplifies how the traversal is called!
  FixupPrettyFunctionVariables astFixupTraversal;

  FixupPrettyFunctionVariablesInheritedAttribute inheritedAttribute;

  astFixupTraversal.traverse(node, inheritedAttribute);
}

FixupPrettyFunctionVariablesInheritedAttribute::
    FixupPrettyFunctionVariablesInheritedAttribute()
    : functionDeclaration(NULL) {
  // Default Constructor
}

FixupPrettyFunctionVariablesInheritedAttribute::
    FixupPrettyFunctionVariablesInheritedAttribute(
        const FixupPrettyFunctionVariablesInheritedAttribute &X) {
  // NOTE: This copy constructor is required to propegate the value of
  // "functionDeclaration" member data across copies.
  functionDeclaration = X.functionDeclaration;
}

FixupPrettyFunctionVariablesInheritedAttribute
FixupPrettyFunctionVariables::evaluateInheritedAttribute(
    SgNode *node,
    FixupPrettyFunctionVariablesInheritedAttribute inheritedAttribute) {

  // We are looking for the variable "__assert_fail" which the legacy frontend
  // has used instead of "__PRETTY_FUNCTION__". An example from CPP is:
  // ((!"Inside of struct Y::foo1 (using assert)") ? static_cast<void> (0) :
  // (__assert_fail ("!\"Inside of struct Y::foo1 (using assert)\"",
  // "/home/dquinlan/ROSE/git-dq-main-rc/tests/nonsmoke/functional/CompileTests/Cxx_tests/test2010_07.C",
  // 21, __PRETTY_FUNCTION__), static_cast<void> (0))); The legacy frontend will
  // substitute "__PRETTY_FUNCTION__" with "__assert_fail", as documented in its
  // change log. So we have to back this out so that we can allow ROSE to
  // generate the same code that GNU would (and other compilers).

  // Se we are looking for every function call.
  SgFunctionCallExp *functionCallExpression = isSgFunctionCallExp(node);
  if (functionCallExpression != NULL) {
    // Now we know that we are in a function call and we find the function
    // declaration.
    SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(
        functionCallExpression->getAssociatedFunctionDeclaration());
    // ROSE_ASSERT(functionDeclaration != NULL);

    if (functionDeclaration != NULL) {
      // Set the function declaration in the inherited attribute.
      inheritedAttribute.functionDeclaration = functionDeclaration;
    }
  }

  // Then if we are in a function call (e.g. evaluating it's function arguments)
  // the inheritect attribute will be set.
  if (inheritedAttribute.functionDeclaration != NULL) {
    // Now we are interested in finding variable names (in variable reference
    // expressions).
    SgVarRefExp *variableReferenceExpression = isSgVarRefExp(node);
    if (variableReferenceExpression != NULL) {
      SgVariableSymbol *variableSymbol =
          isSgVariableSymbol(variableReferenceExpression->get_symbol());
      SgInitializedName *initializedName = variableSymbol->get_declaration();

      SgName functionName = inheritedAttribute.functionDeclaration->get_name();
      // if (initializedName != NULL &&
      // initializedName->get_file_info()->isCompilerGenerated() == true &&
      // initializedName->get_name() == "__PRETTY_FUNCTION__")
      if (initializedName != NULL &&
          initializedName->get_file_info()->isCompilerGenerated() == true &&
          initializedName->get_name() == functionName)
      // if (initializedName != NULL && initializedName->get_name() ==
      // functionName)
      {
        // Now change the name to what is is supposed to be (before
        // legacy frontend normalized it).
        initializedName->set_name("__PRETTY_FUNCTION__");
      }
    }
  }

  return inheritedAttribute;
}
