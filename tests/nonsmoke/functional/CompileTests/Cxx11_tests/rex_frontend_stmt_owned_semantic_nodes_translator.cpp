#include "rose.h"

#include <algorithm>
#include <set>
#include <string>

namespace {
  void requireSynthesizedOutputNode(SgLocatedNode *node) {
    ROSE_ASSERT(node != nullptr);
    for (Sg_File_Info *fileInfo :
         {node->get_file_info(), node->get_startOfConstruct(),
          node->get_endOfConstruct()}) {
      ROSE_ASSERT(fileInfo != nullptr);
      ROSE_ASSERT(fileInfo->isCompilerGenerated());
      ROSE_ASSERT(fileInfo->isOutputInCodeGeneration());
    }
  }

  void requireFrontendSynthesizedOutputNode(SgLocatedNode *node) {
    requireSynthesizedOutputNode(node);
    for (Sg_File_Info *fileInfo :
         {node->get_file_info(), node->get_startOfConstruct(),
          node->get_endOfConstruct()}) {
      ROSE_ASSERT(fileInfo->isFrontendSpecific());
      ROSE_ASSERT(fileInfo->get_parent() == node);
      ROSE_ASSERT(!fileInfo->isTransformation());
      ROSE_ASSERT(!fileInfo->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(fileInfo->get_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(fileInfo->get_physical_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(fileInfo->get_raw_line() == 0);
      ROSE_ASSERT(fileInfo->get_raw_col() == 0);
    }
  }

  void requireSourceNode(SgLocatedNode *node) {
    ROSE_ASSERT(node != nullptr);
    for (Sg_File_Info *fileInfo :
         {node->get_file_info(), node->get_startOfConstruct(),
          node->get_endOfConstruct()}) {
      ROSE_ASSERT(fileInfo != nullptr);
      ROSE_ASSERT(!fileInfo->isCompilerGenerated());
      ROSE_ASSERT(fileInfo->isOutputInCodeGeneration());
      ROSE_ASSERT(fileInfo->get_line() > 0);
      ROSE_ASSERT(fileInfo->get_col() > 0);
      ROSE_ASSERT(fileInfo->get_physical_file_id() >= 0);
    }
  }

  void requireImplicitConversionOfSourceExpression(SgExpression *expression) {
    ROSE_ASSERT(expression != nullptr);
    size_t conversions = 0;
    while (SgCastExp *cast = isSgCastExp(expression)) {
      ROSE_ASSERT(cast->get_file_info() != nullptr);
      ROSE_ASSERT(cast->get_file_info()->isImplicitCast());
      cast->validate_semantic_conversion();
      requireSynthesizedOutputNode(cast);
      expression = cast->get_operand();
      ROSE_ASSERT(expression != nullptr);
      ROSE_ASSERT(expression->get_parent() == cast);
      ++conversions;
    }
    ROSE_ASSERT(conversions > 0);
    requireSourceNode(expression);
  }

  std::string tagName(SgDeclarationStatement *declaration) {
    if (SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration)) {
      ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
      ROSE_ASSERT(classDeclaration->get_definingDeclaration() == declaration);
      ROSE_ASSERT(!classDeclaration->get_isAutonomousDeclaration());
      return classDeclaration->get_name().str();
    }
    SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
    ROSE_ASSERT(enumDeclaration != nullptr);
    ROSE_ASSERT(!enumDeclaration->isForward());
    ROSE_ASSERT(enumDeclaration->get_definingDeclaration() == declaration);
    ROSE_ASSERT(!enumDeclaration->get_isAutonomousDeclaration());
    return enumDeclaration->get_name().str();
  }
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  Rose_STL_Container<SgNode *> lambdas =
      NodeQuery::querySubTree(project, V_SgLambdaExp);
  ROSE_ASSERT(lambdas.size() == 1);
  SgLambdaExp *lambda = isSgLambdaExp(lambdas.front());
  ROSE_ASSERT(lambda != nullptr);
  SgClassDeclaration *closure = lambda->get_lambda_closure_class();
  ROSE_ASSERT(closure != nullptr);
  ROSE_ASSERT(closure->get_parent() == lambda);
  ROSE_ASSERT(!closure->get_isAutonomousDeclaration());
  ROSE_ASSERT(lambda->get_type() == closure->get_type());
  ROSE_ASSERT(isSgClassType(lambda->get_type()) != nullptr);
  SgScopeStatement *closureScope = closure->get_scope();
  ROSE_ASSERT(closureScope != nullptr);
  if (closureScope->containsOnlyDeclarations()) {
    ROSE_ASSERT(std::count(closureScope->getDeclarationList().begin(),
                           closureScope->getDeclarationList().end(),
                           closure) == 0);
  } else {
    ROSE_ASSERT(std::count(closureScope->getStatementList().begin(),
                           closureScope->getStatementList().end(),
                           closure) == 0);
  }
  const SgNodePtrList lambdaSuccessors =
      lambda->get_traversalSuccessorContainer();
  ROSE_ASSERT(std::count(lambdaSuccessors.begin(), lambdaSuccessors.end(),
                         closure) == 1);
  requireFrontendSynthesizedOutputNode(closure);
  ROSE_ASSERT(closure->get_definition() != nullptr);
  ROSE_ASSERT(closure->get_definition()->get_parent() == closure);
  requireFrontendSynthesizedOutputNode(closure->get_definition());

  if (SgClassDeclaration *first =
          isSgClassDeclaration(closure->get_firstNondefiningDeclaration())) {
    if (first != closure) {
      SgAuxiliaryDeclarationList *owner =
          isSgAuxiliaryDeclarationList(first->get_parent());
      ROSE_ASSERT(owner != nullptr);
      ROSE_ASSERT(owner->get_parent() == first->get_scope());
      ROSE_ASSERT(first->get_scope()->get_auxiliary_declarations() == owner);
      ROSE_ASSERT(std::count(owner->get_declarations().begin(),
                             owner->get_declarations().end(), first) == 1);
      requireFrontendSynthesizedOutputNode(first);
    }
  }

  SgFunctionDeclaration *lambdaFunction = lambda->get_lambda_function();
  ROSE_ASSERT(lambdaFunction != nullptr);
  SgFunctionDeclaration *lambdaCanonical = isSgFunctionDeclaration(
      lambdaFunction->get_firstNondefiningDeclaration());
  ROSE_ASSERT(lambdaCanonical != nullptr);
  ROSE_ASSERT(lambdaCanonical->get_firstNondefiningDeclaration() ==
              lambdaCanonical);
  ROSE_ASSERT(lambdaCanonical->get_definingDeclaration() == lambdaFunction);
  ROSE_ASSERT(lambdaFunction->get_definingDeclaration() == lambdaFunction);
  SgFunctionDefinition *lambdaDefinition = lambdaFunction->get_definition();
  ROSE_ASSERT(lambdaDefinition != nullptr);
  requireFrontendSynthesizedOutputNode(lambdaDefinition);
  requireSourceNode(lambdaDefinition->get_body());

  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(project, V_SgForStatement);
  ROSE_ASSERT(loops.size() == 2);
  std::set<std::string> ownedTags;
  for (SgNode *node : loops) {
    SgForStatement *loop = isSgForStatement(node);
    ROSE_ASSERT(loop != nullptr);
    SgForInitStatement *forInit = loop->get_for_init_stmt();
    ROSE_ASSERT(forInit != nullptr);
    ROSE_ASSERT(forInit->get_init_stmt().size() == 1);
    SgVariableDeclaration *variable =
        isSgVariableDeclaration(forInit->get_init_stmt().front());
    ROSE_ASSERT(variable != nullptr);
    SgDeclarationStatement *tag = variable->get_baseTypeDefiningDeclaration();
    ROSE_ASSERT(tag != nullptr);
    ROSE_ASSERT(tag->get_parent() == variable);
    ROSE_ASSERT(tag->get_scope() == loop);
    requireSourceNode(tag);
    if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(tag)) {
      requireSourceNode(classDeclaration->get_definition());
    }
    const SgNodePtrList successors =
        variable->get_traversalSuccessorContainer();
    ROSE_ASSERT(std::count(successors.begin(), successors.end(), tag) == 1);
    ownedTags.insert(tagName(tag));
  }
  ROSE_ASSERT(ownedTags ==
              std::set<std::string>({"RexForEnum", "RexForRecord"}));

  size_t lambdaCalls = 0;
  size_t implicitConversionCalls = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgFunctionDeclaration *declaration =
        call->getAssociatedFunctionDeclaration();
    if (declaration == lambdaCanonical) {
      ROSE_ASSERT(isSgTypeInt(call->get_type()) != nullptr);
      ROSE_ASSERT(call->get_type() != lambda->get_type());
      requireSourceNode(call);
      requireSourceNode(call->get_args());
      SgExpression *callee = call->get_function();
      if (SgDotExp *dot = isSgDotExp(callee)) {
        requireSynthesizedOutputNode(dot);
        requireImplicitConversionOfSourceExpression(dot->get_lhs_operand());
        ROSE_ASSERT(isSgMemberFunctionRefExp(dot->get_rhs_operand()) !=
                    nullptr);
        requireFrontendSynthesizedOutputNode(
            isSgLocatedNode(dot->get_rhs_operand()));
      } else if (SgArrowExp *arrow = isSgArrowExp(callee)) {
        requireSynthesizedOutputNode(arrow);
        requireImplicitConversionOfSourceExpression(arrow->get_lhs_operand());
        ROSE_ASSERT(isSgMemberFunctionRefExp(arrow->get_rhs_operand()) !=
                    nullptr);
        requireFrontendSynthesizedOutputNode(
            isSgLocatedNode(arrow->get_rhs_operand()));
      } else {
        requireSynthesizedOutputNode(isSgLocatedNode(callee));
      }
      ++lambdaCalls;
    }
    if (declaration == nullptr ||
        declaration->get_name().getString().find("operator") != 0 ||
        !call->isCompilerGenerated()) {
      continue;
    }
    requireSynthesizedOutputNode(call);
    requireSynthesizedOutputNode(call->get_args());
    SgExpression *callee = call->get_function();
    if (SgDotExp *dot = isSgDotExp(callee)) {
      requireSynthesizedOutputNode(dot);
      requireImplicitConversionOfSourceExpression(dot->get_lhs_operand());
      requireSynthesizedOutputNode(isSgLocatedNode(dot->get_rhs_operand()));
    } else if (SgArrowExp *arrow = isSgArrowExp(callee)) {
      requireSynthesizedOutputNode(arrow);
      requireImplicitConversionOfSourceExpression(arrow->get_lhs_operand());
      requireSynthesizedOutputNode(isSgLocatedNode(arrow->get_rhs_operand()));
    } else {
      requireSynthesizedOutputNode(isSgLocatedNode(callee));
    }
    ++implicitConversionCalls;
  }
  ROSE_ASSERT(lambdaCalls == 1);
  ROSE_ASSERT(implicitConversionCalls >= 2);

