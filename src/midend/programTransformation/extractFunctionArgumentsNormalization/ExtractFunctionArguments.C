#include "ExtractFunctionArguments.h"

#include "SingleStatementToBlockNormalization.h"

#include "functionEvaluationOrderTraversal.h"

using namespace std;

namespace {

bool invokesOverloadedOperator(SgFunctionCallExp *functionCall) {
  if (functionCall == NULL) {
    return false;
  }

  if (functionCall->get_uses_operator_syntax()) {
    return true;
  }

  SgFunctionDeclaration *functionDeclaration =
      SageInterface::getFunctionDeclaration(functionCall);
  return functionDeclaration != NULL &&
         functionDeclaration->get_specialFunctionModifier().isOperator();
}

bool containsOverloadedOperatorCall(SgExpression *expression) {
  if (expression == NULL) {
    return false;
  }

  Rose_STL_Container<SgNode *> functionCalls =
      NodeQuery::querySubTree(expression, V_SgFunctionCallExp);
  for (SgNode *node : functionCalls) {
    if (invokesOverloadedOperator(isSgFunctionCallExp(node))) {
      return true;
    }
  }

  return false;
}

bool sourceSupportsDeducedTemporaryType(SgNode *node) {
  SgSourceFile *source = SageInterface::getEnclosingSourceFile(node);
  if (source == nullptr || !source->get_Cxx_only()) {
    return false;
  }

  return source->get_Cxx11_only() || source->get_Cxx11_gnu_only() ||
         source->get_Cxx14_only() || source->get_Cxx14_gnu_only() ||
         source->get_Cxx17_only() || source->get_Cxx17_gnu_only() ||
         source->get_Cxx20_only() || source->get_Cxx20_gnu_only() ||
         source->get_Cxx23_only() || source->get_Cxx23_gnu_only() ||
         source->get_Cxx26_only() || source->get_Cxx26_gnu_only();
}

SgVariableDeclaration *createTempVariableDeclaration(SgExpression *expression,
                                                     SgScopeStatement *scope) {
  ROSE_ASSERT(expression != NULL);
  ROSE_ASSERT(scope != NULL);

  SgType *expressionType = expression->get_type();
  SgType *variableType = expressionType;

  // Match the existing extraction behavior for array arguments.
  if (SgArrayType *arrayType = isSgArrayType(expressionType)) {
    if (SgArrayType *strippedArrayType =
            isSgArrayType(arrayType->stripType(SgType::STRIP_TYPEDEF_TYPE))) {
      SgType *strippedArrayBaseType = strippedArrayType->get_base_type();
      variableType = SageBuilder::buildPointerType(strippedArrayBaseType);
    }
  }

  std::string name = SageInterface::generateUniqueVariableName(scope);

  SgVariableDeclaration *tempVarDeclaration =
      SageBuilder::buildUnpublishedVariableDeclaration(name, variableType,
                                                       nullptr, scope);
  ROSE_ASSERT(tempVarDeclaration != NULL);
  SgInitializedName *temporary = tempVarDeclaration->get_decl_item(name);
  if (temporary == nullptr || temporary->get_type() != variableType ||
      tempVarDeclaration->get_parent() != nullptr ||
      scope->find_symbol_from_declaration(temporary) != nullptr) {
    fprintf(stderr,
            "REX_NORMALIZATION_INVARIANT[temporary-type]: declaration=%p "
            "name=%s is not one detached unpublished semantic temporary\n",
            static_cast<void *>(tempVarDeclaration), name.c_str());
    ROSE_ABORT();
  }

  // A converted argument can carry an implementation-owned typedef spelling
  // (for example vector<T>::size_type), and a dependent call can have Clang's
  // explicit __dependent_type semantic placeholder.  Neither is a lexical
  // type-id at the insertion scope.  In C++11 and later, retain the exact
  // semantic type on the initialized name while recording a corresponding
  // deduced source type (`auto`, `auto&`, or `auto&&`) as its independent
  // source surface. The initializer then performs language deduction at the
  // new declaration site without manufacturing a name that was never visible
  // there or discarding the expression's reference category.
  if (sourceSupportsDeducedTemporaryType(expression)) {
    SgType *sourceType = SageBuilder::buildAutoType();
    if (isSgReferenceType(variableType) != nullptr) {
      sourceType = SageBuilder::buildReferenceType(sourceType);
    } else if (isSgRvalueReferenceType(variableType) != nullptr) {
      sourceType = SageBuilder::buildRvalueReferenceType(sourceType);
    }
    temporary->set_auto_decltype(sourceType);
    if (temporary->get_auto_decltype() != sourceType ||
        temporary->get_type() != variableType) {
      fprintf(stderr,
              "REX_NORMALIZATION_INVARIANT[temporary-type]: declaration=%p "
              "name=%s failed to preserve distinct semantic and source type "
              "surfaces\n",
              static_cast<void *>(tempVarDeclaration), name.c_str());
      ROSE_ABORT();
    }
  }

  return tempVarDeclaration;
}

} // namespace

