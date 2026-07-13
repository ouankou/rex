#include "rose.h"

#include "IncludedFilesUnparser.h"
#include "nameQualificationSupport.h"
#include "unparser.h"

#include <string>

namespace {

class IncludedFilesUnparserProbe : public IncludedFilesUnparser {
public:
  explicit IncludedFilesUnparserProbe(SgProject *project)
      : IncludedFilesUnparser(project) {}

  void inspect(SgNode *node) { visit(node); }
};

struct QualificationFixture {
  NameQualificationTraversal::NameQualificationMapType names;
  NameQualificationTraversal::NameQualificationMapType types;
  NameQualificationTraversal::NameQualificationMapOfMapsType type_uses;
  NameQualificationTraversal::NameQualificationSetType referenced_names;
  NameQualificationContext context;
  NameQualificationTraversal traversal;

  QualificationFixture()
      : traversal(names, types, type_uses, referenced_names, context) {}
};

SgInitializedName *buildDetachedInitializedName(const SgName &name,
                                                SgScopeStatement *scope) {
  SgInitializedName *initialized_name =
      SageBuilder::buildInitializedName(name, SageBuilder::buildIntType());
  ROSE_ASSERT(initialized_name != nullptr);
  initialized_name->set_scope(scope);
  return initialized_name;
}

SgInitializedName *buildOwnedVariable(const SgName &name,
                                      SgScopeStatement *scope,
                                      SgInitializedName *prior = nullptr,
                                      bool external = false) {
  ROSE_ASSERT(scope != nullptr);
  SgVariableDeclaration *declaration =
      prior != nullptr
          ? SageBuilder::buildVariableRedeclaration(
                name, SageBuilder::buildIntType(), nullptr, scope, prior)
          : SageBuilder::buildVariableDeclaration(
                name, SageBuilder::buildIntType(), nullptr, scope);
  ROSE_ASSERT(declaration != nullptr);
  if (external) {
    declaration->get_declarationModifier().get_storageModifier().setExtern();
  }
  SageInterface::appendStatement(declaration, scope);
  ROSE_ASSERT(declaration->get_parent() == scope);
  ROSE_ASSERT(declaration->get_variables().size() == 1);
  SgInitializedName *initialized_name = declaration->get_variables().front();
  ROSE_ASSERT(initialized_name != nullptr);
  ROSE_ASSERT(initialized_name->get_parent() == declaration);
  ROSE_ASSERT(initialized_name->get_scope() == scope);
  ROSE_ASSERT(initialized_name->get_prev_decl_item() == prior);
  ROSE_ASSERT(scope->lookup_variable_symbol(name) != nullptr);
  return initialized_name;
}

SgNullStatement *appendPosition(SgScopeStatement *scope) {
  SgNullStatement *position = SageBuilder::buildNullStatement();
  SageInterface::appendStatement(position, scope);
  return position;
}

SgBasicBlock *ownedLocalScope(const SgName &functionName) {
  static SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(
      "rex_unparser_dispatch_local_scope.cpp");
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgFunctionDeclaration *function =
      SageBuilder::buildDefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          functionName, SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), global);
  ROSE_ASSERT(function != nullptr);
  ROSE_ASSERT(function->get_definition() != nullptr);
  SgBasicBlock *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);
  return body;
}

void inspectCompilerGeneratedPhysicalOwner(bool output) {
  SgProject project;
  IncludedFilesUnparserProbe probe(&project);
  SgStatement *statement = new SgNullStatement();
  Sg_File_Info *file_info =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  if (output) {
    file_info->setOutputInCodeGeneration();
  } else {
    file_info->unsetOutputInCodeGeneration();
  }
  statement->set_file_info(file_info);
  statement->set_isModified(false);
  ROSE_ASSERT(file_info->get_physical_file_id() < 0);
  probe.inspect(statement);
}

void inspectTypedZeroWidthAuxiliaryOwner() {
  SgProject project;
  IncludedFilesUnparserProbe probe(&project);
  SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(
      "rex_unparser_dispatch_auxiliary_owner.cpp");
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  SgEmptyDeclaration *declaration = SageBuilder::buildEmptyDeclaration(
      SgEmptyDeclaration::e_empty_declaration_zero_width_source_replacement);
  ROSE_ASSERT(declaration != nullptr);
  SageInterface::publishGeneratedSubtreeOutputOwner(declaration, global);
  SageBuilder::attachAuxiliaryDeclaration(global, declaration);
  probe.inspect(declaration);
}

