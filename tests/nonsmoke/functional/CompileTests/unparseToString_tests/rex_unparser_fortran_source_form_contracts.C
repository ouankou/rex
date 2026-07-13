#include "rose.h"

#include "unparseFortran.h"
#include "unparser.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace {

void initializeFortranFile(SgSourceFile &file) {
  file.set_Cxx_only(false);
  file.set_Fortran_only(true);
  file.set_outputLanguage(SgFile::e_Fortran_language);
  file.set_inputFormat(SgFile::e_free_form_output_format);
  file.set_outputFormat(SgFile::e_free_form_output_format);
  file.set_backendCompileFormat(SgFile::e_free_form_output_format);
}

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

struct Fixture {
  Unparser_Opt options;
  std::ostringstream output;
  Unparser unparser;
  SgSourceFile *source_file;
  SgProject *project;
  SgGlobal *global;
  SgUnparse_Info info;
  FortranCodeGeneration_locatedNode fortran;

  Fixture()
      : unparser(&output, "rex_unparser_fortran_source_form_contracts.f90",
                 options),
        source_file(SageBuilder::buildGeneratedSourceFile(
            "rex_unparser_fortran_source_form_contracts.f90")),
        project(source_file != nullptr ? source_file->get_project() : nullptr),
        global(source_file != nullptr ? source_file->get_globalScope()
                                      : nullptr),
        fortran(&unparser, "rex_unparser_fortran_source_form_contracts.f90") {
    ASSERT_not_null(source_file);
    ASSERT_not_null(project);
    ASSERT_not_null(global);
    initializeFortranFile(*source_file);
    unparser.currentFile = source_file;
    info.set_current_source_file(source_file);
    info.set_current_scope(global);
    info.set_language(SgFile::e_Fortran_language);
    // This fixture invokes isolated located-node emitters into a string stream,
    // rather than performing physical source-file emission. Match the public
    // unparseToString boundary explicitly so generated fixture descendants are
    // not misrepresented as source-backed file output.
    info.set_usedInUparseToStringFunction();
  }

  SgBasicBlock *buildOwnedFunctionBody(const SgName &name) {
    SgProcedureHeaderStatement *function =
        SageBuilder::buildProcedureHeaderStatement(
            SageBuilder::function_declaration_ownership::sourceLexical(), name,
            SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList_nfi(),
            SageBuilder::buildFunctionParameterList_nfi(),
            SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
            SgProcedureHeaderStatement::e_fortran_procedure_source_form_header,
            global);
    ROSE_ASSERT(function != nullptr);
    ROSE_ASSERT(function->get_definition() != nullptr);
    SgBasicBlock *body = function->get_definition()->get_body();
    ROSE_ASSERT(body != nullptr);
    ROSE_ASSERT(body->get_parent() == function->get_definition());
    return body;
  }
};

SgModuleStatement *buildModule(Fixture &fixture, const SgName &name) {
  SgModuleStatement *module = SageBuilder::buildModuleStatement(
      SageBuilder::declaration_ownership::sourceLexical(), name,
      fixture.global);
  ROSE_ASSERT(module != nullptr);
  ROSE_ASSERT(module->get_parent() == fixture.global);
  return module;
}

SgOmpParallelStatement *buildOmpParallel(Fixture &fixture) {
  SgBasicBlock *body = SageBuilder::buildBasicBlock();
  SgOmpParallelStatement *parallel = new SgOmpParallelStatement(body);
  body->set_parent(parallel);
  ROSE_ASSERT(parallel->get_clause_list() != nullptr);
  parallel->get_clause_list()->set_parent(parallel);
  SageInterface::setSourcePositionForTransformation(parallel);
  SgBasicBlock *owner =
      fixture.buildOwnedFunctionBody(SgName("openmp_parallel_owner"));
  SageInterface::appendStatement(parallel, owner);
  ROSE_ASSERT(parallel->get_parent() == owner);
  return parallel;
}

SgAccParallelStatement *buildAccParallel() {
  SgBasicBlock *body = SageBuilder::buildBasicBlock();
  SgAccParallelStatement *parallel = new SgAccParallelStatement(body);
  body->set_parent(parallel);
  return parallel;
}

SgAccAtomicStatement *buildAccAtomicCapture() {
  SgBasicBlock *body = SageBuilder::buildBasicBlock();
  SgAccAtomicStatement *atomic = new SgAccAtomicStatement(body);
  body->set_parent(atomic);
  SgAccCaptureClause *capture = new SgAccCaptureClause();
  atomic->get_clauses().push_back(capture);
  capture->set_parent(atomic);
  return atomic;
}