/** Performs the function argument extraction on all function calls in the given
 * subtree of the AST. */
/** It does not do transofrmations in places where it is not safe. If you pass
 * doUnsafeNormalization= true, we will normalize all callsites ignoring the
 * safety (Suggested by Markus Schordan) */

void ExtractFunctionArguments::NormalizeTree(SgNode *tree,
                                             bool doUnsafeNormalization) {
  // First Normalize each single statememnt body into a block body
  // This transformation is necessary to provide scope to newly created
  // temporaries Note: if this transofrmation is already done, it is benign to
  // rerun it and this would act as a NOP transformation.

  SingleStatementToBlockNormalizer singleStatementToBlockNormalizer;
  singleStatementToBlockNormalizer.Normalize(tree);

  // Obtain functions that are candidates for normalizing (first) and those
  // which are not condidates (second) non candidates are those which are
  // present in places from where it is unsafe to lift: e.g., :
  // while(Foo(Bar())){}
  this->functionCalls =
      FunctionEvaluationOrderTraversal::GetFunctionCalls(tree);

  // Normalize each function from safe place
  for (const FunctionCallInfo &functionCallInfo : this->functionCalls.first) {
    RewriteFunctionCallArguments(functionCallInfo);
  }

  // if doUnsafeNormalization is true, we will normalize calls which are not
  // safe
  if (doUnsafeNormalization) {
    for (const FunctionCallInfo &functionCallInfo :
         this->functionCalls.second) {
      RewriteFunctionCallArguments(functionCallInfo);
    }
  }
}

/*
    return a vector of temporaries introduced during the translation process
 */
std::vector<SgVariableDeclaration *>
ExtractFunctionArguments::GetTemporariesIntroduced() {
  return this->temporariesIntroduced;
}

/** Returns true if the given expression refers to a variable. This could
 * include using the dot and arrow operator to access member variables. A comma
 * op counts as a variable references if all its members are variable references
 * (not just the last expression in the list). */
bool isVariableReference(SgExpression *expression) {
  if (isSgVarRefExp(expression)) {
    return true;
  } else if (isSgThisExp(expression)) {
    return true;
  } else if (isSgDotExp(expression)) {
    SgDotExp *dotExpression = isSgDotExp(expression);
    return isVariableReference(dotExpression->get_lhs_operand()) &&
           isVariableReference(dotExpression->get_rhs_operand());
  } else if (isSgArrowExp(expression)) {
    SgArrowExp *arrowExpression = isSgArrowExp(expression);
    return isVariableReference(arrowExpression->get_lhs_operand()) &&
           isVariableReference(arrowExpression->get_rhs_operand());
  } else if (isSgCommaOpExp(expression)) {
    // Comma op where both the lhs and th rhs are variable references.
    // The lhs would be semantically meaningless since it doesn't have any side
    // effects
    SgCommaOpExp *commaOp = isSgCommaOpExp(expression);
    return isVariableReference(commaOp->get_lhs_operand()) &&
           isVariableReference(commaOp->get_rhs_operand());
  } else if (isSgPointerDerefExp(expression) || isSgCastExp(expression) ||
             isSgAddressOfOp(expression)) {
    return isVariableReference(isSgUnaryOp(expression)->get_operand());
  } else {
    return false;
  }
}

