#include "rose.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

std::string inputBasename(int argc, char **argv) {
  ROSE_ASSERT(argc > 1);
  const std::string path = argv[argc - 1];
  const std::size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

SgType *elementType(SgType *type, int *rank = nullptr) {
  ROSE_ASSERT(type != nullptr);
  int arrayRank = 0;
  while (true) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      ROSE_ASSERT(array->get_rank() > 0);
      arrayRank += array->get_rank();
      type = array->get_base_type();
    } else {
      break;
    }
    ROSE_ASSERT(type != nullptr);
  }
  if (rank != nullptr) {
    *rank = arrayRank;
  }
  return type;
}

void assertExactPhysicalExpressionSource(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  Sg_File_Info *start = expression->get_startOfConstruct();
  Sg_File_Info *end = expression->get_endOfConstruct();
  Sg_File_Info *operatorPosition = expression->get_operatorPosition();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(operatorPosition != nullptr);
  ROSE_ASSERT(start != end);
  ROSE_ASSERT(start != operatorPosition);
  ROSE_ASSERT(end != operatorPosition);
  for (Sg_File_Info *position : {start, end, operatorPosition}) {
    ROSE_ASSERT(position->get_parent() == expression);
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isCompilerGenerated());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(position->get_physical_file_id() >= 0);
  }
  ROSE_ASSERT(start->get_physical_file_id() == end->get_physical_file_id());
  ROSE_ASSERT(start->get_physical_file_id() ==
              operatorPosition->get_physical_file_id());
}

void assertCompleteOwnedExpressionSource(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  Sg_File_Info *start = expression->get_startOfConstruct();
  Sg_File_Info *end = expression->get_endOfConstruct();
  Sg_File_Info *operatorPosition = expression->get_operatorPosition();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(operatorPosition != nullptr);
  ROSE_ASSERT(start != end);
  ROSE_ASSERT(start != operatorPosition);
  ROSE_ASSERT(end != operatorPosition);
  for (Sg_File_Info *position : {start, end, operatorPosition}) {
    ROSE_ASSERT(position->get_parent() == expression);
    ROSE_ASSERT(!position->isShared());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
  }
  ROSE_ASSERT(start->get_physical_file_id() == end->get_physical_file_id());
  ROSE_ASSERT(start->get_physical_file_id() ==
              operatorPosition->get_physical_file_id());
  if (start->isCompilerGenerated()) {
    for (Sg_File_Info *position : {start, end, operatorPosition}) {
      ROSE_ASSERT(position->isCompilerGenerated());
      ROSE_ASSERT(position->isFrontendSpecific());
      ROSE_ASSERT(position->isOutputInCodeGeneration());
      ROSE_ASSERT(position->get_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(position->get_physical_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    }
  } else {
    for (Sg_File_Info *position : {start, end, operatorPosition}) {
      ROSE_ASSERT(!position->isCompilerGenerated());
      ROSE_ASSERT(position->get_physical_file_id() >= 0);
    }
  }
}

void assertExactPhysicalLocatedSource(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(start != end);
  for (Sg_File_Info *position : {start, end}) {
    ROSE_ASSERT(position->get_parent() == node);
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isCompilerGenerated());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(position->get_physical_file_id() >= 0);
  }
  ROSE_ASSERT(start->get_physical_file_id() == end->get_physical_file_id());
  ROSE_ASSERT(end->get_line() > start->get_line() ||
              (end->get_line() == start->get_line() &&
               end->get_col() >= start->get_col()));
}

std::vector<SgInitializedName *> findNames(SgNode *root,
                                           const std::string &name) {
  std::vector<SgInitializedName *> result;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    ROSE_ASSERT(initializedName != nullptr);
    if (initializedName->get_name().getString() == name) {
      result.push_back(initializedName);
    }
  }
  return result;
}

std::vector<SgNode *> frontendAstRoots(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  std::vector<SgNode *> roots{project};
  std::set<SgNode *> uniqueRoots{project};
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    SgFileList *external =
        source != nullptr ? source->get_frontendExternalFileList() : nullptr;
    if (external != nullptr && uniqueRoots.insert(external).second) {
      roots.push_back(external);
    }
  }
  return roots;
}

SgProcedureHeaderStatement *findDefiningProcedure(SgNode *root,
                                                  const std::string &name) {
  SgProcedureHeaderStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    if (candidate->get_name().getString() == name &&
        candidate->get_definition() != nullptr) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgClassType *findDefinedClassType(SgProject *project, const std::string &name) {
  SgClassType *result = nullptr;
  for (SgNode *root : frontendAstRoots(project)) {
    for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
      SgClassDeclaration *declaration = isSgClassDeclaration(node);
      ROSE_ASSERT(declaration != nullptr);
      if (declaration->get_name().getString() == name &&
          declaration->get_definition() != nullptr) {
        SgClassType *type = declaration->get_type();
        ROSE_ASSERT(type != nullptr);
        ROSE_ASSERT(result == nullptr || result == type);
        result = type;
      }
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void verifyCustomImplicitTypes(SgProject *project) {
  const auto requireType = [&](const std::string &name,
                               VariantT expectedVariant,
                               std::size_t minimumCount) {
    const std::vector<SgInitializedName *> declarations =
        findNames(project, name);
    ROSE_ASSERT(declarations.size() >= minimumCount);
    for (SgInitializedName *declaration : declarations) {
      SgType *type = elementType(declaration->get_type());
      ROSE_ASSERT(type->variantT() == expectedVariant);
      ROSE_ASSERT(isSgTypeDefault(type) == nullptr);
      ROSE_ASSERT(isSgTypeUnknown(type) == nullptr);
    }
  };

  requireType("alpha", V_SgTypeComplex, 2);
  requireType("beta", V_SgTypeComplex, 2);
  requireType("cobalt", V_SgTypeComplex, 1);
  requireType("zeta", V_SgTypeBool, 1);
  requireType("zenith", V_SgTypeBool, 1);

  SgProcedureHeaderStatement *function =
      findDefiningProcedure(project, "rex_custom_implicit_function_result");
  SgFunctionType *functionType = function->get_type();
  ROSE_ASSERT(functionType != nullptr);
  ROSE_ASSERT(isSgTypeComplex(elementType(functionType->get_return_type())) !=
              nullptr);
  SgInitializedName *result = function->get_result_name();
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_name() == "beta");
  ROSE_ASSERT(isSgTypeComplex(elementType(result->get_type())) != nullptr);
  ROSE_ASSERT(result->get_type() == functionType->get_return_type());
}

void verifyDefinedOperatorArrayResults(SgProject *project) {
  std::size_t addCount = 0;
  std::size_t multiplyCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgUserDefinedBinaryOp)) {
    SgUserDefinedBinaryOp *operation = isSgUserDefinedBinaryOp(node);
    ROSE_ASSERT(operation != nullptr);
    assertExactPhysicalExpressionSource(operation);
    SgFunctionSymbol *symbol = operation->get_symbol();
    ROSE_ASSERT(symbol != nullptr);
    SgFunctionDeclaration *declaration = symbol->get_declaration();
    ROSE_ASSERT(declaration != nullptr);

    int rank = 0;
    SgType *base = elementType(operation->get_type(), &rank);
    ROSE_ASSERT(isSgTypeInt(base) != nullptr);
    SgFunctionType *functionType = declaration->get_type();
    ROSE_ASSERT(functionType != nullptr);
    int declaredRank = 0;
    SgType *declaredBase =
        elementType(functionType->get_return_type(), &declaredRank);
    ROSE_ASSERT(declaredBase == base);
    if (declaration->get_name() == "add_elements") {
      ROSE_ASSERT(rank == 2);
      ROSE_ASSERT(declaredRank == 0);
      ROSE_ASSERT(operation->get_type() != functionType->get_return_type());
      ++addCount;
    } else if (declaration->get_name() == "matrix_times_vector") {
      ROSE_ASSERT(rank == 1);
      ROSE_ASSERT(declaredRank == 1);
      ++multiplyCount;
    }
  }
  ROSE_ASSERT(addCount == 2);
  ROSE_ASSERT(multiplyCount == 1);
}

