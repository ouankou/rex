#include "rose.h"

#include <algorithm>
#include <string>

namespace {
SgDeclarationStatement *canonicalTag(SgDeclarationStatement *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  SgDeclarationStatement *first =
      declaration->get_firstNondefiningDeclaration();
  return first != nullptr ? first : declaration;
}

std::string tagName(SgDeclarationStatement *declaration) {
  if (SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declaration)) {
    return classDeclaration->get_name().str();
  }
  if (SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration)) {
    return enumDeclaration->get_name().str();
  }
  ROSE_ABORT();
}

void validateOwnedTag(SgNode *owner, SgType *type,
                      SgDeclarationStatement *declaration,
                      SgBasicBlock *lexicalScope,
                      const std::string &expectedName) {
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(lexicalScope != nullptr);
  ROSE_ASSERT(declaration->get_parent() == owner);
  ROSE_ASSERT(declaration->get_scope() == lexicalScope);
  if (expectedName.empty()) {
    if (SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration)) {
      ROSE_ASSERT(classDeclaration->get_isUnNamed());
    } else if (SgEnumDeclaration *enumDeclaration =
                   isSgEnumDeclaration(declaration)) {
      ROSE_ASSERT(enumDeclaration->get_isUnNamed());
    } else {
      ROSE_ABORT();
    }
  } else {
    ROSE_ASSERT(tagName(declaration) == expectedName);
  }
  ROSE_ASSERT(declaration->get_file_info() != nullptr);
  ROSE_ASSERT(!declaration->get_file_info()->isCompilerGenerated());
  ROSE_ASSERT(declaration->get_file_info()->isOutputInCodeGeneration());

  SgNamedType *namedType = isSgNamedType(type->findBaseType());
  ROSE_ASSERT(namedType != nullptr);
  ROSE_ASSERT(namedType->get_declaration() != nullptr);
  ROSE_ASSERT(canonicalTag(namedType->get_declaration()) ==
              canonicalTag(declaration));

  SgDeclarationStatement *canonicalDeclaration = canonicalTag(declaration);
  ROSE_ASSERT(canonicalDeclaration != declaration);
  ROSE_ASSERT(canonicalDeclaration->get_scope() == lexicalScope);
  SgAuxiliaryDeclarationList *auxiliaryDeclarations =
      lexicalScope->get_auxiliary_declarations();
  ROSE_ASSERT(auxiliaryDeclarations != nullptr);
  ROSE_ASSERT(canonicalDeclaration->get_parent() == auxiliaryDeclarations);
  ROSE_ASSERT(auxiliaryDeclarations->get_parent() == lexicalScope);
  ROSE_ASSERT(std::count(auxiliaryDeclarations->get_declarations().begin(),
                         auxiliaryDeclarations->get_declarations().end(),
                         canonicalDeclaration) == 1);
  ROSE_ASSERT(std::count(auxiliaryDeclarations->get_declarations().begin(),
                         auxiliaryDeclarations->get_declarations().end(),
                         declaration) == 0);
  ROSE_ASSERT(std::count(lexicalScope->get_statements().begin(),
                         lexicalScope->get_statements().end(),
                         declaration) == 0);
  SgSymbol *canonicalSymbol =
      lexicalScope->find_symbol_from_declaration(canonicalDeclaration);
  ROSE_ASSERT(canonicalSymbol != nullptr);
  ROSE_ASSERT(canonicalSymbol->get_parent() ==
              lexicalScope->get_symbol_table());
  ROSE_ASSERT(canonicalDeclaration->get_file_info() != nullptr);
  ROSE_ASSERT(canonicalDeclaration->get_file_info()->isCompilerGenerated());

  if (SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declaration)) {
    ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
    ROSE_ASSERT(classDeclaration->get_definingDeclaration() == declaration);
    ROSE_ASSERT(!classDeclaration->get_isAutonomousDeclaration());
  } else if (SgEnumDeclaration *enumDeclaration =
                 isSgEnumDeclaration(declaration)) {
    ROSE_ASSERT(!enumDeclaration->isForward());
    ROSE_ASSERT(enumDeclaration->get_definingDeclaration() == declaration);
    ROSE_ASSERT(!enumDeclaration->get_isAutonomousDeclaration());
    ROSE_ASSERT(enumDeclaration->get_field_type() != nullptr);
    ROSE_ASSERT(
        isSgTypeUnknown(enumDeclaration->get_field_type()->findBaseType()) ==
        nullptr);
  } else {
    ROSE_ABORT();
  }

  const SgNodePtrList successors = owner->get_traversalSuccessorContainer();
  ROSE_ASSERT(std::count(successors.begin(), successors.end(), declaration) ==
              1);
}
} // namespace