/* Given the expression which is the argument to a function call, returns true
   if that expression is trivial. Trivial expressions are those which are simple
   variable references or constants.
 */

bool ExtractFunctionArguments::IsFunctionArgumentTrivial(
    SgExpression *argument) {
  while ((isSgPointerDerefExp(argument) || isSgCastExp(argument) ||
          isSgAddressOfOp(argument))) {
    argument = isSgUnaryOp(argument)->get_operand();
  }

  SgArrowExp *arrowExp = isSgArrowExp(argument);
  if (arrowExp && isSgThisExp(arrowExp->get_lhs_operand()))
    return true;

  // We don't lift these simple types
  if (isVariableReference(argument) || isSgValueExp(argument))
    return true;

  return false;
}

/* Given a vector of function call sites returns true if every argument of every
   function call is a trivial expression (IsFunctionArgumentTrivial). Such
   functions don't need to be normalized.
 */
bool ExtractFunctionArguments::AreAllFunctionCallsTrivial(
    std::vector<FunctionCallInfo> functions) {
  for (FunctionCallInfo functionCallInfo : functions) {
    for (SgExpression *arg :
         functionCallInfo.functionCall->get_args()->get_expressions()) {
      if (!IsFunctionArgumentTrivial(arg)) {
        return false;
      }
    }
  }
  return true;
}

/* Given a vector of function call sites, returns true if every argument
   at every function call site is either trivial (IsFunctionArgumentTrivial) or
   can be normalized (FunctionArgumentCanBeNormalized).
 */
bool ExtractFunctionArguments::AreAllFunctionCallsNormalizable(
    std::vector<FunctionCallInfo> functions) {
  for (FunctionCallInfo functionCallInfo : functions) {
    for (SgExpression *arg :
         functionCallInfo.functionCall->get_args()->get_expressions()) {
      if (!IsFunctionArgumentTrivial(arg) &&
          !FunctionArgumentCanBeNormalized(arg)) {
        return false;
      }
    }
  }
  return true;
}

/*
    IsNormalized: Given a subtree, returns true if every argument of every
   function call is a trivial arguemnt.
*/

bool ExtractFunctionArguments::IsNormalized(SgNode *tree) {
  // Obtain functions that are candidates for normalizing (first) and those
  // which are not condidates (second) non candidates are those which are
  // present in places from where it is unsafe to lift: e.g., :
  // while(Foo(Bar())){}
  pair<std::vector<FunctionCallInfo>, std::vector<FunctionCallInfo>> fCalls =
      FunctionEvaluationOrderTraversal::GetFunctionCalls(tree);

  // Make sure all arguemnts of all SAFE place functions calls are trivial
  if (!AreAllFunctionCallsTrivial(fCalls.first))
    return false;

  // Make sure all arguemnts of all NON-SAFE place functions calls are also
  // trivial
  if (!AreAllFunctionCallsTrivial(fCalls.second))
    return false;

  // All are trivial
  return true;
}

/*
    IsNormalizable: Given a subtree, returns true if every argument of every
   function call is a either trivial arguemnt or present in a SAFE place from
   where lifting is possible.
*/

bool ExtractFunctionArguments::IsNormalizable(SgNode *tree) {
  // Obtain functions that are candidates for normalizing (first) and those
  // which are not condidates (second) non candidates are those which are
  // present in places from where it is unsafe to lift: e.g., :
  // while(Foo(Bar())){}
  pair<std::vector<FunctionCallInfo>, std::vector<FunctionCallInfo>> fCalls =
      FunctionEvaluationOrderTraversal::GetFunctionCalls(tree);

  // Make sure all arguemnts of all SAFE place functions calls are either
  // trivial or normalizable
  if (!AreAllFunctionCallsNormalizable(fCalls.first))
    return false;

  // Must be trivail in NON SAFE places
  if (!AreAllFunctionCallsTrivial(fCalls.second))
    return false;

  // We can normalize
  return true;
}