void verifyFunctionResultIdentity(SgProject *project) {
  SgClassType *canonical = findDefinedClassType(project, "source_result_type");
  auto requireSourceBinding = [canonical](SgSymbol *source,
                                          const SgName &expectedName) {
    ROSE_ASSERT(source != nullptr);
    ROSE_ASSERT(source->get_name() == expectedName);
    std::set<SgAliasSymbol *> aliases;
    while (SgAliasSymbol *alias = isSgAliasSymbol(source)) {
      ROSE_ASSERT(aliases.insert(alias).second);
      source = alias->get_alias();
      ROSE_ASSERT(source != nullptr);
    }
    SgClassSymbol *classSymbol = isSgClassSymbol(source);
    ROSE_ASSERT(classSymbol != nullptr);
    ROSE_ASSERT(classSymbol->get_declaration() != nullptr);
    ROSE_ASSERT(classSymbol->get_declaration()->get_type() == canonical);
  };
  for (const std::string &name :
       {"rex_host_use_result", "rex_local_renamed_use_result"}) {
    SgProcedureHeaderStatement *function = findDefiningProcedure(project, name);
    SgFunctionType *functionType = function->get_type();
    ROSE_ASSERT(functionType != nullptr);
    ROSE_ASSERT(elementType(functionType->get_return_type()) == canonical);
    SgInitializedName *result = function->get_result_name();
    ROSE_ASSERT(result != nullptr);
    ROSE_ASSERT(result->get_name() == "output");
    ROSE_ASSERT(elementType(result->get_type()) == canonical);
    ROSE_ASSERT(result->get_type() == functionType->get_return_type());
    if (name == "rex_host_use_result") {
      requireSourceBinding(function->get_fortran_source_derived_type_symbol(),
                           "renamed_result_type");
      ROSE_ASSERT(result->get_fortran_source_derived_type_symbol() == nullptr);
    } else {
      ROSE_ASSERT(function->get_fortran_source_derived_type_symbol() ==
                  nullptr);
      requireSourceBinding(result->get_fortran_source_derived_type_symbol(),
                           "local_result_type");
    }
  }

  for (const std::string &name : {"first", "second"}) {
    const std::vector<SgInitializedName *> declarations =
        findNames(project, name);
    ROSE_ASSERT(declarations.size() == 1);
    ROSE_ASSERT(elementType(declarations.front()->get_type()) == canonical);
    requireSourceBinding(
        declarations.front()->get_fortran_source_derived_type_symbol(),
        "renamed_result_type");
  }

  const std::vector<SgInitializedName *> decoys =
      findNames(project, "source_result_type");
  ROSE_ASSERT(decoys.size() == 1);
  ROSE_ASSERT(isSgTypeInt(elementType(decoys.front()->get_type())) != nullptr);
}

void verifyDuplicateIntrinsicUse(SgProject *project) {
  const Rose_STL_Container<SgNode *> useNodes =
      NodeQuery::querySubTree(project, V_SgUseStatement);
  ROSE_ASSERT(useNodes.size() == 1);
  SgUseStatement *useStatement = isSgUseStatement(useNodes.front());
  ROSE_ASSERT(useStatement != nullptr);
  ROSE_ASSERT(useStatement->get_name() == "iso_c_binding");
  ROSE_ASSERT(useStatement->get_only_option());
  ROSE_ASSERT(useStatement->get_module_nature() == "Intrinsic");
  ROSE_ASSERT(useStatement->get_rename_list().size() == 2);
  int previousColumn = 0;
  for (SgRenamePair *pair : useStatement->get_rename_list()) {
    ROSE_ASSERT(pair != nullptr);
    ROSE_ASSERT(pair->get_parent() == useStatement);
    ROSE_ASSERT(pair->get_local_name() == "c_int");
    ROSE_ASSERT(pair->get_use_name() == "c_int");
    assertExactPhysicalLocatedSource(pair);
    ROSE_ASSERT(pair->get_startOfConstruct()->get_line() ==
                useStatement->get_startOfConstruct()->get_line());
    ROSE_ASSERT(pair->get_startOfConstruct()->get_col() > previousColumn);
    previousColumn = pair->get_startOfConstruct()->get_col();
  }

  const std::vector<SgInitializedName *> declarations =
      findNames(project, "c_int");
  ROSE_ASSERT(declarations.size() == 1);
  SgInitializedName *declaration = declarations.front();
  ROSE_ASSERT(isSgTypeInt(elementType(declaration->get_type())) != nullptr);
  SgScopeStatement *scope = declaration->get_scope();
  ROSE_ASSERT(scope != nullptr);
  SgVariableSymbol *symbol = scope->lookup_variable_symbol("c_int");
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(symbol->get_declaration() == declaration);
  SgVariableDeclaration *owner =
      isSgVariableDeclaration(declaration->get_parent());
  ROSE_ASSERT(owner != nullptr);
  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(owner->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == scope);
  ROSE_ASSERT(scope->get_auxiliary_declarations() == auxiliary);
  ROSE_ASSERT(!scope->statementExistsInScope(owner));
  for (SgLocatedNode *node : {static_cast<SgLocatedNode *>(owner),
                              static_cast<SgLocatedNode *>(declaration)}) {
    ROSE_ASSERT(node->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(node->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(node->get_startOfConstruct()->get_parent() == node);
    ROSE_ASSERT(node->get_endOfConstruct()->get_parent() == node);
    ROSE_ASSERT(node->get_startOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(node->get_endOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(node->get_startOfConstruct()->get_physical_file_id() ==
                useStatement->get_startOfConstruct()->get_physical_file_id());
  }

  const std::vector<SgInitializedName *> values = findNames(project, "value");
  ROSE_ASSERT(values.size() == 1);
  SgType *valueType = elementType(values.front()->get_type());
  ROSE_ASSERT(isSgTypeInt(valueType) != nullptr);
  ROSE_ASSERT(valueType == elementType(declaration->get_type()));
}

void verifySemanticNameAndComponentIdentity(SgProject *project) {
  SgClassType *payloadType = findDefinedClassType(project, "rex_payload");
  const std::vector<SgInitializedName *> componentDeclarations =
      findNames(project, "value");
  ROSE_ASSERT(componentDeclarations.size() == 1);
  SgInitializedName *component = componentDeclarations.front();
  ROSE_ASSERT(isSgClassDefinition(component->get_scope()) != nullptr);
  SgVariableSymbol *componentSymbol = isSgVariableSymbol(
      component->get_scope()->find_symbol_from_declaration(component));
  ROSE_ASSERT(componentSymbol != nullptr);

  const std::vector<SgInitializedName *> payloadDeclarations =
      findNames(project, "payload");
  ROSE_ASSERT(payloadDeclarations.size() == 1);
  ROSE_ASSERT(elementType(payloadDeclarations.front()->get_type()) ==
              payloadType);

  std::size_t componentReferenceCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != nullptr);
    SgVariableSymbol *symbol = reference->get_symbol();
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(symbol->get_declaration() != nullptr);
    if (symbol->get_name() == "value") {
      ROSE_ASSERT(symbol == componentSymbol);
      ROSE_ASSERT(symbol->get_declaration() == component);
      assertExactPhysicalExpressionSource(reference);
      ++componentReferenceCount;
    }
  }
  ROSE_ASSERT(componentReferenceCount == 4);

  std::size_t constructorCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgAggregateInitializer)) {
    SgAggregateInitializer *constructor = isSgAggregateInitializer(node);
    ROSE_ASSERT(constructor != nullptr);
    if (constructor->get_type() != payloadType) {
      continue;
    }
    assertExactPhysicalExpressionSource(constructor);
    SgExprListExp *arguments = constructor->get_initializers();
    ROSE_ASSERT(arguments != nullptr &&
                arguments->get_parent() == constructor &&
                arguments->get_expressions().size() == 1);
    ++constructorCount;
  }
  ROSE_ASSERT(constructorCount == 1);

  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgFunctionRefExp *function = isSgFunctionRefExp(call->get_function());
    if (function == nullptr || function->get_symbol() == nullptr) {
      continue;
    }
    const SgName name = function->get_symbol()->get_name();
    ROSE_ASSERT(name != "rex_payload");
    ROSE_ASSERT(name != "cmplx");
  }

  const Rose_STL_Container<SgNode *> complexValues =
      NodeQuery::querySubTree(project, V_SgComplexVal);
  ROSE_ASSERT(complexValues.size() == 1);
  SgComplexVal *complexValue = isSgComplexVal(complexValues.front());
  ROSE_ASSERT(complexValue != nullptr);
  assertExactPhysicalExpressionSource(complexValue);
  SgExpression *real = complexValue->get_real_value();
  SgExpression *imaginary = complexValue->get_imaginary_value();
  ROSE_ASSERT(isSgFloatVal(real) != nullptr);
  ROSE_ASSERT(isSgFloatVal(imaginary) != nullptr);
  ROSE_ASSERT(real->get_parent() == complexValue);
  ROSE_ASSERT(imaginary->get_parent() == complexValue);
  assertExactPhysicalExpressionSource(real);
  assertExactPhysicalExpressionSource(imaginary);
  ROSE_ASSERT(isSgTypeFloat(complexValue->get_precisionType()) != nullptr);
  SgTypeComplex *complexType = isSgTypeComplex(complexValue->get_type());
  ROSE_ASSERT(complexType != nullptr);
  ROSE_ASSERT(complexType->get_base_type() ==
              complexValue->get_precisionType());
}

