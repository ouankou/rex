#include "rose.h"

#include <algorithm>
#include <string>

namespace {
bool containsMarker(const PreprocessingInfo *record, const char *marker) {
  return record != nullptr && marker != nullptr &&
         record->getString().find(marker) != std::string::npos;
}

bool samePhysicalFile(const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
  return lhs != nullptr && rhs != nullptr && lhs->get_physical_file_id() >= 0 &&
         rhs->get_physical_file_id() >= 0 && lhs->isSameFile(*rhs) &&
         lhs->get_physical_file_occurrence_id() ==
             rhs->get_physical_file_occurrence_id();
}

bool locationLeq(const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
  ROSE_ASSERT(samePhysicalFile(lhs, rhs));
  return lhs->get_line() < rhs->get_line() ||
         (lhs->get_line() == rhs->get_line() &&
          lhs->get_col() <= rhs->get_col());
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgInitializedName *resultName = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_name() == "result") {
      ROSE_ASSERT(resultName == nullptr);
      resultName = candidate;
    }
  }
  ROSE_ASSERT(resultName != nullptr);

  SgReturnStmt *initializerDirectiveOwner = nullptr;
  SgLocatedNode *ctorDirectiveOwner = nullptr;
  PreprocessingInfo *initializerLambdaDefine = nullptr;
  PreprocessingInfo *ctorInitializerDefine = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *records =
        located->getAttachedPreprocessingInfo();
    if (records == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *records) {
      const bool isInitializerMarker =
          containsMarker(record, "REX_FRONTEND_LAMBDA_INCREMENT");
      const bool isCtorMarker =
          containsMarker(record, "REX_FRONTEND_CTOR_INCREMENT");
      if (!isInitializerMarker && !isCtorMarker) {
        continue;
      }
      ROSE_ASSERT(record->getAttachedOwner() == located);
      ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
      ROSE_ASSERT(record->getTypeOfDirective() ==
                  PreprocessingInfo::CpreprocessorDefineDeclaration);
      if (isInitializerMarker) {
        ROSE_ASSERT(initializerLambdaDefine == nullptr);
        SgReturnStmt *owner = isSgReturnStmt(located);
        ROSE_ASSERT(owner != nullptr);
        initializerLambdaDefine = record;
        initializerDirectiveOwner = owner;
      } else {
        ROSE_ASSERT(ctorInitializerDefine == nullptr);
        ctorInitializerDefine = record;
        ctorDirectiveOwner = located;
      }
    }
  }
  ROSE_ASSERT(initializerLambdaDefine != nullptr);
  ROSE_ASSERT(initializerDirectiveOwner != nullptr);
  ROSE_ASSERT(ctorInitializerDefine != nullptr);
  ROSE_ASSERT(ctorDirectiveOwner != nullptr);

  auto validateLambdaOwner = [&](SgReturnStmt *owner,
                                 PreprocessingInfo *record) {
    SgBasicBlock *lambdaBody = isSgBasicBlock(owner->get_parent());
    ROSE_ASSERT(lambdaBody != nullptr);
    SgFunctionDefinition *lambdaDefinition =
        isSgFunctionDefinition(lambdaBody->get_parent());
    ROSE_ASSERT(lambdaDefinition != nullptr);
    ROSE_ASSERT(lambdaDefinition->get_body() == lambdaBody);
    SgFunctionDeclaration *lambdaFunction = lambdaDefinition->get_declaration();
    ROSE_ASSERT(lambdaFunction != nullptr);
    ROSE_ASSERT(lambdaFunction->get_definition() == lambdaDefinition);
    SgLambdaExp *lambda = isSgLambdaExp(lambdaFunction->get_parent());
    ROSE_ASSERT(lambda != nullptr);
    ROSE_ASSERT(lambda->get_lambda_function() == lambdaFunction);

    Sg_File_Info *recordLocation = record->get_file_info();
    Sg_File_Info *bodyStart = lambdaBody->get_startOfConstruct();
    Sg_File_Info *returnStart = owner->get_startOfConstruct();
    ROSE_ASSERT(samePhysicalFile(recordLocation, bodyStart));
    ROSE_ASSERT(samePhysicalFile(recordLocation, returnStart));
    ROSE_ASSERT(locationLeq(bodyStart, recordLocation));
    ROSE_ASSERT(locationLeq(recordLocation, returnStart));
    return lambda;
  };

  SgLambdaExp *initializerLambda =
      validateLambdaOwner(initializerDirectiveOwner, initializerLambdaDefine);

  bool lambdaOwnedByResult = false;
  for (SgNode *owner = initializerLambda; owner != nullptr;
       owner = owner->get_parent()) {
    if (owner == resultName) {
      lambdaOwnedByResult = true;
      break;
    }
  }
  ROSE_ASSERT(lambdaOwnedByResult);

  ROSE_ASSERT(isSgCtorInitializerList(ctorDirectiveOwner) == nullptr);
  SgInitializedName *ctorInitializer = isSgInitializedName(ctorDirectiveOwner);
  ROSE_ASSERT(ctorInitializer != nullptr);
  ROSE_ASSERT(ctorInitializer->get_name() == "value");
  SgCtorInitializerList *ctorInitializerList =
      isSgCtorInitializerList(ctorInitializer->get_parent());
  ROSE_ASSERT(ctorInitializerList != nullptr);
  ROSE_ASSERT(std::count(ctorInitializerList->get_ctors().begin(),
                         ctorInitializerList->get_ctors().end(),
                         ctorInitializer) == 1);
  ROSE_ASSERT(ctorInitializerDefine->get_file_info() != nullptr);
  ROSE_ASSERT(ctorInitializer->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(samePhysicalFile(ctorInitializerDefine->get_file_info(),
                               ctorInitializer->get_startOfConstruct()));
  ROSE_ASSERT(locationLeq(ctorInitializerDefine->get_file_info(),
                          ctorInitializer->get_startOfConstruct()));

  PreprocessingInfo *ctorExpressionComment = nullptr;
  SgExpression *ctorExpressionOwner = nullptr;
  PreprocessingInfo *emptyCtorIfdef = nullptr;
  SgFunctionDefinition *emptyCtorDefinition = nullptr;
  SgInitializedName *conditionalValue = nullptr;
  PreprocessingInfo *conditionalArgumentIf = nullptr;
  SgIntVal *conditionalArgumentOwner = nullptr;
  PreprocessingInfo *conditionalCtorIf = nullptr;
  SgInitializedName *conditionalCtorOwner = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    if (SgInitializedName *name = isSgInitializedName(located);
        name != nullptr && name->get_name() == "conditional_value") {
      ROSE_ASSERT(conditionalValue == nullptr);
      conditionalValue = name;
    }
    AttachedPreprocessingInfoType *records =
        located->getAttachedPreprocessingInfo();
    if (records == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *records) {
      if (containsMarker(record, "REX_FRONTEND_CTOR_EXPRESSION_COMMENT")) {
        ROSE_ASSERT(ctorExpressionComment == nullptr);
        ctorExpressionComment = record;
        ctorExpressionOwner = isSgExpression(located);
        ROSE_ASSERT(ctorExpressionOwner != nullptr);
        ROSE_ASSERT(record->getAttachedOwner() == ctorExpressionOwner);
      }
      if (containsMarker(record, "REX_FRONTEND_EMPTY_CTOR_BODY") &&
          record->getTypeOfDirective() ==
              PreprocessingInfo::CpreprocessorIfdefDeclaration) {
        ROSE_ASSERT(emptyCtorIfdef == nullptr);
        emptyCtorIfdef = record;
        emptyCtorDefinition = isSgFunctionDefinition(located);
        ROSE_ASSERT(emptyCtorDefinition != nullptr);
        ROSE_ASSERT(record->getAttachedOwner() == emptyCtorDefinition);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
      }
      if (containsMarker(record, "REX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED") &&
          record->getTypeOfDirective() ==
              PreprocessingInfo::CpreprocessorIfDeclaration) {
        ROSE_ASSERT(conditionalArgumentIf == nullptr);
        conditionalArgumentIf = record;
        conditionalArgumentOwner = isSgIntVal(located);
        ROSE_ASSERT(conditionalArgumentOwner != nullptr);
        ROSE_ASSERT(conditionalArgumentOwner->get_value() == 2);
        ROSE_ASSERT(record->getAttachedOwner() == conditionalArgumentOwner);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
        SgExprListExp *arguments =
            isSgExprListExp(conditionalArgumentOwner->get_parent());
        ROSE_ASSERT(arguments != nullptr);
        ROSE_ASSERT(arguments->get_expressions().size() == 2);
        ROSE_ASSERT(arguments->get_expressions()[1] ==
                    conditionalArgumentOwner);
      }
      if (containsMarker(record, "REX_FRONTEND_CONDITIONAL_CTOR_ENABLED") &&
          record->getTypeOfDirective() ==
              PreprocessingInfo::CpreprocessorIfDeclaration) {
        ROSE_ASSERT(conditionalCtorIf == nullptr);
        conditionalCtorIf = record;
        conditionalCtorOwner = isSgInitializedName(located);
        ROSE_ASSERT(conditionalCtorOwner != nullptr);
        ROSE_ASSERT(conditionalCtorOwner->get_name() == "second");
        ROSE_ASSERT(record->getAttachedOwner() == conditionalCtorOwner);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
        SgCtorInitializerList *initializers =
            isSgCtorInitializerList(conditionalCtorOwner->get_parent());
        ROSE_ASSERT(initializers != nullptr);
        ROSE_ASSERT(initializers->get_ctors().size() == 2);
        ROSE_ASSERT(initializers->get_ctors()[1] == conditionalCtorOwner);
      }
    }
  }
  ROSE_ASSERT(ctorExpressionComment != nullptr);
  ROSE_ASSERT(ctorExpressionOwner != nullptr);
  bool expressionOwnedBelowCtorInitializers = false;
  for (SgNode *owner = ctorExpressionOwner; owner != nullptr;
       owner = owner->get_parent()) {
    if (isSgCtorInitializerList(owner) != nullptr) {
      expressionOwnedBelowCtorInitializers = true;
      break;
    }
  }
  ROSE_ASSERT(expressionOwnedBelowCtorInitializers);
  ROSE_ASSERT(emptyCtorIfdef != nullptr);
  ROSE_ASSERT(emptyCtorDefinition != nullptr);
  ROSE_ASSERT(conditionalArgumentIf != nullptr);
  ROSE_ASSERT(conditionalArgumentOwner != nullptr);
  ROSE_ASSERT(conditionalCtorIf != nullptr);
  ROSE_ASSERT(conditionalCtorOwner != nullptr);

  ROSE_ASSERT(conditionalValue != nullptr);
  SgVariableDefinition *conditionalDefinition =
      conditionalValue->get_definition();
  ROSE_ASSERT(conditionalDefinition != nullptr);
  ROSE_ASSERT(conditionalDefinition->get_parent() == conditionalValue);
  ROSE_ASSERT(conditionalDefinition->getAttachedPreprocessingInfo() == nullptr);
  SgVariableDeclaration *conditionalDeclaration =
      isSgVariableDeclaration(conditionalValue->get_declaration());
  ROSE_ASSERT(conditionalDeclaration != nullptr);
  ROSE_ASSERT(conditionalValue->get_parent() == conditionalDeclaration);
  AttachedPreprocessingInfoType *conditionalRecords =
      conditionalDeclaration->getAttachedPreprocessingInfo();
  ROSE_ASSERT(conditionalRecords != nullptr);
  bool foundConditionalIfdef = false;
  bool foundConditionalEndif = false;
  for (PreprocessingInfo *record : *conditionalRecords) {
    ROSE_ASSERT(record != nullptr);
    ROSE_ASSERT(record->getAttachedOwner() == conditionalDeclaration);
    if (record->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorIfdefDeclaration &&
        containsMarker(record, "REX_FRONTEND_CONDITIONAL_STORAGE")) {
      foundConditionalIfdef = true;
      ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
    }
    if (record->getTypeOfDirective() ==
        PreprocessingInfo::CpreprocessorEndifDeclaration) {
      foundConditionalEndif = true;
      ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::after);
    }
  }
  ROSE_ASSERT(foundConditionalIfdef);
  ROSE_ASSERT(foundConditionalEndif);
  return 0;
}