/** Given the information about a function call (obtained through a traversal),
 * extract its arguments into temporary variables where it is necessary. Returns
 * true on success, false on failure (unsupported code). */
void ExtractFunctionArguments::RewriteFunctionCallArguments(
    const FunctionCallInfo &functionCallInfo) {
  SgFunctionCallExp *functionCall = functionCallInfo.functionCall;

  SgExprListExp *functionArgs = functionCall->get_args();
  ROSE_ASSERT(functionArgs != NULL);

  SgExpressionPtrList argumentList = functionArgs->get_expressions();

  // We also normalize the caller if the function called is a member function.
  if (SgBinaryOp *binExp = isSgBinaryOp(functionCall->get_function())) {
    SgExpression *receiver = binExp->get_lhs_operand();
    if (!containsOverloadedOperatorCall(receiver)) {
      argumentList.push_back(receiver);
    }
  }

  // Go over all the function arguments, pull them out

  for (SgExpression *arg : argumentList) {
    // No need to pull out parameters that are not complex expressions and
    // thus don't have side effects
    if (!FunctionArgumentNeedsNormalization(arg))
      continue;

    // Normalize the insertion boundary before building a scope-sensitive
    // declaration.  A condition statement, for example, has no sibling
    // declaration position: its enclosing control statement is the insertion
    // anchor and the anchor's enclosing block is the declaration scope.
    SgStatement *insertionLocation =
        functionCallInfo.tempVarDeclarationLocation;
    SgScopeStatement *scope = nullptr;
    switch (functionCallInfo.tempVarDeclarationInsertionMode) {
    case FunctionCallInfo::INSERT_BEFORE:
      insertionLocation =
          SageInterface::prepareStatementInsertionAnchor(insertionLocation);
      scope = insertionLocation->get_scope();
      break;

    case FunctionCallInfo::APPEND_SCOPE:
      if (SgScopeStatement *locationScope =
              isSgScopeStatement(insertionLocation)) {
        scope = locationScope;
      } else if (SageInterface::isBodyStatement(insertionLocation)) {
        scope =
            SageInterface::makeSingleStatementBodyToBlock(insertionLocation);
        insertionLocation = scope;
      } else {
        scope = isSgScopeStatement(insertionLocation->get_parent());
      }
      break;

    case FunctionCallInfo::INVALID:
    default:
      fprintf(stderr,
              "REX_NORMALIZATION_INVARIANT[temporary-insertion-mode]: "
              "function call=%p has no exact temporary insertion mode\n",
              static_cast<void *>(functionCall));
      ROSE_ABORT();
    }
    if (scope == nullptr) {
      fprintf(stderr,
              "REX_NORMALIZATION_INVARIANT[temporary-insertion-scope]: "
              "location=%p/%s has no exact declaration publication scope\n",
              static_cast<void *>(insertionLocation),
              insertionLocation != nullptr
                  ? insertionLocation->class_name().c_str()
                  : "null");
      ROSE_ABORT();
    }

    // DQ (3/1/2015): Added initialization to these local variables.
    // SgVariableDeclaration* tempVarDeclaration;
    // SgExpression* tempVarReference;
    SgVariableDeclaration *tempVarDeclaration = NULL;
    SgExpression *tempVarReference = NULL;

    // DQ (3/1/2015): Avoid normalization of function arguments of type
    // SgFunctionType (see inputBug317.C). This might be an issue more narrowly
    // defined for compiler-generated functions, this is less clear.
    SgFunctionType *functionType = isSgFunctionType(arg->get_type());
    if (functionType == NULL) {
      tempVarDeclaration = createTempVariableDeclaration(arg, scope);

      // DQ (3/1/2015): This is not always true (see inputBug317.C).
      ROSE_ASSERT(tempVarDeclaration->get_definition(0) != NULL);
      ROSE_ASSERT(
          isSgVariableDefinition(tempVarDeclaration->get_definition()) != NULL);

      // Insert the temporary variable declaration
      InsertStatement(tempVarDeclaration, insertionLocation, functionCallInfo);
      SgInitializedName *publishedName =
          SageInterface::getFirstInitializedName(tempVarDeclaration);
      SgVariableSymbol *publishedSymbol =
          publishedName != nullptr
              ? isSgVariableSymbol(
                    scope->find_symbol_from_declaration(publishedName))
              : nullptr;
      if (tempVarDeclaration->get_parent() != scope ||
          publishedName == nullptr || publishedName->get_scope() != scope ||
          publishedSymbol == nullptr ||
          publishedSymbol->get_declaration() != publishedName) {
        fprintf(stderr,
                "REX_NORMALIZATION_INVARIANT[temporary-publication]: "
                "declaration=%p scope=%p lacks one exact lexical and symbol "
                "owner after insertion\n",
                static_cast<void *>(tempVarDeclaration),
                static_cast<void *>(scope));
        ROSE_ABORT();
      }
      tempVarReference = SageBuilder::buildVarRefExp(tempVarDeclaration);
      if (tempVarReference == nullptr ||
          isSgVarRefExp(tempVarReference) == nullptr ||
          isSgVarRefExp(tempVarReference)->get_symbol() != publishedSymbol) {
        fprintf(stderr,
                "REX_NORMALIZATION_INVARIANT[temporary-reference]: "
                "declaration=%p did not produce a reference to its exact "
                "published symbol\n",
                static_cast<void *>(tempVarDeclaration));
        ROSE_ABORT();
      }

      // remember the introduced temp so that it can be queried
      temporariesIntroduced.push_back(tempVarDeclaration);

      // Replace the argument with the new temporary variable
      SageInterface::replaceExpression(arg, tempVarReference, true);
      SgInitializedNamePtrList &declared_vars =
          tempVarDeclaration->get_variables();
      ROSE_ASSERT(declared_vars.size() == 1);
      SgInitializedName *temp_name = declared_vars.front();
      ROSE_ASSERT(temp_name != nullptr);
      ROSE_ASSERT(temp_name->get_initializer() == nullptr);
      SgAssignInitializer *initializer =
          SageBuilder::buildAssignInitializer(arg, temp_name->get_type());
      temp_name->set_initializer(initializer);
      initializer->set_parent(temp_name);

      tempVarReference->markAsModified();
      if (SgStatement *enclosing_statement =
              SageInterface::getEnclosingStatement(tempVarReference)) {
        enclosing_statement->markAsModified();
      }
    } else {
      printf("In ExtractFunctionArguments::RewriteFunctionCallArguments(): "
             "Skipping normalization of function arguments of type "
             "SgFunctionType \n");
    }
  }
}

