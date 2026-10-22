// Author: Dan Quinlan

// Report global variables referenced in lambda functions.

#include "rose.h"

struct InheritedAttribute {
  bool inLambdaFunction = false;
  SgFunctionDeclaration *outerFunction = nullptr;
};

struct SynthesizedAttribute {};

class Traversal : public SgTopDownBottomUpProcessing<InheritedAttribute,
                                                     SynthesizedAttribute> {
public:
  std::set<SgInitializedName *> globalVariables;
  InheritedAttribute
  evaluateInheritedAttribute(SgNode *astNode,
                             InheritedAttribute inheritedAttribute);
  SynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *astNode, InheritedAttribute inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);
};

InheritedAttribute
Traversal::evaluateInheritedAttribute(SgNode *astNode,
                                      InheritedAttribute inheritedAttribute) {
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(astNode);
  if (functionDeclaration != nullptr) {
    if (SageInterface::isLambdaFunction(functionDeclaration))
      inheritedAttribute.inLambdaFunction = true;
    else if (!inheritedAttribute.inLambdaFunction) {
      auto *classScope = isSgClassDefinition(functionDeclaration->get_scope());
      if (classScope == nullptr ||
          classScope->get_declaration()->get_class_type() !=
              SgClassDeclaration::e_lambda_closure)
        inheritedAttribute.outerFunction = functionDeclaration;
    }
  }

  if (inheritedAttribute.inLambdaFunction) {
    SgVarRefExp *varRefExp = isSgVarRefExp(astNode);

    if (varRefExp != NULL) {
      SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
      ROSE_ASSERT(variableSymbol != NULL);
      SgInitializedName *initializedName = variableSymbol->get_declaration();
      ROSE_ASSERT(initializedName != NULL);

      // Check that this is a variable from global scope.
      SgScopeStatement *scope = initializedName->get_scope();
      if (isSgGlobal(scope) != NULL) {
        globalVariables.insert(initializedName);
      }
    }

    // Look for declarations of arrays and check the array bounds for global
    // variables.
    SgInitializedName *initializedName = isSgInitializedName(astNode);
    if (initializedName != NULL) {
      // Need to look for cases of typedefed types.
      SgType *type = initializedName->get_type();
      ROSE_ASSERT(type != NULL);

      SgArrayType *arrayType = isSgArrayType(type);
      if (arrayType != NULL) {
        SgExpression *indexExpression = arrayType->get_index();

        Traversal nestedTraversal;
        nestedTraversal.traverse(indexExpression, inheritedAttribute);

        globalVariables.insert(nestedTraversal.globalVariables.begin(),
                               nestedTraversal.globalVariables.end());
      }
    }
  }

  return inheritedAttribute;
}

SynthesizedAttribute Traversal::evaluateSynthesizedAttribute(
    SgNode *astNode, InheritedAttribute inheritedAttribute,
    SynthesizedAttributesList /* childAttributes */) {
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(astNode);
  if (functionDeclaration != nullptr &&
      functionDeclaration == inheritedAttribute.outerFunction) {
    if (globalVariables.empty() == false) {
      printf("file: %s function: %s global variables: ",
             functionDeclaration->get_file_info()->get_filenameString().c_str(),
             functionDeclaration->get_name().str());
      for (SgInitializedName *initializedName : globalVariables) {
        printf("%s ", initializedName->get_name().str());
      }
      printf("\n");

      globalVariables.clear();
    }
  }

  return {};
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);
  if (project->get_frontendErrorCode() != 0) {
    const int status = frontendExitStatus(project);
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    return status;
  }

  InheritedAttribute inheritedAttribute;

  Traversal astTraversal;

  astTraversal.traverseInputFiles(project, inheritedAttribute);

  const int status = backendCompilesUsingOriginalInputFile(project);
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return status;
}
