#include "rose.h"

#include <map>
#include <string>

namespace {

enum class ContractMode {
  valid,
  missingIdentifier,
  wrongIdentifierKind,
  emptyIdentifier,
  unspecifiedIdentifier,
  missingType,
  wrongTypeKind,
  missingSemanticType,
  missingLocalScope,
  missingVariable,
  wrongVariableKind,
  wrongVariableScope,
  mismatchedVariableType,
  unmangledScope
};

SgInitializedName *
mapperVariableDeclaration(SgOmpDeclareMapperStatement *mapper) {
  ROSE_ASSERT(mapper != nullptr);
  SgVarRefExp *reference = isSgVarRefExp(mapper->get_mapper_variable());
  ROSE_ASSERT(reference != nullptr);
  SgVariableSymbol *symbol = isSgVariableSymbol(reference->get_symbol());
  ROSE_ASSERT(symbol != nullptr);
  SgInitializedName *declaration = symbol->get_declaration();
  ROSE_ASSERT(declaration != nullptr);
  return declaration;
}

SgOmpDeclareMapperStatement *mapperNamedByVariable(
    const std::map<std::string, SgOmpDeclareMapperStatement *> &mappers,
    const std::string &name) {
  const auto found = mappers.find(name);
  ROSE_ASSERT(found != mappers.end());
  ROSE_ASSERT(found->second != nullptr);
  return found->second;
}

SgTypedefType *requireTypedefType(SgProject *project, const std::string &name) {
  SgTypedefType *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration != nullptr && declaration->get_name().getString() == name) {
      ROSE_ASSERT(result == nullptr);
      result = declaration->get_type();
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

} // namespace

int main(int argc, char **argv) {
  ContractMode mode = ContractMode::valid;
  int writeIndex = 1;
  for (int readIndex = 1; readIndex < argc; ++readIndex) {
    const std::string argument = argv[readIndex];
    if (argument == "--missing-identifier") {
      mode = ContractMode::missingIdentifier;
    } else if (argument == "--wrong-identifier-kind") {
      mode = ContractMode::wrongIdentifierKind;
    } else if (argument == "--empty-identifier") {
      mode = ContractMode::emptyIdentifier;
    } else if (argument == "--unspecified-identifier") {
      mode = ContractMode::unspecifiedIdentifier;
    } else if (argument == "--missing-type") {
      mode = ContractMode::missingType;
    } else if (argument == "--wrong-type-kind") {
      mode = ContractMode::wrongTypeKind;
    } else if (argument == "--missing-semantic-type") {
      mode = ContractMode::missingSemanticType;
    } else if (argument == "--missing-local-scope") {
      mode = ContractMode::missingLocalScope;
    } else if (argument == "--missing-variable") {
      mode = ContractMode::missingVariable;
    } else if (argument == "--wrong-variable-kind") {
      mode = ContractMode::wrongVariableKind;
    } else if (argument == "--wrong-variable-scope") {
      mode = ContractMode::wrongVariableScope;
    } else if (argument == "--mismatched-variable-type") {
      mode = ContractMode::mismatchedVariableType;
    } else if (argument == "--unmangled-scope") {
      mode = ContractMode::unmangledScope;
    } else {
      argv[writeIndex++] = argv[readIndex];
    }
  }
  argc = writeIndex;
  argv[argc] = nullptr;

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  std::map<std::string, SgOmpDeclareMapperStatement *> mappers;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgOmpDeclareMapperStatement)) {
    SgOmpDeclareMapperStatement *mapper = isSgOmpDeclareMapperStatement(node);
    ROSE_ASSERT(mapper != nullptr);
    SgInitializedName *declaration = mapperVariableDeclaration(mapper);
    ROSE_ASSERT(
        mappers.emplace(declaration->get_name().getString(), mapper).second);
  }
  ROSE_ASSERT(mappers.size() == 3);

  SgOmpDeclareMapperStatement *alphaA =
      mapperNamedByVariable(mappers, "item_a");
  SgOmpDeclareMapperStatement *betaA = mapperNamedByVariable(mappers, "item_b");
  SgOmpDeclareMapperStatement *alphaB =
      mapperNamedByVariable(mappers, "item_c");

  if (mode == ContractMode::valid) {
    const SgName alphaAIdentity = alphaA->get_mangled_name();
    const SgName betaAIdentity = betaA->get_mangled_name();
    const SgName alphaBIdentity = alphaB->get_mangled_name();
    ROSE_ASSERT(!alphaAIdentity.is_null());
    ROSE_ASSERT(alphaAIdentity != betaAIdentity);
    ROSE_ASSERT(alphaAIdentity != alphaBIdentity);

    SgInitializedName *dummy = mapperVariableDeclaration(alphaA);
    SgDeclarationScope *localScope = alphaA->get_nonreal_decl_scope();
    SgVariableSymbol *symbol = isSgVariableSymbol(
        isSgVarRefExp(alphaA->get_mapper_variable())->get_symbol());
    ROSE_ASSERT(localScope != nullptr);
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(dummy->get_scope() == localScope);
    ROSE_ASSERT(localScope->lookup_variable_symbol(dummy->get_name()) ==
                symbol);
    SgTypeExpression *alphaAType =
        isSgTypeExpression(alphaA->get_mapper_type());
    SgTypeExpression *betaAType = isSgTypeExpression(betaA->get_mapper_type());
    SgTypeExpression *alphaBType =
        isSgTypeExpression(alphaB->get_mapper_type());
    ROSE_ASSERT(alphaAType != nullptr && betaAType != nullptr &&
                alphaBType != nullptr);
    SgTypedefType *typeA = requireTypedefType(project, "RexMapperIdentityA");
    SgTypedefType *typeB = requireTypedefType(project, "RexMapperIdentityB");
    ROSE_ASSERT(alphaAType->get_represented_type() == typeA);
    ROSE_ASSERT(betaAType->get_represented_type() == typeA);
    ROSE_ASSERT(alphaBType->get_represented_type() == typeB);
    ROSE_ASSERT(mapperVariableDeclaration(alphaA)->get_type() == typeA);
    ROSE_ASSERT(mapperVariableDeclaration(betaA)->get_type() == typeA);
    ROSE_ASSERT(mapperVariableDeclaration(alphaB)->get_type() == typeB);
    localScope->remove_symbol(symbol);
    dummy->set_name("renamed_dummy_without_identity_effect");
    localScope->insert_symbol(dummy->get_name(), symbol);
    ROSE_ASSERT(alphaA->get_mangled_name() == alphaAIdentity);
    return 0;
  }

  switch (mode) {
  case ContractMode::missingIdentifier:
    alphaA->set_user_defined_identifier(nullptr);
    break;
  case ContractMode::wrongIdentifierKind: {
    SgIntVal *value = SageBuilder::buildIntVal(1);
    ROSE_ASSERT(value != nullptr);
    alphaA->set_user_defined_identifier(value);
    value->set_parent(alphaA);
    break;
  }
  case ContractMode::emptyIdentifier: {
    SgOmpNameExpression *identifier =
        isSgOmpNameExpression(alphaA->get_user_defined_identifier());
    ROSE_ASSERT(identifier != nullptr);
    identifier->set_spelling("");
    break;
  }
  case ContractMode::unspecifiedIdentifier:
    alphaA->set_identifier(
        SgOmpClause::e_omp_declare_mapper_identifier_unspecified);
    break;
  case ContractMode::missingType:
    alphaA->set_mapper_type(nullptr);
    break;
  case ContractMode::wrongTypeKind: {
    SgIntVal *value = SageBuilder::buildIntVal(2);
    ROSE_ASSERT(value != nullptr);
    alphaA->set_mapper_type(value);
    value->set_parent(alphaA);
    break;
  }
  case ContractMode::missingSemanticType: {
    SgTypeExpression *type = isSgTypeExpression(alphaA->get_mapper_type());
    ROSE_ASSERT(type != nullptr);
    type->set_represented_type(nullptr);
    break;
  }
  case ContractMode::missingLocalScope:
    alphaA->set_nonreal_decl_scope(nullptr);
    break;
  case ContractMode::missingVariable:
    alphaA->set_mapper_variable(nullptr);
    break;
  case ContractMode::wrongVariableKind: {
    SgIntVal *value = SageBuilder::buildIntVal(3);
    ROSE_ASSERT(value != nullptr);
    alphaA->set_mapper_variable(value);
    value->set_parent(alphaA);
    break;
  }
  case ContractMode::wrongVariableScope:
    mapperVariableDeclaration(alphaA)->set_scope(
        SageInterface::getGlobalScope(alphaA));
    break;
  case ContractMode::mismatchedVariableType: {
    SgTypeExpression *type = isSgTypeExpression(alphaA->get_mapper_type());
    ROSE_ASSERT(type != nullptr);
    type->set_represented_type(SageBuilder::buildLongType());
    break;
  }
  case ContractMode::unmangledScope: {
    SgGlobal *global = SageInterface::getGlobalScope(alphaA);
    ROSE_ASSERT(global != nullptr);
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            "rex_mapper_unmangled_scope_owner", SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), global);
    ROSE_ASSERT(function != nullptr);
    ROSE_ASSERT(function->get_definition() != nullptr);
    SgBasicBlock *block = function->get_definition()->get_body();
    ROSE_ASSERT(block != nullptr);
    SageInterface::removeStatement(alphaA, false);
    SageInterface::appendStatement(alphaA, block);
    ROSE_ASSERT(alphaA->get_parent() == block);
    ROSE_ASSERT(alphaA->get_scope() == block);
    break;
  }
  case ContractMode::valid:
    ROSE_ABORT();
  }

  (void)alphaA->get_mangled_name();
  ROSE_ABORT();
}