void verifyStatementFunctionIdentity(SgProject *project) {
  const Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(project, V_SgStatementFunctionStatement);
  ROSE_ASSERT(statements.size() == 1);
  SgStatementFunctionStatement *statement =
      isSgStatementFunctionStatement(statements.front());
  ROSE_ASSERT(statement != nullptr);
  SgProcedureHeaderStatement *function =
      isSgProcedureHeaderStatement(statement->get_function());
  ROSE_ASSERT(function != nullptr);
  ROSE_ASSERT(function->get_name() == "rex_statement_value");
  SgScopeStatement *scope = isSgScopeStatement(statement->get_parent());
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(function->get_parent() == statement);
  ROSE_ASSERT(function->get_scope() == scope);
  ROSE_ASSERT(function->get_firstNondefiningDeclaration() == function);
  ROSE_ASSERT(function->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(function->get_parameterList() != nullptr);
  ROSE_ASSERT(function->get_parameterList()->get_parent() == function);
  for (SgLocatedNode *node : {static_cast<SgLocatedNode *>(statement),
                              static_cast<SgLocatedNode *>(function)}) {
    ROSE_ASSERT(node->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(node->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(node->get_startOfConstruct()->get_parent() == node);
    ROSE_ASSERT(node->get_endOfConstruct()->get_parent() == node);
    ROSE_ASSERT(!node->get_startOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(!node->get_endOfConstruct()->isCompilerGenerated());
  }
  SgFunctionType *functionType = function->get_type();
  ROSE_ASSERT(functionType != nullptr);
  ROSE_ASSERT(isSgTypeInt(elementType(functionType->get_return_type())) !=
              nullptr);

  std::size_t exactCallCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgFunctionDeclaration *callee = call->getAssociatedFunctionDeclaration();
    if (callee != nullptr && callee->get_name() == "rex_statement_value") {
      ROSE_ASSERT(callee == function);
      ROSE_ASSERT(call->get_type() == functionType->get_return_type());
      ROSE_ASSERT(call->get_args() != nullptr);
      ROSE_ASSERT(call->get_args()->get_expressions().size() == 1);
      ++exactCallCount;
    }
  }
  ROSE_ASSERT(exactCallCount == 1);
}

std::int64_t requireIntegerValue(SgExpression *expression) {
  SgIntVal *value = isSgIntVal(expression);
  ROSE_ASSERT(value != nullptr);
  ROSE_ASSERT(value->get_literal_type() != nullptr);
  SgTypeInt *type = isSgTypeInt(value->get_type());
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(type == value->get_literal_type());
  SgIntVal *kind = isSgIntVal(type->get_type_kind());
  ROSE_ASSERT(kind != nullptr);
  ROSE_ASSERT(kind->get_value() == 4);
  return value->get_value();
}

void verifyZeroIterationImpliedDo(SgProject *project) {
  const Rose_STL_Container<SgNode *> impliedDoNodes =
      NodeQuery::querySubTree(project, V_SgImpliedDo);
  ROSE_ASSERT(impliedDoNodes.size() == 1);
  SgImpliedDo *impliedDo = isSgImpliedDo(impliedDoNodes.front());
  ROSE_ASSERT(impliedDo != nullptr);
  SgAssignOp *initialization =
      isSgAssignOp(impliedDo->get_do_var_initialization());
  ROSE_ASSERT(initialization != nullptr);
  SgVarRefExp *control = isSgVarRefExp(initialization->get_lhs_operand());
  ROSE_ASSERT(control != nullptr);
  ROSE_ASSERT(control->get_symbol() != nullptr);
  ROSE_ASSERT(control->get_symbol()->get_name() == "counter");
  SgBasicBlock *controlScope =
      isSgBasicBlock(impliedDo->get_implied_do_scope());
  ROSE_ASSERT(controlScope != nullptr);
  ROSE_ASSERT(controlScope->get_parent() == impliedDo);
  ROSE_ASSERT(controlScope->get_implied_do_construction_scope() == nullptr);
  ROSE_ASSERT(control->get_symbol()->get_scope() == controlScope);
  SgInitializedName *controlDeclaration =
      control->get_symbol()->get_declaration();
  ROSE_ASSERT(controlDeclaration != nullptr);
  ROSE_ASSERT(controlDeclaration->get_scope() == controlScope);
  SgVariableDeclaration *controlOwner =
      isSgVariableDeclaration(controlDeclaration->get_parent());
  ROSE_ASSERT(controlOwner != nullptr);
  SgAuxiliaryDeclarationList *controlAuxiliary =
      isSgAuxiliaryDeclarationList(controlOwner->get_parent());
  ROSE_ASSERT(controlAuxiliary != nullptr);
  ROSE_ASSERT(controlAuxiliary->get_parent() == controlScope);
  ROSE_ASSERT(controlScope->get_auxiliary_declarations() == controlAuxiliary);
  ROSE_ASSERT(!controlScope->statementExistsInScope(controlOwner));
  SgScopeStatement *outerScope = controlScope->get_scope();
  ROSE_ASSERT(outerScope != nullptr);
  SgVariableSymbol *outerCounter =
      outerScope->lookup_variable_symbol("counter");
  ROSE_ASSERT(outerCounter != nullptr);
  ROSE_ASSERT(outerCounter != control->get_symbol());
  ROSE_ASSERT(requireIntegerValue(initialization->get_rhs_operand()) == 1);
  ROSE_ASSERT(requireIntegerValue(impliedDo->get_last_val()) == 0);
  ROSE_ASSERT(requireIntegerValue(impliedDo->get_increment()) == 1);
  ROSE_ASSERT(impliedDo->get_object_list() != nullptr);
  ROSE_ASSERT(impliedDo->get_object_list()->get_parent() == impliedDo);
  ROSE_ASSERT(impliedDo->get_object_list()->get_expressions().size() == 1);

  std::size_t controlReferenceCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(impliedDo, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != nullptr);
    if (reference->get_symbol() == control->get_symbol()) {
      ++controlReferenceCount;
    }
  }
  ROSE_ASSERT(controlReferenceCount == 2);
}

void verifyDynamicCharacterResultPredeclaration(SgProject *project) {
  SgProcedureHeaderStatement *function = findDefiningProcedure(
      project, "rex_dynamic_character_result_predeclaration");
  SgFunctionType *functionType = function->get_type();
  ROSE_ASSERT(functionType != nullptr);
  SgTypeString *resultType =
      isSgTypeString(elementType(functionType->get_return_type()));
  ROSE_ASSERT(resultType != nullptr);
  ROSE_ASSERT(!resultType->get_fortran_dynamic_length_pending());
  SgExpression *length = resultType->get_lengthExpression();
  ROSE_ASSERT(length != nullptr);
  assertExactPhysicalExpressionSource(length);

  SgInitializedName *result = function->get_result_name();
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_name() == "value");
  ROSE_ASSERT(result->get_type() == functionType->get_return_type());
}

void verifyDynamicCharacterPointerSignature(SgProject *project) {
  const auto requireDynamicPointerCharacter =
      [](SgType *type, const std::string &lengthName) {
        while (SgModifierType *modifier = isSgModifierType(type)) {
          type = modifier->get_base_type();
          ROSE_ASSERT(type != nullptr);
        }
        SgPointerType *pointer = isSgPointerType(type);
        ROSE_ASSERT(pointer != nullptr);
        type = pointer->get_base_type();
        while (SgModifierType *modifier = isSgModifierType(type)) {
          type = modifier->get_base_type();
          ROSE_ASSERT(type != nullptr);
        }
        SgArrayType *array = isSgArrayType(type);
        ROSE_ASSERT(array != nullptr);
        SgTypeString *characterType =
            isSgTypeString(elementType(array->get_base_type()));
        ROSE_ASSERT(characterType != nullptr);
        ROSE_ASSERT(!characterType->get_fortran_dynamic_length_pending());
        ROSE_ASSERT(!characterType->get_fortran_dynamic_result_length());
        SgVarRefExp *length =
            isSgVarRefExp(characterType->get_lengthExpression());
        ROSE_ASSERT(length != nullptr);
        ROSE_ASSERT(length->get_symbol() != nullptr);
        ROSE_ASSERT(length->get_symbol()->get_name() == lengthName);
        assertExactPhysicalExpressionSource(length);
        ROSE_ASSERT(!type->get_mangled().is_null());
        ROSE_ASSERT(!pointer->get_mangled().is_null());
      };

  SgProcedureHeaderStatement *function =
      findDefiningProcedure(project, "rex_make_pointer");
  ROSE_ASSERT(function->get_type() != nullptr);
  requireDynamicPointerCharacter(function->get_type()->get_return_type(),
                                 "rex_length");
  SgInitializedName *result = function->get_result_name();
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_name() == "rex_result");
  ROSE_ASSERT(result->get_type() == function->get_type()->get_return_type());

  SgProcedureHeaderStatement *subroutine =
      findDefiningProcedure(project, "rex_bind_pointer");
  SgInitializedName *value = nullptr;
  for (SgInitializedName *argument :
       subroutine->get_parameterList()->get_args()) {
    ROSE_ASSERT(argument != nullptr);
    if (argument->get_name() == "rex_value") {
      ROSE_ASSERT(value == nullptr);
      value = argument;
    }
  }
  ROSE_ASSERT(value != nullptr);
  requireDynamicPointerCharacter(value->get_type(), "rex_length");
}