SgTypeInt *buildSemanticFortranInteger(int kind = 4) {
  SgExpression *selector = SageBuilder::buildIntVal_nfi(std::to_string(kind));
  ROSE_ASSERT(selector != nullptr);
  SageInterface::setSourcePositionForTransformation(selector);
  SgTypeInt *type = SageBuilder::buildIntType(selector);
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(!type->get_fortran_source_syntax());
  return type;
}

SgTypeInt *buildSourceFortranInteger(int kind = 0) {
  SgTypeInt *type = new SgTypeInt();
  ROSE_ASSERT(type != nullptr);
  type->set_fortran_source_syntax(true);
  if (kind > 0) {
    SgExpression *selector = SageBuilder::buildIntVal_nfi(std::to_string(kind));
    ROSE_ASSERT(selector != nullptr);
    SageInterface::setSourcePositionForTransformation(selector);
    selector->set_fortran_integer_constant_value(kind);
    selector->set_fortran_integer_constant_value_is_available(true);
    type->set_type_kind(selector);
    selector->set_parent(type);
  }
  return type;
}

} // namespace

int main(int argc, char **argv) {
  if (argc > 2) {
    return 2;
  }
  const std::string mode = argc == 2 ? argv[1] : std::string();
  Fixture fixture;

  if (mode == "directive-context-missing-file") {
    fixture.unparser.currentFile = nullptr;
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    return 0;
  }
  if (mode == "directive-context-nonfortran-file") {
    fixture.source_file->set_Fortran_only(false);
    fixture.source_file->set_Cxx_only(true);
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    return 0;
  }

  if (mode == "module-kind-invalid" || mode == "module-parent-on-module" ||
      mode == "submodule-missing-parent" ||
      mode == "submodule-parent-invalid") {
    SgModuleStatement *module = buildModule(fixture, SgName("child"));
    if (mode == "module-kind-invalid") {
      module->set_fortran_module_kind(
          static_cast<SgModuleStatement::fortran_module_kind_enum>(999));
    } else if (mode == "module-parent-on-module") {
      module->set_fortran_submodule_parent(SgName("ancestor"));
    } else {
      module->set_fortran_module_kind(SgModuleStatement::e_fortran_submodule);
      if (mode == "submodule-parent-invalid") {
        module->set_fortran_submodule_parent(SgName("ancestor:parent:extra"));
      }
    }
    fixture.fortran.unparseModuleStmt(module, fixture.info);
    return 0;
  }

  if (mode == "declaration-origin-invalid" ||
      mode == "declaration-origin-pending" ||
      mode == "declaration-origin-semantic-source") {
    SgBasicBlock *block =
        fixture.buildOwnedFunctionBody(SgName("declaration_origin_owner"));
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        "value", SageBuilder::buildIntType(), nullptr, block);
    SageInterface::appendStatement(declaration, block);
    if (mode == "declaration-origin-pending") {
      declaration->set_fortran_declaration_origin(
          SgVariableDeclaration::e_fortran_pending_source_declaration);
    } else if (mode == "declaration-origin-semantic-source") {
      declaration->set_fortran_declaration_origin(
          SgVariableDeclaration::e_fortran_semantic_only_declaration);
    } else {
      declaration->set_fortran_declaration_origin(
          static_cast<SgVariableDeclaration::fortran_declaration_origin_enum>(
              999));
    }
    fixture.fortran.unparseBasicBlockStmt(block, fixture.info);
    return 0;
  }

  if (mode == "object-kind-selector-mismatch") {
    SgBasicBlock *block =
        fixture.buildOwnedFunctionBody(SgName("selector_mismatch_owner"));
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        "value", buildSemanticFortranInteger(), nullptr, block);
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    declaration->get_variables().front()->set_fortran_source_type(
        buildSourceFortranInteger(8));
    declaration->set_fortran_declaration_origin(
        SgVariableDeclaration::e_fortran_source_declaration);
    SageInterface::appendStatement(declaration, block);
    fixture.fortran.unparseBasicBlockStmt(block, fixture.info);
    return 0;
  }

  if (mode == "auxiliary-lexical-owner-conflict") {
    SgBasicBlock *block = fixture.buildOwnedFunctionBody(
        SgName("auxiliary_lexical_owner_conflict"));
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        "value", SageBuilder::buildIntType(), nullptr, block);
    SageInterface::appendStatement(declaration, block);
    SageBuilder::attachAuxiliaryDeclaration(block, declaration);
    return 0;
  }

  if (mode == "pragma-family-invalid") {
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration("omp parallel", fixture.global);
    ROSE_ASSERT(pragma != nullptr);
    pragma->set_fortran_directive_family(
        static_cast<SgPragmaDeclaration::fortran_directive_family_enum>(999));
    fixture.fortran.unparsePragmaDeclStmt(pragma, fixture.info);
    return 0;
  }

  if (mode == "openmp-clause-context-missing-file" ||
      mode == "openmp-clause-context-missing-scope" ||
      mode == "openmp-clause-context-nonfortran-language") {
    if (mode == "openmp-clause-context-missing-file") {
      fixture.info.set_current_source_file(nullptr);
    } else if (mode == "openmp-clause-context-missing-scope") {
      fixture.info.set_current_scope(nullptr);
    } else {
      fixture.info.set_language(SgFile::e_Cxx_language);
    }
    SgOmpParallelStatement *parallel = buildOmpParallel(fixture);
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    fixture.fortran.unparseOmpBeginDirectiveClauses(parallel, fixture.info);
    return 0;
  }

  if (mode == "openmp-end-kind-invalid") {
    SgOmpParallelStatement *parallel = buildOmpParallel(fixture);
    parallel->set_directive_end_kind(
        static_cast<SgStatement::directive_end_kind_enum>(999));
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    fixture.fortran.unparseOmpBeginDirectiveClauses(parallel, fixture.info);
    return 0;
  }

  if (mode == "openmp-explicit-end-unsupported") {
    SgOmpFlushStatement *flush = new SgOmpFlushStatement();
    ROSE_ASSERT(flush->get_clause_list() != nullptr);
    flush->get_clause_list()->set_parent(flush);
    SageInterface::setSourcePositionForTransformation(flush);
    SgBasicBlock *owner =
        fixture.buildOwnedFunctionBody(SgName("openmp_flush_owner"));
    SageInterface::appendStatement(flush, owner);
    ROSE_ASSERT(flush->get_parent() == owner);
    flush->set_directive_end_kind(SgStatement::e_directive_end_explicit);
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    fixture.fortran.unparseOmpBeginDirectiveClauses(flush, fixture.info);
    return 0;
  }

  if (mode == "openmp-spelling-missing" || mode == "openmp-spelling-invalid") {
    SgOmpTargetParallelForStatement *directive =
        new SgOmpTargetParallelForStatement(
            static_cast<SgStatement *>(nullptr));
    if (mode == "openmp-spelling-invalid") {
      directive->set_omp_fortran_spelling(
          static_cast<SgStatement::omp_fortran_spelling_enum>(999));
    }
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    fixture.fortran.unparseOmpDirectivePrefixAndName(directive, fixture.info);
    return 0;
  }

  if (mode == "openacc-end-kind-invalid") {
    SgAccParallelStatement *parallel = buildAccParallel();
    parallel->set_directive_end_kind(
        static_cast<SgStatement::directive_end_kind_enum>(999));
    fixture.fortran.unparseAccGenericStatement(parallel, fixture.info);
    return 0;
  }

  if (mode == "openacc-end-provenance-missing" ||
      mode == "openacc-implicit-end-unsupported") {
    SgAccParallelStatement *parallel = buildAccParallel();
    if (mode == "openacc-implicit-end-unsupported") {
      parallel->set_directive_end_kind(SgStatement::e_directive_end_implicit);
    }
    fixture.fortran.unparseAccGenericStatement(parallel, fixture.info);
    return 0;
  }

  if (mode == "openacc-atomic-capture-end-missing") {
    fixture.fortran.unparseAccGenericStatement(buildAccAtomicCapture(),
                                               fixture.info);
    return 0;
  }

  if (!mode.empty()) {
    return 2;
  }

  SgModuleStatement *module = buildModule(fixture, SgName("plain_module"));
  fixture.fortran.unparseModuleStmt(module, fixture.info);
  fixture.unparser.cur.insert_newline(1);

  SgModuleStatement *submodule = buildModule(fixture, SgName("child_module"));
  submodule->set_fortran_module_kind(SgModuleStatement::e_fortran_submodule);
  submodule->set_fortran_submodule_parent(SgName("ancestor:parent"));
  fixture.fortran.unparseModuleStmt(submodule, fixture.info);
  fixture.unparser.cur.insert_newline(1);

  SgBasicBlock *block =
      fixture.buildOwnedFunctionBody(SgName("declaration_origin_positive"));
  SgVariableDeclaration *semantic_only = SageBuilder::buildVariableDeclaration(
      "semantic_only", SageBuilder::buildIntType(), nullptr, block);
  semantic_only->set_fortran_declaration_origin(
      SgVariableDeclaration::e_fortran_semantic_only_declaration);
  SageBuilder::attachAuxiliaryDeclaration(block, semantic_only);
  SgAuxiliaryDeclarationList *semantic_owner =
      isSgAuxiliaryDeclarationList(semantic_only->get_parent());
  ROSE_ASSERT(semantic_owner != nullptr);
  ROSE_ASSERT(semantic_owner->get_parent() == block);
  ROSE_ASSERT(block->get_auxiliary_declarations() == semantic_owner);
  ROSE_ASSERT(std::count(semantic_owner->get_declarations().begin(),
                         semantic_owner->get_declarations().end(),
                         semantic_only) == 1);
  ROSE_ASSERT(std::find(block->get_statements().begin(),
                        block->get_statements().end(),
                        semantic_only) == block->get_statements().end());
  semantic_owner->validate_semantic_non_output_role();
  SgVariableDeclaration *source = SageBuilder::buildVariableDeclaration(
      "source_value", buildSemanticFortranInteger(), nullptr, block);
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(source->get_variables().size() == 1);
  SgInitializedName *source_name = source->get_variables().front();
  ROSE_ASSERT(source_name != nullptr);
  ROSE_ASSERT(source_name->get_type() != nullptr);
  source_name->set_fortran_source_type(buildSourceFortranInteger());
  source->set_fortran_declaration_origin(
      SgVariableDeclaration::e_fortran_source_declaration);
  SageInterface::appendStatement(source, block);
  fixture.fortran.unparseBasicBlockStmt(block, fixture.info);

  SgPragmaDeclaration *openmp_pragma =
      SageBuilder::buildPragmaDeclaration("omp parallel", fixture.global);
  openmp_pragma->set_fortran_directive_family(
      SgPragmaDeclaration::e_fortran_directive_openmp);
  fixture.fortran.unparsePragmaDeclStmt(openmp_pragma, fixture.info);
  SgPragmaDeclaration *vendor_pragma =
      SageBuilder::buildPragmaDeclaration("vendor payload", fixture.global);
  fixture.fortran.unparsePragmaDeclStmt(vendor_pragma, fixture.info);

  SgOmpTargetParallelForStatement *do_directive =
      new SgOmpTargetParallelForStatement(static_cast<SgStatement *>(nullptr));
  do_directive->set_omp_fortran_spelling(
      SgStatement::e_omp_fortran_spelling_do);
  {
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    fixture.fortran.unparseOmpDirectivePrefixAndName(do_directive,
                                                     fixture.info);
  }
  fixture.unparser.cur.insert_newline(1);

  SgOmpParallelStatement *parallel = buildOmpParallel(fixture);
  parallel->set_directive_end_kind(SgStatement::e_directive_end_explicit);
  {
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openmp);
    fixture.fortran.unparseOmpEndDirectivePrefixAndName(parallel, fixture.info);
  }

  SgAccAtomicStatement *atomic = buildAccAtomicCapture();
  atomic->set_directive_end_kind(SgStatement::e_directive_end_explicit);
  {
    Unparser::FortranDirectiveContextGuard context(
        &fixture.unparser, Unparser::FortranDirectiveKind::openacc);
    fixture.fortran.unparseAccEndDirectivePrefixAndName(atomic, fixture.info);
  }

  const std::string text = lower(fixture.output.str());
  if (text.find("module plain_module") == std::string::npos ||
      text.find("submodule (ancestor:parent) child_module") ==
          std::string::npos ||
      text.find("semantic_only") != std::string::npos ||
      text.find("source_value") == std::string::npos ||
      text.find("!$omp parallel") == std::string::npos ||
      text.find("!pragma vendor payload") == std::string::npos ||
      text.find("target parallel do") == std::string::npos ||
      text.find("end parallel") == std::string::npos ||
      text.find("end atomic") == std::string::npos) {
    return 3;
  }
  return 0;
}