  size_t implicitCasts = 0;
  std::set<SgCastExp::semantic_conversion_kind_enum> implicitConversionKinds;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    Sg_File_Info *fileInfo = cast->get_file_info();
    if (fileInfo == nullptr || !fileInfo->isImplicitCast()) {
      continue;
    }
    const SgCastExp::semantic_conversion_kind_enum conversionKind =
        cast->get_semantic_conversion_kind();
    ROSE_ASSERT(conversionKind > SgCastExp::e_semantic_conversion_unclassified);
    ROSE_ASSERT(conversionKind < SgCastExp::e_semantic_conversion_last);
    cast->validate_semantic_conversion();
    requireSynthesizedOutputNode(cast);
    ROSE_ASSERT(cast->get_type_defining_declaration() == nullptr);
    ROSE_ASSERT(cast->get_operand() != nullptr);
    ROSE_ASSERT(cast->get_operand()->get_parent() == cast);
    for (Sg_File_Info *castInfo :
         {cast->get_file_info(), cast->get_startOfConstruct(),
          cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
      ROSE_ASSERT(castInfo != nullptr);
      ROSE_ASSERT(castInfo->isImplicitCast());
    }
    implicitConversionKinds.insert(conversionKind);
    ++implicitCasts;
  }
  ROSE_ASSERT(implicitCasts >= 4);
  ROSE_ASSERT(implicitConversionKinds.count(
                  SgCastExp::e_semantic_conversion_FunctionToPointerDecay) ==
              1);
  ROSE_ASSERT(implicitConversionKinds.count(
                  SgCastExp::e_semantic_conversion_LValueToRValue) == 1);