void verifyUseProcedureArraySource(SgProject *project) {
  SgFunctionRefExp *renamedReference = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionRefExp)) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    ROSE_ASSERT(reference != nullptr);
    SgFunctionSymbol *sourceVisible =
        reference->get_fortran_source_visible_symbol();
    if (sourceVisible != nullptr &&
        sourceVisible->get_name() == "rex_local_scale") {
      ROSE_ASSERT(renamedReference == nullptr);
      renamedReference = reference;
    }
  }
  ROSE_ASSERT(renamedReference != nullptr);
  ROSE_ASSERT(renamedReference->get_fortran_source_visible_binding_kind() ==
              SgFunctionRefExp::e_fortran_source_visible_binding_use_rename);
  SgRenameSymbol *renamed =
      isSgRenameSymbol(renamedReference->get_fortran_source_visible_symbol());
  ROSE_ASSERT(renamed != nullptr);
  ROSE_ASSERT(renamed->get_original_symbol() != nullptr);
  ROSE_ASSERT(renamedReference->get_symbol() == renamed->get_original_symbol());
  ROSE_ASSERT(renamed->get_original_symbol()->get_name() == "rex_scale_array");
  SgScopeStatement *useScope =
      SageInterface::getEnclosingScope(renamedReference);
  ROSE_ASSERT(useScope != nullptr);
  ROSE_ASSERT(renamed->get_scope() == useScope);
  ROSE_ASSERT(useScope->lookup_function_symbol("rex_local_scale",
                                               renamed->get_type()) == renamed);
  assertExactPhysicalExpressionSource(renamedReference);

  SgProcedureHeaderStatement *function =
      findDefiningProcedure(project, "rex_scale_array");
  const auto assertArrayShapeSource = [](SgType *type) {
    while (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    }
    SgArrayType *array = isSgArrayType(type);
    ROSE_ASSERT(array != nullptr);
    SgExprListExp *dimensions = array->get_dim_info();
    ROSE_ASSERT(dimensions != nullptr);
    ROSE_ASSERT(!dimensions->get_expressions().empty());
    assertCompleteOwnedExpressionSource(dimensions);
    for (SgExpression *dimension : dimensions->get_expressions()) {
      assertCompleteOwnedExpressionSource(dimension);
    }
  };

  SgFunctionParameterList *parameters = function->get_parameterList();
  ROSE_ASSERT(parameters != nullptr);
  ROSE_ASSERT(parameters->get_args().size() == 1);
  SgInitializedName *values = parameters->get_args().front();
  ROSE_ASSERT(values != nullptr);
  ROSE_ASSERT(values->get_name() == "values");
  assertArrayShapeSource(values->get_type());
  ROSE_ASSERT(values->get_fortran_source_type() == nullptr);

  SgInitializedName *valuesDeclaration = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(function->get_definition(),
                                              V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    for (SgInitializedName *name : declaration->get_variables()) {
      if (name != nullptr && name->get_name() == "values") {
        ROSE_ASSERT(valuesDeclaration == nullptr);
        valuesDeclaration = name;
      }
    }
  }
  ROSE_ASSERT(valuesDeclaration != nullptr);
  ROSE_ASSERT(valuesDeclaration != values);
  ROSE_ASSERT(valuesDeclaration->get_type() == values->get_type());
  ROSE_ASSERT(valuesDeclaration->get_fortran_source_type() != nullptr);
  assertArrayShapeSource(valuesDeclaration->get_fortran_source_type());

  SgInitializedName *result = function->get_result_name();
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_name() == "scaled");
  assertArrayShapeSource(result->get_type());
  ROSE_ASSERT(result->get_fortran_source_type() != nullptr);
  assertArrayShapeSource(result->get_fortran_source_type());

  std::size_t ownedShapeExpressionCount = 0;
  for (SgType *type :
       {values->get_type(), valuesDeclaration->get_fortran_source_type(),
        result->get_type(), result->get_fortran_source_type()}) {
    while (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    }
    SgArrayType *array = isSgArrayType(type);
    ROSE_ASSERT(array != nullptr);
    ownedShapeExpressionCount +=
        1 + array->get_dim_info()->get_expressions().size();
  }
  ROSE_ASSERT(ownedShapeExpressionCount >= 8);
}