// If we have a limitation in normalizing the function return false
bool ExtractFunctionArguments::FunctionArgumentCanBeNormalized(
    SgExpression *argument) {
  if (containsOverloadedOperatorCall(argument)) {
    return false;
  }

  while ((isSgPointerDerefExp(argument) || isSgCastExp(argument) ||
          isSgAddressOfOp(argument))) {
    argument = isSgUnaryOp(argument)->get_operand();
  }
  // Don't include SgConstructorInitializer since it will be called even on the
  // temporary, so avoid double copy.
  // A direct callable name is already one side-effect-free argument.  This
  // includes dependent overload sets in template bodies: they deliberately
  // have no standalone object type, so manufacturing a variable declaration
  // from the reference would turn `endl` into the invalid type spelling
  // `endl temporary`.  Preserve the typed callable edge and let overload
  // resolution remain at the original call site.
  if (isSgFunctionRefExp(argument) || isSgTemplateFunctionRefExp(argument) ||
      isSgMemberFunctionRefExp(argument) ||
      isSgTemplateMemberFunctionRefExp(argument) ||
      isSgNonrealRefExp(argument) || isSgConstructorInitializer(argument))
    return false;

  // Unknow Template type expressions can't be normalized.
  if (isSgTypeUnknown(argument->get_type()) ||
      isSgMemberFunctionType(argument->get_type())) {
    // printf("\n Skipping over SgTypeUnknown/SgMemberFunctionType  expr");
    return false;
  }

  return true;
}