int exercisePositiveContracts() {
  if (SageBuilder::buildIntVal(7)->unparseToString() != "7" ||
      SageBuilder::buildIntType()->unparseToString().empty()) {
    return 10;
  }

  QualificationFixture local_fixture;
  SgBasicBlock *local_scope =
      ownedLocalScope(SgName("rex_local_identity_owner"));
  SgInitializedName *local_name =
      buildOwnedVariable(SgName("rex_local_identity"), local_scope);
  SgNullStatement *local_position = appendPosition(local_scope);
  if (local_fixture.traversal.nameQualificationDepth(local_name, local_scope,
                                                     local_position) != 0) {
    return 11;
  }

  QualificationFixture redeclaration_fixture;
  SgBasicBlock *redeclaration_scope =
      ownedLocalScope(SgName("rex_redeclaration_identity_owner"));
  SgInitializedName *first_declaration = buildOwnedVariable(
      SgName("rex_redeclaration_identity"), redeclaration_scope, nullptr, true);
  SgInitializedName *redeclaration =
      buildOwnedVariable(first_declaration->get_name(), redeclaration_scope,
                         first_declaration, true);
  if (redeclaration_fixture.traversal.nameQualificationDepth(
          redeclaration, redeclaration_scope,
          appendPosition(redeclaration_scope)) != 0) {
    return 12;
  }

  QualificationFixture global_fixture;
  SgSourceFile *source_file = SageBuilder::buildGeneratedSourceFile(
      "rex_unparser_dispatch_ownership_contract.cpp");
  ROSE_ASSERT(source_file != nullptr);
  SgGlobal *global_scope = source_file->get_globalScope();
  ROSE_ASSERT(global_scope != nullptr);
  SgInitializedName *global_name =
      buildOwnedVariable(SgName("rex_global_identity"), global_scope);
  SgFunctionDeclaration *use_function =
      SageBuilder::buildDefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          SgName("rex_qualification_use"), SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), global_scope);
  ROSE_ASSERT(use_function != nullptr);
  SgFunctionDeclaration *use_prototype =
      isSgFunctionDeclaration(use_function->get_firstNondefiningDeclaration());
  ROSE_ASSERT(use_prototype != nullptr);
  ROSE_ASSERT(use_prototype != use_function);
  SgBasicBlock *use_scope = use_function->get_definition()->get_body();
  ROSE_ASSERT(use_scope != nullptr);
  SgInitializedName *shadow_name =
      buildOwnedVariable(global_name->get_name(), use_scope);
  SgNullStatement *global_position = appendPosition(use_scope);
  if (global_fixture.traversal.nameQualificationDepth(global_name, use_scope,
                                                      global_position) != 1) {
    return 13;
  }

  inspectTypedZeroWidthAuxiliaryOwner();
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    return exercisePositiveContracts();
  }
  if (argc != 2) {
    return 2;
  }

  const std::string mode = argv[1];
  if (mode == "symbol") {
    SgInitializedName *initialized_name = SageBuilder::buildInitializedName(
        SgName("rex_symbol"), SageBuilder::buildIntType());
    (new SgVariableSymbol(initialized_name))->unparseToString();
    return 0;
  }
  if (mode == "function-symbol") {
    SgFunctionDeclaration *declaration = new SgFunctionDeclaration(
        SgName("rex_function_symbol"), static_cast<SgFunctionType *>(nullptr),
        nullptr);
    (new SgFunctionSymbol(declaration))->unparseToString();
    return 0;
  }
  if (mode == "initialized-name") {
    SageBuilder::buildInitializedName(SgName("rex_initialized_name"),
                                      SageBuilder::buildIntType())
        ->unparseToString();
    return 0;
  }
  if (mode == "openacc-located-support") {
    (new SgAccDefaultClause(0))->unparseToString();
    return 0;
  }
  if (mode == "missing-local-symbol") {
    QualificationFixture fixture;
    SgBasicBlock *scope = ownedLocalScope(SgName("rex_missing_local_owner"));
    SgInitializedName *initialized_name =
        buildDetachedInitializedName(SgName("rex_missing_local"), scope);
    fixture.traversal.nameQualificationDepth(initialized_name, scope,
                                             appendPosition(scope));
    return 0;
  }
  if (mode == "wrong-local-symbol") {
    QualificationFixture fixture;
    SgBasicBlock *scope = ownedLocalScope(SgName("rex_wrong_local_owner"));
    SgInitializedName *initialized_name =
        buildDetachedInitializedName(SgName("rex_wrong_local"), scope);
    SgInitializedName *other =
        buildDetachedInitializedName(initialized_name->get_name(), scope);
    scope->insert_symbol(other->get_name(), new SgVariableSymbol(other));
    fixture.traversal.nameQualificationDepth(initialized_name, scope,
                                             appendPosition(scope));
    return 0;
  }
  if (mode == "compiler-generated-output-owner") {
    inspectCompilerGeneratedPhysicalOwner(true);
    return 0;
  }
  if (mode == "compiler-generated-hidden-lexical-owner") {
    inspectCompilerGeneratedPhysicalOwner(false);
    return 0;
  }
  if (mode == "direct-skipped-file") {
    SgSourceFile *source_file = SageBuilder::buildGeneratedSourceFile(
        "rex_unparser_direct_skipped_file.cpp");
    ROSE_ASSERT(source_file != nullptr);
    source_file->set_skip_unparse(true);
    unparseFile(source_file);
    return 0;
  }
  if (mode == "standalone-auxiliary-statement") {
    SgSourceFile *source_file = SageBuilder::buildGeneratedSourceFile(
        "rex_unparser_standalone_auxiliary.cpp");
    ROSE_ASSERT(source_file != nullptr);
    SgEmptyDeclaration *declaration = SageBuilder::buildEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_zero_width_source_replacement);
    ROSE_ASSERT(declaration != nullptr);
    SageBuilder::attachAuxiliaryDeclaration(source_file->get_globalScope(),
                                            declaration);
    (void)declaration->unparseToString();
    return 0;
  }
  return 3;
}