void verifyUseOverloadedProcedureIdentity(SgProject *project) {
  std::set<SgFunctionDeclaration *> declarations;
  std::size_t callCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgFunctionRefExp *reference = isSgFunctionRefExp(call->get_function());
    SgFunctionSymbol *sourceVisible =
        reference != nullptr ? reference->get_fortran_source_visible_symbol()
                             : nullptr;
    if (reference == nullptr || sourceVisible == nullptr ||
        sourceVisible->get_name() != "rex_local_scale") {
      continue;
    }
    ROSE_ASSERT(
        reference->get_fortran_source_visible_binding_kind() ==
        SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload);
    SgRenameSymbol *renamed = isSgRenameSymbol(sourceVisible);
    ROSE_ASSERT(renamed != nullptr);
    SgFunctionDeclaration *declaration = renamed->get_declaration();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(reference->get_symbol() == renamed->get_original_symbol());
    ROSE_ASSERT(declaration->get_name() == "rex_scale_integer" ||
                declaration->get_name() == "rex_scale_real");
    declarations.insert(declaration);
    SgExprListExp *arguments = call->get_args();
    ROSE_ASSERT(arguments != nullptr);
    ROSE_ASSERT(arguments->get_parent() == call);
    ROSE_ASSERT(arguments->get_expressions().size() == 1);
    assertExactPhysicalExpressionSource(arguments);
    assertExactPhysicalExpressionSource(reference);
    assertExactPhysicalExpressionSource(call);
    SgScopeStatement *scope = SageInterface::getEnclosingScope(call);
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(scope->lookup_function_symbol(
                    "rex_local_scale", declaration->get_type()) == renamed);
    ++callCount;
  }
  ROSE_ASSERT(callCount == 2);
  ROSE_ASSERT(declarations.size() == 2);
}

void verifyIntrinsicProcedureReexportIdentity(SgProject *project) {
  const std::set<std::string> expectedCalls = {"c_associated", "c_f_pointer",
                                               "c_loc"};
  const std::set<std::string> expectedIntrinsicProducers = {
      "__builtin_c_f_pointer", "__builtin_c_loc"};
  std::set<std::string> observedCalls;
  std::set<std::string> observedIntrinsicProducers;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgFunctionRefExp *reference = isSgFunctionRefExp(call->get_function());
    SgFunctionSymbol *sourceVisible =
        reference != nullptr ? reference->get_fortran_source_visible_symbol()
                             : nullptr;
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        sourceVisible == nullptr ||
        expectedCalls.count(sourceVisible->get_name().getString()) == 0) {
      continue;
    }

    SgFunctionSymbol *symbol = reference->get_symbol();
    SgFunctionDeclaration *declaration = symbol->get_declaration();
    SgFunctionType *type =
        declaration != nullptr ? declaration->get_type() : nullptr;
    SgScopeStatement *callScope = SageInterface::getEnclosingScope(call);
    SgScopeStatement *bindingScope = sourceVisible->get_scope();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(type->get_return_type() != nullptr);
    ROSE_ASSERT(isSgTypeUnknown(type->get_return_type()) == nullptr);
    ROSE_ASSERT(callScope != nullptr);
    ROSE_ASSERT(bindingScope != nullptr);
    ROSE_ASSERT(SageInterface::isAncestor(bindingScope, call));
    ROSE_ASSERT(bindingScope->lookup_function_symbol(
                    sourceVisible->get_name(), sourceVisible->get_type()) ==
                sourceVisible);
    if (sourceVisible->get_name() == "c_associated") {
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_generic_overload);
      ROSE_ASSERT(symbol->get_name() == "c_associated_c_ptr");
    } else {
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow);
      SgRenameSymbol *renamed = isSgRenameSymbol(sourceVisible);
      ROSE_ASSERT(renamed != nullptr);
      ROSE_ASSERT(std::any_of(
          renamed->get_causal_nodes().begin(),
          renamed->get_causal_nodes().end(),
          [](SgNode *cause) { return isSgUseStatement(cause) != nullptr; }));
      SgFunctionSymbol *producer =
          isSgFunctionSymbol(renamed->get_original_symbol());
      SgProcedureHeaderStatement *producerDeclaration =
          producer != nullptr
              ? isSgProcedureHeaderStatement(producer->get_declaration())
              : nullptr;
      ROSE_ASSERT(producer != nullptr);
      ROSE_ASSERT(producerDeclaration != nullptr);
      ROSE_ASSERT(producerDeclaration->get_definition() == nullptr);
      ROSE_ASSERT(producerDeclaration->get_type() != nullptr);
      ROSE_ASSERT(producerDeclaration->get_fortran_procedure_source_form() ==
                  SgProcedureHeaderStatement::
                      e_fortran_procedure_source_form_semantic_only);
      ROSE_ASSERT(producerDeclaration->get_firstNondefiningDeclaration() ==
                  producerDeclaration);
      ROSE_ASSERT(producerDeclaration->get_symbol_from_symbol_table() ==
                  producer);
      observedIntrinsicProducers.insert(
          producerDeclaration->get_name().getString());
    }
    ROSE_ASSERT(
        observedCalls.insert(sourceVisible->get_name().getString()).second);
    assertExactPhysicalExpressionSource(reference);
    assertExactPhysicalExpressionSource(call);
  }
  ROSE_ASSERT(observedCalls == expectedCalls);
  ROSE_ASSERT(observedIntrinsicProducers == expectedIntrinsicProducers);
}