/** Given the expression which is the argument to a function call, returns true
 * if that expression should be pulled out into a temporary variable on a
 * separate line. E.g. if the expression contains a function call, it needs to
 * be normalized, while if it is a constant, there is no need to change it. */

bool ExtractFunctionArguments::FunctionArgumentNeedsNormalization(
    SgExpression *argument) {
  // Trivial argument don't need to be listed

  if (IsFunctionArgumentTrivial(argument))
    return false;

  // true/false based on our ability to normalize the argument
  return FunctionArgumentCanBeNormalized(argument);
}

/** Returns true if any of the arguments of the given function call will need to
 * be extracted. */
bool ExtractFunctionArguments::FunctionArgsNeedNormalization(
    SgExprListExp *functionArgs) {
  ROSE_ASSERT(functionArgs != NULL);
  SgExpressionPtrList &argumentList = functionArgs->get_expressions();

  for (SgExpression *functionArgument : argumentList) {
    if (FunctionArgumentNeedsNormalization(functionArgument))
      return true;
  }
  return false;
}

/** Returns true if any function calls in the given subtree will need to be
 * instrumented. (to extract function arguments). */
bool ExtractFunctionArguments::SubtreeNeedsNormalization(SgNode *top) {
  ROSE_ASSERT(top != NULL);
  Rose_STL_Container<SgNode *> functionCalls =
      NodeQuery::querySubTree(top, V_SgFunctionCallExp);
  for (Rose_STL_Container<SgNode *>::const_iterator iter =
           functionCalls.begin();
       iter != functionCalls.end(); iter++) {
    SgExpression *functionCall = isSgFunctionCallExp(*iter);
    ROSE_ASSERT(functionCall != NULL);
    if (FunctionArgumentNeedsNormalization(functionCall))
      return true;
  }

  return false;
}

/** Insert a new statement in the specified location. The actual insertion can
 * occur either before or after the location depending on the insertion mode. */
void ExtractFunctionArguments::InsertStatement(
    SgStatement *newStatement, SgStatement *location,
    const FunctionCallInfo &insertionMode) {
  switch (insertionMode.tempVarDeclarationInsertionMode) {
  case FunctionCallInfo::INSERT_BEFORE:
    SageInterface::insertStatementBefore(location, newStatement);
    break;
  case FunctionCallInfo::APPEND_SCOPE: {
    SgScopeStatement *scopeStatement = isSgScopeStatement(location);
    if (scopeStatement == NULL) {
      // scopeStatement =
      // isSgScopeStatement(SageInterface::ensureBasicBlockAsParent(location));
      if (SageInterface::isBodyStatement(
              location)) // if the location is a single body statement (not a
                         // basic block) at this point
        scopeStatement =
            SageInterface::makeSingleStatementBodyToBlock(location);
      else
        scopeStatement = isSgScopeStatement(location->get_parent());
    }
    ROSE_ASSERT(scopeStatement != NULL);

    SageInterface::appendStatement(newStatement, scopeStatement);
    break;
  }
  case FunctionCallInfo::INVALID:
  default:
    ROSE_ABORT();
  }
}

/** Traverses the subtree of the given AST node and finds all function calls in
 * function-evaluation order. */
/*static*/ pair<std::vector<FunctionCallInfo>, std::vector<FunctionCallInfo>>
FunctionEvaluationOrderTraversal::GetFunctionCalls(SgNode *root) {
  FunctionEvaluationOrderTraversal t;
  FunctionCallInheritedAttribute rootAttribute;
  t.traverse(root, rootAttribute);

  return pair<std::vector<FunctionCallInfo>, std::vector<FunctionCallInfo>>(
      t.normalizableFunctionCalls, t.nonNormalizableFunctionCalls);
}