int main(int argc, char **argv) {
  bool corruptExpressionTagScope = false;
  if (argc > 1 && std::string(argv[1]) == "--corrupt-expression-tag-scope") {
    corruptExpressionTagScope = true;
    for (int index = 1; index + 1 < argc; ++index) {
      argv[index] = argv[index + 1];
    }
    --argc;
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDeclaration *function = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate != nullptr &&
        candidate->get_name() == "rex_expression_tag_ownership" &&
        candidate->get_definition() != nullptr) {
      function = candidate;
      break;
    }
  }
  ROSE_ASSERT(function != nullptr);
  SgBasicBlock *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);

  size_t ownedSizeof = 0;
  size_t ownedAlignof = 0;
  size_t ownedCast = 0;
  size_t ownedAnonymousCast = 0;
  SgDeclarationStatement *corruptionTarget = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgSizeOfOp)) {
    SgSizeOfOp *sizeOf = isSgSizeOfOp(node);
    ROSE_ASSERT(sizeOf != nullptr);
    SgDeclarationStatement *declaration =
        sizeOf->get_type_defining_declaration();
    if (declaration != nullptr) {
      validateOwnedTag(sizeOf, sizeOf->get_operand_type(), declaration, body,
                       "RexSizeTag");
      ++ownedSizeof;
    }
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgAlignOfOp)) {
    SgAlignOfOp *alignOf = isSgAlignOfOp(node);
    ROSE_ASSERT(alignOf != nullptr);
    SgDeclarationStatement *declaration =
        alignOf->get_type_defining_declaration();
    if (declaration != nullptr) {
      validateOwnedTag(alignOf, alignOf->get_operand_type(), declaration, body,
                       "RexAlignTag");
      ++ownedAlignof;
    }
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    SgDeclarationStatement *declaration = cast->get_type_defining_declaration();
    if (declaration != nullptr) {
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
      if (classDeclaration != nullptr && classDeclaration->get_isUnNamed()) {
        validateOwnedTag(cast, cast->get_type(), declaration, body, "");
        ++ownedAnonymousCast;
      } else {
        validateOwnedTag(cast, cast->get_type(), declaration, body,
                         "RexCastTag");
        corruptionTarget = declaration;
        ++ownedCast;
      }
    }
  }
  ROSE_ASSERT(ownedSizeof == 1);
  ROSE_ASSERT(ownedAlignof == 1);
  ROSE_ASSERT(ownedCast == 1);
  ROSE_ASSERT(ownedAnonymousCast == 1);

  size_t ownedCompoundLiteral = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgCompoundLiteralExp)) {
    SgCompoundLiteralExp *compoundLiteral = isSgCompoundLiteralExp(node);
    ROSE_ASSERT(compoundLiteral != nullptr);
    SgVariableSymbol *symbol = compoundLiteral->get_symbol();
    ROSE_ASSERT(symbol != nullptr);
    SgInitializedName *initializedName = symbol->get_declaration();
    ROSE_ASSERT(initializedName != nullptr);
    ROSE_ASSERT(symbol->get_parent() == body->get_symbol_table());
    ROSE_ASSERT(body->find_symbol_from_declaration(initializedName) == symbol);
    SgVariableDeclaration *hiddenDeclaration =
        isSgVariableDeclaration(initializedName->get_parent());
    ROSE_ASSERT(hiddenDeclaration != nullptr);
    ROSE_ASSERT(hiddenDeclaration->get_variables().size() == 1);
    ROSE_ASSERT(hiddenDeclaration->get_variables().front() == initializedName);
    SgAggregateInitializer *aggregateInitializer =
        isSgAggregateInitializer(initializedName->get_initializer());
    ROSE_ASSERT(aggregateInitializer != nullptr);
    ROSE_ASSERT(aggregateInitializer->get_parent() == initializedName);
    ROSE_ASSERT(aggregateInitializer->get_source_form() ==
                SgAggregateInitializer::
                    e_aggregate_initializer_source_compound_literal);
    SgDeclarationStatement *tagDefinition =
        hiddenDeclaration->get_baseTypeDefiningDeclaration();
    validateOwnedTag(hiddenDeclaration, initializedName->get_type(),
                     tagDefinition, body, "RexCompoundTag");
    SgAuxiliaryDeclarationList *hiddenOwner =
        isSgAuxiliaryDeclarationList(hiddenDeclaration->get_parent());
    ROSE_ASSERT(hiddenOwner != nullptr);
    ROSE_ASSERT(hiddenOwner->get_parent() == body);
    ROSE_ASSERT(std::count(hiddenOwner->get_declarations().begin(),
                           hiddenOwner->get_declarations().end(),
                           hiddenDeclaration) == 1);
    ++ownedCompoundLiteral;
  }
  ROSE_ASSERT(ownedCompoundLiteral == 1);

  SgGlobal *global = nullptr;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *sourceFile = isSgSourceFile(file)) {
      if (!sourceFile->get_isHeaderFile()) {
        global = sourceFile->get_globalScope();
        break;
      }
    }
  }
  ROSE_ASSERT(global != nullptr);
  size_t existingTags = 0;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    if (SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration)) {
      if (classDeclaration->get_name() == "RexExistingTag") {
        ROSE_ASSERT(classDeclaration->get_parent() == global);
        ROSE_ASSERT(classDeclaration->get_isAutonomousDeclaration());
        ROSE_ASSERT(classDeclaration->isOutputInCodeGeneration());
        ++existingTags;
      }
    }
  }
  ROSE_ASSERT(existingTags == 1);

  for (SgNode *node : NodeQuery::querySubTree(body, V_SgVariableDeclaration)) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(node);
    ROSE_ASSERT(variable != nullptr);
    if (variable->get_baseTypeDefiningDeclaration() != nullptr) {
      ROSE_ASSERT(variable->get_variables().size() == 1);
      ROSE_ASSERT(
          variable->get_variables().front()->get_name().getString().find(
              "compound_literal_") == 0);
      continue;
    }
    for (SgNode *successor : variable->get_traversalSuccessorContainer()) {
      ROSE_ASSERT(isSgClassDeclaration(successor) == nullptr);
      ROSE_ASSERT(isSgEnumDeclaration(successor) == nullptr);
    }
  }
  size_t autonomousAnonymousTags = 0;
  for (SgStatement *statement : body->get_statements()) {
    SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(statement);
    if (enumDeclaration == nullptr || !enumDeclaration->get_isUnNamed()) {
      continue;
    }
    ROSE_ASSERT(enumDeclaration->get_parent() == body);
    ROSE_ASSERT(enumDeclaration->get_isAutonomousDeclaration());
    ROSE_ASSERT(enumDeclaration->get_definingDeclaration() == enumDeclaration);
    ROSE_ASSERT(!enumDeclaration->isForward());
    ROSE_ASSERT(enumDeclaration->get_file_info() != nullptr);
    ROSE_ASSERT(!enumDeclaration->get_file_info()->isCompilerGenerated());
    ROSE_ASSERT(enumDeclaration->get_file_info()->isOutputInCodeGeneration());
    ROSE_ASSERT(enumDeclaration->get_field_type() != nullptr);
    ROSE_ASSERT(
        isSgTypeUnknown(enumDeclaration->get_field_type()->findBaseType()) ==
        nullptr);
    ++autonomousAnonymousTags;
  }
  ROSE_ASSERT(autonomousAnonymousTags == 1);
  if (corruptExpressionTagScope) {
    ROSE_ASSERT(corruptionTarget != nullptr);
    corruptionTarget->set_scope(function->get_definition());
  }
  return backend(project);
}