void verifyFunctionSourceVisibleBinding(SgProject *project) {
  const std::set<std::string> expectedCalls = {"abs", "lbound",
                                               "rex_function_binding_factorial",
                                               "rex_function_binding_length"};
  const std::set<std::string> expectedDistinctBindings = {"abs"};
  const std::set<std::string> expectedSemanticPublications = {
      "lbound", "rex_function_binding_length"};
  std::set<std::string> observedCalls;
  std::set<std::string> observedDistinctBindings;
  std::set<std::string> observedSemanticPublications;

  std::vector<SgFunctionRefExp *> references;
  std::set<SgFunctionRefExp *> uniqueReferences;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionRefExp)) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    if (uniqueReferences.insert(reference).second) {
      references.push_back(reference);
    }
  }
  SgProcedureHeaderStatement *dynamicFunction =
      findDefiningProcedure(project, "rex_function_binding_dynamic");
  SgFunctionType *dynamicType = dynamicFunction->get_type();
  auto appendSemanticPublicationReferences = [&](SgType *type) {
    SgTypeString *stringType =
        type != nullptr ? isSgTypeString(elementType(type)) : nullptr;
    SgExpression *length =
        stringType != nullptr ? stringType->get_lengthExpression() : nullptr;
    if (length == nullptr) {
      return;
    }
    for (SgNode *node : NodeQuery::querySubTree(length, V_SgFunctionRefExp)) {
      SgFunctionRefExp *reference = isSgFunctionRefExp(node);
      if (reference != nullptr &&
          reference->get_fortran_source_visible_binding_kind() ==
              SgFunctionRefExp::
                  e_fortran_source_visible_binding_semantic_publication &&
          uniqueReferences.insert(reference).second) {
        references.push_back(reference);
      }
    }
  };
  appendSemanticPublicationReferences(
      dynamicType != nullptr ? dynamicType->get_return_type() : nullptr);
  if (dynamicFunction->get_type_syntax_is_available()) {
    SgFunctionType *syntaxType = dynamicFunction->get_type_syntax();
    appendSemanticPublicationReferences(
        syntaxType != nullptr ? syntaxType->get_return_type() : nullptr);
  }
  SgInitializedName *dynamicResultName = dynamicFunction->get_result_name();
  ROSE_ASSERT(dynamicResultName != nullptr);
  appendSemanticPublicationReferences(dynamicResultName->get_type());
  appendSemanticPublicationReferences(
      dynamicResultName->get_fortran_source_type());

  for (SgFunctionRefExp *reference : references) {
    ROSE_ASSERT(reference != nullptr);
    SgFunctionSymbol *semantic = reference->get_symbol();
    if (semantic == nullptr ||
        expectedCalls.count(semantic->get_name().getString()) == 0) {
      continue;
    }
    SgFunctionDeclaration *semanticDeclaration = semantic->get_declaration();

    SgFunctionSymbol *sourceVisible =
        reference->get_fortran_source_visible_symbol();
    SgFunctionDeclaration *sourceDeclaration =
        sourceVisible != nullptr ? sourceVisible->get_declaration() : nullptr;
    SgScopeStatement *sourceScope =
        sourceVisible != nullptr ? sourceVisible->get_scope() : nullptr;
    SgSymbolTable *sourceTable =
        sourceScope != nullptr ? sourceScope->get_symbol_table() : nullptr;
    ROSE_ASSERT(sourceVisible != nullptr);
    ROSE_ASSERT(semanticDeclaration != nullptr);
    ROSE_ASSERT(sourceDeclaration != nullptr);
    ROSE_ASSERT(sourceVisible->get_name() == semantic->get_name());
    ROSE_ASSERT(sourceScope != nullptr);
    ROSE_ASSERT(sourceTable != nullptr);
    ROSE_ASSERT(sourceVisible->get_parent() == sourceTable);
    ROSE_ASSERT(sourceTable->exists(sourceVisible));

    const bool semanticPublicationKind =
        reference->get_fortran_source_visible_binding_kind() ==
        SgFunctionRefExp::e_fortran_source_visible_binding_semantic_publication;
    SgScopeStatement *useScope =
        semanticPublicationKind ? nullptr
                                : SageInterface::getEnclosingScope(reference);
    SgFunctionSymbol *visibleByType =
        useScope != nullptr && semanticDeclaration->get_type() != nullptr
            ? SageInterface::lookupFunctionSymbolInParentScopes(
                  sourceVisible->get_name(), semanticDeclaration->get_type(),
                  useScope)
            : nullptr;
    SgStatement *useStatement = SageInterface::getEnclosingStatement(reference);
    const bool sourceScopeOwnsUse =
        useStatement != nullptr &&
        (sourceScope == useScope ||
         SageInterface::isAncestor(sourceScope, useStatement));
    const bool exactTypedBinding =
        reference->get_fortran_source_visible_binding_kind() ==
            SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed &&
        useScope != nullptr && visibleByType == sourceVisible;
    const bool exactIntrinsicShadow =
        reference->get_fortran_source_visible_binding_kind() ==
            SgFunctionRefExp::
                e_fortran_source_visible_binding_intrinsic_shadow &&
        useScope != nullptr && visibleByType != sourceVisible &&
        sourceScopeOwnsUse;
    SgProcedureHeaderStatement *semanticProcedure =
        isSgProcedureHeaderStatement(semanticDeclaration);
    SgAuxiliaryDeclarationList *semanticOwner =
        semanticProcedure != nullptr
            ? isSgAuxiliaryDeclarationList(semanticProcedure->get_parent())
            : nullptr;
    Sg_File_Info *semanticSource = semanticProcedure != nullptr
                                       ? semanticProcedure->get_file_info()
                                       : nullptr;
    const bool exactSemanticPublication =
        semanticPublicationKind && semantic == sourceVisible &&
        semanticProcedure != nullptr &&
        semanticProcedure->get_firstNondefiningDeclaration() ==
            semanticProcedure &&
        semanticProcedure->get_fortran_procedure_source_form() ==
            SgProcedureHeaderStatement::
                e_fortran_procedure_source_form_semantic_only &&
        semanticOwner != nullptr &&
        semanticOwner->get_parent() == sourceScope &&
        sourceScope->get_auxiliary_declarations() == semanticOwner &&
        semanticSource != nullptr && semanticSource->isCompilerGenerated() &&
        semanticSource->isOutputInCodeGeneration();
    if (!exactTypedBinding && !exactIntrinsicShadow &&
        !exactSemanticPublication) {
      std::cerr << "REX_TEST_INVARIANT[function-source-visible-binding]: "
                   "reference="
                << reference << " semantic=" << semantic
                << " source-visible=" << sourceVisible << " kind="
                << static_cast<int>(
                       reference->get_fortran_source_visible_binding_kind())
                << " visible-by-type=" << visibleByType
                << " source-scope=" << sourceScope << " use-scope=" << useScope
                << " use-statement=" << useStatement
                << " source-scope-owns-use=" << sourceScopeOwnsUse << "\n";
      ROSE_ABORT();
    }

    const std::string name = semantic->get_name().getString();
    observedCalls.insert(name);
    if (exactIntrinsicShadow) {
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow);
      observedDistinctBindings.insert(name);
    } else if (exactTypedBinding) {
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed);
    } else {
      ROSE_ASSERT(exactSemanticPublication);
      observedSemanticPublications.insert(name);
    }
    assertExactPhysicalExpressionSource(reference);
  }

  if (observedCalls != expectedCalls ||
      observedDistinctBindings != expectedDistinctBindings ||
      observedSemanticPublications != expectedSemanticPublications) {
    auto printSet = [](const char *label, const std::set<std::string> &values) {
      std::cerr << label << "={";
      bool first = true;
      for (const std::string &value : values) {
        std::cerr << (first ? "" : ",") << value;
        first = false;
      }
      std::cerr << "}\n";
    };
    printSet("expected-calls", expectedCalls);
    printSet("observed-calls", observedCalls);
    printSet("expected-distinct", expectedDistinctBindings);
    printSet("observed-distinct", observedDistinctBindings);
    printSet("expected-semantic-publications", expectedSemanticPublications);
    printSet("observed-semantic-publications", observedSemanticPublications);
    ROSE_ABORT();
  }
}

void verifyLabeledDoHeaderIdentity(SgProject *project) {
  std::vector<SgNode *> loops = NodeQuery::querySubTree(project, V_SgFortranDo);
  ROSE_ASSERT(loops.size() == 1);
  SgFortranDo *loop = isSgFortranDo(loops.front());
  ROSE_ASSERT(loop != nullptr);

  SgLabelRefExp *headerLabel = loop->get_numeric_label();
  SgLabelSymbol *headerSymbol =
      headerLabel != nullptr ? headerLabel->get_symbol() : nullptr;
  ROSE_ASSERT(headerLabel != nullptr);
  ROSE_ASSERT(headerLabel->get_parent() == loop);
  ROSE_ASSERT(headerSymbol != nullptr);
  ROSE_ASSERT(headerSymbol->get_numeric_label_value() == 2);
  ROSE_ASSERT(headerSymbol->get_label_type() ==
              SgLabelSymbol::e_start_label_type);
  ROSE_ASSERT(headerSymbol->get_fortran_statement() == loop);
  assertExactPhysicalLocatedSource(loop);

  std::vector<SgNode *> gotos =
      NodeQuery::querySubTree(project, V_SgGotoStatement);
  ROSE_ASSERT(gotos.size() == 1);
  SgGotoStatement *gotoStatement = isSgGotoStatement(gotos.front());
  ROSE_ASSERT(gotoStatement != nullptr);
  SgLabelRefExp *target =
      isSgLabelRefExp(gotoStatement->get_label_expression());
  ROSE_ASSERT(target != nullptr);
  ROSE_ASSERT(target->get_parent() == gotoStatement);
  ROSE_ASSERT(target->get_symbol() == headerSymbol);
  ROSE_ASSERT(target->get_numeric_label_value() == 2);
  assertExactPhysicalLocatedSource(gotoStatement);
}

void verifyComponentCharacterLengthContract(SgProject *project) {
  std::vector<SgInitializedName *> fields;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName != nullptr && initializedName->get_name() == "field") {
      fields.push_back(initializedName);
    }
  }
  ROSE_ASSERT(fields.size() == 1);
  SgInitializedName *field = fields.front();
  SgType *sourceType = elementType(field->get_fortran_source_type());
  SgType *semanticType = elementType(field->get_type());
  SgTypeString *sourceString = isSgTypeString(sourceType);
  ROSE_ASSERT(sourceString != nullptr);
  ROSE_ASSERT(sourceString->get_fortran_source_syntax());
  SgLongLongIntVal *length =
      isSgLongLongIntVal(sourceString->get_lengthExpression());
  ROSE_ASSERT(length != nullptr);
  ROSE_ASSERT(length->get_value() == 7);
  ROSE_ASSERT(length->get_valueString() == "7");
  ROSE_ASSERT(length->get_parent() == sourceString);
  assertExactPhysicalExpressionSource(length);
  ROSE_ASSERT(SageInterface::fortranSourceTypeMatchesSemanticType(
      sourceType, semanticType));
}