  size_t sourceStaticCasts = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    if (cast->get_cast_type() != SgCastExp::e_static_cast) {
      continue;
    }
    cast->validate_semantic_conversion();
    requireSourceNode(cast);
    ROSE_ASSERT(cast->get_type_defining_declaration() == nullptr);
    ROSE_ASSERT(cast->get_operand() != nullptr);
    ROSE_ASSERT(cast->get_operand()->get_parent() == cast);
    ++sourceStaticCasts;
  }
  ROSE_ASSERT(sourceStaticCasts == 3);

  size_t sourceDefaultInitializers = 0;
  size_t semanticDefaultInitializers = 0;
  size_t defaultSemanticCasts = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    ROSE_ASSERT(function != nullptr);
    if (function->get_name().getString() !=
        "rex_default_argument_semantic_provenance") {
      continue;
    }
    SgFunctionParameterList *semanticParameters = function->get_parameterList();
    SgFunctionParameterList *syntaxParameters =
        function->get_parameterList_syntax();
    ROSE_ASSERT(semanticParameters != nullptr);
    ROSE_ASSERT(semanticParameters->get_args().size() == 1);
    SgInitializer *semanticInitializer =
        semanticParameters->get_args().front()->get_initializer();
    if (semanticInitializer == nullptr) {
      ROSE_ASSERT(
          syntaxParameters == nullptr ||
          (syntaxParameters->get_args().size() == 1 &&
           syntaxParameters->get_args().front()->get_initializer() == nullptr));
      continue;
    }

    ROSE_ASSERT(syntaxParameters != nullptr);
    ROSE_ASSERT(syntaxParameters != semanticParameters);
    ROSE_ASSERT(syntaxParameters->get_parent() == function);
    ROSE_ASSERT(syntaxParameters->get_args().size() == 1);
    SgInitializer *sourceInitializer =
        syntaxParameters->get_args().front()->get_initializer();
    ROSE_ASSERT(sourceInitializer != nullptr);
    ROSE_ASSERT(sourceInitializer != semanticInitializer);
    ROSE_ASSERT(sourceInitializer->get_parent() ==
                syntaxParameters->get_args().front());
    ROSE_ASSERT(semanticInitializer->get_parent() ==
                semanticParameters->get_args().front());
    requireSourceNode(sourceInitializer);
    requireFrontendSynthesizedOutputNode(semanticInitializer);

    SgAssignInitializer *sourceAssignment =
        isSgAssignInitializer(sourceInitializer);
    SgAssignInitializer *semanticAssignment =
        isSgAssignInitializer(semanticInitializer);
    ROSE_ASSERT(sourceAssignment != nullptr);
    ROSE_ASSERT(semanticAssignment != nullptr);
    SgCastExp *sourceSemanticCast =
        isSgCastExp(sourceAssignment->get_operand());
    SgCastExp *semanticCloneCast =
        isSgCastExp(semanticAssignment->get_operand());
    ROSE_ASSERT(sourceSemanticCast != nullptr);
    ROSE_ASSERT(semanticCloneCast != nullptr);
    ROSE_ASSERT(sourceSemanticCast != semanticCloneCast);
    ROSE_ASSERT(sourceSemanticCast->get_cast_type() ==
                SgCastExp::e_implicit_cast);
    ROSE_ASSERT(semanticCloneCast->get_cast_type() ==
                SgCastExp::e_implicit_cast);
    ROSE_ASSERT(sourceSemanticCast->get_semantic_conversion_kind() >
                SgCastExp::e_semantic_conversion_unclassified);
    ROSE_ASSERT(sourceSemanticCast->get_semantic_conversion_kind() <
                SgCastExp::e_semantic_conversion_last);
    ROSE_ASSERT(semanticCloneCast->get_semantic_conversion_kind() ==
                sourceSemanticCast->get_semantic_conversion_kind());
    sourceSemanticCast->validate_semantic_conversion();
    semanticCloneCast->validate_semantic_conversion();
    ROSE_ASSERT(sourceSemanticCast->get_parent() == sourceAssignment);
    ROSE_ASSERT(semanticCloneCast->get_parent() == semanticAssignment);
    requireSynthesizedOutputNode(sourceSemanticCast);
    requireFrontendSynthesizedOutputNode(semanticCloneCast);
    for (Sg_File_Info *castInfo :
         {sourceSemanticCast->get_file_info(),
          sourceSemanticCast->get_startOfConstruct(),
          sourceSemanticCast->get_endOfConstruct(),
          sourceSemanticCast->get_operatorPosition()}) {
      ROSE_ASSERT(castInfo != nullptr);
      ROSE_ASSERT(castInfo->isImplicitCast());
    }
    SgExpression *sourceOperand = sourceSemanticCast->get_operand();
    SgExpression *semanticOperand = semanticCloneCast->get_operand();
    ROSE_ASSERT(sourceOperand != nullptr);
    ROSE_ASSERT(semanticOperand != nullptr);
    ROSE_ASSERT(sourceOperand != semanticOperand);
    ROSE_ASSERT(sourceOperand->get_parent() == sourceSemanticCast);
    ROSE_ASSERT(semanticOperand->get_parent() == semanticCloneCast);
    requireSourceNode(sourceOperand);
    requireFrontendSynthesizedOutputNode(semanticOperand);
    ROSE_ASSERT(
        sourceInitializer->get_startOfConstruct()->get_physical_file_id() ==
        sourceOperand->get_startOfConstruct()->get_physical_file_id());
    ROSE_ASSERT(
        sourceInitializer->get_endOfConstruct()->get_physical_file_id() ==
        sourceOperand->get_endOfConstruct()->get_physical_file_id());
    ROSE_ASSERT(sourceInitializer->get_startOfConstruct()->get_line() ==
                sourceOperand->get_startOfConstruct()->get_line());
    ROSE_ASSERT(sourceInitializer->get_startOfConstruct()->get_col() ==
                sourceOperand->get_startOfConstruct()->get_col());
    ROSE_ASSERT(sourceInitializer->get_endOfConstruct()->get_line() ==
                sourceOperand->get_endOfConstruct()->get_line());
    ROSE_ASSERT(sourceInitializer->get_endOfConstruct()->get_col() ==
                sourceOperand->get_endOfConstruct()->get_col());
    ++sourceDefaultInitializers;
    ++semanticDefaultInitializers;
    ++defaultSemanticCasts;
  }
  ROSE_ASSERT(sourceDefaultInitializers == 1);
  ROSE_ASSERT(semanticDefaultInitializers == 1);
  ROSE_ASSERT(defaultSemanticCasts == 1);

  size_t builtinBitCasts = 0;
  size_t functionalCasts = 0;
  size_t functionalListCasts = 0;
  size_t typedFunctionalListOperands = 0;
  size_t twoEdgeBasePaths = 0;
  size_t lvalueCasts = 0;
  size_t xvalueCasts = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    cast->validate_semantic_conversion();
    if (cast->get_value_category() == SgCastExp::e_value_category_lvalue) {
      ++lvalueCasts;
    } else if (cast->get_value_category() ==
               SgCastExp::e_value_category_xvalue) {
      ++xvalueCasts;
    }
    if (cast->get_cast_type() == SgCastExp::e_builtin_bit_cast) {
      ROSE_ASSERT(cast->get_semantic_conversion_kind() ==
                  SgCastExp::e_semantic_conversion_LValueToRValueBitCast);
      ROSE_ASSERT(cast->get_value_category() ==
                  SgCastExp::e_value_category_prvalue);
      ++builtinBitCasts;
    } else if (cast->get_cast_type() == SgCastExp::e_functional_cast) {
      ++functionalCasts;
    } else if (cast->get_cast_type() == SgCastExp::e_functional_list_cast) {
      SgAggregateInitializer *construction =
          isSgAggregateInitializer(cast->get_operand());
      ROSE_ASSERT(construction != nullptr);
      ROSE_ASSERT(construction->get_parent() == cast);
      ROSE_ASSERT(
          construction->get_source_form() ==
          SgAggregateInitializer::e_aggregate_initializer_source_braced);
      ROSE_ASSERT(SageInterface::isEquivalentType(construction->get_type(),
                                                  cast->get_type()));
      SgExprListExp *arguments = construction->get_initializers();
      ROSE_ASSERT(arguments != nullptr);
      ROSE_ASSERT(arguments->get_parent() == construction);
      requireSourceNode(construction);
      requireSourceNode(arguments);
      for (SgExpression *argument : arguments->get_expressions()) {
        ROSE_ASSERT(argument != nullptr);
        ROSE_ASSERT(argument->get_parent() == arguments);
      }
      ++typedFunctionalListOperands;
      ++functionalListCasts;
    }
    const SgTypePtrList &path = cast->get_conversion_base_path();
    if (path.size() == 2) {
      SgClassType *intermediate =
          isSgClassType(path[0]->stripTypedefsAndModifiers());
      SgClassType *base = isSgClassType(path[1]->stripTypedefsAndModifiers());
      ROSE_ASSERT(intermediate != nullptr && base != nullptr);
      SgClassDeclaration *intermediateDeclaration =
          isSgClassDeclaration(intermediate->get_declaration());
      SgClassDeclaration *baseDeclaration =
          isSgClassDeclaration(base->get_declaration());
      ROSE_ASSERT(intermediateDeclaration != nullptr &&
                  baseDeclaration != nullptr);
      ROSE_ASSERT(intermediateDeclaration->get_name() ==
                  "RexCheckedCastIntermediate");
      ROSE_ASSERT(baseDeclaration->get_name() == "RexCheckedCastBase");
      ++twoEdgeBasePaths;
    }
  }
  ROSE_ASSERT(builtinBitCasts == 1);
  ROSE_ASSERT(functionalCasts >= 1);
  ROSE_ASSERT(functionalListCasts >= 2);
  ROSE_ASSERT(typedFunctionalListOperands == functionalListCasts);
  ROSE_ASSERT(twoEdgeBasePaths >= 3);
  ROSE_ASSERT(lvalueCasts >= 1);
  ROSE_ASSERT(xvalueCasts >= 1);

  return backend(project);
}