void verifyIntrinsicModuleTypeIdentity(SgProject *project) {
  SgClassType *canonical = findDefinedClassType(project, "__builtin_c_ptr");

  const std::vector<SgInitializedName *> nestedDeclarations =
      findNames(project, "rex_intrinsic_pointer");
  const std::vector<SgInitializedName *> directDeclarations =
      findNames(project, "rex_direct_pointer");
  ROSE_ASSERT(nestedDeclarations.size() == 4);
  ROSE_ASSERT(directDeclarations.size() == 1);
  std::vector<SgInitializedName *> declarations = nestedDeclarations;
  declarations.push_back(directDeclarations.front());
  std::size_t sourceBindings = 0;
  std::size_t semanticDeclarations = 0;
  for (SgInitializedName *declaration : declarations) {
    ROSE_ASSERT(elementType(declaration->get_type()) == canonical);
    SgSymbol *source = declaration->get_fortran_source_derived_type_symbol();
    if (source == nullptr) {
      ++semanticDeclarations;
      continue;
    }
    ++sourceBindings;
    while (SgAliasSymbol *alias = isSgAliasSymbol(source)) {
      source = alias->get_alias();
      ROSE_ASSERT(source != nullptr);
    }
    SgClassSymbol *sourceClass = isSgClassSymbol(source);
    ROSE_ASSERT(sourceClass != nullptr);
    ROSE_ASSERT(sourceClass->get_declaration() != nullptr);
    ROSE_ASSERT(sourceClass->get_declaration()->get_type() == canonical);
  }
  ROSE_ASSERT(sourceBindings == 2);
  ROSE_ASSERT(semanticDeclarations == 3);

  SgModuleStatement *intrinsicModule = nullptr;
  std::size_t intrinsicUses = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgUseStatement)) {
    SgUseStatement *use = isSgUseStatement(node);
    ROSE_ASSERT(use != nullptr);
    if (use->get_name() != "iso_c_binding") {
      continue;
    }
    ROSE_ASSERT(use->get_module() != nullptr);
    ROSE_ASSERT(intrinsicModule == nullptr ||
                intrinsicModule == use->get_module());
    intrinsicModule = use->get_module();
    ++intrinsicUses;
  }
  ROSE_ASSERT(intrinsicUses >= 2);
}

void verifyCoarrayImageSelectorContract(SgProject *project) {
  auto resolveClass = [](SgSymbol *symbol) {
    std::set<SgSymbol *> visited;
    while (symbol != nullptr) {
      ROSE_ASSERT(visited.insert(symbol).second);
      if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
        symbol = alias->get_alias();
      } else if (SgRenameSymbol *renamed = isSgRenameSymbol(symbol)) {
        symbol = renamed->get_original_symbol();
      } else {
        break;
      }
    }
    return isSgClassSymbol(symbol);
  };

  SgUseStatement *intrinsicUse = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgUseStatement)) {
    SgUseStatement *use = isSgUseStatement(node);
    ROSE_ASSERT(use != nullptr);
    if (use->get_name() != "iso_fortran_env") {
      continue;
    }
    ROSE_ASSERT(intrinsicUse == nullptr);
    intrinsicUse = use;
  }
  ROSE_ASSERT(intrinsicUse != nullptr);
  ROSE_ASSERT(intrinsicUse->get_module_nature().empty());

  SgModuleStatement *intrinsicModule = intrinsicUse->get_module();
  ROSE_ASSERT(intrinsicModule != nullptr);
  SgClassDefinition *moduleDefinition = intrinsicModule->get_definition();
  ROSE_ASSERT(moduleDefinition != nullptr);
  SgSymbolTable *moduleSymbols = moduleDefinition->get_symbol_table();
  ROSE_ASSERT(moduleSymbols != nullptr);
  SgSymbol *teamTypeBinding =
      moduleSymbols->find_any("team_type", nullptr, nullptr);
  SgClassSymbol *teamTypeClass = resolveClass(teamTypeBinding);
  ROSE_ASSERT(teamTypeClass != nullptr);
  ROSE_ASSERT(teamTypeClass->get_declaration() != nullptr);
  ROSE_ASSERT(teamTypeClass->get_name() == "__builtin_team_type");
  SgClassType *canonical = teamTypeClass->get_declaration()->get_type();
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_declaration() == teamTypeClass->get_declaration());

  const std::vector<SgInitializedName *> teams = findNames(project, "team");
  const std::vector<SgInitializedName *> stats = findNames(project, "stat");
  const std::vector<SgInitializedName *> teamNumbers =
      findNames(project, "team_number");
  const std::vector<SgInitializedName *> coarrays = findNames(project, "a");
  ROSE_ASSERT(teams.size() == 1);
  ROSE_ASSERT(stats.size() == 1);
  ROSE_ASSERT(teamNumbers.size() == 1);
  ROSE_ASSERT(coarrays.size() == 1);
  SgInitializedName *team = teams.front();
  SgInitializedName *stat = stats.front();
  SgInitializedName *teamNumber = teamNumbers.front();
  SgInitializedName *coarray = coarrays.front();
  ROSE_ASSERT(elementType(team->get_type()) == canonical);
  SgClassSymbol *sourceTeamType =
      resolveClass(team->get_fortran_source_derived_type_symbol());
  ROSE_ASSERT(sourceTeamType == teamTypeClass);

  std::size_t statOnlySelectors = 0;
  std::size_t teamSelectors = 0;
  std::size_t teamNumberSelectors = 0;
  const Rose_STL_Container<SgNode *> coindexedExpressions =
      NodeQuery::querySubTree(project, V_SgCAFCoExpression);
  ROSE_ASSERT(coindexedExpressions.size() == 3);
  for (SgNode *node : coindexedExpressions) {
    SgCAFCoExpression *coindexed = isSgCAFCoExpression(node);
    ROSE_ASSERT(coindexed != nullptr);
    SgVarRefExp *base = isSgVarRefExp(coindexed->get_referData());
    ROSE_ASSERT(base != nullptr);
    ROSE_ASSERT(base->get_symbol()->get_declaration() == coarray);
    ROSE_ASSERT(base->get_parent() == coindexed);

    SgCAFImageSelectorExp *selector =
        isSgCAFImageSelectorExp(coindexed->get_teamRank());
    ROSE_ASSERT(selector != nullptr);
    ROSE_ASSERT(selector->get_parent() == coindexed);
    assertExactPhysicalExpressionSource(selector);

    SgExprListExp *cosubscripts = selector->get_cosubscripts();
    ROSE_ASSERT(cosubscripts != nullptr);
    ROSE_ASSERT(cosubscripts->get_parent() == selector);
    ROSE_ASSERT(cosubscripts->get_expressions().size() == 1);
    assertExactPhysicalExpressionSource(cosubscripts);

    SgVarRefExp *statReference = isSgVarRefExp(selector->get_stat_expression());
    ROSE_ASSERT(statReference != nullptr);
    ROSE_ASSERT(statReference->get_symbol()->get_declaration() == stat);
    ROSE_ASSERT(statReference->get_parent() == selector);

    SgExpression *teamExpression = selector->get_team_expression();
    SgExpression *teamNumberExpression = selector->get_team_number_expression();
    if (teamExpression != nullptr) {
      SgVarRefExp *teamReference = isSgVarRefExp(teamExpression);
      ROSE_ASSERT(teamReference != nullptr);
      ROSE_ASSERT(teamReference->get_symbol()->get_declaration() == team);
      ROSE_ASSERT(elementType(teamReference->get_type()) == canonical);
      ROSE_ASSERT(teamReference->get_parent() == selector);
      ROSE_ASSERT(teamNumberExpression == nullptr);
      ++teamSelectors;
    } else if (teamNumberExpression != nullptr) {
      SgVarRefExp *teamNumberReference = isSgVarRefExp(teamNumberExpression);
      ROSE_ASSERT(teamNumberReference != nullptr);
      ROSE_ASSERT(teamNumberReference->get_symbol()->get_declaration() ==
                  teamNumber);
      ROSE_ASSERT(teamNumberReference->get_parent() == selector);
      ++teamNumberSelectors;
    } else {
      ++statOnlySelectors;
    }
  }
  ROSE_ASSERT(statOnlySelectors == 1);
  ROSE_ASSERT(teamSelectors == 1);
  ROSE_ASSERT(teamNumberSelectors == 1);
}

void verifyIntrinsicModuleShadowIdentity(SgProject *project) {
  SgUseStatement *intrinsicUse = nullptr;
  SgUseStatement *nonIntrinsicUse = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgUseStatement)) {
    SgUseStatement *use = isSgUseStatement(node);
    ROSE_ASSERT(use != nullptr);
    if (use->get_name() != "iso_fortran_env") {
      continue;
    }
    ROSE_ASSERT(use->get_module() != nullptr);
    if (use->get_module_nature() == "Intrinsic") {
      ROSE_ASSERT(intrinsicUse == nullptr);
      intrinsicUse = use;
    } else if (use->get_module_nature() == "Non_Intrinsic") {
      ROSE_ASSERT(nonIntrinsicUse == nullptr);
      nonIntrinsicUse = use;
    } else {
      ROSE_ABORT();
    }
  }

  ROSE_ASSERT(intrinsicUse != nullptr);
  ROSE_ASSERT(nonIntrinsicUse != nullptr);
  SgModuleStatement *intrinsicModule = intrinsicUse->get_module();
  SgModuleStatement *nonIntrinsicModule = nonIntrinsicUse->get_module();
  ROSE_ASSERT(intrinsicModule != nonIntrinsicModule);
  ROSE_ASSERT(intrinsicModule->get_name() == "iso_fortran_env");
  ROSE_ASSERT(nonIntrinsicModule->get_name() == "iso_fortran_env");
  ROSE_ASSERT(intrinsicModule->get_scope() != nonIntrinsicModule->get_scope());
  ROSE_ASSERT(intrinsicModule->get_type() != nonIntrinsicModule->get_type());
  ROSE_ASSERT(intrinsicModule->get_definition() != nullptr);
  ROSE_ASSERT(nonIntrinsicModule->get_definition() != nullptr);
  ROSE_ASSERT(intrinsicModule->get_definition()->get_declaration() ==
              intrinsicModule);
  ROSE_ASSERT(nonIntrinsicModule->get_definition()->get_declaration() ==
              nonIntrinsicModule);

  const std::vector<SgInitializedName *> shadowDeclarations =
      findNames(project, "rex_shadow_value");
  ROSE_ASSERT(shadowDeclarations.size() == 2);
  SgInitializedName *shadowDeclaration = nullptr;
  SgInitializedName *publishedDeclaration = nullptr;
  for (SgInitializedName *declaration : shadowDeclarations) {
    if (declaration->get_scope() == nonIntrinsicModule->get_definition()) {
      ROSE_ASSERT(shadowDeclaration == nullptr);
      shadowDeclaration = declaration;
    } else {
      ROSE_ASSERT(publishedDeclaration == nullptr);
      publishedDeclaration = declaration;
    }
  }
  ROSE_ASSERT(shadowDeclaration != nullptr);
  ROSE_ASSERT(publishedDeclaration != nullptr);
  SgVariableSymbol *shadowSymbol =
      nonIntrinsicModule->get_definition()->lookup_variable_symbol(
          "rex_shadow_value");
  ROSE_ASSERT(shadowSymbol != nullptr);
  ROSE_ASSERT(shadowSymbol->get_declaration() == shadowDeclaration);
  ROSE_ASSERT(intrinsicModule->get_definition()->lookup_variable_symbol(
                  "rex_shadow_value") == nullptr);

  SgProcedureHeaderStatement *consumer =
      findDefiningProcedure(project, "rex_use_nonintrinsic_module_shadow");
  ROSE_ASSERT(consumer->get_definition() != nullptr);
  SgBasicBlock *consumerBody = consumer->get_definition()->get_body();
  ROSE_ASSERT(consumerBody != nullptr);
  ROSE_ASSERT(publishedDeclaration->get_scope() == consumerBody);
  ROSE_ASSERT(publishedDeclaration->get_type() ==
              shadowDeclaration->get_type());
  SgAuxiliaryDeclarationList *auxiliary = isSgAuxiliaryDeclarationList(
      publishedDeclaration->get_declaration()->get_parent());
  ROSE_ASSERT(auxiliary != nullptr);
  ROSE_ASSERT(auxiliary->get_parent() == consumerBody);
  SgVariableSymbol *publishedSymbol =
      consumerBody->lookup_variable_symbol("rex_shadow_value");
  ROSE_ASSERT(publishedSymbol != nullptr);
  ROSE_ASSERT(publishedSymbol->get_declaration() == publishedDeclaration);

  std::vector<SgVarRefExp *> references;
  for (SgNode *node : NodeQuery::querySubTree(consumerBody, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != nullptr);
    if (reference->get_symbol()->get_name() == "rex_shadow_value") {
      references.push_back(reference);
    }
  }
  ROSE_ASSERT(references.size() == 1);
  ROSE_ASSERT(references.front()->get_symbol() == publishedSymbol);
}

} // namespace

int main(int argc, char **argv) {
  const std::string input = inputBasename(argc, argv);
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  if (input == "rex_flang_component_character_length_contract.f90") {
    verifyComponentCharacterLengthContract(project);
  } else if (input == "rex_fortran_custom_implicit_semantic_type.f90") {
    verifyCustomImplicitTypes(project);
  } else if (input ==
             "rex_fortran_defined_intrinsic_operator_array_result.f90") {
    verifyDefinedOperatorArrayResults(project);
  } else if (input == "rex_fortran_function_result_semantic_identity.f90") {
    verifyFunctionResultIdentity(project);
  } else if (input == "rex_fortran_intrinsic_use_duplicate_same_entity.f90") {
    verifyDuplicateIntrinsicUse(project);
  } else if (input == "rex_fortran_semantic_name_and_component_identity.f90") {
    verifySemanticNameAndComponentIdentity(project);
  } else if (input == "rex_fortran_statement_function_semantic_identity.f90") {
    verifyStatementFunctionIdentity(project);
  } else if (input == "rex_fortran_zero_iteration_implied_do.f90") {
    verifyZeroIterationImpliedDo(project);
  } else if (input == "rex_flang_dynamic_character_result_predeclaration.f90") {
    verifyDynamicCharacterResultPredeclaration(project);
  } else if (input == "rex_flang_dynamic_character_pointer_signature.f90") {
    verifyDynamicCharacterPointerSignature(project);
  } else if (input == "rex_flang_use_procedure_array_source.f90") {
    verifyUseProcedureArraySource(project);
  } else if (input == "rex_flang_use_overloaded_procedure_identity.f90") {
    verifyUseOverloadedProcedureIdentity(project);
  } else if (input == "rex_flang_intrinsic_procedure_reexport_identity.f90") {
    verifyIntrinsicProcedureReexportIdentity(project);
  } else if (input == "rex_flang_intrinsic_module_shadow_identity.f90") {
    verifyIntrinsicModuleShadowIdentity(project);
  } else if (input == "rex_flang_intrinsic_module_type_identity.f90") {
    verifyIntrinsicModuleTypeIdentity(project);
  } else if (input == "rex_test2026_coarray_image_selector.f90") {
    verifyCoarrayImageSelectorContract(project);
  } else if (input == "rex_flang_function_source_visible_binding.f90") {
    verifyFunctionSourceVisibleBinding(project);
  } else if (input == "rex_flang_labeled_do_header_identity.f") {
    verifyLabeledDoHeaderIdentity(project);
  } else {
    std::cerr << "unregistered semantic regression input: " << input << '\n';
    return 2;
  }

  AstTests::runAllTests(project);
  return 0;
}
