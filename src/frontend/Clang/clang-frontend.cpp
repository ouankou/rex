
#include <algorithm>

#include <atomic>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>

#include <map>

#include <memory>

#include <set>

#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>

#include "clang-frontend-private.hpp"

#include "FileHelper.h"
#include "sage3basic.h"

#include "clang-to-dot.hpp"

#include "clang_paths.h"

#include <clang/Basic/DiagnosticFrontend.h>
#include <clang/Basic/DiagnosticLex.h>
#include <clang/Basic/DiagnosticSema.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/InputInfo.h>
#include <clang/Driver/Job.h>
#include <clang/Driver/Types.h>
#include <clang/Lex/Lexer.h>
#include <clang/Options/Options.h>
#include <clang/Parse/Parser.h>

#include <llvm/Option/Arg.h>
#include <llvm/Option/ArgList.h>

#include "rose_config.h"

#if defined(ROSE_USE_VALGRIND) && ROSE_USE_VALGRIND
#include <valgrind/memcheck.h>
#include <valgrind/valgrind.h>
#endif

namespace {

void roseClangPhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

#if ROSE_USE_VALGRIND
class ValgrindErrorReportingScope {
public:
  ValgrindErrorReportingScope() : enabled_(RUNNING_ON_VALGRIND != 0) {
    if (enabled_) {
      VALGRIND_DISABLE_ERROR_REPORTING;
    }
  }

  ~ValgrindErrorReportingScope() { enable(); }

  void enable() {
    if (enabled_) {
      VALGRIND_ENABLE_ERROR_REPORTING;
      enabled_ = false;
    }
  }

private:
  bool enabled_;
};
#endif

class RoseDiagnosticConsumer : public clang::DiagnosticConsumer {
  std::unique_ptr<clang::DiagnosticConsumer> delegate_;
  SagePreprocessorRecord *preprocessor_recorder_ = nullptr;

public:
  RoseDiagnosticConsumer(std::unique_ptr<clang::DiagnosticConsumer> delegate,
                         SagePreprocessorRecord *preprocessor_recorder)
      : delegate_(std::move(delegate)),
        preprocessor_recorder_(preprocessor_recorder) {}

  void BeginSourceFile(const clang::LangOptions &LangOpts,
                       const clang::Preprocessor *PP = nullptr) override {
    if (delegate_ != nullptr) {
      delegate_->BeginSourceFile(LangOpts, PP);
    }
  }

  void EndSourceFile() override {
    if (delegate_ != nullptr) {
      delegate_->EndSourceFile();
    }
  }

  void finish() override {
    if (delegate_ != nullptr) {
      delegate_->finish();
    }
  }

  void clear() override {
    clang::DiagnosticConsumer::clear();
    if (delegate_ != nullptr) {
      delegate_->clear();
    }
  }

  bool IncludeInDiagnosticCounts() const override {
    return delegate_ == nullptr
               ? DiagnosticConsumer::IncludeInDiagnosticCounts()
               : delegate_->IncludeInDiagnosticCounts();
  }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                        const clang::Diagnostic &Info) override {
    clang::DiagnosticConsumer::HandleDiagnostic(DiagLevel, Info);

    if (preprocessor_recorder_ != nullptr && Info.hasSourceManager()) {
      PreprocessingInfo::DirectiveType directive_type;
      bool should_record = true;
      switch (Info.getID()) {
      case clang::diag::pp_hash_warning:
        directive_type = PreprocessingInfo::CpreprocessorWarningDeclaration;
        break;
      case clang::diag::err_pp_hash_error:
        directive_type = PreprocessingInfo::CpreprocessorErrorDeclaration;
        break;
      default:
        should_record = false;
        break;
      }

      if (should_record) {
        preprocessor_recorder_->recordSourceDirective(Info.getLocation(),
                                                      directive_type);
      }
    }

    if (delegate_ != nullptr) {
      delegate_->HandleDiagnostic(DiagLevel, Info);
    }
  }
};

const char *
builtinPreincludeForLanguage(ClangToSageTranslator::Language language) {
  switch (language) {
  case ClangToSageTranslator::C:
    return "clang-builtin-c.h";
  case ClangToSageTranslator::CPLUSPLUS:
    return "clang-builtin-cpp.hpp";
  case ClangToSageTranslator::CUDA:
    return "clang-builtin-cuda.hpp";
  case ClangToSageTranslator::OPENCL:
    return "clang-builtin-opencl.h";
  case ClangToSageTranslator::OBJC:
  case ClangToSageTranslator::unknown:
    return nullptr;
  default:
    ROSE_ABORT();
  }
}

bool hasConfiguredPreinclude(const std::vector<std::string> &includes,
                             llvm::StringRef required_include) {
  return std::any_of(includes.begin(), includes.end(),
                     [&](const std::string &include) {
                       return llvm::StringRef(include) == required_include;
                     });
}

void assertRequiredPreincludeConfigured(
    const std::vector<std::string> &includes,
    ClangToSageTranslator::Language language, const char *stage) {
  const char *required_include = builtinPreincludeForLanguage(language);
  if (required_include == nullptr) {
    return;
  }
  if (hasConfiguredPreinclude(includes, required_include)) {
    return;
  }
  llvm::errs() << "ROSE Clang frontend invariant violation: required "
                  "preinclude '"
               << required_include << "' missing during " << stage << ".\n";
  ROSE_ABORT();
}

bool isRoseInternalOption(const std::string &arg) {
  return arg.rfind("-rose:", 0) == 0 || arg.rfind("--rose:", 0) == 0;
}

bool isRexAstJsonOption(const std::string &arg) {
  return arg.rfind("-rex:ast-json-checkpoint=", 0) == 0 ||
         arg.rfind("-rex:ast-json-dir=", 0) == 0;
}

ClangToSageTranslator::Language
languageFromClangInputType(clang::driver::types::ID input_type) {
  using namespace clang::driver::types;
  switch (input_type) {
  case TY_C:
  case TY_PP_C:
  case TY_CHeader:
  case TY_PP_CHeader:
    return ClangToSageTranslator::C;
  case TY_CXX:
  case TY_PP_CXX:
  case TY_CXXHeader:
  case TY_PP_CXXHeader:
  case TY_CXXHUHeader:
  case TY_PP_CXXHeaderUnit:
  case TY_CXXSHeader:
  case TY_CXXUHeader:
  case TY_CXXModule:
  case TY_PP_CXXModule:
    return ClangToSageTranslator::CPLUSPLUS;
  case TY_CUDA:
  case TY_PP_CUDA:
    return ClangToSageTranslator::CUDA;
  case TY_CL:
  case TY_PP_CL:
  case TY_CLHeader:
    return ClangToSageTranslator::OPENCL;
  case TY_ObjC:
  case TY_PP_ObjC:
  case TY_PP_ObjC_Alias:
  case TY_ObjCHeader:
  case TY_PP_ObjCHeader:
    return ClangToSageTranslator::OBJC;
  default:
    return ClangToSageTranslator::unknown;
  }
}

ClangToSageTranslator::Language
languageFromClangTypeName(llvm::StringRef type_name) {
  if (type_name.empty()) {
    return ClangToSageTranslator::unknown;
  }
  if (type_name == "opencl") {
    return ClangToSageTranslator::OPENCL;
  }
  const std::string name = type_name.str();
  return languageFromClangInputType(
      clang::driver::types::lookupTypeForTypeSpecifier(name.c_str()));
}

ClangToSageTranslator::Language
languageFromClangSourcePath(llvm::StringRef path) {
  llvm::StringRef extension = llvm::sys::path::extension(path);
  if (extension.starts_with(".")) {
    extension = extension.drop_front();
  }
  if (extension == "ocl") {
    // REX's established OpenCL source suffix is a frontend API; Clang's
    // driver table only knows the standard .cl spelling.
    return ClangToSageTranslator::OPENCL;
  }
  return languageFromClangInputType(
      clang::driver::types::lookupTypeForExtension(extension));
}

static bool isCommentPreprocessingInfo(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
    return true;
  default:
    return false;
  }
}

static Sg_File_Info *
namespaceDeclarationSyntaxEnd(SgNamespaceDeclarationStatement *namespace_decl) {
  if (namespace_decl == nullptr) {
    return nullptr;
  }
  if (!namespace_decl->has_source_fragments()) {
    std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                 "namespace preprocessing anchor has no typed source "
                 "fragments\n";
    ROSE_ABORT();
  }
  namespace_decl->validate_source_fragments();
  SgNamespaceSourceFragment *closing =
      namespace_decl->get_closing_source_fragment();
  ROSE_ASSERT(closing != nullptr);
  closing->validate();
  return closing->get_endOfConstruct();
}

static bool isSameLineTrailingComment(SgLocatedNode *node,
                                      Sg_File_Info *comment_location,
                                      const PreprocessingInfo *info) {
  if (!isCommentPreprocessingInfo(info) || comment_location == nullptr) {
    return false;
  }

  SgNamespaceDeclarationStatement *namespace_decl =
      isSgNamespaceDeclarationStatement(node);
  Sg_File_Info *syntax_end = namespace_decl != nullptr
                                 ? namespaceDeclarationSyntaxEnd(namespace_decl)
                             : node != nullptr ? node->get_endOfConstruct()
                                               : nullptr;
  if (syntax_end == nullptr || syntax_end->get_line() <= 0 ||
      syntax_end->get_col() <= 0 || comment_location->get_line() <= 0 ||
      comment_location->get_col() <= 0 ||
      syntax_end->get_physical_file_id() < 0 ||
      comment_location->get_physical_file_id() < 0 ||
      !syntax_end->isSameFile(*comment_location) ||
      syntax_end->get_physical_file_occurrence_id() !=
          comment_location->get_physical_file_occurrence_id()) {
    return false;
  }

  return syntax_end->get_line() == comment_location->get_line() &&
         syntax_end->get_col() < comment_location->get_col();
}

static bool hasExactPhysicalSourceInterval(SgLocatedNode *node);

static SgLocatedNode *
canonicalPreprocessingOwner(SgLocatedNode *candidate,
                            const PreprocessingInfo *info) {
  if (candidate == nullptr || info == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: cannot "
                 "canonicalize an incomplete preprocessing attachment\n";
    ROSE_ABORT();
  }
  if (info->getRelativePosition() == PreprocessingInfo::after ||
      info->getRelativePosition() == PreprocessingInfo::after_syntax) {
    SgClassDefinition *definition = isSgClassDefinition(candidate);
    if (definition == nullptr) {
      definition = isSgTemplateClassDefinition(candidate);
    }
    if (definition != nullptr) {
      SgClassDeclaration *declaration = definition->get_declaration();
      if (declaration == nullptr ||
          declaration->get_definition() != definition ||
          definition->get_parent() != declaration) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[preprocessing-owner]: structural class "
               "definition has no exact lexical declaration owner\n";
        ROSE_ABORT();
      }
      Sg_File_Info *definition_end = definition->get_endOfConstruct();
      Sg_File_Info *record_start = info->get_file_info();
      if (definition_end == nullptr || record_start == nullptr ||
          definition_end->get_line() <= 0 || record_start->get_line() <= 0 ||
          definition_end->get_physical_file_id() < 0 ||
          record_start->get_physical_file_id() < 0 ||
          !definition_end->isSameFile(*record_start) ||
          definition_end->get_physical_file_occurrence_id() !=
              record_start->get_physical_file_occurrence_id()) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[preprocessing-owner]: structural class "
               "after surface has no exact physical boundary\n";
        ROSE_ABORT();
      }
      const bool record_follows_definition =
          definition_end->get_line() < record_start->get_line() ||
          (definition_end->get_line() == record_start->get_line() &&
           definition_end->get_col() < record_start->get_col());
      if (record_follows_definition) {
        // The definition is structural payload unparsed with directive output
        // disabled.  Text after its complete source interval belongs to the
        // owning declaration's lexical surface.
        return declaration;
      }
    }
  }
  if (info->getRelativePosition() != PreprocessingInfo::inside) {
    return candidate;
  }

  if (SgNamespaceDeclarationStatement *declaration =
          isSgNamespaceDeclarationStatement(candidate)) {
    SgNamespaceDefinitionStatement *definition = declaration->get_definition();
    if (definition == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: namespace "
                   "inside syntax has no definition owner\n";
      ROSE_ABORT();
    }
    return definition;
  }

  if (SgClassDeclaration *declaration = isSgClassDeclaration(candidate)) {
    SgClassDefinition *definition = declaration->get_definition();
    if (definition == nullptr) {
      // A directive may occur between a template header and a forward class
      // declaration.  That interval is part of the declaration's exact syntax
      // even though there is no class body to own it.
      if (!declaration->isForward() ||
          !hasExactPhysicalSourceInterval(declaration)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: class "
                     "inside syntax has neither an exact definition nor a "
                     "forward-declaration owner\n";
        ROSE_ABORT();
      }
      return declaration;
    }
    return definition;
  }

  if (SgEnumDeclaration *declaration = isSgEnumDeclaration(candidate)) {
    if (declaration->isForward()) {
      if (!hasExactPhysicalSourceInterval(declaration)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: enum "
                     "forward syntax has no exact physical owner\n";
        ROSE_ABORT();
      }
      return declaration;
    }
    SgEnumDeclaration *definition =
        isSgEnumDeclaration(declaration->get_definingDeclaration());
    if (definition == nullptr || definition != declaration) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: enum inside "
                   "syntax is not its exact defining declaration owner\n";
      ROSE_ABORT();
    }
    return declaration;
  }

  return candidate;
}

static bool isSemanticNonLexicalDeclarationSubtree(
    SgNode *node, std::unordered_map<SgNode *, bool> &classification_cache) {
  if (node == nullptr) {
    return false;
  }

  const auto cached_node = classification_cache.find(node);
  if (cached_node != classification_cache.end()) {
    return cached_node->second;
  }

  std::unordered_set<SgNode *> visited;
  std::vector<SgNode *> uncached_path;
  SgDeclarationStatement *direct_declaration = nullptr;
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    if (!visited.insert(cursor).second) {
      std::cerr
          << "REX_FRONTEND_INVARIANT[preprocessing-auxiliary-owner]: parent "
             "cycle while classifying node="
          << node->class_name() << "\n";
      ROSE_ABORT();
    }

    const auto cached = classification_cache.find(cursor);
    if (cached != classification_cache.end()) {
      for (SgNode *path_node : uncached_path) {
        classification_cache.emplace(path_node, cached->second);
      }
      return cached->second;
    }
    uncached_path.push_back(cursor);

    if (SgDeclarationStatement *declaration =
            isSgDeclarationStatement(cursor)) {
      direct_declaration = declaration;
    }

    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(cursor);
    if (auxiliary != nullptr) {
      SgScopeStatement *scope = isSgScopeStatement(auxiliary->get_parent());
      if (scope == nullptr ||
          scope->get_auxiliary_declarations() != auxiliary) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-auxiliary-owner]: "
                     "auxiliary declaration list has no exact scope owner\n";
        ROSE_ABORT();
      }
      if (direct_declaration != nullptr &&
          (direct_declaration->get_parent() != auxiliary ||
           direct_declaration->get_scope() != scope ||
           std::count(auxiliary->get_declarations().begin(),
                      auxiliary->get_declarations().end(),
                      direct_declaration) != 1)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-auxiliary-owner]: "
                     "semantic declaration="
                  << direct_declaration->class_name()
                  << " has malformed typed auxiliary ownership\n";
        ROSE_ABORT();
      }
      for (SgNode *path_node : uncached_path) {
        classification_cache.emplace(path_node, true);
      }
      return true;
    }

    if (SgFunctionParameterList *parameters =
            isSgFunctionParameterList(cursor)) {
      if (SgFunctionDeclaration *function =
              isSgFunctionDeclaration(parameters->get_parent())) {
        SgFunctionParameterList *semantic = function->get_parameterList();
        SgFunctionParameterList *syntax = function->get_parameterList_syntax();
        const bool is_semantic = semantic == parameters;
        const bool is_syntax = syntax == parameters;
        if ((!is_semantic && !is_syntax) ||
            (semantic != nullptr && semantic->get_parent() != function) ||
            (syntax != nullptr && syntax->get_parent() != function)) {
          std::cerr
              << "REX_FRONTEND_INVARIANT[preprocessing-parameter-list-"
                 "owner]: function parameter list has no exact typed role\n";
          ROSE_ABORT();
        }
        if (is_semantic && syntax != nullptr && syntax != semantic) {
          for (SgNode *path_node : uncached_path) {
            classification_cache.emplace(path_node, true);
          }
          return true;
        }
      }
    }

    if (SgVariableDefinition *definition = isSgVariableDefinition(cursor)) {
      SgInitializedName *initialized_name =
          isSgInitializedName(definition->get_parent());
      if (initialized_name == nullptr ||
          initialized_name->get_definition() != definition ||
          definition->get_vardefn() != initialized_name) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-variable-definition-"
                     "owner]: semantic variable definition has no exact "
                     "initialized-name owner\n";
        ROSE_ABORT();
      }
      for (SgNode *path_node : uncached_path) {
        classification_cache.emplace(path_node, true);
      }
      return true;
    }

    if (SgDeclarationScope *declaration_scope = isSgDeclarationScope(cursor)) {
      SgNode *owner = SageBuilder::getDeclarationScopeOwner(declaration_scope);
      SgNode *structural_parent = declaration_scope->get_parent();
      bool has_exact_typed_owner = false;
      if (SgDeclarationScopeList *container =
              isSgDeclarationScopeList(structural_parent)) {
        SgScopeStatement *semantic_owner = isSgScopeStatement(owner);
        has_exact_typed_owner =
            semantic_owner != nullptr && container->get_parent() == owner &&
            semantic_owner->get_auxiliary_declaration_scopes() == container &&
            std::count(container->get_scopes().begin(),
                       container->get_scopes().end(), declaration_scope) == 1;
      } else if (SgDeclarationStatement *declaration_owner =
                     isSgDeclarationStatement(structural_parent)) {
        has_exact_typed_owner =
            owner == declaration_owner &&
            (declaration_owner->get_declarationScope() == declaration_scope ||
             declaration_owner->get_source_declarator_scope() ==
                 declaration_scope ||
             SageBuilder::getNonrealDeclarationScope(declaration_owner) ==
                 declaration_scope ||
             (isSgFunctionDeclaration(declaration_owner) != nullptr &&
              isSgFunctionDeclaration(declaration_owner)
                      ->get_function_declarator_scope() == declaration_scope));
      }
      const bool direct_declaration_is_exact =
          direct_declaration == nullptr ||
          (direct_declaration->get_parent() == declaration_scope &&
           declaration_scope->statementExistsInScope(direct_declaration));
      if (!has_exact_typed_owner || !direct_declaration_is_exact) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-declaration-scope-"
                     "owner]: node="
                  << node << "/" << node->class_name()
                  << " declaration-scope=" << declaration_scope
                  << " parent=" << declaration_scope->get_parent()
                  << " registered-owner=" << owner
                  << " direct-declaration=" << direct_declaration << "/"
                  << (direct_declaration != nullptr
                          ? direct_declaration->class_name()
                          : std::string("<null>"))
                  << " declaration-parent="
                  << (direct_declaration != nullptr
                          ? direct_declaration->get_parent()
                          : nullptr)
                  << " declaration-in-scope="
                  << (direct_declaration != nullptr &&
                              declaration_scope->statementExistsInScope(
                                  direct_declaration)
                          ? 1
                          : 0)
                  << "\n";
        ROSE_ABORT();
      }
      for (SgNode *path_node : uncached_path) {
        classification_cache.emplace(path_node, true);
      }
      return true;
    }
  }
  for (SgNode *path_node : uncached_path) {
    classification_cache.emplace(path_node, false);
  }
  return false;
}

static bool hasExactPhysicalSourceInterval(SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
      start->get_col() <= 0 || end->get_line() <= 0 || end->get_col() <= 0 ||
      start->get_physical_file_id() < 0 || end->get_physical_file_id() < 0 ||
      !start->isSameFile(*end) ||
      start->get_physical_file_occurrence_id() !=
          end->get_physical_file_occurrence_id()) {
    return false;
  }
  return start->get_line() < end->get_line() ||
         (start->get_line() == end->get_line() &&
          start->get_col() <= end->get_col());
}

} // namespace

// DQ (11/28/2020): Use this for testing the DOT graph generator.
#define EXIT_AFTER_BUILDING_DOT_FILE 0

int clang_main(int argc, char **argv, SgSourceFile &sageFile,
               const char *driver_argv0) {
  roseClangPhaseTrace("clang_main.begin");
  // printf ("sageFile.get_clang_il_to_graphviz() = %s
  // \n",sageFile.get_clang_il_to_graphviz() ? "true" : "false");

  // DQ (11/27/2020): Use the -rose:clang_il_to_graphviz option to comntrol the
  // use of the Clang Dot generator.
#if EXIT_AFTER_BUILDING_DOT_FILE
  if (true)
#else
  if (sageFile.get_clang_il_to_graphviz() == true)
#endif
  {
    int clang_to_dot_status = clang_to_dot_main(argc, argv, driver_argv0);
    if (clang_to_dot_status != 0) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-dot]: Clang AST graph generation "
             "failed with status "
          << clang_to_dot_status << "\n";
      ROSE_ABORT();
    }

#if EXIT_AFTER_BUILDING_DOT_FILE
    return 0;
#endif
  }

  beginClangFrontendValgrindPublicationSession();

  // 0 - Analyse Cmd Line

  std::vector<std::string> sys_dirs_list;
  std::vector<std::string> define_list;
  std::vector<std::string> inc_list;
  std::string input_file;
  std::size_t input_passthrough_index = std::numeric_limits<std::size_t>::max();
  clang::driver::types::ID input_type = clang::driver::types::TY_INVALID;
  bool input_has_explicit_language = false;
  bool input_is_preprocessed = false;
  ClangToSageTranslator::Language language = ClangToSageTranslator::unknown;
  // User arguments and the source operand stay in their original order.  The
  // lists above contain only REX-owned injections.
  std::vector<std::string> passthrough_args;
  std::string active_explicit_language_name;
  std::string explicit_resource_dir;
  std::map<std::size_t, std::string> canonical_option_operands;

  std::vector<const char *> raw_clang_args;
  if (argc < 0 || (argc != 0 && argv == nullptr)) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-command]: invalid argc/argv "
                    "pair\n";
    ROSE_ABORT();
  }
  raw_clang_args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == nullptr) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[clang-command]: null argv[" << i
                   << "] before argc\n";
      ROSE_ABORT();
    }
    raw_clang_args.push_back(argv[i]);
  }
  unsigned missing_arg_index = 0;
  unsigned missing_arg_count = 0;
  llvm::opt::InputArgList parsed_clang_args =
      clang::getDriverOptTable().ParseArgs(
          raw_clang_args, missing_arg_index, missing_arg_count,
          llvm::opt::Visibility(clang::options::ClangOption));
  if (missing_arg_count != 0) {
    llvm::StringRef option =
        missing_arg_index < raw_clang_args.size()
            ? llvm::StringRef(raw_clang_args[missing_arg_index])
            : llvm::StringRef("<missing option>");
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-option-operand]: option '"
                 << option << "' is missing " << missing_arg_count
                 << " required argument(s)\n";
    ROSE_ABORT();
  }

  std::vector<const llvm::opt::Arg *> option_at_index(raw_clang_args.size(),
                                                      nullptr);
  std::vector<bool> positional_at_index(raw_clang_args.size(), false);
  for (const llvm::opt::Arg *parsed_arg : parsed_clang_args) {
    ROSE_ASSERT(parsed_arg != nullptr);
    const std::size_t index = parsed_arg->getIndex();
    if (index >= raw_clang_args.size()) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-option-table]: Clang option parser "
             "returned an out-of-range source index\n";
      ROSE_ABORT();
    }
    if (parsed_arg->getOption().getKind() == llvm::opt::Option::InputClass) {
      positional_at_index[index] = true;
    } else {
      if (option_at_index[index] != nullptr) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[clang-option-table]: Clang option "
               "parser returned multiple primary options for argv["
            << index << "]\n";
        ROSE_ABORT();
      }
      option_at_index[index] = parsed_arg;
    }
  }

  auto reject_reserved_macro = [](llvm::StringRef option,
                                  llvm::StringRef operand) {
    const llvm::StringRef macro_name = operand.split('=').first;
    if (macro_name == "USE_ROSE" ||
        macro_name == "ROSE_LLVM_OPENMP_HEADER_FILE") {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[reserved-frontend-macro]: option '"
          << option << "' attempts to modify REX-owned macro '" << macro_name
          << "'\n";
      ROSE_ABORT();
    }
  };
  auto set_active_explicit_language = [&](llvm::StringRef option,
                                          llvm::StringRef type_name) {
    if (type_name == "none") {
      active_explicit_language_name.clear();
      return;
    }
    if (languageFromClangTypeName(type_name) ==
        ClangToSageTranslator::unknown) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-language]: explicit language "
             "option '"
          << option << "' selects unsupported language '" << type_name << "'\n";
      ROSE_ABORT();
    }
    active_explicit_language_name = type_name.str();
  };

  for (int i = 0; i < argc; i++) {
    std::string current_arg(argv[i]);

    if (positional_at_index[static_cast<std::size_t>(i)]) {
      clang::driver::types::ID positional_input_type =
          clang::driver::types::TY_INVALID;
      if (active_explicit_language_name.empty()) {
        llvm::StringRef extension = llvm::sys::path::extension(current_arg);
        if (extension.starts_with(".")) {
          extension = extension.drop_front();
        }
        positional_input_type =
            extension == "ocl"
                ? clang::driver::types::TY_CL
                : clang::driver::types::lookupTypeForExtension(extension);
      } else {
        const std::string clang_type_name =
            active_explicit_language_name == "opencl"
                ? std::string("cl")
                : active_explicit_language_name;
        positional_input_type =
            clang::driver::types::lookupTypeForTypeSpecifier(
                clang_type_name.c_str());
      }
      const ClangToSageTranslator::Language positional_language =
          languageFromClangInputType(positional_input_type);
      if (positional_language == ClangToSageTranslator::unknown) {
        // Clang accepts non-source positional linker inputs such as objects and
        // archives alongside one source. Preserve them for the exact driver
        // invocation, but never let one satisfy REX's one-source contract.
        passthrough_args.push_back(current_arg);
        continue;
      }
      if (!input_file.empty()) {
        llvm::errs() << "REX_FRONTEND_INVARIANT[clang-source-count]: command "
                        "contains more than one positional source operand ('"
                     << input_file << "', '" << current_arg << "')\n";
        ROSE_ABORT();
      }
      input_file = current_arg;
      input_type = positional_input_type;
      input_has_explicit_language = !active_explicit_language_name.empty();
      input_is_preprocessed =
          !clang::driver::types::isSrcFile(positional_input_type);
      language = positional_language;
      input_passthrough_index = passthrough_args.size();
      passthrough_args.push_back(current_arg);
      continue;
    }

    const llvm::opt::Arg *parsed_option =
        option_at_index[static_cast<std::size_t>(i)];
    if (parsed_option == nullptr) {
      // This raw token is owned by the preceding parsed option. Preserve it in
      // place without interpreting option-like operand text as another option.
      auto canonical_operand =
          canonical_option_operands.find(static_cast<std::size_t>(i));
      passthrough_args.push_back(canonical_operand ==
                                         canonical_option_operands.end()
                                     ? current_arg
                                     : canonical_operand->second);
      continue;
    }

    if (isRoseInternalOption(current_arg)) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[unconsumed-rose-option]: ROSE option '"
          << current_arg
          << "' reached Clang after ROSE command-line processing\n";
      ROSE_ABORT();
    }

    if (isRexAstJsonOption(current_arg)) {
      // Consumed by the Sage AST JSON checkpoint path after the frontend AST
      // exists. Clang cc1 must not see REX-only checkpoint options.
      continue;
    }

    if (parsed_option->getOption().matches(clang::options::OPT_x)) {
      if (parsed_option->getNumValues() != 1) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[clang-language]: parsed -x option '"
            << current_arg << "' does not own exactly one language value\n";
        ROSE_ABORT();
      }
      const llvm::StringRef type_name(parsed_option->getValue());
      set_active_explicit_language(current_arg, type_name);
      if (type_name == "opencl") {
        // `opencl` is a documented REX language spelling. Canonicalize only
        // this owned API value to Clang's exact `cl` spelling.
        if (current_arg == "-x") {
          const std::size_t operand_index = static_cast<std::size_t>(i) + 1;
          if (operand_index >= raw_clang_args.size() ||
              llvm::StringRef(raw_clang_args[operand_index]) != type_name) {
            llvm::errs()
                << "REX_FRONTEND_INVARIANT[clang-language]: split -x OpenCL "
                   "alias has no exact owned operand\n";
            ROSE_ABORT();
          }
          canonical_option_operands.emplace(operand_index, "cl");
          passthrough_args.push_back(current_arg);
        } else {
          passthrough_args.push_back("-xcl");
        }
      } else {
        passthrough_args.push_back(current_arg);
      }
    } else if (parsed_option->getOption().matches(
                   clang::options::OPT_resource_dir) ||
               parsed_option->getOption().matches(
                   clang::options::OPT_resource_dir_EQ)) {
      if (parsed_option->getNumValues() != 1 ||
          llvm::StringRef(parsed_option->getValue()).empty()) {
        llvm::errs() << "REX_FRONTEND_INVARIANT[clang-resource-dir]: option '"
                     << current_arg
                     << "' does not own exactly one nonempty path\n";
        ROSE_ABORT();
      }
      if (!explicit_resource_dir.empty()) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[clang-resource-dir]: command contains "
               "more than one explicit resource directory\n";
        ROSE_ABORT();
      }
      explicit_resource_dir = parsed_option->getValue();
      passthrough_args.push_back(current_arg);
    } else if (parsed_option->getOption().matches(clang::options::OPT_D) ||
               parsed_option->getOption().matches(clang::options::OPT_U)) {
      if (parsed_option->getNumValues() != 1) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[reserved-frontend-macro]: parsed macro "
               "option '"
            << current_arg << "' does not own exactly one macro name\n";
        ROSE_ABORT();
      }
      reject_reserved_macro(current_arg, parsed_option->getValue());
      passthrough_args.push_back(current_arg);
    } else if (current_arg == "-fopenmp-version" ||
               current_arg.rfind("-fopenmp-version=", 0) == 0) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[removed-openmp-version-option]: option '"
          << current_arg
          << "' was part of the removed frontend-owned OpenMP design and is "
             "not supported\n";
      ROSE_ABORT();
    } else if (current_arg == "-fopenmp" ||
               current_arg.rfind("-fopenmp=", 0) == 0 ||
               current_arg == "-fopenmp-simd") {
      // Consume the driver option without forwarding it. REX preserves the
      // pragma text and its OpenMP AST constructor owns directive semantics.
    } else if (current_arg == "-rex:clang:disable-access-control" ||
               current_arg == "-rex:clang:delayed-template-parsing" ||
               current_arg == "-rex:clang:respect-rtti-flags") {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[removed-clang-option]: option '"
          << current_arg
          << "' was an inexact compatibility API and is not supported\n";
      ROSE_ABORT();
    } else {
#if DEBUG_ARGS
      std::cerr << "argv[" << i << "] = " << current_arg
                << " preserved as passthrough option." << std::endl;
#endif
      passthrough_args.push_back(current_arg);
    }
  }
  roseClangPhaseTrace("clang_main.argscan.end");

  if (input_file.empty()) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-source-count]: command has "
                    "no source operand\n";
    ROSE_ABORT();
  }
  llvm::SmallString<256> canonical_input_file;
  if (std::error_code ec =
          llvm::sys::fs::real_path(input_file, canonical_input_file)) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-source-path]: source '"
                 << input_file
                 << "' cannot be resolved exactly: " << ec.message() << "\n";
    ROSE_ABORT();
  }

  // Detect if this is a secondary parse during lowering/outlining
  // Check if this file is being re-parsed (project already has a file with same
  // source)
  const bool openmp_ast_mode =
      (sageFile.get_openmp() &&
       (sageFile.get_openmp_ast_only() || sageFile.get_openmp_parse_only())) ||
      (sageFile.get_openacc() &&
       (sageFile.get_openacc_ast_only() || sageFile.get_openacc_parse_only()));
  bool is_secondary_parse = false;
  if (sageFile.get_parent() != NULL) {
    SgProject *project = isSgProject(sageFile.get_parent()->get_parent());
    if (project != NULL) {
      // Check if project already has a file with the same source filename
      // This indicates we're in a secondary parse (e.g., during
      // outlining/lowering)
      SgFilePtrList &file_list = project->get_fileList();

      const std::string normalized_input = canonical_input_file.str().str();

      for (SgFilePtrList::iterator it = file_list.begin();
           it != file_list.end(); ++it) {
        SgSourceFile *existing_file = isSgSourceFile(*it);
        if (existing_file != NULL && existing_file != &sageFile) {
          std::string existing_filename =
              existing_file->get_sourceFileNameWithPath();

          llvm::SmallString<256> canonical_existing_filename;
          if (std::error_code ec = llvm::sys::fs::real_path(
                  existing_filename, canonical_existing_filename)) {
            llvm::errs()
                << "REX_FRONTEND_INVARIANT[clang-source-path]: project source '"
                << existing_filename
                << "' cannot be resolved exactly: " << ec.message() << "\n";
            ROSE_ABORT();
          }
          const std::string normalized_existing =
              canonical_existing_filename.str().str();

          // Use exact path comparison only (no substring matching)
          if (normalized_existing == normalized_input) {
            is_secondary_parse = true;
            break;
          }
        }
      }
    }
  }

  if (language == ClangToSageTranslator::unknown) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-language]: source '"
                 << input_file
                 << "' has no supported extension and no exact -x language\n";
    ROSE_ABORT();
  }

  auto has_passthrough_optimization_flag = [&]() {
    return std::any_of(passthrough_args.begin(), passthrough_args.end(),
                       [](const std::string &arg) {
                         return arg.size() >= 2 && arg[0] == '-' &&
                                arg[1] == 'O';
                       });
  };

  if (!input_is_preprocessed && (language == ClangToSageTranslator::C ||
                                 language == ClangToSageTranslator::CPLUSPLUS ||
                                 language == ClangToSageTranslator::CUDA)) {
    // Frontend parsing sees the generic REX translation marker.
    define_list.push_back("USE_ROSE");
  }

  if (sageFile.get_optimization() && !has_passthrough_optimization_flag()) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-optimization-option]: Sage records "
           "optimization as enabled, but the exact -O option was lost before "
           "Clang frontend invocation\n";
    ROSE_ABORT();
  }

  RoseClangPathRoots clang_paths = resolveRoseClangPaths(driver_argv0);
  std::string builtin_header_root = clang_paths.builtin_header_root;

  sys_dirs_list.push_back(builtin_header_root);

  std::vector<std::string> frontend_support_preinclude_paths;
  auto add_frontend_support_preinclude = [&](llvm::StringRef filename) {
    if (filename.empty()) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[frontend-support-ownership]: "
                      "cannot configure an unnamed frontend support header\n";
      ROSE_ABORT();
    }
    llvm::SmallString<256> path(builtin_header_root);
    llvm::sys::path::append(path, filename);
    const std::string normalized =
        FileHelper::normalizePathIfPossible(path.str().str());
    if (normalized.empty() || !FileHelper::fileExists(normalized)) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[frontend-support-ownership]: "
                      "configured frontend support header does not identify "
                      "one physical file: "
                   << path << "\n";
      ROSE_ABORT();
    }
    inc_list.push_back(filename.str());
    frontend_support_preinclude_paths.push_back(normalized);
  };

  if (!input_is_preprocessed) {
    switch (language) {
    case ClangToSageTranslator::C:
      add_frontend_support_preinclude("clang-builtin-c.h");
      break;
    case ClangToSageTranslator::CPLUSPLUS:
      add_frontend_support_preinclude("clang-builtin-cpp.hpp");
      break;
    case ClangToSageTranslator::CUDA:
      add_frontend_support_preinclude("clang-builtin-cuda.hpp");
      break;
    case ClangToSageTranslator::OPENCL:
      add_frontend_support_preinclude("clang-builtin-opencl.h");
      break;
    case ClangToSageTranslator::OBJC: {
      printf("Objective C langauge support is not available in ROSE \n");
      ROSE_ABORT();
    }
    default: {
      printf("Default reached in switch(language) support \n");
      ROSE_ABORT();
    }
    }
  }

#if defined(ROSE_BUILD_CUDA_LANGUAGE_SUPPORT) &&                               \
    defined(ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR)
  if (language == ClangToSageTranslator::CUDA) {
    const std::string cuda_runtime_include_dir =
        ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR;
    llvm::SmallString<256> required_header(cuda_runtime_include_dir);
    llvm::sys::path::append(required_header, "curand_mtgp32_kernel.h");
    if (!llvm::sys::fs::exists(required_header)) {
      llvm::errs()
          << "Configured CUDA toolkit is missing the header required by "
             "Clang's CUDA runtime wrapper: "
          << required_header << "\n";
      ROSE_ABORT();
    }
    sys_dirs_list.push_back(cuda_runtime_include_dir);
  }
#elif defined(ROSE_BUILD_CUDA_LANGUAGE_SUPPORT)
  if (language == ClangToSageTranslator::CUDA) {
    llvm::errs()
        << "REX was configured with CUDA support but without the exact "
           "runtime-wrapper dependency header directory.\n";
    ROSE_ABORT();
  }
#else
  if (language == ClangToSageTranslator::CUDA) {
    llvm::errs() << "REX CUDA frontend input requires ENABLE-CUDA=ON.\n";
    ROSE_ABORT();
  }
#endif

  // Keep REX OpenMP/OpenACC extension APIs visible during frontend parsing.
  if (!input_is_preprocessed &&
      (sageFile.get_openmp() || openmp_ast_mode || sageFile.get_openacc())) {
    add_frontend_support_preinclude("clang-builtin-openmp-compat.h");
#ifdef LLVM_OPENMP_INCLUDE_PATH
    if (std::strlen(LLVM_OPENMP_INCLUDE_PATH) != 0) {
      // Prefer including the configured LLVM omp.h by absolute path from the
      // compatibility wrapper to avoid mixing incompatible Clang resource
      // header directories in the active include search.
      define_list.push_back(std::string("ROSE_LLVM_OPENMP_HEADER_FILE=\"") +
                            LLVM_OPENMP_INCLUDE_PATH + "/omp.h\"");
    }
#endif
  }

  if (!input_is_preprocessed) {
    assertRequiredPreincludeConfigured(inc_list, language,
                                       "driver argument construction");
  }

  // Build cc1-style argument list for the in-process Clang invocation.

  const size_t estimated_argc = 4 + define_list.size() +
                                (sys_dirs_list.size() * 2) +
                                (inc_list.size() * 2) + passthrough_args.size();
  std::vector<std::string> args_storage;
  args_storage.reserve(estimated_argc);
  for (const auto &define : define_list) {
    args_storage.push_back("-D" + define);
  }
  for (const auto &sys_dir : sys_dirs_list) {
    args_storage.push_back("-isystem");
    args_storage.push_back(sys_dir);
  }
  for (const auto &inc : inc_list) {
    args_storage.push_back("-include");
    args_storage.push_back(inc);
  }
  if (input_passthrough_index >= passthrough_args.size() ||
      input_type == clang::driver::types::TY_INVALID) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-source-role]: source input "
                    "has no exact passthrough position or Clang input type\n";
    ROSE_ABORT();
  }
  for (std::size_t index = 0; index < passthrough_args.size(); ++index) {
    if (index == input_passthrough_index && !input_has_explicit_language) {
      args_storage.push_back("-x");
      args_storage.push_back(clang::driver::types::getTypeName(input_type));
    }
    args_storage.push_back(passthrough_args[index]);
  }

  std::vector<const char *> args;
  args.reserve(args_storage.size());
  for (const auto &arg : args_storage) {
    args.push_back(arg.c_str());
  }
  if (std::getenv("ROSE_CLANG_DUMP_INCLUDES") != nullptr) {
    std::cerr << "ROSE clang invocation args:\n";
    for (const auto &arg : args_storage) {
      std::cerr << "  " << arg << '\n';
    }
  }
#if DEBUG_ARGS
  for (size_t index = 0; index < args.size(); ++index) {
    std::cerr << "args[" << index << "] = " << args[index] << std::endl;
  }
#endif

  // 2 - Create a compiler instance

  auto diag_opts = std::make_shared<clang::DiagnosticOptions>();

  auto compiler_instance = std::make_unique<clang::CompilerInstance>();

  // Create diagnostics with instance-specific physical filesystem (avoids
  // global singleton double-free) Use createPhysicalFileSystem() instead of
  // getRealFileSystem() to avoid sharing the global singleton.
  // getRealFileSystem() returns a static IntrusiveRefCntPtr that libLLVM.so's
  // global destructor also tries to clean up, causing double-free.
  // createPhysicalFileSystem() creates a new instance that CompilerInstance can
  // safely own and destroy.
  //
  // Persist the VFS IntrusiveRefCntPtr so it lives beyond createDiagnostics()
  // call. Otherwise the temporary unique_ptr is destroyed immediately, leaving
  // a dangling reference.
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs =
      llvm::vfs::createPhysicalFileSystem();
  compiler_instance->setVirtualFileSystem(vfs);

  // TextDiagnosticPrinter keeps a reference to DiagnosticOptions; ensure it
  // outlives the diagnostics engine for LLVM 22.
  clang::TextDiagnosticPrinter *diag_printer =
      new clang::TextDiagnosticPrinter(llvm::errs(), *diag_opts);

  // LLVM 22 returns a DiagnosticsEngine; wire it into the CompilerInstance.
  compiler_instance->setDiagnostics(clang::CompilerInstance::createDiagnostics(
      *vfs, *diag_opts, diag_printer, true));

  clang::CompilerInvocation &invocation = compiler_instance->getInvocation();
  const std::string default_triple = llvm::sys::getDefaultTargetTriple();

  // Use Clang's driver to build cc1 arguments so target options (e.g. RISC-V
  // ABI defaults) match the toolchain instead of cc1's soft-float defaults.
  std::string driver_executable;
  auto resource_dir_has_header = [](const std::string &resource_dir,
                                    llvm::StringRef header) -> bool {
    if (resource_dir.empty()) {
      return false;
    }
    llvm::SmallString<256> path(resource_dir);
    llvm::sys::path::append(path, "include", header);
    return llvm::sys::fs::exists(path);
  };
  auto canonicalize_existing_path = [](llvm::StringRef path,
                                       llvm::StringRef contract,
                                       llvm::StringRef description) {
    if (path.empty()) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[" << contract
                   << "]: " << description << " path is empty\n";
      ROSE_ABORT();
    }
    llvm::SmallString<256> resolved;
    if (std::error_code ec = llvm::sys::fs::real_path(path, resolved)) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[" << contract
                   << "]: " << description << " path '" << path
                   << "' cannot be resolved exactly: " << ec.message() << "\n";
      ROSE_ABORT();
    }
    return resolved.str().str();
  };
  const bool prefer_cxx_driver = language == ClangToSageTranslator::CPLUSPLUS ||
                                 language == ClangToSageTranslator::CUDA;
  const std::string clang_version_suffix = std::to_string(LLVM_VERSION_MAJOR);
#if !defined(REX_WASM_IN_PROCESS_CLANG)
  auto normalize_llvm_root = [](const std::string &llvm_dir_in) -> std::string {
    std::string llvm_dir = llvm_dir_in;
    if (!llvm_dir.empty() && llvm_dir.back() == '/') {
      llvm_dir.pop_back();
    }
    if (llvm_dir.empty()) {
      return llvm_dir;
    }
    const std::string cmake_suffix = "/lib/cmake/llvm";
    const std::string cmake_suffix_version =
        "/lib/cmake/llvm-" + std::to_string(LLVM_VERSION_MAJOR);
    const std::string packaged_cmake_suffix = "/cmake";
    llvm::StringRef dir_ref(llvm_dir);
    if (dir_ref.ends_with(cmake_suffix) ||
        dir_ref.ends_with(cmake_suffix_version)) {
      llvm::StringRef root_ref = llvm::sys::path::parent_path(dir_ref);
      root_ref = llvm::sys::path::parent_path(root_ref);
      root_ref = llvm::sys::path::parent_path(root_ref);
      return root_ref.str();
    }
    // Debian-family LLVM packages publish LLVMConfig.cmake directly under
    // <llvm-root>/cmake while keeping the exact compiler drivers under
    // <llvm-root>/bin.  LLVM_DIR denotes the package directory, never the
    // executable root itself.
    if (dir_ref.ends_with(packaged_cmake_suffix)) {
      return llvm::sys::path::parent_path(dir_ref).str();
    }
    return llvm_dir;
  };
  std::string llvm_root;
  if (const char *llvm_dir_env = std::getenv("LLVM_DIR")) {
    llvm_root = normalize_llvm_root(llvm_dir_env);
  }
  const std::vector<std::string> versioned_clang_driver_names =
      prefer_cxx_driver
          ? std::vector<std::string>{"clang++-" + clang_version_suffix,
                                     "clang-" + clang_version_suffix}
          : std::vector<std::string>{"clang-" + clang_version_suffix,
                                     "clang++-" + clang_version_suffix};
  std::vector<std::string> configured_root_driver_names =
      versioned_clang_driver_names;
  if (prefer_cxx_driver) {
    configured_root_driver_names.push_back("clang++");
    configured_root_driver_names.push_back("clang");
  } else {
    configured_root_driver_names.push_back("clang");
    configured_root_driver_names.push_back("clang++");
  }
  auto find_clang_in_root =
      [&](const std::string &root,
          const std::vector<std::string> &driver_names) -> std::string {
    if (root.empty()) {
      return std::string();
    }
    for (const std::string &driver_name : driver_names) {
      std::string candidate = root + "/bin/" + driver_name;
      if (llvm::sys::fs::exists(candidate)) {
        return candidate;
      }
    }
    return std::string();
  };
#endif
  std::string clang_driver_path;
  {
#if defined(REX_WASM_IN_PROCESS_CLANG)
    clang_driver_path =
        prefer_cxx_driver ? REX_WASM_CLANG_CXX_DRIVER : REX_WASM_CLANG_C_DRIVER;
#else
    if (!llvm_root.empty()) {
      clang_driver_path =
          find_clang_in_root(llvm_root, configured_root_driver_names);
      if (clang_driver_path.empty()) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[clang-driver-path]: LLVM_DIR selects '"
            << llvm_root
            << "', but that root has no executable for linked LLVM "
            << LLVM_VERSION_MAJOR << "\n";
        ROSE_ABORT();
      }
    } else {
      for (const std::string &driver_name : versioned_clang_driver_names) {
        if (auto clang_path = llvm::sys::findProgramByName(driver_name)) {
          clang_driver_path = clang_path.get();
          break;
        }
      }
    }
    if (clang_driver_path.empty()) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-driver-path]: no versioned Clang "
             "driver for linked LLVM "
          << LLVM_VERSION_MAJOR << " is available on PATH\n";
      ROSE_ABORT();
    }
#endif
    driver_executable = canonicalize_existing_path(
        clang_driver_path, "clang-driver-path", "Clang driver executable");
    if (!llvm::sys::fs::can_execute(driver_executable)) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[clang-driver-path]: selected "
                      "Clang driver '"
                   << driver_executable << "' is not executable\n";
      ROSE_ABORT();
    }
  }

  std::vector<const char *> driver_args;
  driver_args.reserve(args_storage.size() + 2);
  driver_args.push_back(driver_executable.c_str());
  for (const auto &arg : args_storage) {
    driver_args.push_back(arg.c_str());
  }

  clang::driver::Driver driver(driver_executable, default_triple,
                               compiler_instance->getDiagnostics());
  driver.setTitle(prefer_cxx_driver ? "clang++" : "clang");
#if defined(REX_WASM_IN_PROCESS_CLANG)
  const std::string resource_dir_candidate = canonicalize_existing_path(
      explicit_resource_dir.empty() ? REX_WASM_CLANG_RESOURCE_DIR
                                    : explicit_resource_dir,
      "clang-resource-dir",
      explicit_resource_dir.empty()
          ? "configured REX WASM Clang resource directory"
          : "explicit REX WASM Clang resource directory");
#else
  const std::string resource_dir_candidate = canonicalize_existing_path(
      explicit_resource_dir.empty() ? driver.ResourceDir
                                    : explicit_resource_dir,
      "clang-resource-dir",
      explicit_resource_dir.empty()
          ? "selected Clang driver's resource directory"
          : "explicit Clang resource directory");
#endif
  llvm::StringRef resource_version =
      llvm::sys::path::filename(resource_dir_candidate);
  if (resource_version != clang_version_suffix &&
      !resource_version.starts_with(clang_version_suffix + ".")) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-resource-dir]: selected driver '"
        << driver_executable << "' resolved resource version '"
        << resource_version << "', but REX is linked to LLVM "
        << LLVM_VERSION_MAJOR << "\n";
    ROSE_ABORT();
  }
  llvm::SmallString<256> driver_install_root(driver_executable);
  llvm::sys::path::remove_filename(driver_install_root);
  llvm::sys::path::remove_filename(driver_install_root);
  const std::string driver_install_prefix = driver_install_root.str().str();
  const std::string driver_install_prefix_with_separator =
      driver_install_prefix + llvm::sys::path::get_separator().str();
  if (resource_dir_candidate != driver_install_prefix &&
      !llvm::StringRef(resource_dir_candidate)
           .starts_with(driver_install_prefix_with_separator)) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-resource-dir]: selected driver '"
        << driver_executable << "' and resource directory '"
        << resource_dir_candidate << "' belong to different install roots\n";
    ROSE_ABORT();
  }
  if (!resource_dir_has_header(resource_dir_candidate, "stddef.h")) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-resource-dir]: selected LLVM "
                 << LLVM_VERSION_MAJOR << " resource directory '"
                 << resource_dir_candidate << "' has no include/stddef.h\n";
    ROSE_ABORT();
  }
  driver.ResourceDir = resource_dir_candidate;

  std::unique_ptr<clang::driver::Compilation> compilation(
      driver.BuildCompilation(driver_args));
  if (!compilation) {
    llvm::errs() << "Failed to build Clang driver compilation for frontend.\n";
    ROSE_ABORT();
  }

  const clang::driver::Command *cc1_command = nullptr;
  for (const auto &job : compilation->getJobs()) {
    const auto *command = llvm::dyn_cast<clang::driver::Command>(&job);
    if (!command) {
      continue;
    }
    const auto &command_args = command->getArguments();
    bool is_cc1_command = false;
    bool is_cuda_device_command = false;
    for (const char *arg : command_args) {
      if (arg && std::strcmp(arg, "-cc1") == 0) {
        is_cc1_command = true;
      } else if (arg && std::strcmp(arg, "-fcuda-is-device") == 0) {
        is_cuda_device_command = true;
      }
    }
    if (!is_cc1_command) {
      continue;
    }

    // A CUDA driver compilation contains one or more device cc1 jobs followed
    // by the host cc1 job.  REX constructs one source-to-source AST, so its
    // target and system-header environment must come from the host job; the
    // device job still uses the same parsed CUDA declarations but has an
    // NVPTX target that cannot own the translation unit's host ABI.
    if (language == ClangToSageTranslator::CUDA && is_cuda_device_command) {
      continue;
    }

    bool owns_current_source = false;
    for (const clang::driver::InputInfo &input : command->getInputInfos()) {
      for (const char *candidate :
           {input.isFilename() ? input.getFilename() : nullptr,
            input.getBaseInput()}) {
        if (candidate == nullptr || *candidate == '\0') {
          continue;
        }
        llvm::SmallString<256> resolved_candidate;
        if (!llvm::sys::fs::real_path(candidate, resolved_candidate) &&
            resolved_candidate == canonical_input_file) {
          owns_current_source = true;
        }
      }
    }
    if (!owns_current_source) {
      continue;
    }
    if (cc1_command != nullptr) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-driver-source-job]: Clang driver "
             "produced more than one host cc1 command for exact source '"
          << canonical_input_file << "'\n";
      ROSE_ABORT();
    }
    cc1_command = command;
  }

  if (!cc1_command) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-driver-source-job]: failed to locate "
           "one host cc1 command for exact source '"
        << canonical_input_file << "'\n";
    ROSE_ABORT();
  }

  const auto &cc1_args = cc1_command->getArguments();
  std::vector<std::string> cc1_args_storage;
  cc1_args_storage.reserve(cc1_args.size());
  for (const char *arg : cc1_args) {
    if (arg) {
      cc1_args_storage.emplace_back(arg);
    } else {
      cc1_args_storage.emplace_back();
    }
  }

  auto canonical_path = [&](const std::string &path) -> std::string {
    return canonicalize_existing_path(path, "clang-include-path",
                                      "system include directory");
  };

  std::set<std::string> cc1_system_include_dirs;
  auto record_system_include = [&](const std::string &dir) {
    if (dir.empty()) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-include-path]: Clang driver "
             "produced an empty system include candidate\n";
      ROSE_ABORT();
    }

    // The Clang driver deliberately emits a superset of candidate system
    // directories.  cc1's header-search initialization discards candidates
    // that do not exist (for example a target-specific GCC include directory
    // absent from an otherwise complete host installation).  Such a candidate
    // is not an effective include path and therefore must not be canonicalized
    // as one.  Preserve that exact driver/cc1 boundary while keeping every
    // effective path strict and canonical.
    llvm::sys::fs::file_status include_status;
    if (std::error_code ec = llvm::sys::fs::status(dir, include_status, true)) {
      if (ec == std::errc::no_such_file_or_directory ||
          ec == std::errc::not_a_directory) {
        return;
      }
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-include-path]: system include "
             "candidate '"
          << dir << "' cannot be inspected exactly: " << ec.message() << "\n";
      ROSE_ABORT();
    }
    if (!llvm::sys::fs::exists(include_status)) {
      return;
    }
    if (!llvm::sys::fs::is_directory(include_status)) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[clang-include-path]: effective system "
             "include path '"
          << dir << "' is not a directory\n";
      ROSE_ABORT();
    }
    cc1_system_include_dirs.insert(canonical_path(dir));
  };
  auto parse_system_include_flag = [&](const std::string &flag,
                                       std::size_t index) -> bool {
    auto consume_prefixed = [&](const char *prefix) -> bool {
      const std::size_t prefix_len = std::strlen(prefix);
      if (flag.rfind(prefix, 0) != 0 || flag.size() <= prefix_len ||
          flag[prefix_len] != '=') {
        return false;
      }
      std::string suffix = flag.substr(prefix_len + 1);
      if (suffix.empty()) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[clang-include-path]: Clang driver "
               "produced an empty operand for '"
            << prefix << "='\n";
        ROSE_ABORT();
      }
      record_system_include(suffix);
      return true;
    };

    if (flag == "-isystem" || flag == "-internal-isystem" ||
        flag == "-internal-externc-isystem" || flag == "-c-isystem" ||
        flag == "-cxx-isystem") {
      if (index + 1 >= cc1_args_storage.size()) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[clang-include-path]: Clang driver "
               "produced system include option '"
            << flag << "' without an operand\n";
        ROSE_ABORT();
      }
      record_system_include(cc1_args_storage[index + 1]);
      return true;
    }
    return consume_prefixed("-isystem") ||
           consume_prefixed("-internal-isystem") ||
           consume_prefixed("-internal-externc-isystem") ||
           consume_prefixed("-c-isystem") || consume_prefixed("-cxx-isystem");
  };
  auto is_system_include_flag = [](llvm::StringRef flag) {
    return flag == "-isystem" || flag == "-internal-isystem" ||
           flag == "-internal-externc-isystem" || flag == "-c-isystem" ||
           flag == "-cxx-isystem" || flag.starts_with("-isystem=") ||
           flag.starts_with("-internal-isystem=") ||
           flag.starts_with("-internal-externc-isystem=") ||
           flag.starts_with("-c-isystem=") || flag.starts_with("-cxx-isystem=");
  };
  for (std::size_t i = 0; i < cc1_args_storage.size(); ++i) {
    parse_system_include_flag(cc1_args_storage[i], i);
  }

  // OpenMP parsing in REX is pragma-driven (without forwarding -fopenmp to
  // Clang), so explicitly add a compatibility wrapper include dir (contains
  // omp.h wrapper). The wrapper injects the configured omp.h by absolute path.
  if (sageFile.get_openmp()) {
    llvm::SmallString<256> configured_openmp_compat_dir(
        clang_paths.compiler_header_root);
    llvm::sys::path::append(configured_openmp_compat_dir, "openmp-compat");
    const std::string openmp_compat_include_dir = canonicalize_existing_path(
        configured_openmp_compat_dir, "openmp-compat-path",
        "configured REX OpenMP compatibility directory");
    llvm::SmallString<256> compat_header(openmp_compat_include_dir);
    llvm::sys::path::append(compat_header, "omp.h");
    if (!llvm::sys::fs::exists(compat_header)) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[openmp-compat-path]: configured "
                      "REX OpenMP compatibility directory has no omp.h: "
                   << compat_header << "\n";
      ROSE_ABORT();
    }
    if (cc1_system_include_dirs.count(openmp_compat_include_dir) == 0) {
      // This compatibility wrapper is injected by the frontend and is not
      // application source.  Classify it as a system include in Clang itself;
      // merely recording the path in cc1_system_include_dirs leaves its
      // SourceManager characteristic as C_User and causes wrapper/vendor
      // pragmas to be materialized in the user's AST.  Insert it before the
      // active resource/system directories so <omp.h> reaches the wrapper
      // before LLVM's vendor header.
      std::size_t insert_at = cc1_args_storage.size();
      for (std::size_t i = 0; i < cc1_args_storage.size(); ++i) {
        if (is_system_include_flag(cc1_args_storage[i])) {
          insert_at = i;
          break;
        }
      }
      cc1_args_storage.insert(cc1_args_storage.begin() + insert_at,
                              "-internal-isystem");
      cc1_args_storage.insert(cc1_args_storage.begin() + insert_at + 1,
                              openmp_compat_include_dir);
      cc1_system_include_dirs.insert(openmp_compat_include_dir);
    }

    std::string llvm_openmp_include_dir;
#ifdef LLVM_OPENMP_INCLUDE_PATH
    llvm_openmp_include_dir = canonical_path(LLVM_OPENMP_INCLUDE_PATH);
#endif
    if (llvm_openmp_include_dir.empty()) {
      llvm::errs()
          << "REX OpenMP frontend requires LLVM_OPENMP_INCLUDE_PATH to be "
             "configured.\n";
      ROSE_ABORT();
    }

    llvm::SmallString<256> llvm_openmp_header(llvm_openmp_include_dir);
    llvm::sys::path::append(llvm_openmp_header, "omp.h");
    if (!llvm::sys::fs::exists(llvm_openmp_header)) {
      llvm::errs() << "REX OpenMP frontend cannot find LLVM omp.h at: "
                   << llvm_openmp_header << "\n";
      ROSE_ABORT();
    }

    if (cc1_system_include_dirs.count(llvm_openmp_include_dir) == 0) {
      cc1_args_storage.push_back("-internal-isystem");
      cc1_args_storage.push_back(llvm_openmp_include_dir);
      cc1_system_include_dirs.insert(llvm_openmp_include_dir);
    }
  }

  std::vector<const char *> cc1_args_cstr;
  cc1_args_cstr.reserve(cc1_args_storage.size());
  for (const auto &arg : cc1_args_storage) {
    cc1_args_cstr.push_back(arg.c_str());
  }
  llvm::ArrayRef<const char *> argsArrayRef(cc1_args_cstr.data(),
                                            cc1_args_cstr.size());
  bool invocation_ok = clang::CompilerInvocation::CreateFromArgs(
      invocation, argsArrayRef, compiler_instance->getDiagnostics());
  if (!invocation_ok) {
    llvm::errs() << "Failed to create Clang invocation from cc1 arguments.\n";
    ROSE_ABORT();
  }
  clang::TargetOptions &target_opts = invocation.getTargetOpts();

  if (target_opts.Triple.empty()) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-target-state]: exact Clang driver cc1 "
           "invocation has no target triple\n";
    ROSE_ABORT();
  }
  const clang::PreprocessorOptions &parsed_preprocessor_opts =
      invocation.getPreprocessorOpts();
  const bool forces_intel_intrinsics = std::any_of(
      parsed_preprocessor_opts.Includes.begin(),
      parsed_preprocessor_opts.Includes.end(),
      [](const std::string &forced_include) {
        return llvm::sys::path::filename(forced_include) == "immintrin.h";
      });
  if (forces_intel_intrinsics) {
    const llvm::Triple target_triple(target_opts.Triple);
    if (target_triple.getArch() != llvm::Triple::x86 &&
        target_triple.getArch() != llvm::Triple::x86_64) {
      llvm::errs()
          << "REX_FRONTEND_INVARIANT[intel-simd-target]: forced "
             "immintrin.h requires an exact x86 or x86_64 frontend target, "
             "but Clang selected '"
          << target_opts.Triple << "'\n";
      ROSE_ABORT();
    }
  }
  // CreateFromArgs must retain the exact resource and system-header state
  // selected by the validated driver job. Validate that state below; do not
  // rebuild it from host defaults.
  clang::HeaderSearchOptions &headerSearchOpts =
      invocation.getHeaderSearchOpts();

  if (headerSearchOpts.ResourceDir.empty()) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-resource-dir]: exact Clang driver cc1 "
           "invocation has no resource directory\n";
    ROSE_ABORT();
  }
  const std::string invocation_resource_dir = canonicalize_existing_path(
      headerSearchOpts.ResourceDir, "clang-resource-dir",
      "cc1 resource directory");
  if (invocation_resource_dir != resource_dir_candidate) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-resource-dir]: driver selected '"
        << resource_dir_candidate << "', but cc1 selected '"
        << invocation_resource_dir << "'\n";
    ROSE_ABORT();
  }
  if (!resource_dir_has_header(invocation_resource_dir, "stddef.h")) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-resource-dir]: exact cc1 resource "
           "directory '"
        << invocation_resource_dir << "' has no include/stddef.h\n";
    ROSE_ABORT();
  }

  if (std::getenv("ROSE_CLANG_DUMP_INCLUDES") != nullptr) {
    llvm::errs() << "ROSE clang header search paths:\n";
    llvm::errs() << "  [driver-executable] " << driver_executable << "\n";
    if (const char *llvm_dir_env = std::getenv("LLVM_DIR")) {
      llvm::errs() << "  [env LLVM_DIR] " << llvm_dir_env << "\n";
    }
    llvm::errs() << "  [resource-dir] " << headerSearchOpts.ResourceDir << "\n";
    llvm::errs() << "  [builtin-includes] "
                 << (headerSearchOpts.UseBuiltinIncludes ? "on" : "off")
                 << "\n";
    llvm::errs() << "  [standard-system-includes] "
                 << (headerSearchOpts.UseStandardSystemIncludes ? "on" : "off")
                 << "\n";
    llvm::errs() << "  [standard-cxx-includes] "
                 << (headerSearchOpts.UseStandardCXXIncludes ? "on" : "off")
                 << "\n";
    for (const auto &entry : headerSearchOpts.UserEntries) {
      llvm::StringRef group_name = "user";
      if (entry.Group == clang::frontend::System) {
        group_name = "system";
      } else if (entry.Group == clang::frontend::CXXSystem) {
        group_name = "cxx-system";
      } else if (entry.Group == clang::frontend::CSystem) {
        group_name = "c-system";
      }
      llvm::errs() << "  [" << group_name << "] " << entry.Path << "\n";
    }
  }

  clang::LangOptions &lang_opts = compiler_instance->getLangOpts();
  clang::Language clang_lang = clang::Language::C;
  bool enable_cuda = false;
  bool enable_opencl = false;

  switch (language) {
  case ClangToSageTranslator::C:
    clang_lang = clang::Language::C;
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    clang_lang = clang::Language::CXX;
    break;
  case ClangToSageTranslator::CUDA:
    clang_lang = clang::Language::CUDA;
    enable_cuda = true;
    break;
  case ClangToSageTranslator::OPENCL:
    clang_lang = clang::Language::OpenCL;
    enable_opencl = true;
    break;
  case ClangToSageTranslator::OBJC:
    ROSE_ASSERT(!"Objective-C is not supported by ROSE Compiler.");
  default:
    ROSE_ABORT();
  }

  clang::FrontendOptions &fe_opts = invocation.getFrontendOpts();
  if (fe_opts.Inputs.size() != 1 || !fe_opts.Inputs.front().isFile()) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[clang-input-state]: exact Clang driver cc1 "
           "invocation must contain one file input, found "
        << fe_opts.Inputs.size() << "\n";
    ROSE_ABORT();
  }
  const std::string cc1_input_file = canonicalize_existing_path(
      fe_opts.Inputs.front().getFile(), "clang-input-state", "cc1 input file");
  if (cc1_input_file != canonical_input_file.str()) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[clang-input-state]: driver input '"
                 << cc1_input_file << "' does not match frontend source '"
                 << canonical_input_file << "'\n";
    ROSE_ABORT();
  }
  if (fe_opts.Inputs.front().getKind().getLanguage() != clang_lang) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[driver-language-state]: cc1 input language "
           "does not match the exact source/-x language selection\n";
    ROSE_ABORT();
  }

  // CompilerInvocation::CreateFromArgs already derived the complete language
  // mode from the selected driver cc1 job.  In particular, CUDA host and
  // device jobs carry different CUDAIsDevice and auxiliary-target state.
  // Re-running setLangDefaults here destroys that distinction and leaves the
  // preprocessor without the builtins for the selected side of the CUDA
  // compilation.
  if (lang_opts.GNUCVersion == 0) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[driver-language-state]: the exact Clang "
           "driver invocation did not publish a GNU compatibility version\n";
    ROSE_ABORT();
  }

  if (language == ClangToSageTranslator::CPLUSPLUS &&
      (!lang_opts.CPlusPlus || !lang_opts.Bool)) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[driver-language-state]: exact C++ cc1 "
           "invocation did not enable C++ and bool language semantics\n";
    ROSE_ABORT();
  }
  if (enable_cuda && !lang_opts.CUDA) {
    llvm::errs() << "REX_FRONTEND_INVARIANT[driver-language-state]: exact CUDA "
                    "cc1 invocation did not enable CUDA semantics\n";
    ROSE_ABORT();
  }
  if (enable_opencl && !lang_opts.OpenCL) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[driver-language-state]: exact OpenCL cc1 "
           "invocation did not enable OpenCL semantics\n";
    ROSE_ABORT();
  }
  // Now create file manager with FileSystemOptions from the parsed invocation
  compiler_instance->createFileManager();

  clang::PreprocessorOptions &pp_opts =
      compiler_instance->getInvocation().getPreprocessorOpts();
  if (!input_is_preprocessed) {
    assertRequiredPreincludeConfigured(pp_opts.Includes, language,
                                       "compiler invocation setup");
  }

  // CUDA and other offload cc1 jobs carry a paired auxiliary triple.  Let
  // CompilerInstance construct and connect both targets so target macros and
  // builtin registration reflect the exact driver job.  Constructing only the
  // primary TargetInfo loses the host ABI for a CUDA device job and the device
  // builtin set for a CUDA host job.
  if (!compiler_instance->createTarget()) {
    llvm::errs() << "Failed to create Clang primary and auxiliary targets for "
                    "frontend invocation.\n";
    ROSE_ABORT();
  }

  compiler_instance->createSourceManager();

  // getFileRef returns Expected<FileEntryRef> instead of ErrorOr
  llvm::Expected<clang::FileEntryRef> ret =
      compiler_instance->getFileManager().getFileRef(input_file);
  if (!ret) {
    llvm::errs() << "Error opening file: " << input_file << "\n";
    ROSE_ABORT();
  }
  clang::FileEntryRef input_file_entry = *ret;

  clang::FileID mainFileID = compiler_instance->getSourceManager().createFileID(
      input_file_entry, clang::SourceLocation(), clang::SrcMgr::C_User);
  compiler_instance->getSourceManager().setMainFileID(mainFileID);

  if (!compiler_instance->hasPreprocessor())
    compiler_instance->createPreprocessor(clang::TU_Complete);

  // Register pragma and preprocessor callbacks (OpenMP and includes).
  RoseOpenMPPragmaCallback *omp_callback = nullptr;
  SagePreprocessorRecord *preprocessor_recorder = nullptr;
  {
    clang::Preprocessor &PP = compiler_instance->getPreprocessor();
    auto omp_callback_owner = std::make_unique<RoseOpenMPPragmaCallback>(
        compiler_instance->getSourceManager(), PP, mainFileID,
        sageFile.get_openmp(), sageFile.get_openacc());
    omp_callback = omp_callback_owner.get();
    PP.addPPCallbacks(std::move(omp_callback_owner));

    auto preprocessor_recorder_owner = std::make_unique<SagePreprocessorRecord>(
        &(compiler_instance->getSourceManager()), &PP, !is_secondary_parse,
        sageFile.get_collectAllCommentsAndDirectives());
    preprocessor_recorder = preprocessor_recorder_owner.get();
    for (const std::string &path : frontend_support_preinclude_paths) {
      preprocessor_recorder->recordFrontendSupportOwnershipPath(
          path, "clang_main:configured-preinclude");
    }
    PP.addPPCallbacks(std::move(preprocessor_recorder_owner));
    if (!is_secondary_parse) {
      PP.addCommentHandler(preprocessor_recorder);
    }
  }
  if (!is_secondary_parse && preprocessor_recorder != nullptr) {
    clang::DiagnosticsEngine &diags = compiler_instance->getDiagnostics();
    std::unique_ptr<clang::DiagnosticConsumer> existing_client =
        diags.takeClient();
    diags.setClient(new RoseDiagnosticConsumer(std::move(existing_client),
                                               preprocessor_recorder),
                    true);
  }
  if (!compiler_instance->hasASTContext())
    compiler_instance->createASTContext();

  compiler_instance->getPreprocessor().getBuiltinInfo().initializeBuiltins(
      compiler_instance->getPreprocessor().getIdentifierTable(), lang_opts);

  auto translator_ptr = std::make_unique<ClangToSageTranslator>(
      compiler_instance.get(), language, &sageFile, preprocessor_recorder);
  ClangToSageTranslator *translator = translator_ptr.get();
  compiler_instance->getPreprocessor().setTokenWatcher(
      [translator](const clang::Token &token) {
        translator->recordExpandedToken(token);
      });

  // Pass pragma callback to translator
  if (omp_callback) {
    translator->setOpenMPPragmaCallback(omp_callback);
  }

  compiler_instance->setASTConsumer(std::move(translator_ptr));

  // FrontendAction eagerly loads every unnamed module file before Sema starts.
  // REX owns the equivalent in-process lifecycle, so it must perform the same
  // step explicitly; named prebuilt modules load lazily, but C++20 header-unit
  // files in FrontendOptions::ModuleFiles do not.
  for (const std::string &module_file : fe_opts.ModuleFiles) {
    clang::serialization::ModuleFile *loaded_module = nullptr;
    if (!compiler_instance->loadModuleFile(module_file, loaded_module)) {
      llvm::errs() << "REX_FRONTEND_INVARIANT[clang-module-file]: cannot load "
                      "exact frontend module file '"
                   << module_file << "'\n";
      ROSE_ABORT();
    }
  }

  if (!compiler_instance->hasSema())
    compiler_instance->createSema(clang::TU_Complete, NULL);

  if (omp_callback != nullptr && sageFile.get_openacc()) {
    omp_callback->setSema(&compiler_instance->getSema());
  }

  ROSE_ASSERT(compiler_instance->hasDiagnostics());
  ROSE_ASSERT(compiler_instance->hasTarget());
  ROSE_ASSERT(compiler_instance->hasFileManager());
  ROSE_ASSERT(compiler_instance->hasSourceManager());
  ROSE_ASSERT(compiler_instance->hasPreprocessor());
  ROSE_ASSERT(compiler_instance->hasASTContext());
  ROSE_ASSERT(compiler_instance->hasSema());

  // 3 - Translate

  //  printf ("Calling clang::ParseAST()\n");

  auto destroy_clang_compiler_instance = [&]() {
#if ROSE_USE_VALGRIND
    ValgrindErrorReportingScope clang_cleanup_valgrind_scope;
#endif
    compiler_instance.reset();
  };

  struct SourcePositionModeGuard {
    SageBuilder::SourcePositionClassification saved;
    explicit SourcePositionModeGuard(
        SageBuilder::SourcePositionClassification mode)
        : saved(SageBuilder::getSourcePositionClassificationMode()) {
      SageBuilder::setSourcePositionClassificationMode(mode);
    }
    ~SourcePositionModeGuard() {
      SageBuilder::setSourcePositionClassificationMode(saved);
    }
  };

  // Ensure frontend-created nodes do not start as transformations.
  SourcePositionModeGuard source_position_guard(
      SageBuilder::e_sourcePositionFrontendConstruction);

  {
    roseClangPhaseTrace("clang_main.parse.begin");
    compiler_instance->getDiagnosticClient().BeginSourceFile(
        compiler_instance->getLangOpts(),
        &(compiler_instance->getPreprocessor()));
#if ROSE_USE_VALGRIND
    // Clang 22's parser/Sema path reads padding and partially initialized
    // private AST fields while building its own AST. Keep those upstream
    // reports out of REX MemCheck, then re-enable before REX traverses the
    // Clang AST.
    ValgrindErrorReportingScope clang_parse_valgrind_scope;
#endif
    if (language == ClangToSageTranslator::CPLUSPLUS) {
      clang::IdentifierInfo &builtin_id =
          compiler_instance->getPreprocessor().getIdentifierTable().get(
              "__builtin_clzll");
      ROSE_ASSERT(builtin_id.getBuiltinID() !=
                      static_cast<unsigned>(clang::Builtin::NotBuiltin) &&
                  "Expected Clang builtins to be initialised for C++ mode");
    }
    compiler_instance->getPreprocessor().EnterMainSourceFile();
    clang::Sema &sema = compiler_instance->getSema();
    clang::Parser parser(compiler_instance->getPreprocessor(), sema,
                         /*SkipFunctionBodies=*/false);
    translator->setActiveParser(&parser);
    parser.Initialize();
    sema.ActOnStartOfTranslationUnit();

    clang::Parser::DeclGroupPtrTy top_level_decl;
    clang::Sema::ModuleImportState import_state =
        clang::Sema::ModuleImportState::NotACXX20Module;
    for (bool reached_eof =
             parser.ParseFirstTopLevelDecl(top_level_decl, import_state);
         !reached_eof;
         reached_eof = parser.ParseTopLevelDecl(top_level_decl, import_state)) {
    }
    roseClangPhaseTrace("clang_main.parse.end");

    sema.ActOnEndOfTranslationUnit();
#if ROSE_USE_VALGRIND
    clang_parse_valgrind_scope.enable();
#endif
    const unsigned parseDiagnosticErrors =
        compiler_instance->getDiagnostics().getNumErrors();
    if (translator->getGlobalScope() == nullptr && parseDiagnosticErrors == 0) {
      roseClangPhaseTrace("clang_main.translation_unit.begin");
      translator->HandleTranslationUnit(compiler_instance->getASTContext());
      roseClangPhaseTrace("clang_main.translation_unit.end");
    } else if (parseDiagnosticErrors > 0) {
      roseClangPhaseTrace(
          "clang_main.translation_unit.skipped_after_diagnostics");
    }
    translator->setActiveParser(nullptr);
    compiler_instance->getDiagnosticClient().EndSourceFile();
  }

  // get error count from diagnostics directly
  unsigned numErrors = compiler_instance->getDiagnostics().getNumErrors();
  if (numErrors > 0) {
    printf("Clang found %d diagnostic errors during parsing\n", numErrors);
  }
  if (numErrors > 0) {
    sageFile.set_skipfinalCompileStep(true);
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-diagnostics]: Clang reported %u "
            "diagnostic error(s); frontend AST translation is forbidden\n",
            numErrors);
    ROSE_ABORT();
  }

  SgGlobal *global_scope = translator->getGlobalScope();
  if (global_scope == NULL) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[global-scope]: diagnostic-free Clang "
           "translation produced no Sage global scope\n";
    ROSE_ABORT();
  }

  // 4 - Attach to the file

  if (sageFile.get_globalScope() != NULL) {
    SgGlobal *old_global_scope = sageFile.get_globalScope();
    sageFile.set_globalScope(nullptr);
    old_global_scope->set_parent(nullptr);
    auto map_it = Rose::tokenSubsequenceMapOfMapsBySourceFile.find(&sageFile);
    if (map_it != Rose::tokenSubsequenceMapOfMapsBySourceFile.end() &&
        map_it->second != NULL) {
      // Clear stale token mappings from the previous AST to avoid
      // dangling SgNode* references on re-parse.
      map_it->second->clear();
    }
    SageInterface::deleteAST(
        old_global_scope,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
  }

  sageFile.set_globalScope(global_scope);

  // Parent relationship already set up during global scope creation

  // The global scope is the typed lexical owner for preprocessing-only main
  // files.  Give it the exact physical translation-unit interval while the
  // Clang SourceManager is still alive; a line-zero placeholder would force
  // late attachment code to guess at a first/last declaration instead.
  clang::SourceManager &source_manager = compiler_instance->getSourceManager();
  const clang::FileID main_file_id = source_manager.getMainFileID();
  const auto main_buffer = source_manager.getBufferDataOrNone(main_file_id);
  if (main_file_id.isInvalid() || !main_buffer) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[translation-unit-source-interval]: main "
           "file has no exact SourceManager buffer\n";
    ROSE_ABORT();
  }
  clang::SourceLocation translation_unit_start =
      source_manager.getLocForStartOfFile(main_file_id);
  if (!translation_unit_start.isValid()) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[translation-unit-source-interval]: main "
           "file has no exact start location\n";
    ROSE_ABORT();
  }
  const std::size_t main_size = main_buffer->size();
  if (main_size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    llvm::errs()
        << "REX_FRONTEND_INVARIANT[translation-unit-source-interval]: main "
           "file is too large for an exact Clang source offset\n";
    ROSE_ABORT();
  }
  clang::SourceLocation translation_unit_end =
      main_size == 0 ? translation_unit_start
                     : translation_unit_start.getLocWithOffset(
                           static_cast<int>(main_size - 1));
  translator->applySourceRange(
      global_scope,
      clang::SourceRange(translation_unit_start, translation_unit_end));

  // 6 - Finish the complete, diagnostic-free AST.
  if (!is_secondary_parse) {
    ROSE_ASSERT(preprocessor_recorder != nullptr);
    preprocessor_recorder->publishIncludeOwnership(&sageFile);
  }
  roseClangPhaseTrace("clang_main.finishSageAST.begin");
  finishSageAST(*translator);
  roseClangPhaseTrace("clang_main.finishSageAST.finish.end");

  // 7 - Cleanup LLVM objects
  //
  // Now that we use createPhysicalFileSystem() instead of
  // getRealFileSystem(), the CompilerInstance owns its own VFS instance
  // rather than sharing the global singleton. This means we can safely delete
  // the CompilerInstance without causing double-free errors.
  //
  // The translator is owned by CompilerInstance via unique_ptr<ASTConsumer>,
  // so it will also be destroyed when CompilerInstance is deleted. The ROSE
  // AST (global_scope, etc.) persists in sageFile and is NOT owned by the
  // translator, so it remains valid after cleanup.

  roseClangPhaseTrace("clang_main.end");
  destroy_clang_compiler_instance();
  endClangFrontendValgrindPublicationSession();
  return 0; // Success - AST was built
}

void finishSageAST(ClangToSageTranslator &translator) {
  SgGlobal *global_scope = translator.getGlobalScope();
  std::unordered_map<SgNode *, bool> semantic_nonlexical_subtrees;

  // Insert captured pragmas that were not attached during statement
  // translation (e.g., standalone directives or file-scope pragmas).
  roseClangPhaseTrace("clang_main.finishSageAST.appendUnattachedPragmas.begin");
  translator.appendUnattachedPragmas();

  // 1 - Label Statements: Move sub-statement after the label statement.

  // Pei-Hung (05/13/2022) This step is no longer needed as subStatement is
  // properly handled.
  /*
      std::vector<SgLabelStatement *> label_stmts =
     SageInterface::querySubTree<SgLabelStatement>(global_scope,
     V_SgLabelStatement); std::vector<SgLabelStatement *>::iterator
     label_stmt_it; for (label_stmt_it = label_stmts.begin(); label_stmt_it !=
     label_stmts.end(); label_stmt_it++) { SgLabelStatement* labelStmt =
     isSgLabelStatement(*label_stmt_it); SgStatement * sub_stmt =
     labelStmt->get_statement(); if (!isSgNullStatement(sub_stmt)) {
              SgNullStatement * null_stmt =
     SageBuilder::buildNullStatement_nfi();
              translator.setSynthesizedFileInfo(null_stmt);
              labelStmt->set_statement(null_stmt);
              null_stmt->set_parent(labelStmt);
              SageInterface::insertStatementAfter(labelStmt, sub_stmt);
          }
      }
  */
  // 2 - Place Preprocessor informations

  roseClangPhaseTrace("clang_main.finishSageAST.sortPreprocessorList.begin");
  translator.sortPreprocessorList();
  if (translator.preprocessor_list_size() > 0) {
    if (global_scope == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: recorded "
                   "preprocessing records have no translation-unit owner\n";
      ROSE_ABORT();
    }
    roseClangPhaseTrace(
        "clang_main.finishSageAST.preprocessorGlobalAttach.begin");
    auto is_include_directive = [](const PreprocessingInfo *info) -> bool {
      if (info == nullptr) {
        return false;
      }
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
             type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    };
    auto is_same_file = [](Sg_File_Info *a, Sg_File_Info *b) -> bool {
      if (a == nullptr || b == nullptr || a->get_physical_file_id() < 0 ||
          b->get_physical_file_id() < 0) {
        return false;
      }
      return a->isSameFile(*b) && a->get_physical_file_occurrence_id() ==
                                      b->get_physical_file_occurrence_id();
    };
    auto location_leq = [](Sg_File_Info *lhs, Sg_File_Info *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };
    auto node_start = [](SgLocatedNode *node) -> Sg_File_Info * {
      return node != nullptr ? node->get_startOfConstruct() : nullptr;
    };
    auto node_end = [](SgLocatedNode *node) -> Sg_File_Info * {
      return node != nullptr ? node->get_endOfConstruct() : nullptr;
    };
    auto cursor_inside_node = [&](SgLocatedNode *node,
                                  Sg_File_Info *cursor) -> bool {
      if (node == nullptr || cursor == nullptr) {
        return false;
      }
      Sg_File_Info *start = node_start(node);
      Sg_File_Info *end = node_end(node);
      if (start == nullptr || end == nullptr) {
        return false;
      }
      if (!is_same_file(start, cursor)) {
        return false;
      }
      if (!is_same_file(end, cursor)) {
        return false;
      }
      if (!location_leq(start, end)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: node="
                  << node->class_name()
                  << " has a retrograde physical source interval\n";
        ROSE_ABORT();
      }
      return location_leq(start, cursor) && location_leq(cursor, end);
    };
    auto source_coordinate = [](Sg_File_Info *info) -> uint64_t {
      if (info == nullptr || info->get_line() <= 0 || info->get_col() <= 0) {
        return 0;
      }
      const uint64_t line = static_cast<uint64_t>(info->get_line());
      const uint64_t column = static_cast<uint64_t>(info->get_col());
      return (line << 32) | column;
    };
    using PhysicalSourceOccurrenceKey = std::pair<int, unsigned int>;
    auto source_file_key =
        [](Sg_File_Info *info) -> PhysicalSourceOccurrenceKey {
      if (info == nullptr || info->get_physical_file_id() < 0) {
        return {-1, 0};
      }
      return {info->get_physical_file_id(),
              info->get_physical_file_occurrence_id()};
    };
    std::map<PhysicalSourceOccurrenceKey, std::vector<uint64_t>>
        preprocessing_positions;
    std::map<PhysicalSourceOccurrenceKey, std::vector<uint64_t>>
        directive_positions;
    for (const auto &[location, record] :
         translator.preprocessor_remaining_records()) {
      const PhysicalSourceOccurrenceKey key = source_file_key(location);
      const uint64_t position = source_coordinate(location);
      if (key.first < 0 || position == 0 || record == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                     "residual preprocessing record has no exact physical "
                     "position\n";
        ROSE_ABORT();
      }
      preprocessing_positions[key].push_back(position);
      if (!isCommentPreprocessingInfo(record)) {
        directive_positions[key].push_back(position);
      }
    }
    auto sort_position_index = [](auto &index) {
      for (auto &[key, positions] : index) {
        (void)key;
        std::sort(positions.begin(), positions.end());
      }
    };
    sort_position_index(preprocessing_positions);
    sort_position_index(directive_positions);
    auto interval_contains_position = [&](const auto &position_index,
                                          Sg_File_Info *start,
                                          Sg_File_Info *end) -> bool {
      if (start == nullptr || end == nullptr || !is_same_file(start, end)) {
        return false;
      }
      const auto found = position_index.find(source_file_key(start));
      if (found == position_index.end()) {
        return false;
      }
      const uint64_t interval_start = source_coordinate(start);
      const uint64_t interval_end = source_coordinate(end);
      const auto position = std::lower_bound(
          found->second.begin(), found->second.end(), interval_start);
      return position != found->second.end() && *position <= interval_end;
    };
    struct AnchorCandidate {
      SgLocatedNode *node = nullptr;
      uint64_t start = 0;
      uint64_t end = 0;
      size_t depth = 0;
      size_t order = 0;
    };
    struct FileAnchorIndex {
      std::vector<AnchorCandidate> candidates;
      std::vector<uint64_t> prefix_max_end;
    };
    std::map<PhysicalSourceOccurrenceKey, FileAnchorIndex> anchor_index;
    std::unordered_set<SgNode *> indexed_nodes;
    std::unordered_set<SgNode *> active_nodes;
    size_t next_anchor_order = 0;
    std::function<void(SgNode *, size_t)> index_anchor_node;
    index_anchor_node = [&](SgNode *node, size_t depth) {
      if (node == nullptr) {
        return;
      }
      if (isSemanticNonLexicalDeclarationSubtree(
              node, semantic_nonlexical_subtrees)) {
        return;
      }
      if (!active_nodes.insert(node).second) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                     "structural cycle at "
                  << node->class_name() << "\n";
        ROSE_ABORT();
      }
      if (!indexed_nodes.insert(node).second) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                     "node="
                  << node->class_name()
                  << " is reachable through more than one exact owned "
                     "traversal path\n";
        ROSE_ABORT();
      }

      // Walk every exact owned traversal edge so expression-owned lexical
      // scopes (for example a lambda inside an initializer) cannot disappear
      // between their enclosing declaration and their statement surfaces.
      // Traversing through expressions is required to reach their typed
      // lexical scopes.  Ordinary comments retain statement ownership, while
      // directives may use an exact expression boundary because moving a
      // definition past its use changes the program.  Expression lists are
      // also typed delimiter surfaces: their direct-element router below can
      // preserve comments and directives at exact argument/initializer
      // boundaries.  Restrict both indexes to residual records so this remains
      // a focused lexical index rather than a complete expression index.
      // Other non-statement nodes provide topology only.  The statement
      // grammar also contains semantic containers which are not unparsed as
      // independent syntax and whose attachment contract therefore rejects
      // preprocessing records.
      const bool is_semantic_statement_container =
          isSgTypedefSeq(node) != nullptr ||
          isSgCatchStatementSeq(node) != nullptr;
      SgLocatedNode *located = isSgLocatedNode(node);
      const bool is_statement_anchor = isSgStatement(node) != nullptr;
      auto expression_list_has_owner_file_boundary =
          [&](SgExprListExp *expression_list) {
            ROSE_ASSERT(expression_list != nullptr);
            if (expression_list->get_expressions().empty()) {
              return true;
            }
            Sg_File_Info *list_start = node_start(expression_list);
            if (list_start == nullptr ||
                list_start->get_physical_file_id() < 0) {
              return false;
            }
            for (SgExpression *expression :
                 expression_list->get_expressions()) {
              if (expression == nullptr ||
                  expression->get_parent() != expression_list) {
                std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-expression-"
                             "list-owner]: expression list contains malformed "
                             "typed element ownership\n";
                ROSE_ABORT();
              }
              if (expression->isCompilerGenerated()) {
                continue;
              }
              Sg_File_Info *expression_start = node_start(expression);
              Sg_File_Info *expression_end = node_end(expression);
              if (expression_start != nullptr && expression_end != nullptr &&
                  expression_start->get_line() > 0 &&
                  expression_start->get_col() > 0 &&
                  expression_end->get_line() > 0 &&
                  expression_end->get_col() > 0 &&
                  is_same_file(expression_start, expression_end) &&
                  is_same_file(list_start, expression_start)) {
                return true;
              }
            }
            return false;
          };
      SgExprListExp *expression_list = isSgExprListExp(node);
      const bool is_expression_list_anchor =
          expression_list != nullptr && located != nullptr &&
          expression_list_has_owner_file_boundary(expression_list) &&
          interval_contains_position(preprocessing_positions,
                                     node_start(located), node_end(located));
      const bool is_directive_expression_anchor =
          isSgExpression(node) != nullptr && located != nullptr &&
          interval_contains_position(directive_positions, node_start(located),
                                     node_end(located));
      SgLocatedNode *anchor_candidate =
          !is_semantic_statement_container &&
                  (is_statement_anchor || is_expression_list_anchor ||
                   is_directive_expression_anchor)
              ? located
              : nullptr;
      if (anchor_candidate != nullptr) {
        Sg_File_Info *start_info = node_start(anchor_candidate);
        Sg_File_Info *end_info = node_end(anchor_candidate);
        SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(anchor_candidate);
        if (namespace_declaration != nullptr) {
          if (!namespace_declaration->has_source_fragments()) {
            std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                         "preprocessing anchor index reached namespace without "
                         "typed source fragments\n";
            ROSE_ABORT();
          }
          namespace_declaration->validate_source_fragments();
          SgNamespaceSourceFragment *fragments[] = {
              namespace_declaration->get_opening_introducer_source_fragment(),
              namespace_declaration->get_opening_source_fragment(),
              namespace_declaration->get_closing_source_fragment()};
          for (SgNamespaceSourceFragment *fragment : fragments) {
            if (fragment == nullptr) {
              continue;
            }
            Sg_File_Info *fragment_start = fragment->get_startOfConstruct();
            Sg_File_Info *fragment_end = fragment->get_endOfConstruct();
            ROSE_ASSERT(fragment_start != nullptr && fragment_end != nullptr);
            const PhysicalSourceOccurrenceKey key =
                source_file_key(fragment_start);
            if (key.first < 0 || fragment_start->get_line() <= 0 ||
                fragment_start->get_col() <= 0 ||
                fragment_end->get_line() <= 0 || fragment_end->get_col() <= 0 ||
                !is_same_file(fragment_start, fragment_end)) {
              std::cerr
                  << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                     "preprocessing anchor fragment has no exact physical "
                     "range\n";
              ROSE_ABORT();
            }
            uint64_t start = source_coordinate(fragment_start);
            uint64_t end = source_coordinate(fragment_end);
            if (start > end) {
              std::cerr
                  << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                     "namespace source fragment has a retrograde interval\n";
              ROSE_ABORT();
            }
            anchor_index[key].candidates.push_back(
                {anchor_candidate, start, end, depth, next_anchor_order++});
          }
        }
        const bool has_source_start =
            start_info != nullptr && start_info->get_line() > 0;
        const bool has_source_end =
            end_info != nullptr && end_info->get_line() > 0;
        if (namespace_declaration == nullptr &&
            has_source_start != has_source_end) {
          std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                       "node="
                    << node->class_name()
                    << " has a partial physical source interval\n";
          ROSE_ABORT();
        }
        if (namespace_declaration == nullptr && has_source_start &&
            has_source_end) {
          const PhysicalSourceOccurrenceKey key = source_file_key(start_info);
          if (key.first < 0 || !is_same_file(start_info, end_info) ||
              source_coordinate(start_info) == 0 ||
              source_coordinate(end_info) == 0) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                         "node="
                      << node->class_name()
                      << " has no exact single-file physical interval\n";
            ROSE_ABORT();
          }
          uint64_t start = source_coordinate(start_info);
          uint64_t end = source_coordinate(end_info);
          if (start > end) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                         "node="
                      << node->class_name() << " has a retrograde interval\n";
            ROSE_ABORT();
          }
          anchor_index[key].candidates.push_back(
              {anchor_candidate, start, end, depth, next_anchor_order++});
        }
      }
      for (SgNode *child : node->get_traversalSuccessorContainer()) {
        if (child->get_parent() == node) {
          index_anchor_node(child, depth + 1);
        }
      }
      active_nodes.erase(node);
    };
    if (global_scope != nullptr) {
      for (SgDeclarationStatement *declaration :
           global_scope->get_declarations()) {
        index_anchor_node(isSgLocatedNode(declaration), 0);
      }
    }
    for (auto &[key, index] : anchor_index) {
      (void)key;
      std::stable_sort(
          index.candidates.begin(), index.candidates.end(),
          [](const AnchorCandidate &lhs, const AnchorCandidate &rhs) {
            if (lhs.start != rhs.start) {
              return lhs.start < rhs.start;
            }
            return lhs.order < rhs.order;
          });
      uint64_t max_end = 0;
      index.prefix_max_end.reserve(index.candidates.size());
      for (const AnchorCandidate &candidate : index.candidates) {
        max_end = std::max(max_end, candidate.end);
        index.prefix_max_end.push_back(max_end);
      }
    }
    auto find_anchor_for_cursor = [&](Sg_File_Info *cursor) -> SgLocatedNode * {
      if (cursor == nullptr || global_scope == nullptr) {
        return nullptr;
      }
      const auto found = anchor_index.find(source_file_key(cursor));
      if (found == anchor_index.end()) {
        return nullptr;
      }
      const uint64_t position = source_coordinate(cursor);
      const FileAnchorIndex &index = found->second;
      auto upper = std::upper_bound(
          index.candidates.begin(), index.candidates.end(), position,
          [](uint64_t value, const AnchorCandidate &candidate) {
            return value < candidate.start;
          });
      SgLocatedNode *best = nullptr;
      size_t best_depth = 0;
      size_t best_order = std::numeric_limits<size_t>::max();
      for (size_t i = static_cast<size_t>(
               std::distance(index.candidates.begin(), upper));
           i > 0;) {
        --i;
        if (index.prefix_max_end[i] < position) {
          break;
        }
        const AnchorCandidate &candidate = index.candidates[i];
        if (candidate.end < position) {
          continue;
        }
        if (best == nullptr || candidate.depth > best_depth ||
            (candidate.depth == best_depth && candidate.order < best_order)) {
          best = candidate.node;
          best_depth = candidate.depth;
          best_order = candidate.order;
        }
      }
      return best;
    };
    struct BoundaryCandidate {
      SgLocatedNode *node = nullptr;
      uint64_t start = 0;
      uint64_t end = 0;
      size_t order = 0;
    };
    struct FileBoundaryIndex {
      std::vector<BoundaryCandidate> by_start;
      std::vector<BoundaryCandidate> by_end;
    };
    std::map<PhysicalSourceOccurrenceKey, FileBoundaryIndex> boundary_index;
    // A source-file gap must be owned by one of the same typed lexical
    // surfaces that can own a cursor inside its interval.  Querying every
    // SgStatement here also admits structural declarator children such as
    // SgFunctionParameterList.  Those nodes can own preprocessing inside their
    // syntax, but they are not sibling boundaries for text after the enclosing
    // declaration; attaching an EOF directive to one makes it unreachable to
    // statement unparsing.  Reuse the explicitly constructed lexical anchor
    // index so inside and gap ownership cannot disagree.
    for (const auto &[key, anchors] : anchor_index) {
      for (const AnchorCandidate &anchor : anchors.candidates) {
        if (anchor.node == nullptr || anchor.start == 0 || anchor.end == 0 ||
            anchor.start > anchor.end) {
          std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-boundary]: "
                       "lexical anchor index contains an invalid boundary\n";
          ROSE_ABORT();
        }
        if (isSgStatement(anchor.node) == nullptr) {
          // Expression anchors own records inside exact syntax but are not
          // sibling boundaries for gaps outside their enclosing statement.
          continue;
        }
        SgFunctionParameterList *parameter_list =
            isSgFunctionParameterList(anchor.node);
        if (parameter_list == nullptr) {
          SgInitializedName *parameter = isSgInitializedName(anchor.node);
          parameter_list =
              parameter != nullptr
                  ? isSgFunctionParameterList(parameter->get_parent())
                  : nullptr;
        }
        if (parameter_list != nullptr) {
          // Declarator children are exact inside-syntax anchors, but they are
          // never sibling statement boundaries for text outside the complete
          // function declaration.
          continue;
        }
        BoundaryCandidate indexed = {anchor.node, anchor.start, anchor.end,
                                     anchor.order};
        boundary_index[key].by_start.push_back(indexed);
        boundary_index[key].by_end.push_back(indexed);
      }
    }
    for (auto &[key, index] : boundary_index) {
      (void)key;
      std::stable_sort(
          index.by_start.begin(), index.by_start.end(),
          [](const BoundaryCandidate &lhs, const BoundaryCandidate &rhs) {
            if (lhs.start != rhs.start) {
              return lhs.start < rhs.start;
            }
            return lhs.order < rhs.order;
          });
      std::stable_sort(
          index.by_end.begin(), index.by_end.end(),
          [](const BoundaryCandidate &lhs, const BoundaryCandidate &rhs) {
            if (lhs.end != rhs.end) {
              return lhs.end < rhs.end;
            }
            return lhs.order < rhs.order;
          });
    }
    auto find_file_boundary_anchor =
        [&](Sg_File_Info *cursor,
            PreprocessingInfo::RelativePositionType &relative_position,
            const PreprocessingInfo *info) -> SgLocatedNode * {
      if (cursor == nullptr || cursor->get_line() <= 0 ||
          global_scope == nullptr) {
        return nullptr;
      }

      const auto found = boundary_index.find(source_file_key(cursor));
      if (found == boundary_index.end()) {
        return nullptr;
      }
      const uint64_t position = source_coordinate(cursor);
      const FileBoundaryIndex &index = found->second;

      SgLocatedNode *prior = nullptr;
      auto prior_end = std::upper_bound(
          index.by_end.begin(), index.by_end.end(), position,
          [](uint64_t value, const BoundaryCandidate &candidate) {
            return value < candidate.end;
          });
      if (prior_end != index.by_end.begin()) {
        const uint64_t prior_coordinate = std::prev(prior_end)->end;
        auto prior_begin = std::lower_bound(
            index.by_end.begin(), prior_end, prior_coordinate,
            [](const BoundaryCandidate &candidate, uint64_t value) {
              return candidate.end < value;
            });
        prior = prior_begin->node;
      }
      if (prior != nullptr && isSameLineTrailingComment(prior, cursor, info)) {
        relative_position = isSgNamespaceDeclarationStatement(prior) != nullptr
                                ? PreprocessingInfo::after_syntax
                                : PreprocessingInfo::after;
        return prior;
      }

      SgLocatedNode *following = nullptr;
      auto following_begin = std::lower_bound(
          index.by_start.begin(), index.by_start.end(), position,
          [](const BoundaryCandidate &candidate, uint64_t value) {
            return candidate.start < value;
          });
      if (following_begin != index.by_start.end()) {
        // Equal source boundaries occur for a lexical declaration and its
        // structural definition payload.  The earliest stable candidate is
        // the declaration surface; attaching to the later definition emits a
        // before-directive only after `class X {` and can also strand
        // conditional directives on suppressed structural children.
        following = following_begin->node;
      }

      if (following != nullptr) {
        if (SgNamespaceDeclarationStatement *namespace_declaration =
                isSgNamespaceDeclarationStatement(following)) {
          namespace_declaration->validate_source_fragments();
          SgNamespaceSourceFragment *closing =
              namespace_declaration->get_closing_source_fragment();
          Sg_File_Info *closing_start =
              closing != nullptr ? closing->get_startOfConstruct() : nullptr;
          if (closing_start != nullptr &&
              source_file_key(closing_start) == source_file_key(cursor) &&
              source_coordinate(closing_start) == following_begin->start) {
            SgNamespaceDefinitionStatement *definition =
                namespace_declaration->get_definition();
            if (definition == nullptr) {
              std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                           "closing preprocessing boundary has no namespace "
                           "definition owner\n";
              ROSE_ABORT();
            }
            relative_position = PreprocessingInfo::inside;
            return definition;
          }
        }
        relative_position = PreprocessingInfo::before;
        return following;
      }

      if (prior != nullptr) {
        relative_position = PreprocessingInfo::after;
        return prior;
      }
      return nullptr;
    };
    auto is_comment_directive = [](const PreprocessingInfo *info) -> bool {
      if (info == nullptr) {
        return false;
      }
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      return type == PreprocessingInfo::C_StyleComment ||
             type == PreprocessingInfo::CplusplusStyleComment;
    };
    auto has_direct_lexical_children = [](SgLocatedNode *node) {
      return isSgScopeStatement(node) != nullptr ||
             isSgFunctionDeclaration(node) != nullptr ||
             isSgFunctionDefinition(node) != nullptr ||
             isSgForStatement(node) != nullptr ||
             isSgRangeBasedForStatement(node) != nullptr ||
             isSgWhileStmt(node) != nullptr ||
             isSgDoWhileStmt(node) != nullptr || isSgIfStmt(node) != nullptr ||
             isSgSwitchStatement(node) != nullptr ||
             isSgCaseOptionStmt(node) != nullptr ||
             isSgDefaultOptionStmt(node) != nullptr ||
             isSgTryStmt(node) != nullptr ||
             isSgCatchOptionStmt(node) != nullptr ||
             isSgLabelStatement(node) != nullptr ||
             isSgEnumDeclaration(node) != nullptr;
    };
    auto find_sibling_anchor_after_cursor =
        [&](SgLocatedNode *anchor, Sg_File_Info *cursor) -> SgLocatedNode * {
      if (anchor == nullptr || cursor == nullptr) {
        return nullptr;
      }

      auto better_anchor = [&](SgLocatedNode *lhs, SgLocatedNode *rhs) -> bool {
        if (lhs == nullptr) {
          return false;
        }
        if (rhs == nullptr) {
          return true;
        }
        return location_leq(node_start(lhs), node_start(rhs));
      };

      auto consider_at = [&](SgLocatedNode *candidate,
                             Sg_File_Info *candidate_start,
                             SgLocatedNode *&best) {
        if (candidate == nullptr ||
            isSemanticNonLexicalDeclarationSubtree(
                candidate, semantic_nonlexical_subtrees)) {
          return;
        }
        if (!hasExactPhysicalSourceInterval(candidate) ||
            candidate_start == nullptr || candidate_start->get_line() <= 0) {
          return;
        }
        if (!is_same_file(candidate_start, cursor)) {
          return;
        }
        if (!location_leq(cursor, candidate_start)) {
          return;
        }
        if (better_anchor(candidate, best)) {
          best = candidate;
        }
      };
      auto consider = [&](SgLocatedNode *candidate, SgLocatedNode *&best) {
        consider_at(candidate, node_start(candidate), best);
      };
      auto consider_group_member = [&](SgDeclarationStatement *declaration,
                                       SgLocatedNode *&best) {
        Sg_File_Info *boundary_start = node_start(declaration);
        if (SgVariableDeclaration *variable =
                isSgVariableDeclaration(declaration)) {
          if (!variable->get_variables().empty()) {
            boundary_start = node_start(variable->get_variables().front());
          }
        }
        consider_at(isSgLocatedNode(declaration), boundary_start, best);
      };

      auto consider_direct_children = [&](SgNode *owner, SgLocatedNode *&best) {
        if (SgFunctionDeclaration *declaration =
                isSgFunctionDeclaration(owner)) {
          consider(isSgLocatedNode(declaration->get_definition()), best);
        } else if (SgFunctionDefinition *definition =
                       isSgFunctionDefinition(owner)) {
          consider(isSgLocatedNode(definition->get_body()), best);
        } else if (SgForStatement *statement = isSgForStatement(owner)) {
          consider(isSgLocatedNode(statement->get_loop_body()), best);
        } else if (SgRangeBasedForStatement *statement =
                       isSgRangeBasedForStatement(owner)) {
          consider(isSgLocatedNode(statement->get_loop_body()), best);
        } else if (SgWhileStmt *statement = isSgWhileStmt(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
        } else if (SgDoWhileStmt *statement = isSgDoWhileStmt(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
        } else if (SgIfStmt *statement = isSgIfStmt(owner)) {
          consider(isSgLocatedNode(statement->get_true_body()), best);
          consider(isSgLocatedNode(statement->get_false_body()), best);
        } else if (SgSwitchStatement *statement = isSgSwitchStatement(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
        } else if (SgCaseOptionStmt *statement = isSgCaseOptionStmt(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
        } else if (SgDefaultOptionStmt *statement =
                       isSgDefaultOptionStmt(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
        } else if (SgTryStmt *statement = isSgTryStmt(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
          for (SgStatement *handler : statement->get_catch_statement_seq()) {
            consider(isSgLocatedNode(handler), best);
          }
        } else if (SgCatchOptionStmt *statement = isSgCatchOptionStmt(owner)) {
          consider(isSgLocatedNode(statement->get_body()), best);
        } else if (SgLabelStatement *statement = isSgLabelStatement(owner)) {
          consider(isSgLocatedNode(statement->get_statement()), best);
        } else if (SgEnumDeclaration *declaration =
                       isSgEnumDeclaration(owner)) {
          for (SgInitializedName *enumerator : declaration->get_enumerators()) {
            if (enumerator->get_enum_constant_source_ownership() ==
                SgInitializedName::e_enum_constant_source_body) {
              consider(isSgLocatedNode(enumerator), best);
            }
          }
        } else if (SgDeclarationGroupStatement *group =
                       isSgDeclarationGroupStatement(owner)) {
          for (SgDeclarationStatement *declaration :
               group->get_declarations()) {
            consider_group_member(declaration, best);
          }
        }
      };

      SgLocatedNode *best = nullptr;
      consider_direct_children(anchor, best);
      if (best != nullptr) {
        return best;
      }

      SgNode *owner = isSgScopeStatement(anchor) != nullptr
                          ? static_cast<SgNode *>(anchor)
                          : anchor->get_parent();
      if (SgBasicBlock *block = isSgBasicBlock(owner)) {
        for (SgStatement *stmt : block->get_statements()) {
          consider(isSgLocatedNode(stmt), best);
        }
      } else if (SgGlobal *scope = isSgGlobal(owner)) {
        for (SgDeclarationStatement *decl : scope->get_declarations()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgNamespaceDefinitionStatement *scope =
                     isSgNamespaceDefinitionStatement(owner)) {
        for (SgDeclarationStatement *decl : scope->get_declarations()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgClassDefinition *scope = isSgClassDefinition(owner)) {
        for (SgDeclarationStatement *decl : scope->get_members()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgTemplateClassDefinition *scope =
                     isSgTemplateClassDefinition(owner)) {
        for (SgDeclarationStatement *decl : scope->get_members()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgDeclarationGroupStatement *group =
                     isSgDeclarationGroupStatement(owner)) {
        for (SgDeclarationStatement *decl : group->get_declarations()) {
          consider_group_member(decl, best);
        }
      }
      return best;
    };

    while (translator.preprocessor_list_size() > 0) {
      std::pair<Sg_File_Info *, PreprocessingInfo *> entry =
          translator.preprocessor_top();
      if (entry.first == nullptr || entry.second == nullptr) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: recorder "
                     "published an incomplete residual record\n";
        ROSE_ABORT();
      }
      {
        Sg_File_Info *cursor = entry.first;
        const bool is_include = is_include_directive(entry.second);
        SgLocatedNode *anchor = nullptr;
        bool has_forced_anchor = false;
        PreprocessingInfo::RelativePositionType forced_relative_position =
            PreprocessingInfo::after;

        auto attach_preprocessor_record = [&](SgLocatedNode *target,
                                              PreprocessingInfo *info) {
          if (target == nullptr || info == nullptr ||
              info->get_file_info() == nullptr) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: "
                         "incomplete preprocessing attachment\n";
            ROSE_ABORT();
          }
          if (isSemanticNonLexicalDeclarationSubtree(
                  target, semantic_nonlexical_subtrees)) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: "
                         "directive cannot attach to semantic auxiliary node="
                      << target->class_name() << "\n";
            ROSE_ABORT();
          }
          Sg_File_Info *target_info = node_start(target);
          bool exact_namespace_closing_boundary = false;
          if (SgNamespaceDefinitionStatement *namespace_definition =
                  isSgNamespaceDefinitionStatement(target)) {
            SgNamespaceDeclarationStatement *namespace_declaration =
                namespace_definition->get_namespaceDeclaration();
            if (namespace_declaration == nullptr ||
                !namespace_declaration->has_source_fragments()) {
              std::cerr
                  << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                     "namespace preprocessing boundary has no typed source "
                     "fragment owner\n";
              ROSE_ABORT();
            }
            namespace_declaration->validate_source_fragments();
            SgNamespaceSourceFragment *closing =
                namespace_declaration->get_closing_source_fragment();
            Sg_File_Info *closing_start =
                closing != nullptr ? closing->get_startOfConstruct() : nullptr;
            if (info->getRelativePosition() == PreprocessingInfo::inside &&
                closing_start != nullptr &&
                closing_start->get_physical_file_id() >= 0 &&
                info->get_file_info()->get_physical_file_id() >= 0 &&
                is_same_file(closing_start, info->get_file_info())) {
              target_info = closing_start;
              exact_namespace_closing_boundary = true;
            }
          }
          if (target_info != nullptr &&
              !is_same_file(target_info, info->get_file_info())) {
            if (SgNamespaceDeclarationStatement *namespace_declaration =
                    isSgNamespaceDeclarationStatement(target)) {
              if (!namespace_declaration->has_source_fragments()) {
                std::cerr
                    << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                       "preprocessing attachment target has no typed source "
                       "fragments\n";
                ROSE_ABORT();
              }
              namespace_declaration->validate_source_fragments();
              SgNamespaceSourceFragment *fragments[] = {
                  namespace_declaration
                      ->get_opening_introducer_source_fragment(),
                  namespace_declaration->get_opening_source_fragment(),
                  namespace_declaration->get_closing_source_fragment()};
              for (SgNamespaceSourceFragment *fragment : fragments) {
                if (fragment == nullptr) {
                  continue;
                }
                Sg_File_Info *fragment_start = fragment->get_startOfConstruct();
                if (fragment_start != nullptr &&
                    fragment_start->get_physical_file_id() >= 0 &&
                    info->get_file_info()->get_physical_file_id() >= 0 &&
                    is_same_file(fragment_start, info->get_file_info())) {
                  target_info = fragment_start;
                  break;
                }
              }
            }
          }
          const std::string preprocessing_path =
              FileHelper::normalizePathIfPossible(
                  info->get_file_info()->get_filenameString());
          const bool global_owns_frontend_file = [&]() {
            SgGlobal *global = isSgGlobal(target);
            SgSourceFile *source_file =
                global != nullptr ? isSgSourceFile(global->get_parent())
                                  : nullptr;
            if (source_file == nullptr || preprocessing_path.empty()) {
              return false;
            }
            const SgStringList &owned_paths =
                source_file->get_frontendIncludeOwnershipPathList();
            const bool owns_file =
                std::find(owned_paths.begin(), owned_paths.end(),
                          preprocessing_path) != owned_paths.end();
            const auto boundaries =
                boundary_index.find(source_file_key(info->get_file_info()));
            return owns_file && (boundaries == boundary_index.end() ||
                                 boundaries->second.by_start.empty());
          }();
          if (isSgGlobal(target) == nullptr &&
              isSgNamespaceDeclarationStatement(target) == nullptr &&
              !exact_namespace_closing_boundary &&
              !hasExactPhysicalSourceInterval(target)) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: node="
                      << target->class_name()
                      << " has no exact physical source interval\n";
            ROSE_ABORT();
          }
          if ((target_info == nullptr ||
               !is_same_file(target_info, info->get_file_info())) &&
              !global_owns_frontend_file) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[preprocessing-owner]: directive "
                << PreprocessingInfo::directiveTypeName(
                       info->getTypeOfDirective())
                << " from " << info->getFilename() << " cannot attach to "
                << target->class_name() << " in "
                << (target_info != nullptr
                        ? target_info->get_filenameString()
                        : std::string("<missing source identity>"))
                << "\n";
            ROSE_ABORT();
          }
          target = canonicalPreprocessingOwner(target, info);
          if (isSemanticNonLexicalDeclarationSubtree(
                  target, semantic_nonlexical_subtrees) ||
              (isSgGlobal(target) == nullptr &&
               !exact_namespace_closing_boundary &&
               !hasExactPhysicalSourceInterval(target))) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: "
                         "canonical owner="
                      << target->class_name()
                      << " has no exact lexical source interval\n";
            ROSE_ABORT();
          }
          translator.preprocessor_mark_attached(info);
          target->addToAttachedPreprocessingInfo(info);
        };

        if (!has_forced_anchor) {
          anchor = find_anchor_for_cursor(cursor);
        }
        if (!has_forced_anchor && anchor != nullptr && cursor != nullptr &&
            !is_comment_directive(entry.second)) {
          SgFunctionParameterList *parameter_list =
              isSgFunctionParameterList(anchor);
          if (parameter_list == nullptr) {
            SgInitializedName *parameter = isSgInitializedName(anchor);
            parameter_list =
                parameter != nullptr
                    ? isSgFunctionParameterList(parameter->get_parent())
                    : nullptr;
          }
          if (parameter_list == nullptr) {
            SgFunctionDeclaration *function = isSgFunctionDeclaration(anchor);
            SgFunctionParameterList *candidate =
                function != nullptr ? function->get_parameterList_syntax()
                                    : nullptr;
            if (candidate == nullptr && function != nullptr) {
              candidate = function->get_parameterList();
            }
            if (candidate != nullptr && cursor_inside_node(candidate, cursor)) {
              parameter_list = candidate;
            }
          }
          if (parameter_list != nullptr) {
            SgFunctionDeclaration *function =
                isSgFunctionDeclaration(parameter_list->get_parent());
            SgFunctionParameterList *source_parameters =
                function != nullptr ? function->get_parameterList_syntax()
                                    : nullptr;
            if (source_parameters == nullptr && function != nullptr) {
              source_parameters = function->get_parameterList();
            }
            if (function == nullptr || source_parameters != parameter_list ||
                !cursor_inside_node(parameter_list, cursor)) {
              std::cerr
                  << "REX_FRONTEND_INVARIANT[preprocessing-parameter-owner]: "
                     "parameter directive has no exact source parameter-list "
                     "owner list="
                  << parameter_list
                  << " parent=" << parameter_list->get_parent() << "/"
                  << (parameter_list->get_parent() != nullptr
                          ? parameter_list->get_parent()->class_name()
                          : std::string("<null>"))
                  << " function=" << function << " semantic="
                  << (function != nullptr ? function->get_parameterList()
                                          : nullptr)
                  << " syntax="
                  << (function != nullptr ? function->get_parameterList_syntax()
                                          : nullptr)
                  << " selected-source=" << source_parameters << " old-style="
                  << (function != nullptr && function->get_oldStyleDefinition()
                          ? 1
                          : 0)
                  << " cursor-inside="
                  << (cursor_inside_node(parameter_list, cursor) ? 1 : 0)
                  << "\n";
              ROSE_ABORT();
            }

            SgInitializedName *prior_parameter = nullptr;
            SgInitializedName *following_parameter = nullptr;
            for (SgInitializedName *parameter : parameter_list->get_args()) {
              Sg_File_Info *parameter_start = node_start(parameter);
              Sg_File_Info *parameter_end = node_end(parameter);
              if (parameter == nullptr ||
                  parameter->get_parent() != parameter_list ||
                  !hasExactPhysicalSourceInterval(parameter) ||
                  !is_same_file(parameter_start, cursor) ||
                  !is_same_file(parameter_end, cursor)) {
                std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-parameter-"
                             "owner]: source parameter list contains a "
                             "malformed parameter boundary\n";
                ROSE_ABORT();
              }
              if (location_leq(cursor, parameter_start)) {
                following_parameter = parameter;
                break;
              }
              if (location_leq(parameter_end, cursor)) {
                prior_parameter = parameter;
                continue;
              }
              std::cerr
                  << "REX_FRONTEND_INVARIANT[preprocessing-parameter-"
                     "interior]: directive lies inside one parameter's typed "
                     "source interval and has no exact declarator boundary\n";
              ROSE_ABORT();
            }

            if (following_parameter != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
              attach_preprocessor_record(following_parameter, entry.second);
            } else if (prior_parameter != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::after);
              attach_preprocessor_record(prior_parameter, entry.second);
            } else if (parameter_list->get_args().empty()) {
              entry.second->setRelativePosition(PreprocessingInfo::inside);
              attach_preprocessor_record(parameter_list, entry.second);
            } else {
              std::cerr
                  << "REX_FRONTEND_INVARIANT[preprocessing-parameter-owner]: "
                     "directive has no exact parameter boundary\n";
              ROSE_ABORT();
            }
            goto inserted_preproc;
          }
        }

        if (!has_forced_anchor && anchor != nullptr && cursor != nullptr) {
          if (SgExprListExp *expression_list = isSgExprListExp(anchor)) {
            SgAggregateInitializer *aggregate =
                isSgAggregateInitializer(expression_list->get_parent());
            if (aggregate != nullptr &&
                aggregate->get_initializers() != expression_list) {
              std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-expression-"
                           "list-owner]: aggregate initializer does not own "
                           "its exact expression list\n";
              ROSE_ABORT();
            }

            SgExpression *prior_expression = nullptr;
            SgExpression *following_expression = nullptr;
            for (SgExpression *expression :
                 expression_list->get_expressions()) {
              Sg_File_Info *expression_start = node_start(expression);
              Sg_File_Info *expression_end = node_end(expression);
              if (expression == nullptr ||
                  expression->get_parent() != expression_list) {
                std::cerr
                    << "REX_FRONTEND_INVARIANT[preprocessing-expression-"
                       "list-owner]: expression list contains a malformed "
                       "typed element ownership\n";
                ROSE_ABORT();
              }
              if (expression->isCompilerGenerated()) {
                continue;
              }
              if (expression_start == nullptr || expression_end == nullptr ||
                  expression_start->get_line() <= 0 ||
                  expression_start->get_col() <= 0 ||
                  expression_end->get_line() <= 0 ||
                  expression_end->get_col() <= 0 ||
                  expression_start->get_physical_file_id() < 0 ||
                  expression_end->get_physical_file_id() < 0 ||
                  !is_same_file(expression_start, expression_end) ||
                  !location_leq(expression_start, expression_end)) {
                std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-expression-"
                             "list-owner]: source expression-list element="
                          << expression->class_name()
                          << " has no exact ordered physical interval\n";
                ROSE_ABORT();
              }
              // Include-expanded elements retain their own physical file
              // occurrence.  They are not lexical boundaries for a residual
              // owner-file record; their typed include/source-role contract
              // remains responsible for emitting them.
              if (!is_same_file(expression_start, cursor)) {
                continue;
              }
              if (location_leq(cursor, expression_start)) {
                following_expression = expression;
                break;
              }
              if (location_leq(expression_end, cursor)) {
                prior_expression = expression;
                continue;
              }
              if (!is_comment_directive(entry.second)) {
                std::cerr
                    << "REX_FRONTEND_INVARIANT[preprocessing-expression-"
                       "interior]: directive lies inside one expression-list "
                       "element without an exact descendant boundary\n";
                ROSE_ABORT();
              }
              prior_expression = expression;
              break;
            }

            if (following_expression != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
              attach_preprocessor_record(following_expression, entry.second);
            } else if (prior_expression != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::after);
              attach_preprocessor_record(prior_expression, entry.second);
            } else if (expression_list->get_expressions().empty()) {
              entry.second->setRelativePosition(
                  aggregate != nullptr ? PreprocessingInfo::inside
                                       : PreprocessingInfo::before);
              attach_preprocessor_record(
                  aggregate != nullptr
                      ? static_cast<SgLocatedNode *>(aggregate)
                      : static_cast<SgLocatedNode *>(expression_list),
                  entry.second);
            } else {
              std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-expression-"
                           "list-owner]: record has no exact expression-list "
                           "boundary\n";
              ROSE_ABORT();
            }
            goto inserted_preproc;
          }

          SgCtorInitializerList *ctor_initializer_list =
              isSgCtorInitializerList(anchor);
          if (ctor_initializer_list != nullptr) {
            SgMemberFunctionDeclaration *constructor =
                isSgMemberFunctionDeclaration(
                    ctor_initializer_list->get_parent());
            if (constructor == nullptr ||
                constructor->get_CtorInitializerList() !=
                    ctor_initializer_list) {
              std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-ctor-"
                           "initializer-owner]: initializer list has no exact "
                           "constructor owner\n";
              ROSE_ABORT();
            }

            SgInitializedName *prior_initializer = nullptr;
            SgInitializedName *following_initializer = nullptr;
            const SgInitializedNamePtrList &initializers =
                ctor_initializer_list->get_ctors();
            for (SgInitializedName *initializer : initializers) {
              Sg_File_Info *initializer_start = node_start(initializer);
              Sg_File_Info *initializer_end = node_end(initializer);
              if (initializer == nullptr ||
                  initializer->get_parent() != ctor_initializer_list ||
                  !hasExactPhysicalSourceInterval(initializer) ||
                  !is_same_file(initializer_start, cursor) ||
                  !is_same_file(initializer_end, cursor) ||
                  std::count(initializers.begin(), initializers.end(),
                             initializer) != 1) {
                std::cerr
                    << "REX_FRONTEND_INVARIANT[preprocessing-ctor-"
                       "initializer-owner]: constructor contains a malformed "
                       "typed initializer boundary\n";
                ROSE_ABORT();
              }
              if (location_leq(cursor, initializer_start)) {
                following_initializer = initializer;
                break;
              }
              if (location_leq(initializer_end, cursor)) {
                prior_initializer = initializer;
                continue;
              }
              std::cerr
                  << "REX_FRONTEND_INVARIANT[preprocessing-ctor-initializer-"
                     "interior]: directive lies inside one constructor "
                     "initializer's typed source interval and has no exact "
                     "lexical descendant owner\n";
              ROSE_ABORT();
            }

            if (following_initializer != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
              attach_preprocessor_record(following_initializer, entry.second);
            } else if (prior_initializer != nullptr || initializers.empty()) {
              SgFunctionDefinition *definition = constructor->get_definition();
              Sg_File_Info *definition_start = node_start(definition);
              if (definition == nullptr || definition_start == nullptr ||
                  !is_same_file(definition_start, cursor) ||
                  !location_leq(cursor, definition_start)) {
                std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-ctor-"
                             "initializer-owner]: trailing initializer-list "
                             "directive has no exact function-body boundary\n";
                ROSE_ABORT();
              }
              entry.second->setRelativePosition(PreprocessingInfo::before);
              attach_preprocessor_record(definition, entry.second);
            } else {
              std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-ctor-"
                           "initializer-owner]: directive has no exact "
                           "initializer boundary\n";
              ROSE_ABORT();
            }
            goto inserted_preproc;
          }
        }
        if (anchor == nullptr) {
          anchor = find_file_boundary_anchor(cursor, forced_relative_position,
                                             entry.second);
          has_forced_anchor = anchor != nullptr;
        }
        if (anchor == nullptr && cursor != nullptr) {
          const PhysicalSourceOccurrenceKey cursor_key =
              source_file_key(cursor);
          const auto boundaries = boundary_index.find(cursor_key);
          const bool has_lexical_boundary =
              boundaries != boundary_index.end() &&
              !boundaries->second.by_start.empty();
          if (cursor_key.first < 0 || cursor->get_line() <= 0 ||
              cursor->get_col() <= 0) {
            std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: "
                         "directive has no exact physical source coordinate\n";
            ROSE_ABORT();
          }

          Sg_File_Info *global_start = global_scope->get_startOfConstruct();
          Sg_File_Info *global_end = global_scope->get_endOfConstruct();
          const bool exact_main_file_owner =
              !has_lexical_boundary &&
              hasExactPhysicalSourceInterval(global_scope) &&
              is_same_file(global_start, cursor) &&
              is_same_file(global_end, cursor) &&
              location_leq(global_start, cursor) &&
              location_leq(cursor, global_end);

          SgSourceFile *source_file =
              isSgSourceFile(global_scope->get_parent());
          const std::string cursor_path =
              FileHelper::normalizePathIfPossible(cursor->get_filenameString());
          const SgStringList *owned_include_paths =
              source_file != nullptr
                  ? &source_file->get_frontendIncludeOwnershipPathList()
                  : nullptr;
          const bool exact_header_file_owner =
              !has_lexical_boundary && !cursor_path.empty() &&
              owned_include_paths != nullptr &&
              std::find(owned_include_paths->begin(),
                        owned_include_paths->end(),
                        cursor_path) != owned_include_paths->end();

          if (exact_main_file_owner || exact_header_file_owner) {
            // A preprocessing-only physical file has no declaration interval.
            // Its explicitly published source-file ownership edge is the sole
            // typed lexical owner; this is not a first/last/global guess.
            anchor = global_scope;
            has_forced_anchor = true;
            forced_relative_position = PreprocessingInfo::before;
          }
        }
        // A directive in the gap between two direct children of a lexical
        // scope belongs before the following child.  Attaching it merely
        // "inside" the broad scope interval emits it at the end of the scope;
        // for an in-function #define that moves the definition after every
        // expansion and produces uncompilable output.  This boundary rule is
        // directive-kind independent: the source interval, not whether the
        // record is an #include, determines its exact owner.
        if (!has_forced_anchor && anchor != nullptr && cursor != nullptr &&
            cursor_inside_node(anchor, cursor) &&
            has_direct_lexical_children(anchor)) {
          PreprocessingInfo::RelativePositionType gap_position =
              PreprocessingInfo::undef;
          SgLocatedNode *gap_anchor =
              find_file_boundary_anchor(cursor, gap_position, entry.second);
          if (gap_anchor != nullptr &&
              isSameLineTrailingComment(gap_anchor, cursor, entry.second) &&
              (gap_position == PreprocessingInfo::after ||
               gap_position == PreprocessingInfo::after_syntax)) {
            anchor = gap_anchor;
            has_forced_anchor = true;
            forced_relative_position = gap_position;
          } else if (SgLocatedNode *following =
                         find_sibling_anchor_after_cursor(anchor, cursor)) {
            anchor = following;
            has_forced_anchor = true;
            forced_relative_position = PreprocessingInfo::before;
          }
        }
        if (is_include && anchor != nullptr && cursor != nullptr &&
            isSgScopeStatement(anchor) == nullptr &&
            cursor_inside_node(anchor, cursor)) {
          Sg_File_Info *anchor_start = node_start(anchor);
          Sg_File_Info *anchor_end = node_end(anchor);
          const bool strictly_after_start =
              anchor_start != nullptr && location_leq(anchor_start, cursor) &&
              !location_leq(cursor, anchor_start);
          const bool strictly_before_end = anchor_end != nullptr &&
                                           location_leq(cursor, anchor_end) &&
                                           !location_leq(anchor_end, cursor);
          if (strictly_after_start && strictly_before_end) {
            std::cerr
                << "REX_FRONTEND_INVARIANT[include-syntax-fragment]: embedded "
                   "include reached generic preprocessing attachment instead "
                   "of being consumed by its exact typed syntax producer at "
                << cursor->get_filenameString() << ":" << cursor->get_line()
                << ":" << cursor->get_col() << "\n";
            ROSE_ABORT();
          }
        }
        if (anchor != nullptr) {
          if (has_forced_anchor) {
            entry.second->setRelativePosition(forced_relative_position);
            attach_preprocessor_record(anchor, entry.second);
            goto inserted_preproc;
          }
          if (is_comment_directive(entry.second) && entry.first != nullptr) {
            Sg_File_Info *anchor_start = node_start(anchor);
            if (anchor_start != nullptr && anchor_start->get_line() > 0 &&
                is_same_file(anchor_start, entry.first) &&
                location_leq(entry.first, anchor_start) &&
                !location_leq(anchor_start, entry.first)) {
              if (SgLocatedNode *better_anchor =
                      find_sibling_anchor_after_cursor(anchor, entry.first)) {
                anchor = better_anchor;
              }
            }
          }

          Sg_File_Info *start = node_start(anchor);
          Sg_File_Info *end = node_end(anchor);
          if (isSgBasicBlock(anchor) != nullptr ||
              isSgClassDefinition(anchor) != nullptr ||
              isSgTemplateClassDefinition(anchor) != nullptr ||
              isSgNamespaceDefinitionStatement(anchor) != nullptr) {
            entry.second->setRelativePosition(PreprocessingInfo::inside);
          } else if (isSgNamespaceDeclarationStatement(anchor) != nullptr ||
                     isSgClassDeclaration(anchor) != nullptr ||
                     isSgTemplateClassDeclaration(anchor) != nullptr ||
                     isSgEnumDeclaration(anchor) != nullptr) {
            if (start != nullptr && end != nullptr &&
                cursor_inside_node(anchor, entry.first) &&
                !location_leq(entry.first, start) &&
                !location_leq(end, entry.first)) {
              entry.second->setRelativePosition(PreprocessingInfo::inside);
            } else if (start != nullptr && location_leq(entry.first, start)) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
            } else if (isSameLineTrailingComment(anchor, entry.first,
                                                 entry.second)) {
              entry.second->setRelativePosition(
                  isSgNamespaceDeclarationStatement(anchor) != nullptr
                      ? PreprocessingInfo::after_syntax
                      : PreprocessingInfo::after);
            } else {
              entry.second->setRelativePosition(PreprocessingInfo::after);
            }
          } else if (start != nullptr && location_leq(entry.first, start)) {
            entry.second->setRelativePosition(PreprocessingInfo::before);
          } else {
            entry.second->setRelativePosition(PreprocessingInfo::after);
          }
          attach_preprocessor_record(anchor, entry.second);
        } else {
          std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: no "
                       "source-file-local AST owner for "
                    << PreprocessingInfo::directiveTypeName(
                           entry.second->getTypeOfDirective())
                    << " at "
                    << (cursor != nullptr ? cursor->get_filenameString()
                                          : std::string("<unknown>"))
                    << ":" << (cursor != nullptr ? cursor->get_line() : -1)
                    << "\n";
          ROSE_ABORT();
        }
      }
    inserted_preproc:
      if (!translator.preprocessor_pop()) {
        break;
      }
    }
  }

  // Construction-time indexes replace repeated whole-list scans.  Audit each
  // index once at the transaction boundary so a missed mutation is exposed as
  // a hard frontend error rather than surviving into later AST consumers.
  translator.validateDeclarationAttachmentSession();
  std::unordered_set<SgSymbolTable *> audited_symbol_tables;
  Rose_STL_Container<SgNode *> scopes =
      NodeQuery::querySubTree(global_scope, V_SgScopeStatement);
  for (SgNode *node : scopes) {
    SgScopeStatement *scope = isSgScopeStatement(node);
    SgSymbolTable *table =
        scope != nullptr ? scope->get_symbol_table() : nullptr;
    if (table != nullptr && audited_symbol_tables.insert(table).second) {
      table->validate_exact_symbol_indexes();
    }
  }
}

SgGlobal *ClangToSageTranslator::getGlobalScope() const {
  return p_global_scope;
}

ClangToSageTranslator::ClangToSageTranslator(
    clang::CompilerInstance *compiler_instance, Language language_,
    SgSourceFile *sage_source_file,
    SagePreprocessorRecord *preprocessor_recorder)
    : clang::ASTConsumer(), p_decl_translation_map(), p_stmt_translation_map(),
      p_type_translation_map(), p_template_inst_cache(), p_global_scope(NULL),
      p_class_type_decl_first_see_in_type(),
      p_enum_type_decl_first_see_in_type(),
      p_compiler_instance(compiler_instance),
      p_sage_preprocessor_recorder(preprocessor_recorder),
      p_sage_source_file(sage_source_file), language(language_),
      p_openmp_pragma_callback(nullptr) {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
}

ClangToSageTranslator::~ClangToSageTranslator() {}

void ClangToSageTranslator::validateDeclarationAttachmentSession() {
  p_decl_attachment_session.validateAll();
}

void ClangToSageTranslator::recordExpandedToken(const clang::Token &token) {
  if (token.isAnnotation()) {
    return;
  }
  if (p_expanded_tokens.size() >=
      static_cast<size_t>(std::numeric_limits<unsigned int>::max())) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expanded-token-order]: final expanded "
            "token stream exceeds the exact AST order representation\n");
    ROSE_ABORT();
  }
  std::string identifier_spelling;
  if (token.isAnyIdentifier()) {
    bool invalid_spelling = false;
    identifier_spelling = clang::Lexer::getSpelling(
        token, p_compiler_instance->getSourceManager(),
        p_compiler_instance->getLangOpts(), &invalid_spelling);
    if (invalid_spelling || identifier_spelling.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expanded-token-spelling]: identifier "
              "token kind=%u location=%u has no exact spelling\n",
              static_cast<unsigned int>(token.getKind()),
              token.getLocation().getRawEncoding());
      ROSE_ABORT();
    }
  }
  p_expanded_tokens.push_back({token, token.getKind(), token.getLocation(),
                               std::move(identifier_spelling)});
  const unsigned int order =
      static_cast<unsigned int>(p_expanded_tokens.size());

  auto publish =
      [order](std::unordered_map<unsigned, ClangExpandedTokenOrder> &index,
              clang::SourceLocation location) {
        if (!location.isValid()) {
          return;
        }
        const unsigned raw = location.getRawEncoding();
        auto [entry, inserted] =
            index.emplace(raw, ClangExpandedTokenOrder::unique(order));
        if (!inserted) {
          entry->second.publish(order);
        }
      };
  publish(p_expanded_token_order_by_raw_location, token.getLocation());

  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  publish(p_expanded_token_order_by_equivalent_location, token.getLocation());
  publish(p_expanded_token_order_by_equivalent_location,
          source_manager.getSpellingLoc(token.getLocation()));
  publish(p_expanded_token_order_by_equivalent_location,
          source_manager.getExpansionLoc(token.getLocation()));
  publish(p_expanded_token_order_by_equivalent_location,
          source_manager.getFileLoc(token.getLocation()));
  if (token.getLocation().isMacroID()) {
    const clang::CharSourceRange expansion =
        source_manager.getExpansionRange(token.getLocation());
    publish(p_expanded_token_order_by_equivalent_location,
            expansion.getBegin());
    publish(p_expanded_token_order_by_equivalent_location, expansion.getEnd());
  }
}

unsigned int ClangToSageTranslator::requireExpandedTokenSourceOrder(
    clang::SourceLocation location, const char *context,
    ExpandedTokenBoundary boundary) const {
  if (!location.isValid() || p_compiler_instance == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expanded-token-order]: context=%s has "
            "invalid source location or compiler state\n",
            context != nullptr ? context : "<unknown>");
    ROSE_ABORT();
  }

  const unsigned raw = location.getRawEncoding();
  auto selected_order = [&](const ClangExpandedTokenOrder &orders)
      -> std::optional<unsigned int> {
    switch (boundary) {
    case ExpandedTokenBoundary::unique:
      return orders.uniqueOrder();
    case ExpandedTokenBoundary::first:
      return orders.firstOrder();
    case ExpandedTokenBoundary::last:
      return orders.lastOrder();
    }
    ROSE_ABORT();
    __builtin_unreachable();
  };
  auto exact = p_expanded_token_order_by_raw_location.find(raw);
  if (exact != p_expanded_token_order_by_raw_location.end()) {
    const std::optional<unsigned int> order = selected_order(exact->second);
    if (!order.has_value()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expanded-token-order]: context=%s "
              "source-location=%u names multiple final expanded tokens\n",
              context != nullptr ? context : "<unknown>", raw);
      ROSE_ABORT();
    }
    return *order;
  }

  std::optional<unsigned int> resolvedOrder;
  auto resolve = [&](clang::SourceLocation candidate) {
    if (!candidate.isValid()) {
      return;
    }
    auto found = p_expanded_token_order_by_equivalent_location.find(
        candidate.getRawEncoding());
    if (found == p_expanded_token_order_by_equivalent_location.end()) {
      return;
    }
    const std::optional<unsigned int> order = selected_order(found->second);
    if (!order.has_value() ||
        (resolvedOrder.has_value() && *resolvedOrder != *order)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expanded-token-order]: context=%s "
              "source-location=%u has ambiguous final expanded-token "
              "ownership\n",
              context != nullptr ? context : "<unknown>", raw);
      ROSE_ABORT();
    }
    resolvedOrder = *order;
  };

  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  resolve(location);
  resolve(source_manager.getSpellingLoc(location));
  resolve(source_manager.getExpansionLoc(location));
  resolve(source_manager.getFileLoc(location));
  if (location.isMacroID()) {
    const clang::CharSourceRange expansion =
        source_manager.getExpansionRange(location);
    resolve(expansion.getBegin());
    resolve(expansion.getEnd());
  }
  if (!resolvedOrder.has_value()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expanded-token-order]: context=%s "
            "source-location=%u is absent from the final expanded token "
            "stream\n",
            context != nullptr ? context : "<unknown>", raw);
    ROSE_ABORT();
  }
  return *resolvedOrder;
}

/* (protected) Helper methods */

namespace {

void setFileInfosWithParent(SgLocatedNode *located_node, Sg_File_Info *start_fi,
                            Sg_File_Info *end_fi) {
  ROSE_ASSERT(located_node != NULL);
  ROSE_ASSERT(start_fi != NULL);
  ROSE_ASSERT(end_fi != NULL);

  located_node->set_startOfConstruct(start_fi);
  located_node->set_endOfConstruct(end_fi);

  start_fi->set_parent(located_node);
  end_fi->set_parent(located_node);
}

void setFileInfosWithParent(SgInitializedName *init_name,
                            Sg_File_Info *start_fi, Sg_File_Info *end_fi) {
  ROSE_ASSERT(init_name != NULL);
  ROSE_ASSERT(start_fi != NULL);
  ROSE_ASSERT(end_fi != NULL);

  init_name->set_startOfConstruct(start_fi);
  init_name->set_endOfConstruct(end_fi);

  start_fi->set_parent(init_name);
  end_fi->set_parent(init_name);
}

} // namespace

bool ClangToSageTranslator::hasExactSemanticExpressionConstructionRole(
    const char *context) const {
  if (context == nullptr || *context == '\0') {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[semantic-expression-construction]: "
                    "missing exact producer context\n");
    ROSE_ABORT();
  }

  const bool explicit_semantic_expression =
      p_semantic_function_body_traversal_depth != 0 ||
      p_semantic_template_argument_expression_depth != 0 ||
      p_semantic_expression_subtree_depth != 0 ||
      p_user_defined_literal_semantic_expression_depth != 0 ||
      (!p_imported_external_semantic_traversal_stack.empty() &&
       p_imported_external_semantic_traversal_stack.back());
  // A function-parameter construction frame identifies declaration and scope
  // ownership, not the source role of every recursively translated expression.
  // Building a semantic signature can translate an on-demand source
  // declaration before the outer frame unwinds.  Treating the outer frame as
  // an expression role leaks semantic provenance into that nested source
  // surface and poisons the lexical expression cache.  Semantic expression
  // producers must therefore activate one of the explicit subtree
  // transactions above.
  return explicit_semantic_expression;
}

bool ClangToSageTranslator::hasExactSemanticExpressionConstructionEvidence(
    clang::SourceRange construction_evidence) const {
  const bool exact_physical_range =
      construction_evidence.isValid() &&
      construction_evidence.getBegin().isValid() &&
      construction_evidence.getEnd().isValid();
  const clang::Expr *active_expression =
      !p_clang_statement_traversal_stack.empty()
          ? llvm::dyn_cast<clang::Expr>(
                p_clang_statement_traversal_stack.back())
          : nullptr;
  if (active_expression == nullptr) {
    return exact_physical_range;
  }

  const clang::SourceRange active_range = active_expression->getSourceRange();
  const bool exact_active_expression_range =
      active_range.getBegin().getRawEncoding() ==
          construction_evidence.getBegin().getRawEncoding() &&
      active_range.getEnd().getRawEncoding() ==
          construction_evidence.getEnd().getRawEncoding();
  return exact_physical_range || exact_active_expression_range;
}

void ClangToSageTranslator::publishSemanticExpressionSourceProvenance(
    SgExpression *expression, clang::SourceRange construction_evidence,
    const char *context) {
  if (expression == nullptr ||
      !hasExactSemanticExpressionConstructionRole(context) ||
      !hasExactSemanticExpressionConstructionEvidence(construction_evidence)) {
    const clang::Stmt *active_producer =
        !p_clang_statement_traversal_stack.empty()
            ? p_clang_statement_traversal_stack.back()
            : nullptr;
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
            "context=%s expression=%p/%s range-valid=%d begin-valid=%d "
            "end-valid=%d raw-begin=%u raw-end=%u semantic-depths="
            "body:%u/template-argument:%u/subtree:%u/udl:%u active-clang="
            "%p/%s lacks an exact semantic producer or Clang construction "
            "evidence\n",
            context != nullptr ? context : "<null>",
            static_cast<void *>(expression),
            expression != nullptr ? expression->class_name().c_str() : "<null>",
            construction_evidence.isValid(),
            construction_evidence.getBegin().isValid(),
            construction_evidence.getEnd().isValid(),
            construction_evidence.getBegin().getRawEncoding(),
            construction_evidence.getEnd().getRawEncoding(),
            p_semantic_function_body_traversal_depth,
            p_semantic_template_argument_expression_depth,
            p_semantic_expression_subtree_depth,
            p_user_defined_literal_semantic_expression_depth,
            static_cast<const void *>(active_producer),
            active_producer != nullptr ? active_producer->getStmtClassName()
                                       : "<null>");
    ROSE_ABORT();
  }
  if (p_lexical_source_nodes.count(expression) != 0 ||
      p_synthesized_source_nodes.count(expression) != 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
            "context=%s expression=%p/%s was already published with "
            "lexical/synthesized roles=%zu/%zu\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str(),
            p_lexical_source_nodes.count(expression),
            p_synthesized_source_nodes.count(expression));
    ROSE_ABORT();
  }

  // Semantic expressions are born without file information.  Their semantic
  // producer owns the one atomic provenance publication; the general
  // synthesized-node helper would preclassify and then overwrite that state.
  SageBuilder::initializeSemanticExpressionSourceProvenance(expression);
  if (!p_synthesized_source_nodes.insert(expression).second) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
            "context=%s expression=%p/%s was registered more than once\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }
  if (p_synthesized_source_nodes.count(expression) != 1 ||
      p_lexical_source_nodes.count(expression) != 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
            "context=%s expression=%p/%s did not retain one exact "
            "translator semantic publication\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }
}

void ClangToSageTranslator::publishCanonicalSemanticExpressionSourceProvenance(
    SgExpression *expression, const char *context) {
  if (expression == nullptr || context == nullptr || *context == '\0' ||
      expression->get_parent() != nullptr ||
      p_lexical_source_nodes.count(expression) != 0 ||
      p_synthesized_source_nodes.count(expression) != 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[canonical-semantic-expression-"
            "provenance]: context=%s expression=%p/%s parent=%p has "
            "lexical/synthesized roles=%zu/%zu before producer publication\n",
            context != nullptr ? context : "<null>",
            static_cast<void *>(expression),
            expression != nullptr ? expression->class_name().c_str() : "<null>",
            static_cast<void *>(expression != nullptr ? expression->get_parent()
                                                      : nullptr),
            expression != nullptr ? p_lexical_source_nodes.count(expression)
                                  : 0,
            expression != nullptr ? p_synthesized_source_nodes.count(expression)
                                  : 0);
    ROSE_ABORT();
  }

  SageBuilder::initializeSemanticExpressionSourceProvenance(expression);
  if (!p_synthesized_source_nodes.insert(expression).second) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[canonical-semantic-expression-"
            "provenance]: context=%s expression=%p/%s registration was not "
            "unique\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }
}

void ClangToSageTranslator::
    publishCanonicalSemanticImplicitConversionProvenance(SgCastExp *conversion,
                                                         const char *context) {
  if (conversion == nullptr ||
      conversion->get_cast_type() != SgCastExp::e_implicit_cast) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[canonical-semantic-implicit-conversion]: "
            "context=%s conversion=%p has no exact typed implicit role\n",
            context != nullptr ? context : "<null>",
            static_cast<void *>(conversion));
    ROSE_ABORT();
  }
  conversion->validate_semantic_conversion();
  publishCanonicalSemanticExpressionSourceProvenance(conversion, context);
  for (Sg_File_Info *position :
       {conversion->get_file_info(), conversion->get_startOfConstruct(),
        conversion->get_endOfConstruct(), conversion->get_operatorPosition()}) {
    if (position == nullptr || position->isShared() ||
        !position->isCompilerGenerated() ||
        !position->isOutputInCodeGeneration() || position->isTransformation()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[canonical-semantic-implicit-conversion]: "
              "context=%s conversion=%p has malformed semantic provenance\n",
              context, static_cast<void *>(conversion));
      ROSE_ABORT();
    }
    position->setImplicitCast();
  }
}

void ClangToSageTranslator::applySourceRange(SgNode *node,
                                             clang::SourceRange source_range) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-range-target]: cannot apply a "
            "source range to a null Sage node\n");
    ROSE_ABORT();
  }
  const bool semantic_range_for_declaration_shell =
      p_semantic_range_declaration_shell_nodes.count(node) != 0;
  const bool semantic_auxiliary_physical_source =
      p_semantic_auxiliary_physical_source_range_nodes.count(node) != 0;
  const bool semantic_expression_construction =
      isSgExpression(node) != nullptr &&
      hasExactSemanticExpressionConstructionRole(
          "ClangToSageTranslator::applySourceRange:cache-role");
  // An exact auxiliary physical-source transaction is the typed producer for
  // a non-lexical declaration whose semantic identity still comes from one
  // written declaration.  Ambient semantic traversal (notably while building
  // a lambda expression) must not replace that explicit producer with the
  // detached -4/-4 semantic identity.
  if (!semantic_auxiliary_physical_source &&
      (p_semantic_function_body_traversal_depth != 0 ||
       p_semantic_expression_subtree_depth != 0 ||
       semantic_range_for_declaration_shell ||
       semantic_expression_construction)) {
    if (p_lexical_source_nodes.count(node) != 0) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-subtree-provenance]: "
              "node=%p type=%s already owns a lexical source role\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    SgExpression *semantic_expression = isSgExpression(node);
    if (p_synthesized_source_nodes.count(node) == 0) {
      if (semantic_expression != nullptr) {
        publishSemanticExpressionSourceProvenance(
            semantic_expression, source_range,
            "ClangToSageTranslator::applySourceRange:semantic-subtree");
      } else {
        setSynthesizedFileInfo(node);
        mark_compiler_generated_frontend_specific(node);
      }
    } else if (semantic_expression != nullptr) {
      SageBuilder::initializeSemanticExpressionSourceProvenance(
          semantic_expression);
    }
    SgLocatedNode *located = isSgLocatedNode(node);
    SgInitializedName *initialized = isSgInitializedName(node);
    if (located == nullptr && initialized == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-subtree-provenance]: "
              "node=%p type=%s cannot own semantic file provenance\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    std::array<Sg_File_Info *, 4> file_infos = {
        located != nullptr ? located->get_file_info()
                           : initialized->get_file_info(),
        located != nullptr ? located->get_startOfConstruct()
                           : initialized->get_startOfConstruct(),
        located != nullptr ? located->get_endOfConstruct()
                           : initialized->get_endOfConstruct(),
        isSgExpression(located) != nullptr
            ? isSgExpression(located)->get_operatorPosition()
            : nullptr};
    for (std::size_t index = 0; index < file_infos.size(); ++index) {
      Sg_File_Info *file_info = file_infos[index];
      if (file_info == nullptr && index == 3 &&
          isSgExpression(located) == nullptr) {
        continue;
      }
      if (file_info == nullptr || file_info->get_parent() != node ||
          !file_info->isCompilerGenerated() ||
          !file_info->isFrontendSpecific() || file_info->isTransformation() ||
          file_info->isSourcePositionUnavailableInFrontend() ||
          !file_info->isOutputInCodeGeneration() ||
          file_info->get_file_id() !=
              Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
          file_info->get_physical_file_id() !=
              Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-subtree-provenance]: "
                "node=%p type=%s position=%zu has incomplete semantic "
                "provenance\n",
                static_cast<void *>(node), node->class_name().c_str(), index);
        ROSE_ABORT();
      }
    }
    return;
  }
  if ((p_semantic_template_argument_expression_depth != 0 ||
       p_user_defined_literal_semantic_expression_depth != 0) &&
      isSgExpression(node) != nullptr) {
    if (p_synthesized_source_nodes.count(node) == 1) {
      if (!hasExactSemanticExpressionConstructionEvidence(source_range) ||
          p_lexical_source_nodes.count(node) != 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-expression-provenance]: "
                "prepublished semantic expression=%p/%s has no exact Clang "
                "construction evidence or has a lexical role\n",
                static_cast<void *>(node), node->class_name().c_str());
        ROSE_ABORT();
      }
      SageBuilder::initializeSemanticExpressionSourceProvenance(
          isSgExpression(node));
      return;
    }
    publishSemanticExpressionSourceProvenance(
        isSgExpression(node), source_range,
        "ClangToSageTranslator::applySourceRange:semantic-expression");
    return;
  }
  const bool imported_external_semantic =
      !p_imported_external_semantic_traversal_stack.empty() &&
      p_imported_external_semantic_traversal_stack.back();
  if (imported_external_semantic) {
    if (!source_range.isValid() || !source_range.getBegin().isValid() ||
        !source_range.getEnd().isValid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[imported-source-provenance]: node=%p "
              "type=%s has no exact serialized source range evidence\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }

    if (p_synthesized_source_nodes.count(node) == 0) {
      setSynthesizedFileInfo(node);
      mark_compiler_generated_frontend_specific(node);
    }

    SgLocatedNode *imported_located = isSgLocatedNode(node);
    SgInitializedName *imported_name = isSgInitializedName(node);
    if (imported_located == nullptr && imported_name == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[imported-source-provenance]: node=%p "
              "type=%s cannot own semantic file provenance\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    std::array<Sg_File_Info *, 3> imported_file_infos = {
        imported_located != nullptr ? imported_located->get_file_info()
                                    : imported_name->get_file_info(),
        imported_located != nullptr ? imported_located->get_startOfConstruct()
                                    : imported_name->get_startOfConstruct(),
        imported_located != nullptr ? imported_located->get_endOfConstruct()
                                    : imported_name->get_endOfConstruct()};
    for (Sg_File_Info *file_info : imported_file_infos) {
      if (file_info == nullptr || !file_info->isCompilerGenerated() ||
          !file_info->isFrontendSpecific() || file_info->isTransformation() ||
          file_info->get_source_sequence_number() != 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[imported-source-provenance]: node=%p "
                "type=%s did not publish one exact external semantic role\n",
                static_cast<void *>(node), node->class_name().c_str());
        ROSE_ABORT();
      }
    }
    return;
  }
  if (p_synthesized_source_nodes.count(node) != 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-provenance]: synthesized node=%p "
            "type=%s cannot acquire a Clang source range\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
  if (!source_range.isValid() || !source_range.getBegin().isValid() ||
      !source_range.getEnd().isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-range]: node=%p type=%s requires "
            "one valid exact Clang source range\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
  if (semantic_auxiliary_physical_source) {
    if (p_lexical_source_nodes.count(node) != 0 ||
        p_synthesized_source_nodes.count(node) != 0 ||
        !p_semantic_auxiliary_source_nodes.insert(node).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-declaration-source]: "
              "node=%p type=%s did not begin one exact semantic physical "
              "source transaction\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
  } else if (!p_lexical_source_nodes.insert(node).second) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-provenance]: node=%p type=%s "
            "consumed more than one lexical Clang source range\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
  SgLocatedNode *located_node = isSgLocatedNode(node);
  SgInitializedName *init_name = isSgInitializedName(node);

#if DEBUG_SOURCE_LOCATION
  std::cerr << "Set File_Info for " << node << " of type " << node->class_name()
            << std::endl;
#endif

  if (located_node == NULL && init_name == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-range-target]: node=%p type=%s is "
            "neither SgLocatedNode nor SgInitializedName\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  std::set<Sg_File_Info *> previous_file_infos;
  auto remember_file_info = [&](Sg_File_Info *file_info) {
    if (file_info != nullptr) {
      if (file_info->get_parent() != node) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[source-range-owner]: node=%p type=%s "
                "does not exclusively own prior file info=%p parent=%p\n",
                static_cast<void *>(node), node->class_name().c_str(),
                static_cast<void *>(file_info),
                static_cast<void *>(file_info->get_parent()));
        ROSE_ABORT();
      }
      previous_file_infos.insert(file_info);
    }
  };
  if (located_node != nullptr) {
    remember_file_info(located_node->get_startOfConstruct());
    remember_file_info(located_node->get_endOfConstruct());
    if (SgExpression *expression = isSgExpression(located_node)) {
      remember_file_info(expression->get_operatorPosition());
    } else {
      remember_file_info(located_node->get_file_info());
    }
  } else {
    remember_file_info(init_name->get_startOfConstruct());
    remember_file_info(init_name->get_endOfConstruct());
    remember_file_info(init_name->get_file_info());
  }

  Sg_File_Info *start_fi = NULL;
  Sg_File_Info *end_fi = NULL;
  std::optional<unsigned int> translation_unit_order;
  SgDeclarationStatement *source_declaration = isSgDeclarationStatement(node);
  // An embedded class/enum declaration has real lexical provenance, but its
  // typed declarator is the sole physical output owner.  Classify that role
  // while the file information is created; changing it after publication
  // would be the same late suppression that previously hid malformed owners.
  const bool nonautonomous_declarator_tag =
      !semantic_auxiliary_physical_source &&
      ((isSgClassDeclaration(source_declaration) != nullptr &&
        !isSgClassDeclaration(source_declaration)
             ->get_isAutonomousDeclaration()) ||
       (isSgEnumDeclaration(source_declaration) != nullptr &&
        !isSgEnumDeclaration(source_declaration)
             ->get_isAutonomousDeclaration()));
  SgDeclarationScope *source_declarator_owner = isSgDeclarationScope(
      source_declaration != nullptr ? source_declaration->get_parent()
                                    : nullptr);
  const bool attached_source_declarator_tag =
      nonautonomous_declarator_tag && source_declarator_owner != nullptr &&
      std::count(source_declarator_owner->get_declarations().begin(),
                 source_declarator_owner->get_declarations().end(),
                 source_declaration) == 1;
  const bool published_source_declarator_tag =
      attached_source_declarator_tag &&
      source_declarator_owner->get_parent() != nullptr &&
      SageBuilder::getDeclarationScopeOwner(source_declarator_owner) != nullptr;
  const bool embedded_declarator_tag =
      nonautonomous_declarator_tag && !published_source_declarator_tag;
  if (source_declaration != nullptr && !semantic_auxiliary_physical_source) {
    if (SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(source_declaration)) {
      const bool source_spelled_namespace =
          namespace_declaration->has_source_fragments() &&
          namespace_declaration->get_opening_source_fragment()
                  ->get_source_form() ==
              SgNamespaceSourceFragment::
                  e_namespace_source_fragment_source_spelled;
      const std::optional<unsigned int> namespace_order =
          namespace_declaration->get_translation_unit_source_order();
      if (!source_spelled_namespace) {
        if (namespace_order.has_value()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[expanded-token-order]: semantic "
                  "namespace=%p name=%s carries a lexical source order\n",
                  static_cast<void *>(namespace_declaration),
                  namespace_declaration->get_name().getString().c_str());
          ROSE_ABORT();
        }
      } else if (!namespace_order.has_value()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expanded-token-order]: source "
                "namespace=%p name=%s has no typed producer order before "
                "source provenance is applied\n",
                static_cast<void *>(namespace_declaration),
                namespace_declaration->get_name().getString().c_str());
        ROSE_ABORT();
      } else {
        translation_unit_order = namespace_order;
      }
    } else {
      translation_unit_order = requireExpandedTokenSourceOrder(
          source_range.getBegin(), node->class_name().c_str(),
          ExpandedTokenBoundary::first);
    }
  }

  if (source_range.isValid()) {
    clang::SourceLocation begin = source_range.getBegin();
    clang::SourceLocation end = source_range.getEnd();

    if (!begin.isValid() || !end.isValid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[source-range]: node=%p type=%s has a "
              "nominally valid range with an invalid endpoint\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();
      const bool begin_is_macro = begin.isMacroID();
      const bool end_is_macro = end.isMacroID();
      clang::SourceLocation expansion_begin = sm.getExpansionLoc(begin);
      clang::SourceLocation expansion_end =
          end_is_macro ? sm.getExpansionRange(end).getEnd()
                       : sm.getExpansionLoc(end);
      const bool begin_needs_expansion =
          begin_is_macro ||
          (expansion_begin.isValid() &&
           expansion_begin.getRawEncoding() != begin.getRawEncoding());
      const bool end_needs_expansion =
          end_is_macro ||
          (expansion_end.isValid() &&
           expansion_end.getRawEncoding() != end.getRawEncoding());

      if (begin_needs_expansion && expansion_begin.isValid()) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation begin as it is a MacroID: ";
        begin.dump(sm);
        std::cerr << std::endl;
#endif
        begin = expansion_begin;
      }

      if (end_needs_expansion && expansion_end.isValid()) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation end as it is a MacroID: ";
        end.dump(sm);
        std::cerr << std::endl;
#endif
        end = expansion_end;
      }

      ROSE_ASSERT(begin.isValid());
      ROSE_ASSERT(end.isValid());
      if (sm.isWrittenInScratchSpace(begin) ||
          sm.isWrittenInScratchSpace(end)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[source-range]: node=%p type=%s "
                "attempted to publish Clang scratch storage as a lexical "
                "source interval\n",
                static_cast<void *>(node), node->class_name().c_str());
        ROSE_ABORT();
      }

      clang::FileID file_begin = sm.getFileID(begin);
      clang::FileID file_end = sm.getFileID(end);

      if (!file_begin.isInvalid() && !file_end.isInvalid() &&
          file_begin != file_end) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[source-range]: node=%p type=%s has "
                "cross-file endpoints that cannot be represented by one "
                "exact Sage source range\n",
                static_cast<void *>(node), node->class_name().c_str());
        ROSE_ABORT();
      }

      if (!file_begin.isInvalid() && !file_end.isInvalid()) {
        auto end_buffer = sm.getBufferDataOrNone(file_end);
        const bool in_main_file = file_begin == sm.getMainFileID();
        bool inv_begin_line = false;
        bool inv_begin_col = false;
        bool inv_end_line = false;
        bool inv_end_col = false;

        unsigned ls = sm.getSpellingLineNumber(begin, &inv_begin_line);
        unsigned cs = sm.getSpellingColumnNumber(begin, &inv_begin_col);

        // Token-stream mapping expects end-of-construct to be
        // token-accurate. Clang SourceRange ends are often at the
        // *start* of the last token; convert to the last character
        // of that token.
        clang::SourceLocation end_for_fi = end;
        bool can_lex_end = end_for_fi.isFileID() && in_main_file && end_buffer;
        if (can_lex_end) {
          unsigned end_offset = sm.getFileOffset(end_for_fi);
          if (end_offset >= end_buffer->size()) {
            can_lex_end = false;
          }
        }
        if (can_lex_end) {
          bool invalid_char_data = false;
          (void)sm.getCharacterData(end_for_fi, &invalid_char_data);
          if (invalid_char_data) {
            can_lex_end = false;
          }
        }
        if (can_lex_end) {
          clang::SourceLocation after_token =
              clang::Lexer::getLocForEndOfToken(end_for_fi, 0, sm, lang_opts);
          if (after_token.isValid() &&
              after_token.getRawEncoding() != end_for_fi.getRawEncoding()) {
            clang::SourceLocation last_char = after_token.getLocWithOffset(-1);
            if (last_char.isValid()) {
              end_for_fi = last_char;
            }
          }
        }

        unsigned le = sm.getSpellingLineNumber(end_for_fi, &inv_end_line);
        unsigned ce = sm.getSpellingColumnNumber(end_for_fi, &inv_end_col);

        if (inv_begin_line || inv_begin_col || inv_end_line || inv_end_col) {
          ROSE_ASSERT(!"Should not happen as everything have been "
                       "check before...");
        }
        // getFileEntryForID still returns const FileEntry*.
        const clang::FileEntry *fileEntry = sm.getFileEntryForID(file_begin);
        std::string file;
        if (fileEntry) {
          // FileEntry uses tryGetRealPathName() instead of
          // getName().
          file = fileEntry->tryGetRealPathName().str();
        }
        if (file.empty()) {
          file = sm.getFilename(begin).str();
        }
        if ((file.empty() || file == "<built-in>") &&
            sm.isWrittenInMainFile(begin)) {
          const clang::FileEntry *main_entry =
              sm.getFileEntryForID(sm.getMainFileID());
          if (main_entry) {
            file = main_entry->tryGetRealPathName().str();
          }
        }
        if (file.empty()) {
          clang::PresumedLoc ploc = sm.getPresumedLoc(begin);
          if (ploc.isValid()) {
            file = ploc.getFilename();
          }
        }

        if (!file.empty()) {
          // start_fi = new Sg_File_Info(file, ls, cs);
          // end_fi   = new Sg_File_Info(file, le, ce);
          // Preserve source-backed nodes as output-capable regardless of
          // which physical file owns them. File ownership is resolved later by
          // the unparser using physical file ids; keeping header declarations
          // output-capable here ensures AST traversals such as
          // traverseInputFiles() still visit real included declarations that
          // are structurally attached to the current translation unit.
          start_fi = new Sg_File_Info(file, ls, cs);
          end_fi = new Sg_File_Info(file, le, ce);
          const unsigned file_occurrence =
              p_sage_preprocessor_recorder->requirePhysicalFileOccurrence(
                  file_begin, "source-range");
          start_fi->set_physical_file_occurrence_id(file_occurrence);
          end_fi->set_physical_file_occurrence_id(file_occurrence);
          if (!embedded_declarator_tag) {
            start_fi->setOutputInCodeGeneration();
            end_fi->setOutputInCodeGeneration();
          }

#if DEBUG_SOURCE_LOCATION
          std::cerr << "\tCreate FI for node in " << file << ":" << ls << ":"
                    << cs << std::endl;
#endif
        }
#if DEBUG_SOURCE_LOCATION
        else {
          std::cerr << "\tDump SourceLocation for \"Invalid FileID\": "
                    << std::endl
                    << "\t";
          begin.dump(p_compiler_instance->getSourceManager());
          std::cerr << std::endl << "\t";
          end.dump(p_compiler_instance->getSourceManager());
          std::cerr << std::endl;
        }
#endif
      }
    }
  }

  if (start_fi == NULL && end_fi == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-range]: node=%p type=%s has a valid "
            "Clang source range but no exact Sage file identity\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  if (start_fi == NULL || end_fi == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[source-range]: node=%p type=%s produced "
            "an incomplete Sage source interval\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  if (located_node != NULL) {
    setFileInfosWithParent(located_node, start_fi, end_fi);

    // Pei-Hung (09/29/2022) SgExpression::get_file_info() checks and
    // returns get_operatorPosition(), so ensure operatorPosition is set.
    SgExpression *expr = isSgExpression(located_node);
    if (expr != NULL) {
      Sg_File_Info *op_fi = new Sg_File_Info(*start_fi);
      expr->set_operatorPosition(op_fi);
      op_fi->set_parent(expr);
    }
  } else {
    if (init_name != NULL) {
      setFileInfosWithParent(init_name, start_fi, end_fi);
    }
  }

  if (semantic_auxiliary_physical_source) {
    SgLocatedNode *semantic_declaration = isSgLocatedNode(node);
    if (semantic_declaration == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[semantic-declaration-source]: "
              "node=%p type=%s is not a located declaration\n",
              static_cast<void *>(node), node->class_name().c_str());
      ROSE_ABORT();
    }
    for (Sg_File_Info *position :
         {semantic_declaration->get_file_info(),
          semantic_declaration->get_startOfConstruct(),
          semantic_declaration->get_endOfConstruct()}) {
      if (position == nullptr || position->get_parent() != node ||
          position->isCompilerGenerated() || position->isFrontendSpecific() ||
          position->isTransformation() ||
          position->isSourcePositionUnavailableInFrontend() ||
          !position->isOutputInCodeGeneration() || position->get_line() <= 0 ||
          position->get_col() <= 0 || position->get_physical_file_id() < 0 ||
          position->get_physical_filename().empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[semantic-declaration-source]: "
                "node=%p type=%s lost its exact physical semantic "
                "provenance\n",
                static_cast<void *>(node), node->class_name().c_str());
        ROSE_ABORT();
      }
    }
  }

  if (embedded_declarator_tag &&
      (located_node == nullptr || located_node->isOutputInCodeGeneration() ||
       start_fi->isOutputInCodeGeneration() ||
       end_fi->isOutputInCodeGeneration())) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[embedded-tag-source-role]: "
            "declaration=%p type=%s failed exact declarator-owned "
            "publication\n",
            static_cast<void *>(source_declaration),
            source_declaration->class_name().c_str());
    ROSE_ABORT();
  }

  if (translation_unit_order.has_value()) {
    if (start_fi->get_source_sequence_number() != 0) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expanded-token-order]: declaration=%p "
              "type=%s cannot publish exact source-position sequence=%u "
              "from current sequence=%u\n",
              static_cast<void *>(source_declaration),
              source_declaration->class_name().c_str(), *translation_unit_order,
              start_fi->get_source_sequence_number());
      ROSE_ABORT();
    }
    start_fi->set_source_sequence_number(*translation_unit_order);
  }

  for (Sg_File_Info *file_info : previous_file_infos) {
    delete file_info;
  }

  // Source namespaces publish their order atomically with their typed source
  // fragments in buildNamespaceDeclaration_nfi. Other declarations publish
  // the order exactly once after their source position is complete.
  if (translation_unit_order.has_value() &&
      isSgNamespaceDeclarationStatement(source_declaration) == nullptr) {
    publishClangTranslationUnitSourceOrder(source_declaration,
                                           *translation_unit_order);
  }
}

void ClangToSageTranslator::applySemanticAuxiliarySourceRange(
    SgNode *node, clang::SourceRange source_range) {
  SgDeclarationStatement *semantic_owner = isSgDeclarationStatement(node);
  if (SgClassDefinition *definition = isSgClassDefinition(node)) {
    semantic_owner = definition->get_declaration();
  }
  SgAuxiliaryDeclarationList *auxiliary = isSgAuxiliaryDeclarationList(
      semantic_owner != nullptr ? semantic_owner->get_parent() : nullptr);
  SgScopeStatement *semantic_scope =
      auxiliary != nullptr ? isSgScopeStatement(auxiliary->get_parent())
                           : nullptr;
  if (node == nullptr || semantic_owner == nullptr ||
      semantic_scope == nullptr ||
      semantic_owner->get_scope() != semantic_scope ||
      semantic_scope->get_auxiliary_declarations() != auxiliary ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), semantic_owner) != 1 ||
      !p_semantic_auxiliary_physical_source_range_nodes.insert(node).second) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-source]: node=%p "
            "cannot begin an exact auxiliary source transaction\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }
  applySourceRange(node, source_range);
  if (p_semantic_auxiliary_physical_source_range_nodes.erase(node) != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[semantic-declaration-source]: node=%p "
            "lost its active source transaction\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }
}

void ClangToSageTranslator::setSynthesizedFileInfo(SgNode *node) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[synthesized-provenance]: null Sage node "
            "cannot be classified as synthesized\n");
    ROSE_ABORT();
  }
  if (p_lexical_source_nodes.count(node) != 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[synthesized-provenance]: lexical node=%p "
            "type=%s cannot be reclassified as synthesized\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  SgLocatedNode *located_node = isSgLocatedNode(node);
  SgInitializedName *init_name = isSgInitializedName(node);
  if (located_node == nullptr && init_name == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[synthesized-provenance]: node=%p "
            "type=%s is neither SgLocatedNode nor SgInitializedName\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }
  if (!p_synthesized_source_nodes.insert(node).second) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[synthesized-provenance]: node=%p type=%s "
            "was classified as synthesized more than once\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  std::set<Sg_File_Info *> previous_file_infos;
  auto remember_file_info = [&](Sg_File_Info *file_info) {
    if (file_info == nullptr) {
      return;
    }
    const bool is_unclassified_builder_info =
        file_info->isSourcePositionUnavailableInFrontend() &&
        !file_info->isTransformation();
    if (!file_info->isCompilerGenerated() && !is_unclassified_builder_info) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[synthesized-provenance]: node=%p "
              "type=%s parent=%p/%s already has source/transformation file "
              "info=%p at %s:%d:%d compiler-generated=%d frontend-specific=%d "
              "transformation=%d unavailable=%d\n",
              static_cast<void *>(node), node->class_name().c_str(),
              static_cast<void *>(node->get_parent()),
              node->get_parent() != nullptr
                  ? node->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(file_info),
              file_info->get_filenameString().c_str(), file_info->get_line(),
              file_info->get_col(), file_info->isCompilerGenerated() ? 1 : 0,
              file_info->isFrontendSpecific() ? 1 : 0,
              file_info->isTransformation() ? 1 : 0,
              file_info->isSourcePositionUnavailableInFrontend() ? 1 : 0);
      ROSE_ABORT();
    }
    if (file_info->get_parent() != node) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[synthesized-provenance-owner]: node=%p "
              "type=%s does not exclusively own prior file info=%p "
              "parent=%p\n",
              static_cast<void *>(node), node->class_name().c_str(),
              static_cast<void *>(file_info),
              static_cast<void *>(file_info->get_parent()));
      ROSE_ABORT();
    }
    previous_file_infos.insert(file_info);
  };

  if (located_node != nullptr) {
    remember_file_info(located_node->get_startOfConstruct());
    remember_file_info(located_node->get_endOfConstruct());
    if (SgExpression *expression = isSgExpression(located_node)) {
      remember_file_info(expression->get_operatorPosition());
    } else {
      remember_file_info(located_node->get_file_info());
    }
  } else {
    remember_file_info(init_name->get_startOfConstruct());
    remember_file_info(init_name->get_endOfConstruct());
    remember_file_info(init_name->get_file_info());
  }

  auto make_synthesized_file_info = []() -> Sg_File_Info * {
    Sg_File_Info *file_info =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    ASSERT_not_null(file_info);
    file_info->setCompilerGenerated();
    file_info->setFrontendSpecific();
    file_info->unsetTransformation();
    file_info->unsetSourcePositionUnavailableInFrontend();
    file_info->setOutputInCodeGeneration();
    file_info->set_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    file_info->set_physical_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    return file_info;
  };

  Sg_File_Info *start_fi = make_synthesized_file_info();
  Sg_File_Info *end_fi = make_synthesized_file_info();
  if (located_node != nullptr) {
    setFileInfosWithParent(located_node, start_fi, end_fi);
    if (SgExpression *expression = isSgExpression(located_node)) {
      Sg_File_Info *operator_fi = make_synthesized_file_info();
      expression->set_operatorPosition(operator_fi);
      operator_fi->set_parent(expression);
    }
  } else {
    setFileInfosWithParent(init_name, start_fi, end_fi);
  }

  for (Sg_File_Info *file_info : previous_file_infos) {
    delete file_info;
  }
}

SgExpression *
ClangToSageTranslator::copyExpressionForFrontendReuse(SgExpression *expr) {
  if (expr == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[expression-cache-copy]: null source "
            "expression\n");
    ROSE_ABORT();
  }

  SgExpression *copy = SageInterface::copyExpression(expr);
  if (copy == nullptr || copy == expr || copy->get_parent() != nullptr) {
    fprintf(
        stderr,
        "REX_FRONTEND_INVARIANT[expression-cache-copy]: source=%p/%s "
        "produced copy=%p with parent=%p\n",
        static_cast<void *>(expr), expr->class_name().c_str(),
        static_cast<void *>(copy),
        static_cast<void *>(copy != nullptr ? copy->get_parent() : nullptr));
    ROSE_ABORT();
  }

  struct NodePair {
    SgNode *source = nullptr;
    SgNode *target = nullptr;
  };
  std::vector<NodePair> node_pairs;
  std::vector<NodePair> worklist{{expr, copy}};
  std::unordered_set<SgNode *> source_nodes;
  std::unordered_set<SgNode *> target_nodes;

  // Validate the complete owned topology before changing any copied source
  // state.  Non-owning semantic references are deliberately excluded: the
  // generic copy transaction fixes those through its own declaration/symbol
  // maps, while every expression-tree child must have one exact copied owner.
  while (!worklist.empty()) {
    NodePair current = worklist.back();
    worklist.pop_back();
    if (current.source == nullptr || current.target == nullptr ||
        current.source->variantT() != current.target->variantT() ||
        !source_nodes.insert(current.source).second ||
        !target_nodes.insert(current.target).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-topology]: "
              "source=%p/%s target=%p/%s is null, aliased, or changes node "
              "kind\n",
              static_cast<void *>(current.source),
              current.source != nullptr ? current.source->class_name().c_str()
                                        : "<null>",
              static_cast<void *>(current.target),
              current.target != nullptr ? current.target->class_name().c_str()
                                        : "<null>");
      ROSE_ABORT();
    }
    node_pairs.push_back(current);

    const std::vector<SgNode *> source_children =
        current.source->get_traversalSuccessorContainer();
    const std::vector<SgNode *> target_children =
        current.target->get_traversalSuccessorContainer();
    if (source_children.size() != target_children.size()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-topology]: "
              "source=%p/%s has %zu traversal edges but copy=%p/%s has %zu\n",
              static_cast<void *>(current.source),
              current.source->class_name().c_str(), source_children.size(),
              static_cast<void *>(current.target),
              current.target->class_name().c_str(), target_children.size());
      ROSE_ABORT();
    }

    for (std::size_t i = 0; i < source_children.size(); ++i) {
      SgNode *source_child = source_children[i];
      SgNode *target_child = target_children[i];
      const bool source_owned = source_child != nullptr &&
                                source_child->get_parent() == current.source;
      const bool target_owned = target_child != nullptr &&
                                target_child->get_parent() == current.target;
      if (source_owned != target_owned) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-topology]: "
                "owner=%p/%s copy=%p/%s edge=%zu changes ownership "
                "source-child=%p target-child=%p\n",
                static_cast<void *>(current.source),
                current.source->class_name().c_str(),
                static_cast<void *>(current.target),
                current.target->class_name().c_str(), i,
                static_cast<void *>(source_child),
                static_cast<void *>(target_child));
        ROSE_ABORT();
      }
      if (source_owned) {
        worklist.push_back({source_child, target_child});
      }
    }
  }

  enum class FileInfoSlot {
    located_start,
    located_end,
    expression_operator,
    initialized_name_start,
    initialized_name_end,
    initialized_name_root
  };
  struct FileInfoTransfer {
    SgNode *target_owner = nullptr;
    FileInfoSlot slot = FileInfoSlot::located_start;
    const Sg_File_Info *source = nullptr;
    Sg_File_Info *prior_target = nullptr;
    Sg_File_Info *replacement = nullptr;
  };
  std::vector<FileInfoTransfer> transfers;
  std::vector<SgNode *> synthesized_copies;
  std::vector<SgNode *> lexical_copies;
  std::unordered_set<Sg_File_Info *> prior_target_file_infos;

  auto require_source_file_info = [](const Sg_File_Info *file_info,
                                     SgNode *owner, const char *slot) {
    if (file_info == nullptr || file_info->get_parent() != owner ||
        file_info->isShared() || file_info->isTransformation() ||
        file_info->isSourcePositionUnavailableInFrontend() ||
        !file_info->isOutputInCodeGeneration() ||
        (file_info->isFrontendSpecific() &&
         !file_info->isCompilerGenerated()) ||
        (file_info->isImplicitCast() && !file_info->isCompilerGenerated())) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
              "source=%p/%s slot=%s file-info=%p owner=%p has incomplete or "
              "contradictory frontend provenance\n",
              static_cast<void *>(owner), owner->class_name().c_str(), slot,
              static_cast<const void *>(file_info),
              static_cast<void *>(file_info != nullptr ? file_info->get_parent()
                                                       : nullptr));
      ROSE_ABORT();
    }
  };

  auto same_classification = [](const Sg_File_Info *lhs,
                                const Sg_File_Info *rhs) {
    return lhs->isCompilerGenerated() == rhs->isCompilerGenerated() &&
           lhs->isFrontendSpecific() == rhs->isFrontendSpecific() &&
           lhs->isTransformation() == rhs->isTransformation() &&
           lhs->isSourcePositionUnavailableInFrontend() ==
               rhs->isSourcePositionUnavailableInFrontend() &&
           lhs->isOutputInCodeGeneration() == rhs->isOutputInCodeGeneration() &&
           lhs->isImplicitCast() == rhs->isImplicitCast() &&
           lhs->get_file_id() == rhs->get_file_id() &&
           lhs->get_physical_file_id() == rhs->get_physical_file_id();
  };

  auto add_transfer =
      [&](SgNode *source_owner, SgNode *target_owner, FileInfoSlot slot,
          const Sg_File_Info *source_file_info, Sg_File_Info *target_file_info,
          const char *slot_name) {
        require_source_file_info(source_file_info, source_owner, slot_name);
        if (target_file_info != nullptr &&
            (target_file_info->get_parent() != target_owner ||
             !prior_target_file_infos.insert(target_file_info).second)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                  "target=%p/%s slot=%s file-info=%p owner=%p is shared or "
                  "misowned before publication\n",
                  static_cast<void *>(target_owner),
                  target_owner->class_name().c_str(), slot_name,
                  static_cast<void *>(target_file_info),
                  static_cast<void *>(target_file_info != nullptr
                                          ? target_file_info->get_parent()
                                          : nullptr));
          ROSE_ABORT();
        }
        transfers.push_back(
            {target_owner, slot, source_file_info, target_file_info, nullptr});
      };

  // Collect and validate every provenance edge first.  No target is modified
  // until the source tree, copied topology, and all existing target ownership
  // have passed as one transaction.
  for (const NodePair &node_pair : node_pairs) {
    if (isSgLocatedNode(node_pair.source) != nullptr &&
        p_synthesized_source_nodes.count(node_pair.source) +
                p_lexical_source_nodes.count(node_pair.source) !=
            1) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
              "source=%p/%s has no single source/generated producer role\n",
              static_cast<void *>(node_pair.source),
              node_pair.source->class_name().c_str());
      ROSE_ABORT();
    }
    if (p_synthesized_source_nodes.count(node_pair.source) != 0) {
      if (p_synthesized_source_nodes.count(node_pair.target) != 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                "fresh copy=%p/%s is already registered as synthesized\n",
                static_cast<void *>(node_pair.target),
                node_pair.target->class_name().c_str());
        ROSE_ABORT();
      }
      synthesized_copies.push_back(node_pair.target);
    }
    if (p_lexical_source_nodes.count(node_pair.source) != 0) {
      if (p_lexical_source_nodes.count(node_pair.target) != 0) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                "fresh copy=%p/%s is already registered as lexical\n",
                static_cast<void *>(node_pair.target),
                node_pair.target->class_name().c_str());
        ROSE_ABORT();
      }
      lexical_copies.push_back(node_pair.target);
    }

    if (SgLocatedNode *source_located = isSgLocatedNode(node_pair.source)) {
      SgLocatedNode *target_located = isSgLocatedNode(node_pair.target);
      if (target_located == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                "located source=%p/%s maps to non-located copy=%p/%s\n",
                static_cast<void *>(node_pair.source),
                node_pair.source->class_name().c_str(),
                static_cast<void *>(node_pair.target),
                node_pair.target->class_name().c_str());
        ROSE_ABORT();
      }
      const Sg_File_Info *source_start = source_located->get_startOfConstruct();
      const Sg_File_Info *source_end = source_located->get_endOfConstruct();
      require_source_file_info(source_start, source_located, "start");
      require_source_file_info(source_end, source_located, "end");
      if (!same_classification(source_start, source_end)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                "source=%p/%s start/end classifications disagree\n",
                static_cast<void *>(source_located),
                source_located->class_name().c_str());
        ROSE_ABORT();
      }
      add_transfer(source_located, target_located, FileInfoSlot::located_start,
                   source_start, target_located->get_startOfConstruct(),
                   "start");
      add_transfer(source_located, target_located, FileInfoSlot::located_end,
                   source_end, target_located->get_endOfConstruct(), "end");

      if (SgExpression *source_expression = isSgExpression(source_located)) {
        SgExpression *target_expression = isSgExpression(target_located);
        const Sg_File_Info *source_operator =
            source_expression->get_operatorPosition();
        require_source_file_info(source_operator, source_expression,
                                 "operator");
        if (target_expression == nullptr ||
            !same_classification(source_start, source_operator)) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[expression-cache-copy-"
                  "provenance]: source=%p/%s operator classification or "
                  "copied expression kind disagrees\n",
                  static_cast<void *>(source_expression),
                  source_expression->class_name().c_str());
          ROSE_ABORT();
        }
        add_transfer(source_expression, target_expression,
                     FileInfoSlot::expression_operator, source_operator,
                     target_expression->get_operatorPosition(), "operator");
      }
      continue;
    }

    if (SgInitializedName *source_name =
            isSgInitializedName(node_pair.source)) {
      SgInitializedName *target_name = isSgInitializedName(node_pair.target);
      if (target_name == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                "initialized name source=%p maps to non-name copy=%p\n",
                static_cast<void *>(source_name),
                static_cast<void *>(node_pair.target));
        ROSE_ABORT();
      }
      const Sg_File_Info *source_start = source_name->get_startOfConstruct();
      const Sg_File_Info *source_end = source_name->get_endOfConstruct();
      const Sg_File_Info *source_root = source_name->get_file_info();
      require_source_file_info(source_start, source_name, "name-start");
      require_source_file_info(source_end, source_name, "name-end");
      require_source_file_info(source_root, source_name, "name-root");
      if (!same_classification(source_start, source_end) ||
          !same_classification(source_start, source_root)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
                "initialized name=%p source classifications disagree\n",
                static_cast<void *>(source_name));
        ROSE_ABORT();
      }
      add_transfer(source_name, target_name,
                   FileInfoSlot::initialized_name_start, source_start,
                   target_name->get_startOfConstruct(), "name-start");
      add_transfer(source_name, target_name, FileInfoSlot::initialized_name_end,
                   source_end, target_name->get_endOfConstruct(), "name-end");
      add_transfer(source_name, target_name,
                   FileInfoSlot::initialized_name_root, source_root,
                   target_name->get_file_info(), "name-root");
    }
  }

  for (FileInfoTransfer &transfer : transfers) {
    transfer.replacement = new Sg_File_Info(*transfer.source);
    if (transfer.replacement == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
              "could not allocate copied file information\n");
      ROSE_ABORT();
    }
  }

  for (FileInfoTransfer &transfer : transfers) {
    switch (transfer.slot) {
    case FileInfoSlot::located_start:
      isSgLocatedNode(transfer.target_owner)
          ->set_startOfConstruct(transfer.replacement);
      break;
    case FileInfoSlot::located_end:
      isSgLocatedNode(transfer.target_owner)
          ->set_endOfConstruct(transfer.replacement);
      break;
    case FileInfoSlot::expression_operator:
      isSgExpression(transfer.target_owner)
          ->set_operatorPosition(transfer.replacement);
      break;
    case FileInfoSlot::initialized_name_start:
      isSgInitializedName(transfer.target_owner)
          ->set_startOfConstruct(transfer.replacement);
      break;
    case FileInfoSlot::initialized_name_end:
      isSgInitializedName(transfer.target_owner)
          ->set_endOfConstruct(transfer.replacement);
      break;
    case FileInfoSlot::initialized_name_root:
      isSgInitializedName(transfer.target_owner)
          ->set_file_info(transfer.replacement);
      break;
    }
    transfer.replacement->set_parent(transfer.target_owner);
  }

  for (Sg_File_Info *prior_file_info : prior_target_file_infos) {
    delete prior_file_info;
  }
  for (SgNode *synthesized_copy : synthesized_copies) {
    if (!p_synthesized_source_nodes.insert(synthesized_copy).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
              "copy=%p/%s synthesized registration is not unique\n",
              static_cast<void *>(synthesized_copy),
              synthesized_copy->class_name().c_str());
      ROSE_ABORT();
    }
  }
  for (SgNode *lexical_copy : lexical_copies) {
    if (!p_lexical_source_nodes.insert(lexical_copy).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[expression-cache-copy-provenance]: "
              "copy=%p/%s lexical registration is not unique\n",
              static_cast<void *>(lexical_copy),
              lexical_copy->class_name().c_str());
      ROSE_ABORT();
    }
  }

  return copy;
}

SgExpression *
ClangToSageTranslator::prepareExpressionForAttachment(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  // The statement translation cache stores the first translated SgExpression
  // for a Clang Expr. That first translation can still be detached until some
  // owning node adopts it. Deep-copying a detached cached tree preserves the
  // null-parent root in every clone and triggers fixupCopy_scopes warnings.
  // Let the first owner adopt the original tree, then deep-copy only once the
  // cached translation is already attached somewhere in the AST.
  if (expr->get_parent() == nullptr) {
    return expr;
  }

  return copyExpressionForFrontendReuse(expr);
}

SgTemplateArgumentPtrList
ClangToSageTranslator::cloneTemplateArgumentSurfacePreservingIdentity(
    const SgTemplateArgumentPtrList &source) const {
  SgTemplateArgumentPtrList result;
  result.reserve(source.size());

  for (SgTemplateArgument *arg : source) {
    ASSERT_not_null(arg);

    SgTemplateArgument *copy =
        isSgTemplateArgument(SageInterface::deepCopy(arg));
    ROSE_ASSERT(copy != nullptr);
    copy->set_parent(nullptr);
    if (copy == arg || copy->get_parent() != nullptr ||
        !SageInterface::templateArgumentEquivalence(copy, arg)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[template-argument-clone-identity]: "
              "source=%p clone=%p does not provide one distinct surface "
              "with the exact canonical semantic payload\n",
              static_cast<void *>(arg), static_cast<void *>(copy));
      ROSE_ABORT();
    }
    result.push_back(copy);
  }

  return result;
}

void ClangToSageTranslator::requireDetachedSourceExpressionProvenance(
    SgExpression *expression, const char *context) const {
  if (expression == nullptr || context == nullptr ||
      expression->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[detached-source-expression-provenance]: "
            "context=%s expression=%p parent=%p is not one exact detached "
            "expression\n",
            context != nullptr ? context : "<null>",
            static_cast<void *>(expression),
            static_cast<void *>(expression != nullptr ? expression->get_parent()
                                                      : nullptr));
    ROSE_ABORT();
  }

  auto is_exact_source = [](const Sg_File_Info *file_info,
                            const SgNode *owner) {
    return file_info != nullptr && file_info->get_parent() == owner &&
           file_info->get_line() > 0 && file_info->get_col() > 0 &&
           file_info->get_physical_file_id() >= 0 &&
           !file_info->isCompilerGenerated() &&
           !file_info->isFrontendSpecific() && !file_info->isTransformation() &&
           !file_info->isSourcePositionUnavailableInFrontend() &&
           file_info->isOutputInCodeGeneration();
  };
  auto has_exact_source = [&](const SgExpression *candidate) {
    if (candidate == nullptr) {
      return false;
    }
    const Sg_File_Info *primary = candidate->get_file_info();
    const Sg_File_Info *start = candidate->get_startOfConstruct();
    const Sg_File_Info *end = candidate->get_endOfConstruct();
    return is_exact_source(primary, candidate) &&
           is_exact_source(start, candidate) &&
           is_exact_source(end, candidate) &&
           primary->get_physical_file_id() == start->get_physical_file_id() &&
           start->get_physical_file_id() == end->get_physical_file_id() &&
           primary->get_line() == start->get_line() &&
           primary->get_col() == start->get_col();
  };

  const size_t lexical_root_roles = p_lexical_source_nodes.count(expression);
  const size_t semantic_root_roles =
      p_synthesized_source_nodes.count(expression);
  if (lexical_root_roles + semantic_root_roles != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[detached-source-expression-role]: "
            "context=%s expression=%p/%s published lexical=%zu semantic=%zu "
            "root roles instead of one exact role\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str(), lexical_root_roles,
            semantic_root_roles);
    ROSE_ABORT();
  }

  const bool semantic_root = semantic_root_roles == 1;
  if (!semantic_root) {
    if (!has_exact_source(expression) ||
        !is_exact_source(expression->get_operatorPosition(), expression)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[detached-source-expression-"
              "provenance]: context=%s source expression=%p/%s has no exact "
              "physical provenance\n",
              context, static_cast<void *>(expression),
              expression->class_name().c_str());
      ROSE_ABORT();
    }
    return;
  }

  for (Sg_File_Info *file_info :
       {expression->get_file_info(), expression->get_startOfConstruct(),
        expression->get_endOfConstruct(), expression->get_operatorPosition()}) {
    if (file_info == nullptr || file_info->get_parent() != expression ||
        !file_info->isCompilerGenerated() || file_info->isFrontendSpecific() ||
        file_info->isTransformation() ||
        file_info->isSourcePositionUnavailableInFrontend() ||
        !file_info->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[detached-source-expression-"
              "provenance]: context=%s semantic expression=%p/%s has "
              "inconsistent generated provenance\n",
              context, static_cast<void *>(expression),
              expression->class_name().c_str());
      ROSE_ABORT();
    }
  }

  if (SgCastExp *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    if (cast->get_cast_type() != SgCastExp::e_implicit_cast ||
        cast->get_operand() == nullptr ||
        cast->get_operand()->get_parent() != cast) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[detached-source-expression-"
              "provenance]: context=%s synthesized cast=%p has no exact typed "
              "implicit-cast operand\n",
              context, static_cast<void *>(cast));
      ROSE_ABORT();
    }
    for (Sg_File_Info *file_info :
         {cast->get_file_info(), cast->get_startOfConstruct(),
          cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
      if (!file_info->isImplicitCast()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[detached-source-expression-"
                "provenance]: context=%s synthesized cast=%p lost its "
                "implicit-cast role\n",
                context, static_cast<void *>(cast));
        ROSE_ABORT();
      }
    }
  }

  bool has_source_spelled_owner = false;
  std::vector<SgNode *> worklist{expression};
  std::unordered_set<SgNode *> visited;
  while (!worklist.empty()) {
    SgNode *owner = worklist.back();
    worklist.pop_back();
    if (owner == nullptr || !visited.insert(owner).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[detached-source-expression-owner]: "
              "context=%s semantic expression=%p/%s has a null or aliased "
              "owned subtree\n",
              context, static_cast<void *>(expression),
              expression->class_name().c_str());
      ROSE_ABORT();
    }
    for (SgNode *child : owner->get_traversalSuccessorContainer()) {
      if (child == nullptr || child->get_parent() != owner) {
        continue;
      }
      if (SgExpression *child_expression = isSgExpression(child);
          child_expression != nullptr &&
          p_synthesized_source_nodes.count(child_expression) == 0 &&
          has_exact_source(child_expression)) {
        has_source_spelled_owner = true;
      }
      worklist.push_back(child);
    }
  }
  if (!has_source_spelled_owner) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[detached-source-expression-owner]: "
            "context=%s semantic expression=%p/%s has no source-spelled owned "
            "expression\n",
            context, static_cast<void *>(expression),
            expression->class_name().c_str());
    ROSE_ABORT();
  }
}

/* Overload of ASTConsumer::HandleTranslationUnit, it is the "entry point" */

void ClangToSageTranslator::HandleTranslationUnit(
    clang::ASTContext &ast_context) {
  if (p_compiler_instance != nullptr && p_compiler_instance->hasSema()) {
    roseClangPhaseTrace(
        "clang_main.HandleTranslationUnit.late_templates.begin");
    clang::Sema &sema = p_compiler_instance->getSema();

    // When we drive Clang's parser directly, delayed template parsing can leave
    // bodies queued in Sema when HandleTranslationUnit fires.  A complete Sage
    // AST requires every queued body and every diagnostic from parsing it.
    if (!sema.LateParsedTemplateMap.empty()) {
      if (sema.LateTemplateParser == nullptr) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[late-template-parser]: Clang queued "
               "delayed template bodies without a parser callback\n";
        ROSE_ABORT();
      }
      size_t previous_size = static_cast<size_t>(-1);
      while (!sema.LateParsedTemplateMap.empty() &&
             sema.LateParsedTemplateMap.size() != previous_size) {
        previous_size = sema.LateParsedTemplateMap.size();

        std::vector<const clang::FunctionDecl *> pending_templates;
        pending_templates.reserve(sema.LateParsedTemplateMap.size());
        for (const auto &entry : sema.LateParsedTemplateMap) {
          pending_templates.push_back(entry.first);
        }

        for (const clang::FunctionDecl *function_decl : pending_templates) {
          auto it = sema.LateParsedTemplateMap.find(function_decl);
          if (it == sema.LateParsedTemplateMap.end()) {
            continue;
          }
          if (!it->second) {
            llvm::errs()
                << "REX_FRONTEND_INVARIANT[late-template-parser]: pending "
                   "template body has no parser state\n";
            ROSE_ABORT();
          }

#if ROSE_USE_VALGRIND
          ValgrindErrorReportingScope late_template_valgrind_scope;
#endif
          sema.LateTemplateParser(sema.OpaqueParser, *it->second);
#if ROSE_USE_VALGRIND
          late_template_valgrind_scope.enable();
#endif
        }
      }
      if (!sema.LateParsedTemplateMap.empty()) {
        llvm::errs()
            << "REX_FRONTEND_INVARIANT[late-template-parser]: Clang left "
            << sema.LateParsedTemplateMap.size()
            << " delayed template body/bodies unparsed\n";
        ROSE_ABORT();
      }
    }
    roseClangPhaseTrace("clang_main.HandleTranslationUnit.late_templates.end");
  }

  roseClangPhaseTrace("clang_main.HandleTranslationUnit.traverse.begin");
  Traverse(ast_context.getTranslationUnitDecl());
  roseClangPhaseTrace("clang_main.HandleTranslationUnit.traverse.end");

  // REX owns OpenMP parsing.  Clang contributes only ordinary base-language
  // declarations: enumerators referenced solely from a captured pragma would
  // otherwise remain hidden with their non-emitted header owner.
  materializeCapturedOpenMPReferencedEnumConstants(
      ast_context.getTranslationUnitDecl());

  // Written typedef dependencies are output-surface invariants.  Validate
  // them only after translation-unit construction has committed every typed
  // structural owner; checking while a control statement or declaration
  // group is still detached mistakes a valid construction transaction for a
  // malformed final AST.
  validateQueuedWrittenTypedefDependenciesForOutputSurfaces();
}

/* Preprocessor Stack */

std::pair<Sg_File_Info *, PreprocessingInfo *>
ClangToSageTranslator::preprocessor_top() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->top();
}

bool ClangToSageTranslator::preprocessor_pop() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->pop();
}

size_t ClangToSageTranslator::preprocessor_list_size() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->size();
}

std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
ClangToSageTranslator::preprocessor_remaining_records() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->remainingRecords();
}

void ClangToSageTranslator::preprocessor_mark_attached(
    PreprocessingInfo *preprocessing_info) {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  p_sage_preprocessor_recorder->markAttached(preprocessing_info);
}

void ClangToSageTranslator::sortPreprocessorList() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  p_sage_preprocessor_recorder->sortRecordedDirectives();
}

std::string
ClangToSageTranslator::getSourceText(clang::SourceRange range) const {
  if (p_compiler_instance == nullptr || !range.isValid()) {
    return std::string();
  }
  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();

  clang::SourceLocation begin = range.getBegin();
  clang::SourceLocation end = range.getEnd();
  if (begin.isMacroID()) {
    begin = sm.getSpellingLoc(begin);
  }
  if (end.isMacroID()) {
    end = sm.getSpellingLoc(end);
  }
  if (!begin.isValid() || !end.isValid()) {
    return std::string();
  }

  clang::CharSourceRange char_range =
      clang::CharSourceRange::getTokenRange(begin, end);
  bool invalid = false;
  llvm::StringRef text =
      clang::Lexer::getSourceText(char_range, sm, lang_opts, &invalid);
  if (invalid) {
    return std::string();
  }
  return text.str();
}

// class SagePreprocessorRecord

namespace {

bool isStructuralConditionalDirective(
    PreprocessingInfo::DirectiveType directive_type) {
  switch (directive_type) {
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
    return true;
  default:
    return false;
  }
}

unsigned lineDirectiveEndOffset(clang::SourceManager *source_manager,
                                clang::FileID file_id, unsigned begin_offset) {
  if (source_manager == nullptr || !file_id.isValid()) {
    return begin_offset;
  }

  auto buffer = source_manager->getBufferDataOrNone(file_id);
  if (!buffer || begin_offset >= buffer->size()) {
    return begin_offset;
  }

  unsigned end_offset = begin_offset;
  while (end_offset < buffer->size() && (*buffer)[end_offset] != '\n' &&
         (*buffer)[end_offset] != '\r') {
    ++end_offset;
  }

  if (end_offset < buffer->size()) {
    if ((*buffer)[end_offset] == '\r' && end_offset + 1 < buffer->size() &&
        (*buffer)[end_offset + 1] == '\n') {
      end_offset += 2;
    } else {
      ++end_offset;
    }
  }

  return end_offset;
}

} // namespace

SagePreprocessorRecord::SagePreprocessorRecord(
    clang::SourceManager *source_manager, clang::Preprocessor *preprocessor,
    bool record_directive_stream, bool record_header_directives)
    : p_source_manager(source_manager), p_preprocessor(preprocessor),
      p_preprocessor_record_list(), p_preprocessor_records_by_location(),
      p_preprocessor_records_by_line(), p_preprocessor_record_positions(),
      p_preprocessor_records_by_file_offset(),
      p_include_records_by_file_offset(), p_removed_preprocessor_records(),
      p_attached_preprocessor_records(), p_preprocessor_record_cursor(0),
      p_preprocessor_record_list_sorted(true),
      p_record_directive_stream(record_directive_stream),
      p_record_header_directives(record_header_directives),
      p_include_ownership_paths(), p_resolved_include_directives(),
      p_normalized_physical_paths_by_file_id(),
      p_physical_occurrences_by_clang_file_id() {}

SagePreprocessorRecord::~SagePreprocessorRecord() {
  for (auto &record : p_preprocessor_record_list) {
    releaseRecordedDirective(record);
  }
}

unsigned
SagePreprocessorRecord::requirePhysicalFileOccurrence(clang::FileID file_id,
                                                      const char *context) {
  if (!file_id.isValid() || context == nullptr || context[0] == '\0') {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[physical-file-occurrence]: context=%s "
            "has no exact Clang FileID\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }
  const unsigned localFileId = file_id.getHashValue();
  if (localFileId == 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[physical-file-occurrence]: context=%s "
            "has zero Clang FileID\n",
            context);
    ROSE_ABORT();
  }
  const auto existing =
      p_physical_occurrences_by_clang_file_id.find(localFileId);
  if (existing != p_physical_occurrences_by_clang_file_id.end()) {
    if (existing->second == 0) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[physical-file-occurrence]: context=%s "
              "Clang FileID=%u maps to an invalid occurrence\n",
              context, localFileId);
      ROSE_ABORT();
    }
    return existing->second;
  }

  static std::atomic<unsigned> nextOccurrence{1};
  const unsigned occurrence =
      nextOccurrence.fetch_add(1, std::memory_order_relaxed);
  if (occurrence == 0 || occurrence == std::numeric_limits<unsigned>::max()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[physical-file-occurrence]: exhausted "
            "the process-wide exact occurrence space\n");
    ROSE_ABORT();
  }
  const auto inserted =
      p_physical_occurrences_by_clang_file_id.emplace(localFileId, occurrence);
  if (!inserted.second || inserted.first->second != occurrence) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[physical-file-occurrence]: context=%s "
            "failed exact Clang FileID=%u publication\n",
            context, localFileId);
    ROSE_ABORT();
  }
  return occurrence;
}

void SagePreprocessorRecord::markAttached(
    PreprocessingInfo *preprocessing_info) {
  if (preprocessing_info == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessing-owner]: cannot attach a "
            "null preprocessing record\n");
    ROSE_ABORT();
  }
  if (!p_attached_preprocessor_records.insert(preprocessing_info).second) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessing-owner]: preprocessing "
            "record=%p was attached more than once\n",
            static_cast<void *>(preprocessing_info));
    ROSE_ABORT();
  }
}

std::vector<SagePreprocessorRecord::RecordedDirectiveRef>
SagePreprocessorRecord::recordedDirectivesWithin(
    clang::SourceRange source_range, bool allow_at_interval_start,
    const char *contract) const {
  if (p_source_manager == nullptr || p_preprocessor == nullptr ||
      source_range.getBegin().isInvalid() ||
      source_range.getEnd().isInvalid() || contract == nullptr ||
      *contract == '\0') {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[recorded-directive-range]: range query "
            "requires an exact frontend range and producer contract\n");
    ROSE_ABORT();
  }
  clang::SourceLocation begin =
      p_source_manager->getFileLoc(source_range.getBegin());
  clang::SourceLocation end =
      p_source_manager->getFileLoc(source_range.getEnd());
  if (begin.isInvalid() || end.isInvalid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[recorded-directive-range]: %s has no "
            "exact physical endpoints\n",
            contract);
    ROSE_ABORT();
  }
  const clang::FileID file_id = p_source_manager->getFileID(begin);
  if (!file_id.isValid() || p_source_manager->getFileID(end) != file_id) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[recorded-directive-range]: %s crosses "
            "physical files\n",
            contract);
    ROSE_ABORT();
  }
  clang::SourceLocation after_end = clang::Lexer::getLocForEndOfToken(
      end, 0, *p_source_manager, p_preprocessor->getLangOpts());
  after_end = p_source_manager->getFileLoc(after_end);
  if (after_end.isInvalid() ||
      p_source_manager->getFileID(after_end) != file_id) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[recorded-directive-range]: %s has no "
            "exact half-open token interval\n",
            contract);
    ROSE_ABORT();
  }
  const unsigned begin_offset = p_source_manager->getFileOffset(begin);
  const unsigned end_offset = p_source_manager->getFileOffset(after_end);
  if (begin_offset >= end_offset) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[recorded-directive-range]: %s has empty "
            "or retrograde offsets=%u/%u\n",
            contract, begin_offset, end_offset);
    ROSE_ABORT();
  }

  std::vector<RecordedDirectiveRef> result;
  const auto file_records =
      p_preprocessor_records_by_file_offset.find(file_id.getHashValue());
  if (file_records == p_preprocessor_records_by_file_offset.end()) {
    return result;
  }
  auto offset_records = file_records->second.lower_bound(begin_offset);
  if (!allow_at_interval_start &&
      offset_records != file_records->second.end() &&
      offset_records->first == begin_offset) {
    ++offset_records;
  }
  for (; offset_records != file_records->second.end() &&
         offset_records->first < end_offset;
       ++offset_records) {
    for (const RecordedDirectiveRef &record : offset_records->second) {
      const auto position =
          p_preprocessor_record_positions.find(record.preprocessing_info);
      if (record.file_info == nullptr || record.preprocessing_info == nullptr ||
          position == p_preprocessor_record_positions.end() ||
          position->second.file_id != file_id ||
          position->second.file_offset != offset_records->first) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[recorded-directive-range]: %s has "
                "an inconsistent physical directive index\n",
                contract);
        ROSE_ABORT();
      }
      result.push_back(record);
    }
  }
  return result;
}

std::vector<clang::SourceLocation>
SagePreprocessorRecord::includeDirectiveLocationsWithin(
    clang::SourceRange source_range, bool allow_at_interval_start) const {
  if (p_source_manager == nullptr || p_preprocessor == nullptr ||
      source_range.getBegin().isInvalid() ||
      source_range.getEnd().isInvalid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-range]: initializer "
            "has no exact source manager, preprocessor, or source range\n");
    ROSE_ABORT();
  }

  clang::SourceLocation begin =
      p_source_manager->getFileLoc(source_range.getBegin());
  clang::SourceLocation end =
      p_source_manager->getFileLoc(source_range.getEnd());
  if (begin.isInvalid() || end.isInvalid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-range]: initializer "
            "range has no exact file locations\n");
    ROSE_ABORT();
  }
  clang::FileID file_id = p_source_manager->getFileID(begin);
  if (!file_id.isValid() || p_source_manager->getFileID(end) != file_id) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-range]: initializer "
            "range crosses physical files\n");
    ROSE_ABORT();
  }
  const unsigned begin_offset = p_source_manager->getFileOffset(begin);
  clang::SourceLocation after_end = clang::Lexer::getLocForEndOfToken(
      end, 0, *p_source_manager, p_preprocessor->getLangOpts());
  after_end = p_source_manager->getFileLoc(after_end);
  if (after_end.isInvalid() ||
      p_source_manager->getFileID(after_end) != file_id) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-range]: initializer "
            "has no exact half-open token range\n");
    ROSE_ABORT();
  }
  const unsigned end_offset = p_source_manager->getFileOffset(after_end);
  if (begin_offset >= end_offset) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-range]: initializer "
            "range is empty or retrograde raw-begin=%u raw-end=%u "
            "file-begin=%u file-end=%u after-end=%u begin-offset=%u "
            "end-offset=%u begin-macro=%d end-macro=%d at %s:%u:%u\n",
            source_range.getBegin().getRawEncoding(),
            source_range.getEnd().getRawEncoding(), begin.getRawEncoding(),
            end.getRawEncoding(), after_end.getRawEncoding(), begin_offset,
            end_offset, source_range.getBegin().isMacroID() ? 1 : 0,
            source_range.getEnd().isMacroID() ? 1 : 0,
            p_source_manager->getFilename(begin).str().c_str(),
            p_source_manager->getSpellingLineNumber(begin),
            p_source_manager->getSpellingColumnNumber(begin));
    ROSE_ABORT();
  }

  std::vector<clang::SourceLocation> result;
  const auto file_records =
      p_include_records_by_file_offset.find(file_id.getHashValue());
  if (file_records == p_include_records_by_file_offset.end()) {
    return result;
  }
  const unsigned first_offset =
      allow_at_interval_start ? begin_offset : begin_offset + 1;
  for (auto record = file_records->second.lower_bound(first_offset);
       record != file_records->second.end() && record->first < end_offset;
       ++record) {
    PreprocessingInfo *info = record->second.preprocessing_info;
    const auto position = p_preprocessor_record_positions.find(info);
    if (record->second.file_info == nullptr || info == nullptr ||
        position == p_preprocessor_record_positions.end() ||
        position->second.file_id != file_id ||
        position->second.file_offset != record->first) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[initializer-include-range]: indexed "
              "include has no exact physical source position\n");
      ROSE_ABORT();
    }
    result.push_back(position->second.file_location);
  }
  return result;
}

std::vector<clang::SourceLocation>
SagePreprocessorRecord::includeDirectiveLocationsStrictlyInside(
    clang::SourceRange source_range) const {
  return includeDirectiveLocationsWithin(source_range,
                                         /*allow_at_interval_start=*/false);
}

std::vector<clang::SourceLocation>
SagePreprocessorRecord::includeDirectiveLocationsAtStartOrStrictlyInside(
    clang::SourceRange source_range) const {
  return includeDirectiveLocationsWithin(source_range,
                                         /*allow_at_interval_start=*/true);
}

void SagePreprocessorRecord::transferRecordedDirectiveToBoundary(
    RecordedDirectiveRef directive, SgLocatedNode *source_interval_owner,
    SgLocatedNode *boundary_owner,
    PreprocessingInfo::RelativePositionType relative_position,
    bool allow_at_interval_start, const char *contract) {
  if (directive.file_info == nullptr ||
      directive.preprocessing_info == nullptr ||
      source_interval_owner == nullptr || boundary_owner == nullptr ||
      contract == nullptr || *contract == '\0') {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[directive-boundary-owner]: transfer "
            "requires an exact directive, source interval, boundary, and "
            "producer contract\n");
    ROSE_ABORT();
  }

  Sg_File_Info *interval_start = source_interval_owner->get_startOfConstruct();
  Sg_File_Info *interval_end = source_interval_owner->get_endOfConstruct();
  Sg_File_Info *directive_info = directive.file_info;
  auto source_less = [](Sg_File_Info *lhs, Sg_File_Info *rhs) {
    return lhs->get_line() < rhs->get_line() ||
           (lhs->get_line() == rhs->get_line() &&
            lhs->get_col() < rhs->get_col());
  };
  if (interval_start == nullptr || interval_end == nullptr ||
      interval_start->get_line() <= 0 || interval_start->get_col() <= 0 ||
      interval_end->get_line() <= 0 || interval_end->get_col() <= 0 ||
      directive_info->get_line() <= 0 || directive_info->get_col() <= 0 ||
      interval_start->get_physical_file_id() < 0 ||
      interval_end->get_physical_file_id() < 0 ||
      directive_info->get_physical_file_id() < 0 ||
      !interval_start->isSameFile(*interval_end) ||
      !interval_start->isSameFile(*directive_info) ||
      interval_start->get_physical_file_occurrence_id() !=
          interval_end->get_physical_file_occurrence_id() ||
      interval_start->get_physical_file_occurrence_id() !=
          directive_info->get_physical_file_occurrence_id() ||
      (!source_less(interval_start, directive_info) &&
       !(allow_at_interval_start &&
         interval_start->get_line() == directive_info->get_line() &&
         interval_start->get_col() == directive_info->get_col())) ||
      !source_less(directive_info, interval_end)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[directive-boundary-owner]: %s directive "
            "at %s:%d:%d occurrence %u is not strictly inside its "
            "source-owner interval %s:%d:%d-%d:%d occurrence %u\n",
            contract, directive_info->get_filenameString().c_str(),
            directive_info->get_line(), directive_info->get_col(),
            directive_info->get_physical_file_occurrence_id(),
            interval_start != nullptr
                ? interval_start->get_filenameString().c_str()
                : "<missing>",
            interval_start != nullptr ? interval_start->get_line() : -1,
            interval_start != nullptr ? interval_start->get_col() : -1,
            interval_end != nullptr ? interval_end->get_line() : -1,
            interval_end != nullptr ? interval_end->get_col() : -1,
            interval_start != nullptr
                ? interval_start->get_physical_file_occurrence_id()
                : 0);
    ROSE_ABORT();
  }

  PreprocessingInfo *info = directive.preprocessing_info;
  if (p_attached_preprocessor_records.find(info) !=
          p_attached_preprocessor_records.end() ||
      p_removed_preprocessor_records.find(info) !=
          p_removed_preprocessor_records.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[directive-boundary-owner]: %s "
            "directive was already claimed by another AST owner\n",
            contract);
    ROSE_ABORT();
  }
  info->setRelativePosition(relative_position);
  markAttached(info);
  boundary_owner->addToAttachedPreprocessingInfo(info);

  AttachedPreprocessingInfoType *attached =
      boundary_owner->getAttachedPreprocessingInfo();
  if (attached == nullptr ||
      std::count(attached->begin(), attached->end(), info) != 1 ||
      info->getRelativePosition() != relative_position) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[directive-boundary-owner]: %s transfer "
            "did not publish one exact typed boundary\n",
            contract);
    ROSE_ABORT();
  }
  markRecordedDirectiveRemoved(directive.file_info, info);
}

void SagePreprocessorRecord::transferIncludeDirectiveToBoundary(
    clang::SourceLocation include_location,
    SgLocatedNode *source_interval_owner, SgLocatedNode *boundary_owner,
    PreprocessingInfo::RelativePositionType relative_position,
    bool allow_at_interval_start, const char *contract) {
  if (p_source_manager == nullptr || include_location.isInvalid() ||
      source_interval_owner == nullptr || boundary_owner == nullptr ||
      contract == nullptr || *contract == '\0') {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[include-boundary-owner]: transfer "
                    "requires an exact include, source interval, boundary, and "
                    "producer contract\n");
    ROSE_ABORT();
  }

  clang::SourceLocation file_location =
      p_source_manager->getFileLoc(include_location);
  if (file_location.isInvalid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s include has "
            "no exact physical source position\n",
            contract);
    ROSE_ABORT();
  }
  clang::FileID file_id = p_source_manager->getFileID(file_location);
  if (!file_id.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s include has "
            "no exact physical file identity\n",
            contract);
    ROSE_ABORT();
  }
  const unsigned file_offset = p_source_manager->getFileOffset(file_location);

  const auto file_records =
      p_include_records_by_file_offset.find(file_id.getHashValue());
  const auto match =
      file_records != p_include_records_by_file_offset.end()
          ? file_records->second.find(file_offset)
          : std::map<unsigned, RecordedDirectiveRef>::const_iterator{};
  if (file_records == p_include_records_by_file_offset.end() ||
      match == file_records->second.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s expected one "
            "include at physical offset %u, found none\n",
            contract, file_offset);
    ROSE_ABORT();
  }
  const RecordedDirectiveRef &matched_record = match->second;
  if (matched_record.file_info == nullptr ||
      matched_record.preprocessing_info == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s indexed "
            "include at physical offset %u is incomplete\n",
            contract, file_offset);
    ROSE_ABORT();
  }

  transferRecordedDirectiveToBoundary(matched_record, source_interval_owner,
                                      boundary_owner, relative_position,
                                      allow_at_interval_start, contract);
}

void SagePreprocessorRecord::transferIncludeDirectiveToInitializerBoundary(
    clang::SourceLocation include_location,
    SgAggregateInitializer *aggregate_initializer,
    SgExpression *following_initializer_element) {
  if (aggregate_initializer == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-owner]: transfer "
            "requires an exact aggregate initializer\n");
    ROSE_ABORT();
  }
  SgExprListExp *initializer_list = aggregate_initializer->get_initializers();
  const auto source_form = aggregate_initializer->get_source_form();
  if (initializer_list == nullptr ||
      source_form !=
          SgAggregateInitializer::e_aggregate_initializer_source_braced ||
      initializer_list->get_parent() != aggregate_initializer) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-owner]: include "
            "owner is not one exact source-braced aggregate initializer\n");
    ROSE_ABORT();
  }
  if (following_initializer_element != nullptr &&
      (following_initializer_element->get_parent() != initializer_list ||
       std::count(initializer_list->get_expressions().begin(),
                  initializer_list->get_expressions().end(),
                  following_initializer_element) != 1)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[initializer-include-owner]: following "
            "initializer element is not one direct aggregate child\n");
    ROSE_ABORT();
  }
  transferIncludeDirectiveToBoundary(
      include_location, aggregate_initializer,
      following_initializer_element != nullptr ? following_initializer_element
                                               : aggregate_initializer,
      following_initializer_element != nullptr ? PreprocessingInfo::before
                                               : PreprocessingInfo::inside,
      /*allow_at_interval_start=*/false, "aggregate-initializer");
}

void SagePreprocessorRecord::transferIncludeDirectiveToAssignmentInitializer(
    clang::SourceLocation include_location,
    SgAssignInitializer *assignment_initializer) {
  if (assignment_initializer == nullptr ||
      assignment_initializer->get_operand_i() == nullptr ||
      assignment_initializer->get_operand_i()->get_parent() !=
          assignment_initializer ||
      (assignment_initializer->get_source_form() !=
           SgAssignInitializer::
               e_assignment_initializer_source_include_operand_expansion &&
       assignment_initializer->get_source_form() !=
           SgAssignInitializer::
               e_assignment_initializer_source_include_complete_expansion)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[assignment-initializer-include-owner]: "
            "include owner is not one exact include-expanded assignment "
            "initializer\n");
    ROSE_ABORT();
  }
  transferIncludeDirectiveToBoundary(
      include_location, assignment_initializer, assignment_initializer,
      PreprocessingInfo::inside,
      assignment_initializer->get_source_form() ==
          SgAssignInitializer::
              e_assignment_initializer_source_include_complete_expansion,
      "assignment-initializer");
}

void SagePreprocessorRecord::transferIncludeDirectiveToClassBoundary(
    clang::SourceLocation include_location, SgClassDefinition *class_definition,
    SgDeclarationStatement *following_member) {
  if (class_definition == nullptr ||
      class_definition->get_declaration() == nullptr ||
      class_definition->get_declaration()->get_definition() !=
          class_definition) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[class-include-owner]: include owner is "
            "not one exact class definition\n");
    ROSE_ABORT();
  }
  if (following_member != nullptr &&
      (following_member->get_parent() != class_definition ||
       std::count(class_definition->get_members().begin(),
                  class_definition->get_members().end(),
                  following_member) != 1)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[class-include-owner]: following member "
            "is not one direct class child\n");
    ROSE_ABORT();
  }
  SgLocatedNode *boundary_owner =
      following_member != nullptr
          ? static_cast<SgLocatedNode *>(following_member)
          : static_cast<SgLocatedNode *>(class_definition);
  transferIncludeDirectiveToBoundary(
      include_location, class_definition, boundary_owner,
      following_member != nullptr ? PreprocessingInfo::before
                                  : PreprocessingInfo::inside,
      /*allow_at_interval_start=*/false, "class-definition");
}

void SagePreprocessorRecord::transferDirectivesToExternalEnumBoundaries(
    clang::SourceRange source_range, SgEnumDeclaration *enum_declaration,
    const std::vector<std::pair<clang::SourceLocation, SgInitializedName *>>
        &source_body_enumerators) {
  if (p_source_manager == nullptr || enum_declaration == nullptr ||
      enum_declaration->isForward()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[enum-directive-owner]: producer has no "
            "exact defining enum declaration\n");
    ROSE_ABORT();
  }
  std::vector<std::pair<unsigned, SgInitializedName *>> boundaries;
  boundaries.reserve(source_body_enumerators.size());
  clang::FileID boundary_file;
  for (const auto &[location, enumerator] : source_body_enumerators) {
    const clang::SourceLocation physical_location =
        p_source_manager->getFileLoc(location);
    if (physical_location.isInvalid() || enumerator == nullptr ||
        enumerator->get_parent() != enum_declaration ||
        enumerator->get_declptr() != enum_declaration ||
        enumerator->get_enum_constant_source_ownership() !=
            SgInitializedName::e_enum_constant_source_body ||
        std::count(enum_declaration->get_enumerators().begin(),
                   enum_declaration->get_enumerators().end(),
                   enumerator) != 1) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[enum-directive-owner]: source-body "
              "enumerator is not one exact direct child\n");
      ROSE_ABORT();
    }
    const clang::FileID current_file =
        p_source_manager->getFileID(physical_location);
    if (!current_file.isValid() ||
        (boundary_file.isValid() && current_file != boundary_file)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[enum-directive-owner]: source-body "
              "enumerators cross physical files\n");
      ROSE_ABORT();
    }
    boundary_file = current_file;
    const unsigned offset = p_source_manager->getFileOffset(physical_location);
    if (!boundaries.empty() && offset < boundaries.back().first) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[enum-directive-owner]: source-body "
              "enumerator offsets are retrograde\n");
      ROSE_ABORT();
    }
    boundaries.emplace_back(offset, enumerator);
  }

  for (RecordedDirectiveRef directive :
       recordedDirectivesWithin(source_range, /*allow_at_interval_start=*/false,
                                "external-enumerator-definition")) {
    const auto position =
        p_preprocessor_record_positions.find(directive.preprocessing_info);
    if (position == p_preprocessor_record_positions.end() ||
        (boundary_file.isValid() &&
         position->second.file_id != boundary_file)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[enum-directive-owner]: directive has "
              "no exact enum-body position\n");
      ROSE_ABORT();
    }
    SgInitializedName *following_enumerator = nullptr;
    for (const auto &[offset, enumerator] : boundaries) {
      if (offset > position->second.file_offset) {
        following_enumerator = enumerator;
        break;
      }
    }
    transferRecordedDirectiveToBoundary(
        directive, enum_declaration,
        following_enumerator != nullptr
            ? static_cast<SgLocatedNode *>(following_enumerator)
            : static_cast<SgLocatedNode *>(enum_declaration),
        following_enumerator != nullptr ? PreprocessingInfo::before
                                        : PreprocessingInfo::inside,
        /*allow_at_interval_start=*/false, "external-enumerator-definition");
  }
}

void SagePreprocessorRecord::transferDirectivesToFunctionBodyBoundary(
    clang::SourceRange source_range,
    SgFunctionDeclaration *function_declaration,
    SgFunctionDefinition *function_definition) {
  if (function_declaration == nullptr || function_definition == nullptr ||
      function_declaration->get_definition() != function_definition ||
      function_definition->get_declaration() != function_declaration ||
      function_definition->get_parent() != function_declaration ||
      function_definition->get_body() == nullptr ||
      function_definition->get_body()->get_parent() != function_definition) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[function-directive-owner]: producer has "
            "no reciprocal declaration/definition/body surface\n");
    ROSE_ABORT();
  }
  for (RecordedDirectiveRef directive :
       recordedDirectivesWithin(source_range, /*allow_at_interval_start=*/false,
                                "function-definition-prefix")) {
    transferRecordedDirectiveToBoundary(
        directive, function_declaration, function_definition,
        PreprocessingInfo::before,
        /*allow_at_interval_start=*/false, "function-definition-prefix");
  }
}

void SagePreprocessorRecord::transferDirectivesToDeclarationGroupBoundary(
    clang::SourceRange source_range,
    SgDeclarationGroupStatement *declaration_group,
    SgDeclarationStatement *following_member) {
  if (declaration_group == nullptr || following_member == nullptr ||
      following_member->get_parent() != declaration_group ||
      std::count(declaration_group->get_declarations().begin(),
                 declaration_group->get_declarations().end(),
                 following_member) != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[declaration-group-directive-owner]: "
            "producer has no exact following declaration-group member\n");
    ROSE_ABORT();
  }

  // A variable member's declaration range begins at the group's shared
  // declaration specifier.  Its SgInitializedName is the exact typed
  // declarator occurrence that follows the separator, and the variable
  // unparser reaches that child immediately after emitting the comma.  Using
  // the broad declaration as the boundary would place a source comment after
  // its purported `before` anchor whenever the declarator starts on a later
  // line.
  SgLocatedNode *boundary_owner = following_member;
  if (SgVariableDeclaration *variable =
          isSgVariableDeclaration(following_member)) {
    if (variable->get_variables().size() != 1 ||
        variable->get_variables().front() == nullptr ||
        variable->get_variables().front()->get_parent() != variable) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-group-directive-owner]: "
              "following variable member has no exact initialized-name "
              "boundary\n");
      ROSE_ABORT();
    }
    boundary_owner = variable->get_variables().front();
  }
  for (RecordedDirectiveRef directive :
       recordedDirectivesWithin(source_range, /*allow_at_interval_start=*/false,
                                "declaration-group-separator")) {
    transferRecordedDirectiveToBoundary(
        directive, declaration_group, boundary_owner, PreprocessingInfo::before,
        /*allow_at_interval_start=*/false, "declaration-group-separator");
  }
}

void SagePreprocessorRecord::transferDirectivesToDeclarationGroupTerminator(
    clang::SourceRange source_range,
    SgDeclarationGroupStatement *declaration_group,
    SgDeclarationStatement *final_member) {
  if (declaration_group == nullptr || final_member == nullptr ||
      final_member->get_parent() != declaration_group ||
      declaration_group->get_declarations().empty() ||
      declaration_group->get_declarations().back() != final_member ||
      std::count(declaration_group->get_declarations().begin(),
                 declaration_group->get_declarations().end(),
                 final_member) != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[declaration-group-directive-owner]: "
            "producer has no exact final declaration-group member\n");
    ROSE_ABORT();
  }

  SgLocatedNode *boundary_owner = final_member;
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(final_member)) {
    if (variable->get_variables().size() != 1 ||
        variable->get_variables().front() == nullptr ||
        variable->get_variables().front()->get_parent() != variable) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[declaration-group-directive-owner]: "
              "final variable member has no exact initialized-name "
              "boundary\n");
      ROSE_ABORT();
    }
    boundary_owner = variable->get_variables().front();
  }
  for (RecordedDirectiveRef directive :
       recordedDirectivesWithin(source_range, /*allow_at_interval_start=*/false,
                                "declaration-group-terminator")) {
    transferRecordedDirectiveToBoundary(
        directive, declaration_group, boundary_owner, PreprocessingInfo::after,
        /*allow_at_interval_start=*/false, "declaration-group-terminator");
  }
}

bool SagePreprocessorRecord::sourceRangeContainsSkippedTokens(
    clang::SourceRange source_range) const {
  for (RecordedDirectiveRef directive :
       recordedDirectivesWithin(source_range, /*allow_at_interval_start=*/false,
                                "inactive-declaration-group-tail")) {
    if (directive.preprocessing_info->getTypeOfDirective() ==
        PreprocessingInfo::CSkippedToken) {
      return true;
    }
  }
  return false;
}

void SagePreprocessorRecord::consumeIncludeDirectiveInFlattenedSyntax(
    clang::SourceLocation include_location,
    SgLocatedNode *source_interval_owner, const char *contract) {
  if (p_source_manager == nullptr || include_location.isInvalid() ||
      source_interval_owner == nullptr || contract == nullptr ||
      *contract == '\0') {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[flattened-include-owner]: consumption "
            "requires an exact include, source interval, and producer "
            "contract\n");
    ROSE_ABORT();
  }

  clang::SourceLocation file_location =
      p_source_manager->getFileLoc(include_location);
  if (file_location.isInvalid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[flattened-include-owner]: %s include has "
            "no exact physical source position\n",
            contract);
    ROSE_ABORT();
  }
  clang::FileID file_id = p_source_manager->getFileID(file_location);
  if (!file_id.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[flattened-include-owner]: %s include has "
            "no exact physical file identity\n",
            contract);
    ROSE_ABORT();
  }
  const unsigned file_offset = p_source_manager->getFileOffset(file_location);

  RecordedDirectiveRef match{nullptr, nullptr};
  size_t match_count = 0;
  for (const auto &record : p_preprocessor_record_list) {
    Sg_File_Info *file_info = record.first;
    PreprocessingInfo *info = record.second;
    if (file_info == nullptr || info == nullptr) {
      continue;
    }
    const PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    if (type != PreprocessingInfo::CpreprocessorIncludeDeclaration &&
        type != PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
      continue;
    }
    const auto position = p_preprocessor_record_positions.find(info);
    if (position != p_preprocessor_record_positions.end() &&
        position->second.file_id == file_id &&
        position->second.file_offset == file_offset) {
      match = {file_info, info};
      ++match_count;
    }
  }
  if (match_count != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[flattened-include-owner]: %s expected "
            "one include at physical offset %u, found %zu\n",
            contract, file_offset, match_count);
    ROSE_ABORT();
  }

  Sg_File_Info *interval_start = source_interval_owner->get_startOfConstruct();
  Sg_File_Info *interval_end = source_interval_owner->get_endOfConstruct();
  Sg_File_Info *include_info = match.file_info;
  auto source_less = [](Sg_File_Info *lhs, Sg_File_Info *rhs) {
    return lhs->get_line() < rhs->get_line() ||
           (lhs->get_line() == rhs->get_line() &&
            lhs->get_col() < rhs->get_col());
  };
  if (interval_start == nullptr || interval_end == nullptr ||
      include_info == nullptr || interval_start->get_line() <= 0 ||
      interval_start->get_col() <= 0 || interval_end->get_line() <= 0 ||
      interval_end->get_col() <= 0 || include_info->get_line() <= 0 ||
      include_info->get_col() <= 0 ||
      interval_start->get_physical_file_id() < 0 ||
      interval_end->get_physical_file_id() < 0 ||
      include_info->get_physical_file_id() < 0 ||
      !interval_start->isSameFile(*interval_end) ||
      !interval_start->isSameFile(*include_info) ||
      interval_start->get_physical_file_occurrence_id() !=
          interval_end->get_physical_file_occurrence_id() ||
      interval_start->get_physical_file_occurrence_id() !=
          include_info->get_physical_file_occurrence_id() ||
      !source_less(interval_start, include_info) ||
      !source_less(include_info, interval_end)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[flattened-include-owner]: %s include is "
            "not strictly inside its typed producer interval\n",
            contract);
    ROSE_ABORT();
  }

  PreprocessingInfo *info = match.preprocessing_info;
  if (p_attached_preprocessor_records.find(info) !=
          p_attached_preprocessor_records.end() ||
      p_removed_preprocessor_records.find(info) !=
          p_removed_preprocessor_records.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[flattened-include-owner]: %s include was "
            "already claimed by another AST owner\n",
            contract);
    ROSE_ABORT();
  }

  // The typed producer already owns the flattened semantic payload. Re-emitting
  // the include would duplicate that payload, so consume exactly this record.
  markRecordedDirectiveRemoved(match.file_info, info);
}

void SagePreprocessorRecord::sortRecordedDirectives() {
  compactRemovedRecordedDirectives();
  if (p_preprocessor_record_list_sorted || size() == 0) {
    return;
  }
  ROSE_ASSERT(p_source_manager != nullptr);
  const clang::FileID main_file_id = p_source_manager->getMainFileID();
  using Record = std::pair<Sg_File_Info *, PreprocessingInfo *>;
  struct SortEntry {
    Record record;
    bool is_main_file = false;
    std::string normalized_path;
    unsigned file_offset = 0;
  };

  std::vector<SortEntry> entries;
  entries.reserve(p_preprocessor_record_list.size() -
                  p_preprocessor_record_cursor);
  for (auto record =
           p_preprocessor_record_list.begin() + p_preprocessor_record_cursor;
       record != p_preprocessor_record_list.end(); ++record) {
    if (record->first == nullptr || record->second == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[preprocessor-record-order]: record "
              "has no exact file information or preprocessing node\n");
      ROSE_ABORT();
    }
    auto position = p_preprocessor_record_positions.find(record->second);
    if (position == p_preprocessor_record_positions.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[preprocessor-record-order]: record=%p "
              "has no exact physical source position\n",
              static_cast<void *>(record->second));
      ROSE_ABORT();
    }
    std::string normalized_path = FileHelper::normalizePathIfPossible(
        record->first->get_filenameString());
    if (normalized_path.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[preprocessor-record-order]: record=%p "
              "has an empty physical source path\n",
              static_cast<void *>(record->second));
      ROSE_ABORT();
    }
    entries.push_back({*record, position->second.file_id == main_file_id,
                       std::move(normalized_path),
                       position->second.file_offset});
  }

  std::stable_sort(entries.begin(), entries.end(),
                   [](const SortEntry &lhs, const SortEntry &rhs) {
                     if (lhs.is_main_file != rhs.is_main_file) {
                       return lhs.is_main_file;
                     }
                     if (lhs.normalized_path != rhs.normalized_path) {
                       return lhs.normalized_path < rhs.normalized_path;
                     }
                     return lhs.file_offset < rhs.file_offset;
                   });
  auto destination =
      p_preprocessor_record_list.begin() + p_preprocessor_record_cursor;
  for (const SortEntry &entry : entries) {
    *destination++ = entry.record;
  }
  p_preprocessor_record_list_sorted = true;
}

SagePreprocessorRecord::IncludeOwnership
SagePreprocessorRecord::includeOwnershipForPath(const std::string &path) const {
  auto cached = p_normalized_include_ownership_paths.find(path);
  if (cached == p_normalized_include_ownership_paths.end()) {
    const std::string normalized =
        path.empty() ? std::string()
                     : FileHelper::normalizePathIfPossible(path);
    if (normalized.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: cannot classify "
              "an empty physical path\n");
      ROSE_ABORT();
    }
    cached =
        p_normalized_include_ownership_paths.emplace(path, normalized).first;
  }
  if (cached->second.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: cached physical path "
            "normalization is empty\n");
    ROSE_ABORT();
  }
  auto found = p_include_ownership_paths.find(cached->second);
  if (found == p_include_ownership_paths.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: physical path=%s has "
            "no exact preprocessor ownership producer\n",
            cached->second.c_str());
    ROSE_ABORT();
  }
  return found->second;
}

SagePreprocessorRecord::IncludeOwnership
SagePreprocessorRecord::includeOwnershipForLocation(
    clang::SourceLocation location) const {
  if (p_source_manager == nullptr || !location.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: source location has "
            "no exact source-manager ownership producer\n");
    ROSE_ABORT();
  }

  clang::SourceLocation resolved = location;
  if (resolved.isMacroID()) {
    // Ownership follows the physical file that contributed the declaration
    // token.  Clang's getFileLoc selects the spelling file for a macro-body
    // token and the expansion file for a macro-argument token; blindly taking
    // the spelling location can instead select the pathless scratch buffer
    // used by token pasting.
    resolved = p_source_manager->getFileLoc(resolved);
  }
  if (!resolved.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: source location has "
            "no exact spelling ownership producer\n");
    ROSE_ABORT();
  }
  if (p_source_manager->isWrittenInMainFile(resolved)) {
    return IncludeOwnership::main_file;
  }
  if (p_source_manager->isWrittenInBuiltinFile(resolved) ||
      p_source_manager->isWrittenInCommandLineFile(resolved)) {
    return IncludeOwnership::clang_pseudo_file;
  }

  const clang::FileID file_id = p_source_manager->getFileID(resolved);
  if (!file_id.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: physical source "
            "location has no exact FileID\n");
    ROSE_ABORT();
  }
  const unsigned file_key = file_id.getHashValue();
  auto cached_path = p_normalized_physical_paths_by_file_id.find(file_key);
  if (cached_path == p_normalized_physical_paths_by_file_id.end()) {
    const std::string path =
        FileHelper::normalizePathIfPossible(getFilenameForLocation(resolved));
    if (path.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: physical FileID=%u "
              "has no exact normalized path\n",
              file_key);
      ROSE_ABORT();
    }
    cached_path =
        p_normalized_physical_paths_by_file_id.emplace(file_key, path).first;
  }

  auto ownership = p_include_ownership_paths.find(cached_path->second);
  if (ownership == p_include_ownership_paths.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: physical FileID=%u "
            "path=%s has no exact preprocessor ownership producer\n",
            file_key, cached_path->second.c_str());
    ROSE_ABORT();
  }
  return ownership->second;
}

void SagePreprocessorRecord::recordIncludeOwnershipPath(
    const std::string &path, IncludeOwnership ownership, const char *context) {
  const std::string normalized =
      path.empty() ? std::string() : FileHelper::normalizePathIfPossible(path);
  if (normalized.empty() || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: context=%s cannot "
            "publish an empty physical path\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }
  auto cache_path = [&](const std::string &input) {
    auto cached =
        p_normalized_include_ownership_paths.emplace(input, normalized);
    if (!cached.second && cached.first->second != normalized) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: context=%s input "
              "path=%s has conflicting normalized identities=%s/%s\n",
              context, input.c_str(), cached.first->second.c_str(),
              normalized.c_str());
      ROSE_ABORT();
    }
  };
  cache_path(path);
  cache_path(normalized);
  if (ownership == IncludeOwnership::main_file ||
      ownership == IncludeOwnership::clang_pseudo_file) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: context=%s cannot "
            "publish non-physical ownership=%u through physical path=%s\n",
            context, static_cast<unsigned>(ownership), normalized.c_str());
    ROSE_ABORT();
  }
  auto inserted = p_include_ownership_paths.emplace(normalized, ownership);
  if (!inserted.second && inserted.first->second != ownership) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: context=%s physical "
            "path=%s has conflicting typed ownership=%u/%u\n",
            context, normalized.c_str(),
            static_cast<unsigned>(inserted.first->second),
            static_cast<unsigned>(ownership));
    ROSE_ABORT();
  }
}

void SagePreprocessorRecord::recordExternalOwnershipPath(
    const std::string &path, const char *context) {
  recordIncludeOwnershipPath(path, IncludeOwnership::external, context);
}

void SagePreprocessorRecord::recordFrontendSupportOwnershipPath(
    const std::string &path, const char *context) {
  recordIncludeOwnershipPath(path, IncludeOwnership::frontend_support, context);
}

void SagePreprocessorRecord::recordImportedModuleOwnership(
    const clang::Module *imported_module, const char *context) {
  const std::string module_name =
      imported_module != nullptr
          ? imported_module->getFullModuleName(/*AllowStringLiterals=*/true)
          : std::string();
  if (imported_module == nullptr || module_name.empty() || context == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[module-ownership]: context=%s has no "
            "exact imported module identity\n",
            context != nullptr ? context : "<null>");
    ROSE_ABORT();
  }

  auto pending = p_pending_module_import_names.find(module_name);
  if (pending != p_pending_module_import_names.end()) {
    if (pending->second == 0) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[module-ownership]: module=%s has an "
              "empty pending-import count\n",
              module_name.c_str());
      ROSE_ABORT();
    }
    if (--pending->second == 0) {
      p_pending_module_import_names.erase(pending);
    }
  }

  if (auto ast_file = imported_module->getASTFile()) {
    std::string path = ast_file->getFileEntry().tryGetRealPathName().str();
    if (path.empty()) {
      path = ast_file->getName().str();
    }
    recordExternalOwnershipPath(path, context);
  }
  if (imported_module->isHeaderUnit()) {
    const std::string header_path =
        imported_module->getTopLevelModuleName().str();
    if (header_path.empty() || !FileHelper::fileExists(header_path)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[module-ownership]: header-unit=%s has "
              "no exact physical header path\n",
              module_name.c_str());
      ROSE_ABORT();
    }
    recordExternalOwnershipPath(header_path, context);
  }
}

void SagePreprocessorRecord::publishIncludeOwnership(
    SgSourceFile *source_file) const {
  if (source_file == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: cannot publish to a "
            "null source file\n");
    ROSE_ABORT();
  }
  if (!p_pending_module_import_names.empty()) {
    const auto &pending = *p_pending_module_import_names.begin();
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[module-ownership]: module=%s still has "
            "%zu unresolved explicit import declaration(s)\n",
            pending.first.c_str(), pending.second);
    ROSE_ABORT();
  }

  SgStringList &paths = source_file->get_frontendIncludeOwnershipPathList();
  SgStringList &system_paths =
      source_file->get_frontendSystemIncludeOwnershipPathList();
  SgStringList &external_paths =
      source_file->get_frontendExternalOwnershipPathList();
  std::map<std::string, std::set<PreprocessingInfo *>> resolved_directives;
  paths.clear();
  system_paths.clear();
  external_paths.clear();
  for (const auto &entry : p_include_ownership_paths) {
    const std::string &path = entry.first;
    const std::string normalized = FileHelper::normalizePathIfPossible(path);
    if (normalized.empty() || !FileHelper::fileExists(normalized)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: included path=%s "
              "does not identify an existing physical file\n",
              path.c_str());
      ROSE_ABORT();
    }
    switch (entry.second) {
    case IncludeOwnership::main_file:
    case IncludeOwnership::clang_pseudo_file:
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: physical path=%s "
              "carries non-physical typed ownership=%u\n",
              normalized.c_str(), static_cast<unsigned>(entry.second));
      ROSE_ABORT();
    case IncludeOwnership::application_textual:
      paths.push_back(normalized);
      break;
    case IncludeOwnership::system_textual:
      paths.push_back(normalized);
      system_paths.push_back(normalized);
      external_paths.push_back(normalized);
      break;
    case IncludeOwnership::external:
      external_paths.push_back(normalized);
      break;
    case IncludeOwnership::frontend_support:
      // Driver-injected support headers are physical forced includes.  Preserve
      // both facts on SgSourceFile: they participate in C-family header/system
      // provenance queries, and they remain external mutation dependencies.
      // Collapsing this typed ownership to external-only makes declarations
      // from the support header impossible to classify after Clang teardown.
      paths.push_back(normalized);
      system_paths.push_back(normalized);
      external_paths.push_back(normalized);
      break;
    }
  }

  for (const auto &[included_path, directives] :
       p_resolved_include_directives) {
    const std::string normalized =
        FileHelper::normalizePathIfPossible(included_path);
    if (normalized.empty() || normalized != included_path ||
        !FileHelper::fileExists(normalized) || directives.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[resolved-include-edge]: target=%s has "
              "no exact physical path or owning directives\n",
              included_path.c_str());
      ROSE_ABORT();
    }
    for (PreprocessingInfo *directive : directives) {
      if (directive == nullptr || directive->get_file_info() == nullptr ||
          (directive->getTypeOfDirective() !=
               PreprocessingInfo::CpreprocessorIncludeDeclaration &&
           directive->getTypeOfDirective() !=
               PreprocessingInfo::CpreprocessorIncludeNextDeclaration)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[resolved-include-edge]: target=%s "
                "has an incomplete or non-include owner=%p\n",
                normalized.c_str(), static_cast<void *>(directive));
        ROSE_ABORT();
      }
      resolved_directives[normalized].insert(directive);
    }
  }
  source_file->set_frontendResolvedIncludeDirectivesMap(resolved_directives);
}

bool SagePreprocessorRecord::shouldRecordDirective(
    clang::SourceLocation loc) const {
  if (p_source_manager == nullptr || !loc.isValid()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessor-directive-ownership]: "
            "directive callback has no exact source manager/location\n");
    ROSE_ABORT();
  }
  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[preprocessor-directive-ownership]: "
                    "directive callback has no exact spelling location\n");
    ROSE_ABORT();
  }
  if (!p_record_directive_stream) {
    return false;
  }
  const IncludeOwnership ownership = includeOwnershipForLocation(resolved);
  if (ownership == IncludeOwnership::main_file) {
    return true;
  }
  if (ownership == IncludeOwnership::clang_pseudo_file) {
    return false;
  }
  if (!p_record_header_directives) {
    return false;
  }
  switch (ownership) {
  case IncludeOwnership::application_textual:
  case IncludeOwnership::system_textual:
  case IncludeOwnership::external:
  case IncludeOwnership::frontend_support:
    return true;
  case IncludeOwnership::main_file:
  case IncludeOwnership::clang_pseudo_file:
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessor-directive-ownership]: "
            "ownership=%u escaped its dedicated directive policy\n",
            static_cast<unsigned>(ownership));
    ROSE_ABORT();
  }
  fprintf(stderr,
          "REX_FRONTEND_INVARIANT[preprocessor-directive-ownership]: "
          "unknown ownership=%u\n",
          static_cast<unsigned>(ownership));
  ROSE_ABORT();
}

std::string SagePreprocessorRecord::getFilenameForLocation(
    clang::SourceLocation loc) const {
  if (p_source_manager == nullptr || !loc.isValid()) {
    return std::string();
  }
  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  clang::FileID file_id = p_source_manager->getFileID(resolved);
  std::string file;
  const clang::FileEntry *fileEntry =
      p_source_manager->getFileEntryForID(file_id);
  if (fileEntry) {
    file = fileEntry->tryGetRealPathName().str();
  }
  if (file.empty()) {
    file = p_source_manager->getFilename(resolved).str();
  }
  if ((file.empty() || file == "<built-in>") &&
      p_source_manager->isWrittenInMainFile(resolved)) {
    const clang::FileEntry *main_entry =
        p_source_manager->getFileEntryForID(p_source_manager->getMainFileID());
    if (main_entry) {
      file = main_entry->tryGetRealPathName().str();
    }
  }
  if (file.empty()) {
    clang::PresumedLoc ploc = p_source_manager->getPresumedLoc(resolved);
    if (ploc.isValid()) {
      file = ploc.getFilename();
    }
  }
  return file;
}

std::string
SagePreprocessorRecord::collectDirectiveText(clang::SourceLocation loc) const {
  if (p_source_manager == nullptr || !loc.isValid()) {
    return std::string();
  }
  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    return std::string();
  }

  const char *current = p_source_manager->getCharacterData(resolved);
  if (current == nullptr) {
    return std::string();
  }

  std::string text;
  while (current != nullptr) {
    const char *line_end = current;
    while (*line_end != '\n' && *line_end != '\r' && *line_end != '\0') {
      ++line_end;
    }

    const char *back = line_end;
    while (back > current &&
           std::isspace(static_cast<unsigned char>(*(back - 1)))) {
      --back;
    }
    bool has_continuation = (back > current && *(back - 1) == '\\');

    if (has_continuation) {
      const char *effective_end = back - 1;
      while (effective_end > current &&
             std::isspace(static_cast<unsigned char>(*(effective_end - 1)))) {
        --effective_end;
      }
      text.append(current, effective_end - current);
      text.push_back(' ');
    } else {
      text.append(current, line_end - current);
    }

    if (*line_end == '\0') {
      break;
    }

    if (*line_end == '\r' && *(line_end + 1) == '\n') {
      current = line_end + 2;
    } else {
      current = line_end + 1;
    }

    if (!has_continuation) {
      break;
    }
  }

  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }

  return text;
}

void SagePreprocessorRecord::registerRecordedDirective(
    Sg_File_Info *file_info, PreprocessingInfo *preprocessing_info,
    clang::SourceLocation file_location) {
  if (file_info == nullptr || preprocessing_info == nullptr) {
    return;
  }
  ROSE_ASSERT(p_source_manager != nullptr);
  ROSE_ASSERT(file_location.isValid());
  clang::FileID file_id = p_source_manager->getFileID(file_location);
  ROSE_ASSERT(file_id.isValid());
  const unsigned file_occurrence =
      requirePhysicalFileOccurrence(file_id, "recorded-directive");
  file_info->set_physical_file_occurrence_id(file_occurrence);
  Sg_File_Info *preprocessing_file_info = preprocessing_info->get_file_info();
  if (preprocessing_file_info == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[physical-file-occurrence]: directive=%s "
            "has no source position\n",
            preprocessing_info->getString().c_str());
    ROSE_ABORT();
  }
  preprocessing_file_info->set_physical_file_occurrence_id(file_occurrence);
  const unsigned file_offset = p_source_manager->getFileOffset(file_location);
  const std::string filename = file_info->get_filenameString();

  const RecordedDirectiveRef entry{file_info, preprocessing_info};
  p_preprocessor_records_by_location
      [RecordedDirectiveLocationKey{
           filename, static_cast<unsigned>(std::max(file_info->get_line(), 0)),
           static_cast<unsigned>(std::max(file_info->get_col(), 0)),
           static_cast<int>(preprocessing_info->getTypeOfDirective()),
           file_occurrence}]
          .push_back(entry);
  p_preprocessor_record_positions[preprocessing_info] = {file_id, file_location,
                                                         file_offset};
  auto &physical_records =
      p_preprocessor_records_by_file_offset[file_id.getHashValue()]
                                           [file_offset];
  if (std::any_of(physical_records.begin(), physical_records.end(),
                  [&](const RecordedDirectiveRef &record) {
                    return record.preprocessing_info == preprocessing_info;
                  })) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[recorded-directive-index]: directive=%p "
            "was indexed more than once\n",
            static_cast<void *>(preprocessing_info));
    ROSE_ABORT();
  }
  physical_records.push_back(entry);
  const PreprocessingInfo::DirectiveType directive_type =
      preprocessing_info->getTypeOfDirective();
  if (directive_type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
      directive_type ==
          PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
    auto &includes = p_include_records_by_file_offset[file_id.getHashValue()];
    if (!includes.emplace(file_offset, entry).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[recorded-include-index]: multiple "
              "include directives occupy physical FileID=%u offset=%u\n",
              file_id.getHashValue(), file_offset);
      ROSE_ABORT();
    }
  }

  auto &line_state = p_preprocessor_records_by_line[RecordedDirectiveLineKey{
      filename, static_cast<unsigned>(std::max(file_info->get_line(), 0)),
      file_occurrence}];
  switch (preprocessing_info->getTypeOfDirective()) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
    line_state.comment_records.push_back(entry);
    break;
  default:
    ++line_state.non_comment_count;
    break;
  }
}

void SagePreprocessorRecord::unregisterRecordedDirective(
    Sg_File_Info *file_info, PreprocessingInfo *preprocessing_info) {
  if (file_info == nullptr || preprocessing_info == nullptr) {
    return;
  }

  const auto physical_position =
      p_preprocessor_record_positions.find(preprocessing_info);
  const PreprocessingInfo::DirectiveType directive_type =
      preprocessing_info->getTypeOfDirective();
  if (physical_position != p_preprocessor_record_positions.end() &&
      (directive_type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
       directive_type ==
           PreprocessingInfo::CpreprocessorIncludeNextDeclaration)) {
    const unsigned file_key = physical_position->second.file_id.getHashValue();
    auto file_records = p_include_records_by_file_offset.find(file_key);
    auto record =
        file_records != p_include_records_by_file_offset.end()
            ? file_records->second.find(physical_position->second.file_offset)
            : std::map<unsigned, RecordedDirectiveRef>::iterator{};
    if (file_records == p_include_records_by_file_offset.end() ||
        record == file_records->second.end() ||
        record->second.preprocessing_info != preprocessing_info) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[recorded-include-index]: include=%p "
              "has no exact physical index entry\n",
              static_cast<void *>(preprocessing_info));
      ROSE_ABORT();
    }
    file_records->second.erase(record);
    if (file_records->second.empty()) {
      p_include_records_by_file_offset.erase(file_records);
    }
  }
  if (physical_position != p_preprocessor_record_positions.end()) {
    const unsigned file_key = physical_position->second.file_id.getHashValue();
    auto file_records = p_preprocessor_records_by_file_offset.find(file_key);
    auto offset_records =
        file_records != p_preprocessor_records_by_file_offset.end()
            ? file_records->second.find(physical_position->second.file_offset)
            : std::map<unsigned, std::vector<RecordedDirectiveRef>>::iterator{};
    if (file_records == p_preprocessor_records_by_file_offset.end() ||
        offset_records == file_records->second.end()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[recorded-directive-index]: "
              "directive=%p has no physical index entry\n",
              static_cast<void *>(preprocessing_info));
      ROSE_ABORT();
    }
    auto &records = offset_records->second;
    const size_t old_size = records.size();
    records.erase(std::remove_if(records.begin(), records.end(),
                                 [&](const RecordedDirectiveRef &record) {
                                   return record.preprocessing_info ==
                                          preprocessing_info;
                                 }),
                  records.end());
    if (records.size() + 1 != old_size) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[recorded-directive-index]: "
              "directive=%p does not occur exactly once in its physical "
              "index\n",
              static_cast<void *>(preprocessing_info));
      ROSE_ABORT();
    }
    if (records.empty()) {
      file_records->second.erase(offset_records);
    }
    if (file_records->second.empty()) {
      p_preprocessor_records_by_file_offset.erase(file_records);
    }
  }

  const RecordedDirectiveLocationKey location_key{
      file_info->get_filenameString(),
      static_cast<unsigned>(std::max(file_info->get_line(), 0)),
      static_cast<unsigned>(std::max(file_info->get_col(), 0)),
      static_cast<int>(preprocessing_info->getTypeOfDirective()),
      file_info->get_physical_file_occurrence_id()};
  if (auto location_it = p_preprocessor_records_by_location.find(location_key);
      location_it != p_preprocessor_records_by_location.end()) {
    auto &entries = location_it->second;
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const RecordedDirectiveRef &entry) {
                                   return entry.preprocessing_info ==
                                          preprocessing_info;
                                 }),
                  entries.end());
    if (entries.empty()) {
      p_preprocessor_records_by_location.erase(location_it);
    }
  }

  const RecordedDirectiveLineKey line_key{
      file_info->get_filenameString(),
      static_cast<unsigned>(std::max(file_info->get_line(), 0)),
      file_info->get_physical_file_occurrence_id()};
  if (auto line_it = p_preprocessor_records_by_line.find(line_key);
      line_it != p_preprocessor_records_by_line.end()) {
    auto &line_state = line_it->second;
    switch (preprocessing_info->getTypeOfDirective()) {
    case PreprocessingInfo::C_StyleComment:
    case PreprocessingInfo::CplusplusStyleComment:
    case PreprocessingInfo::FortranStyleComment:
    case PreprocessingInfo::F90StyleComment: {
      auto &comments = line_state.comment_records;
      comments.erase(std::remove_if(comments.begin(), comments.end(),
                                    [&](const RecordedDirectiveRef &entry) {
                                      return entry.preprocessing_info ==
                                             preprocessing_info;
                                    }),
                     comments.end());
      break;
    }
    default:
      if (line_state.non_comment_count > 0) {
        --line_state.non_comment_count;
      }
      break;
    }

    if (line_state.comment_records.empty() &&
        line_state.non_comment_count == 0) {
      p_preprocessor_records_by_line.erase(line_it);
    }
  }

  p_preprocessor_record_positions.erase(preprocessing_info);
}

void SagePreprocessorRecord::markRecordedDirectiveRemoved(
    Sg_File_Info *file_info, PreprocessingInfo *preprocessing_info) {
  if (preprocessing_info == nullptr) {
    return;
  }
  if (!p_removed_preprocessor_records.insert(preprocessing_info).second) {
    return;
  }

  unregisterRecordedDirective(file_info, preprocessing_info);
}

void SagePreprocessorRecord::releaseRecordedDirective(
    std::pair<Sg_File_Info *, PreprocessingInfo *> &record) {
  Sg_File_Info *file_info = record.first;
  PreprocessingInfo *preprocessing_info = record.second;

  if (preprocessing_info != nullptr) {
    unregisterRecordedDirective(file_info, preprocessing_info);
    p_removed_preprocessor_records.erase(preprocessing_info);
    if (p_attached_preprocessor_records.erase(preprocessing_info) == 0) {
      delete preprocessing_info;
    }
  }
  delete file_info;

  record.first = nullptr;
  record.second = nullptr;
}

void SagePreprocessorRecord::compactRemovedRecordedDirectives() {
  if (p_removed_preprocessor_records.empty()) {
    return;
  }
  if (p_preprocessor_record_cursor > 0) {
    const size_t erase_count = std::min(p_preprocessor_record_cursor,
                                        p_preprocessor_record_list.size());
    for (size_t i = 0; i < erase_count; ++i) {
      releaseRecordedDirective(p_preprocessor_record_list[i]);
    }
    p_preprocessor_record_list.erase(p_preprocessor_record_list.begin(),
                                     p_preprocessor_record_list.begin() +
                                         erase_count);
    p_preprocessor_record_cursor = 0;
  }

  for (auto it = p_preprocessor_record_list.begin();
       it != p_preprocessor_record_list.end();) {
    if (it->second == nullptr ||
        p_removed_preprocessor_records.find(it->second) ==
            p_removed_preprocessor_records.end()) {
      ++it;
      continue;
    }

    releaseRecordedDirective(*it);
    it = p_preprocessor_record_list.erase(it);
  }

  p_removed_preprocessor_records.clear();
}

namespace {

clang::SourceLocation
alignDirectiveLocationToHash(clang::SourceManager *source_manager,
                             clang::SourceLocation loc) {
  if (source_manager == nullptr || !loc.isValid()) {
    return clang::SourceLocation();
  }

  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    return clang::SourceLocation();
  }

  clang::SourceLocation file_loc = source_manager->getFileLoc(resolved);
  if (!file_loc.isValid()) {
    return resolved;
  }

  clang::FileID file_id = source_manager->getFileID(file_loc);
  if (file_id.isInvalid()) {
    return resolved;
  }

  auto buffer = source_manager->getBufferDataOrNone(file_id);
  if (!buffer) {
    return resolved;
  }

  unsigned hash_offset = source_manager->getFileOffset(file_loc);
  while (hash_offset > 0 && (*buffer)[hash_offset - 1] != '\n' &&
         (*buffer)[hash_offset - 1] != '\r') {
    --hash_offset;
  }
  while (hash_offset < buffer->size() &&
         ((*buffer)[hash_offset] == ' ' || (*buffer)[hash_offset] == '\t')) {
    ++hash_offset;
  }

  if (hash_offset < buffer->size() && (*buffer)[hash_offset] == '#') {
    return source_manager->getLocForStartOfFile(file_id).getLocWithOffset(
        hash_offset);
  }

  return resolved;
}

bool locationIsAtDirectiveLineStart(clang::SourceManager *source_manager,
                                    clang::SourceLocation loc) {
  if (source_manager == nullptr || !loc.isValid()) {
    return false;
  }

  clang::SourceLocation file_loc = source_manager->getFileLoc(loc);
  if (!file_loc.isValid()) {
    return false;
  }

  clang::FileID file_id = source_manager->getFileID(file_loc);
  if (file_id.isInvalid()) {
    return false;
  }

  auto buffer = source_manager->getBufferDataOrNone(file_id);
  if (!buffer) {
    return false;
  }

  unsigned offset = source_manager->getFileOffset(file_loc);
  if (offset >= buffer->size()) {
    return false;
  }

  unsigned line_start = offset;
  while (line_start > 0 && (*buffer)[line_start - 1] != '\n' &&
         (*buffer)[line_start - 1] != '\r') {
    --line_start;
  }

  unsigned first_nonspace = line_start;
  while (
      first_nonspace < buffer->size() &&
      ((*buffer)[first_nonspace] == ' ' || (*buffer)[first_nonspace] == '\t')) {
    ++first_nonspace;
  }

  return offset == first_nonspace && (*buffer)[offset] == '#';
}

bool lineEndsWithPhysicalDirectiveContinuation(const char *line_begin,
                                               const char *line_end) {
  const char *back = line_end;
  while (back > line_begin &&
         std::isspace(static_cast<unsigned char>(*(back - 1)))) {
    --back;
  }

  if (back > line_begin && *(back - 1) == '\\') {
    return true;
  }

  return back - line_begin >= 3 && *(back - 3) == '?' && *(back - 2) == '?' &&
         *(back - 1) == '/';
}

bool isCommentDirectiveType(PreprocessingInfo::DirectiveType type) {
  switch (type) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
    return true;
  default:
    return false;
  }
}

std::string collectDirectiveTextFromSource(clang::SourceManager *source_manager,
                                           clang::SourceLocation loc) {
  clang::SourceLocation resolved =
      alignDirectiveLocationToHash(source_manager, loc);
  if (source_manager == nullptr || !resolved.isValid()) {
    return std::string();
  }

  const char *current = source_manager->getCharacterData(resolved);
  if (current == nullptr) {
    return std::string();
  }

  std::string text;
  while (current != nullptr) {
    const char *line_end = current;
    while (*line_end != '\n' && *line_end != '\r' && *line_end != '\0') {
      ++line_end;
    }

    text.append(current, line_end - current);
    const bool has_continuation =
        lineEndsWithPhysicalDirectiveContinuation(current, line_end);

    if (*line_end == '\0') {
      break;
    }

    if (*line_end == '\r' && *(line_end + 1) == '\n') {
      text.append(line_end, 2);
      current = line_end + 2;
    } else {
      text.push_back(*line_end);
      current = line_end + 1;
    }

    if (!has_continuation) {
      break;
    }
  }

  return text;
}

} // namespace

PreprocessingInfo *SagePreprocessorRecord::recordDirective(
    clang::SourceLocation loc, PreprocessingInfo::DirectiveType directive_type,
    const std::string &text) {
  if (!shouldRecordDirective(loc)) {
    return nullptr;
  }
  if (p_removed_preprocessor_records.size() > 256) {
    compactRemovedRecordedDirectives();
  }

  auto directive_requires_hash = [](PreprocessingInfo::DirectiveType type) {
    switch (type) {
    case PreprocessingInfo::CpreprocessorIncludeDeclaration:
    case PreprocessingInfo::CpreprocessorIncludeNextDeclaration:
    case PreprocessingInfo::CpreprocessorIfdefDeclaration:
    case PreprocessingInfo::CpreprocessorIfndefDeclaration:
    case PreprocessingInfo::CpreprocessorIfDeclaration:
    case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
    case PreprocessingInfo::CpreprocessorElseDeclaration:
    case PreprocessingInfo::CpreprocessorElifDeclaration:
    case PreprocessingInfo::CpreprocessorEndifDeclaration:
    case PreprocessingInfo::CpreprocessorLineDeclaration:
    case PreprocessingInfo::CpreprocessorWarningDeclaration:
    case PreprocessingInfo::CpreprocessorErrorDeclaration:
    case PreprocessingInfo::CpreprocessorEmptyDeclaration:
    case PreprocessingInfo::CpreprocessorDefineDeclaration:
    case PreprocessingInfo::CpreprocessorUndefDeclaration:
    case PreprocessingInfo::CpreprocessorIdentDeclaration:
      return true;
    default:
      return false;
    }
  };

  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[preprocessor-directive-ownership]: "
                    "recorded directive has no exact spelling location\n");
    ROSE_ABORT();
  }

  clang::SourceLocation file_loc = p_source_manager->getFileLoc(resolved);
  clang::FileID file_id = file_loc.isValid()
                              ? p_source_manager->getFileID(file_loc)
                              : clang::FileID();
  if (directive_requires_hash(directive_type) && file_loc.isValid() &&
      !file_id.isInvalid()) {
    file_loc = alignDirectiveLocationToHash(p_source_manager, file_loc);
    file_id = file_loc.isValid() ? p_source_manager->getFileID(file_loc)
                                 : clang::FileID();
  }
  unsigned directive_offset = (file_loc.isValid() && file_id.isValid())
                                  ? p_source_manager->getFileOffset(file_loc)
                                  : 0;
  bool inside_skipped_range = false;

  // If we already recorded a whole skipped inactive range (CSkippedToken) for a
  // section of the file, suppress any other directive/comment records that land
  // inside that preserved text to avoid duplicates during unparsing.
  if (directive_type != PreprocessingInfo::CSkippedToken &&
      p_source_manager != nullptr && !p_skipped_ranges.empty()) {
    if (file_id.isValid()) {
      for (const SkippedFileRange &range : p_skipped_ranges) {
        if (!range.file_id.isValid() || range.file_id != file_id) {
          continue;
        }
        if (range.begin_offset <= directive_offset &&
            directive_offset < range.end_offset) {
          inside_skipped_range = true;
          break;
        }
      }
      if (inside_skipped_range &&
          !isStructuralConditionalDirective(directive_type)) {
        return nullptr;
      }
    }
  }

  clang::SourceLocation info_loc = file_loc.isValid() ? file_loc : resolved;
  bool inv_begin_line = false;
  bool inv_begin_col = false;
  unsigned ls =
      p_source_manager->getSpellingLineNumber(info_loc, &inv_begin_line);
  unsigned cs =
      p_source_manager->getSpellingColumnNumber(info_loc, &inv_begin_col);
  (void)inv_begin_line;
  (void)inv_begin_col;

  std::string file = getFilenameForLocation(resolved);
  std::string content = text;
  if (!content.empty() && content.back() != '\n') {
    content.push_back('\n');
  }

  if (inside_skipped_range && file_id.isValid() &&
      isStructuralConditionalDirective(directive_type)) {
    unsigned directive_end_offset =
        lineDirectiveEndOffset(p_source_manager, file_id, directive_offset);
    for (auto it = p_preprocessor_record_list.begin();
         it != p_preprocessor_record_list.end();) {
      Sg_File_Info *existing_info = it->first;
      PreprocessingInfo *existing_pp = it->second;
      if (existing_info == nullptr || existing_pp == nullptr ||
          p_removed_preprocessor_records.find(existing_pp) !=
              p_removed_preprocessor_records.end() ||
          existing_pp->getTypeOfDirective() !=
              PreprocessingInfo::CSkippedToken) {
        ++it;
        continue;
      }

      auto position_it = p_preprocessor_record_positions.find(existing_pp);
      if (position_it == p_preprocessor_record_positions.end() ||
          position_it->second.file_id != file_id) {
        ++it;
        continue;
      }
      const unsigned skipped_begin_offset = position_it->second.file_offset;
      const clang::FileID skipped_file_id = position_it->second.file_id;
      unsigned skipped_end_offset =
          skipped_begin_offset + existing_pp->getString().size();
      if (directive_end_offset <= skipped_begin_offset ||
          skipped_end_offset <= directive_offset) {
        ++it;
        continue;
      }

      const std::string skipped_text = existing_pp->getString();
      std::vector<std::pair<unsigned, std::string>> replacements;
      if (skipped_begin_offset < directive_offset) {
        replacements.emplace_back(
            skipped_begin_offset,
            skipped_text.substr(0, directive_offset - skipped_begin_offset));
      }
      if (directive_end_offset < skipped_end_offset) {
        replacements.emplace_back(
            directive_end_offset,
            skipped_text.substr(directive_end_offset - skipped_begin_offset));
      }

      releaseRecordedDirective(*it);
      it = p_preprocessor_record_list.erase(it);

      for (const auto &replacement : replacements) {
        if (replacement.second.empty()) {
          continue;
        }
        clang::SourceLocation replacement_loc =
            p_source_manager->getLocForStartOfFile(file_id).getLocWithOffset(
                replacement.first);
        bool replacement_inv_line = false;
        bool replacement_inv_col = false;
        unsigned replacement_line = p_source_manager->getSpellingLineNumber(
            replacement_loc, &replacement_inv_line);
        unsigned replacement_col = p_source_manager->getSpellingColumnNumber(
            replacement_loc, &replacement_inv_col);
        (void)replacement_inv_line;
        (void)replacement_inv_col;
        Sg_File_Info *replacement_info =
            new Sg_File_Info(file, replacement_line, replacement_col);
        PreprocessingInfo *replacement_pp = new PreprocessingInfo(
            PreprocessingInfo::CSkippedToken, replacement.second, file,
            replacement_line, replacement_col, 0, PreprocessingInfo::before);
        const unsigned replacement_occurrence = requirePhysicalFileOccurrence(
            skipped_file_id, "skipped-token-replacement");
        if (replacement_occurrence == 0 ||
            replacement_pp->get_file_info() == nullptr) {
          fprintf(stderr, "REX_FRONTEND_INVARIANT[physical-file-occurrence]: "
                          "replacement preprocessing record has no exact Clang "
                          "FileID\n");
          ROSE_ABORT();
        }
        replacement_info->set_physical_file_occurrence_id(
            replacement_occurrence);
        replacement_pp->get_file_info()->set_physical_file_occurrence_id(
            replacement_occurrence);
        p_preprocessor_record_list.emplace_back(replacement_info,
                                                replacement_pp);
        registerRecordedDirective(replacement_info, replacement_pp,
                                  replacement_loc);
      }
    }
  }

  if (directive_requires_hash(directive_type)) {
    size_t first_nonspace = content.find_first_not_of(" \t");
    if (first_nonspace != std::string::npos && content[first_nonspace] != '#') {
      content.insert(first_nonspace, "#");
    }
  }

  // Clang can report the same main-file comment more than once in some
  // preprocessing flows. Keep only exact duplicates at the same source point.
  const unsigned file_occurrence =
      requirePhysicalFileOccurrence(file_id, "source-directive");
  const RecordedDirectiveLocationKey location_key{
      file, ls, cs, static_cast<int>(directive_type), file_occurrence};
  if (auto location_it = p_preprocessor_records_by_location.find(location_key);
      location_it != p_preprocessor_records_by_location.end()) {
    for (const RecordedDirectiveRef &existing : location_it->second) {
      if (existing.preprocessing_info != nullptr &&
          existing.preprocessing_info->getString() == content) {
        return existing.preprocessing_info;
      }
    }
  }

  // A trailing comment on the same source line as a preprocessor directive
  // should stay embedded in that directive text, not as a separate comment
  // record.
  const RecordedDirectiveLineKey line_key{file, ls, file_occurrence};
  if (auto line_it = p_preprocessor_records_by_line.find(line_key);
      line_it != p_preprocessor_records_by_line.end()) {
    if (isCommentDirectiveType(directive_type)) {
      if (line_it->second.non_comment_count > 0) {
        return nullptr;
      }
    } else if (!line_it->second.comment_records.empty()) {
      const std::vector<RecordedDirectiveRef> comments_to_remove =
          line_it->second.comment_records;
      for (const RecordedDirectiveRef &comment : comments_to_remove) {
        markRecordedDirectiveRemoved(comment.file_info,
                                     comment.preprocessing_info);
      }
    }
  }

  Sg_File_Info *file_info = new Sg_File_Info(file, ls, cs);
  PreprocessingInfo *preproc_info = new PreprocessingInfo(
      directive_type, content, file, ls, cs, 0, PreprocessingInfo::before);
  if (file_occurrence == 0 || preproc_info->get_file_info() == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[physical-file-occurrence]: directive=%s "
            "has no exact Clang FileID\n",
            content.c_str());
    ROSE_ABORT();
  }
  file_info->set_physical_file_occurrence_id(file_occurrence);
  preproc_info->get_file_info()->set_physical_file_occurrence_id(
      file_occurrence);

  p_preprocessor_record_list.push_back(
      std::pair<Sg_File_Info *, PreprocessingInfo *>(file_info, preproc_info));
  registerRecordedDirective(file_info, preproc_info, file_loc);
  p_preprocessor_record_list_sorted = false;
  return preproc_info;
}

void SagePreprocessorRecord::recordSourceDirective(
    clang::SourceLocation loc,
    PreprocessingInfo::DirectiveType directive_type) {
  if (!shouldRecordDirective(loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, loc);
  if (!text.empty()) {
    recordDirective(loc, directive_type, text);
  }
}

void SagePreprocessorRecord::InclusionDirective(
    clang::SourceLocation HashLoc, const clang::Token &IncludeTok,
    llvm::StringRef FileName, bool IsAngled,
    clang::CharSourceRange FilenameRange, clang::OptionalFileEntryRef File,
    llvm::StringRef SearchPath, llvm::StringRef RelativePath,
    const clang::Module *SuggestedModule, bool ModuleImported,
    clang::SrcMgr::CharacteristicKind FileType) {
  (void)FilenameRange;
  (void)RelativePath;
  (void)SuggestedModule;

  auto normalize_path = [](const std::string &path) {
    return path.empty() ? std::string()
                        : FileHelper::normalizePathIfPossible(path);
  };

  auto included_path = [&]() {
    std::string path;
    if (File) {
      const clang::FileEntry &file_entry = File->getFileEntry();
      path = file_entry.tryGetRealPathName().str();
      if (path.empty()) {
        path = File->getName().str();
      }
    }
    if (path.empty()) {
      if (llvm::sys::path::is_absolute(FileName)) {
        path = FileName.str();
      } else if (!SearchPath.empty()) {
        llvm::SmallString<512> joined(SearchPath);
        llvm::sys::path::append(joined, FileName);
        path = joined.str().str();
      } else {
        path = FileName.str();
      }
    }
    return normalize_path(path);
  }();
  if (File) {
    if (included_path.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: resolved include=%s "
              "has no physical path\n",
              FileName.str().c_str());
      ROSE_ABORT();
    }
    if (p_source_manager == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: include=%s has no "
              "exact source manager\n",
              FileName.str().c_str());
      ROSE_ABORT();
    }

    IncludeOwnership ownership = IncludeOwnership::external;
    auto configured_support = p_include_ownership_paths.find(included_path);
    if (configured_support != p_include_ownership_paths.end() &&
        configured_support->second == IncludeOwnership::frontend_support) {
      ownership = IncludeOwnership::frontend_support;
    } else if (ModuleImported) {
      ownership = IncludeOwnership::external;
    } else if (FileType != clang::SrcMgr::C_User) {
      ownership = IncludeOwnership::system_textual;
    } else {
      clang::SourceLocation resolved_hash = HashLoc;
      if (resolved_hash.isMacroID()) {
        resolved_hash = p_source_manager->getSpellingLoc(resolved_hash);
      }
      bool application_parent = false;
      if (!resolved_hash.isValid() ||
          p_source_manager->isWrittenInBuiltinFile(resolved_hash) ||
          p_source_manager->isWrittenInCommandLineFile(resolved_hash)) {
        // A user header injected by the driver is an application input.  It has
        // no textual parent include but is still an exact frontend-owned root.
        application_parent = true;
      } else if (p_source_manager->isWrittenInMainFile(resolved_hash)) {
        application_parent = true;
      } else {
        const std::string including_path =
            normalize_path(getFilenameForLocation(resolved_hash));
        application_parent = includeOwnershipForPath(including_path) ==
                             IncludeOwnership::application_textual;
      }
      ownership = application_parent ? IncludeOwnership::application_textual
                                     : IncludeOwnership::external;
    }
    recordIncludeOwnershipPath(included_path, ownership, "InclusionDirective");
  }

  if (!shouldRecordDirective(HashLoc)) {
    return;
  }

  PreprocessingInfo::DirectiveType directive_type =
      PreprocessingInfo::CpreprocessorIncludeDeclaration;
  std::string directive = "#include ";
  clang::tok::PPKeywordKind pp_kind = clang::tok::pp_not_keyword;
  if (const clang::IdentifierInfo *ident = IncludeTok.getIdentifierInfo()) {
    pp_kind = ident->getPPKeywordID();
  }
  if (pp_kind == clang::tok::pp_include_next) {
    directive_type = PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    directive = "#include_next ";
  }

  // Preserve original include spelling (including trailing comments) when
  // available from the main-file source buffer.
  std::string include_text =
      collectDirectiveTextFromSource(p_source_manager, HashLoc);
  if (include_text.empty()) {
    std::string include_target;
    if (IsAngled) {
      include_target = "<" + FileName.str() + ">";
    } else {
      include_target = "\"" + FileName.str() + "\"";
    }
    include_text = directive + include_target;
  }

  PreprocessingInfo *record =
      recordDirective(HashLoc, directive_type, include_text);
  if (File) {
    if (record == nullptr || included_path.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[resolved-include-edge]: active "
              "include=%s has no exact preprocessing record or target\n",
              include_text.c_str());
      ROSE_ABORT();
    }
    p_resolved_include_directives[included_path].insert(record);
  }
}

void SagePreprocessorRecord::FileChanged(
    clang::SourceLocation Loc, FileChangeReason Reason,
    clang::SrcMgr::CharacteristicKind FileType, clang::FileID PrevFID) {
  if (Reason == EnterFile) {
    if (p_source_manager == nullptr || !Loc.isValid()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: entered file has no "
              "exact source-manager location\n");
      ROSE_ABORT();
    }
    if (p_source_manager->isWrittenInMainFile(Loc) ||
        p_source_manager->isWrittenInBuiltinFile(Loc) ||
        p_source_manager->isWrittenInCommandLineFile(Loc)) {
      return;
    }
    const std::string path =
        FileHelper::normalizePathIfPossible(getFilenameForLocation(Loc));
    if (path.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-ownership]: entered textual "
              "file has no exact physical path\n");
      ROSE_ABORT();
    }

    IncludeOwnership ownership = IncludeOwnership::external;
    auto configured_support = p_include_ownership_paths.find(path);
    if (configured_support != p_include_ownership_paths.end() &&
        configured_support->second == IncludeOwnership::frontend_support) {
      ownership = IncludeOwnership::frontend_support;
    } else if (FileType != clang::SrcMgr::C_User) {
      ownership = IncludeOwnership::system_textual;
    } else {
      bool application_parent = !PrevFID.isValid();
      if (PrevFID.isValid()) {
        clang::SourceLocation parent_start =
            p_source_manager->getLocForStartOfFile(PrevFID);
        if (!parent_start.isValid() ||
            p_source_manager->isWrittenInBuiltinFile(parent_start) ||
            p_source_manager->isWrittenInCommandLineFile(parent_start) ||
            p_source_manager->isWrittenInMainFile(parent_start)) {
          application_parent = true;
        } else {
          application_parent =
              includeOwnershipForPath(getFilenameForLocation(parent_start)) ==
              IncludeOwnership::application_textual;
        }
      }
      ownership = application_parent ? IncludeOwnership::application_textual
                                     : IncludeOwnership::external;
    }
    recordIncludeOwnershipPath(path, ownership, "FileChanged:enter-textual");
    return;
  }
  if (Reason != SystemHeaderPragma) {
    return;
  }
  if (p_source_manager == nullptr || !Loc.isValid() ||
      FileType == clang::SrcMgr::C_User) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: system-header pragma "
            "did not publish an exact non-user physical file\n");
    ROSE_ABORT();
  }
  const std::string path =
      FileHelper::normalizePathIfPossible(getFilenameForLocation(Loc));
  if (path.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: system-header pragma "
            "has no exact physical path\n");
    ROSE_ABORT();
  }
  auto found = p_include_ownership_paths.find(path);
  if (found == p_include_ownership_paths.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: system-header pragma "
            "path=%s has no prior textual include producer\n",
            path.c_str());
    ROSE_ABORT();
  }
  if (found->second == IncludeOwnership::frontend_support) {
    return;
  }
  found->second = IncludeOwnership::system_textual;
}

void SagePreprocessorRecord::moduleImport(clang::SourceLocation ImportLoc,
                                          clang::ModuleIdPath Path,
                                          const clang::Module *Imported) {
  if (!ImportLoc.isValid() || Path.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[module-ownership]: explicit module import "
            "has no exact source location or module path\n");
    ROSE_ABORT();
  }

  std::string path_name;
  for (const clang::IdentifierLoc &component : Path) {
    clang::IdentifierInfo *identifier = component.getIdentifierInfo();
    if (identifier == nullptr || !component.getLoc().isValid() ||
        identifier->getName().empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[module-ownership]: explicit module "
              "import has a malformed path component\n");
      ROSE_ABORT();
    }
    if (!path_name.empty()) {
      path_name += ".";
    }
    path_name += identifier->getName().str();
  }

  if (Imported == nullptr) {
    ++p_pending_module_import_names[path_name];
    return;
  }

  const std::string imported_name =
      Imported->getFullModuleName(/*AllowStringLiterals=*/true);
  if (imported_name != path_name) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[module-ownership]: explicit import path=%s "
            "resolved to contradictory module=%s\n",
            path_name.c_str(), imported_name.c_str());
    ROSE_ABORT();
  }
  recordImportedModuleOwnership(Imported, "moduleImport:resolved-module");
}

bool SagePreprocessorRecord::FileNotFound(llvm::StringRef FileName) {
  (void)FileName;
  return false;
}

void SagePreprocessorRecord::EndOfMainFile() {}

void SagePreprocessorRecord::Ident(clang::SourceLocation Loc,
                                   llvm::StringRef Str) {
  (void)Loc;
  (void)Str;
}

void SagePreprocessorRecord::PragmaComment(clang::SourceLocation Loc,
                                           const clang::IdentifierInfo *Kind,
                                           llvm::StringRef Str) {
  (void)Loc;
  (void)Kind;
  (void)Str;
}

void SagePreprocessorRecord::PragmaMessage(
    clang::SourceLocation Loc, llvm::StringRef Namespace,
    clang::PPCallbacks::PragmaMessageKind Kind, llvm::StringRef Str) {
  (void)Loc;
  (void)Namespace;
  (void)Kind;
  (void)Str;
}

void SagePreprocessorRecord::PragmaDiagnosticPush(clang::SourceLocation Loc,
                                                  llvm::StringRef Namespace) {
  (void)Loc;
  (void)Namespace;
}

void SagePreprocessorRecord::PragmaDiagnosticPop(clang::SourceLocation Loc,
                                                 llvm::StringRef Namespace) {
  (void)Loc;
  (void)Namespace;
}

void SagePreprocessorRecord::PragmaDiagnostic(clang::SourceLocation Loc,
                                              llvm::StringRef Namespace,
                                              clang::diag::Severity Severity,
                                              llvm::StringRef Str) {
  (void)Loc;
  (void)Namespace;
  (void)Severity;
  (void)Str;
}

bool SagePreprocessorRecord::HandleComment(clang::Preprocessor &PP,
                                           clang::SourceRange Comment) {
  clang::SourceLocation loc = Comment.getBegin();
  if (!shouldRecordDirective(loc)) {
    return false;
  }

  std::string text;
  if (p_source_manager != nullptr && Comment.isValid()) {
    clang::CharSourceRange char_range =
        clang::CharSourceRange::getTokenRange(Comment);
    bool invalid = false;
    llvm::StringRef source_text = clang::Lexer::getSourceText(
        char_range, *p_source_manager, PP.getLangOpts(), &invalid);
    if (!invalid) {
      text = source_text.str();
    }
  }
  if (text.empty()) {
    text = collectDirectiveText(loc);
  }
  if (text.empty()) {
    return false;
  }
  if (text.rfind("/*", 0) == 0) {
    size_t end_comment = text.rfind("*/");
    if (end_comment != std::string::npos) {
      text = text.substr(0, end_comment + 2);
    }
  }

  PreprocessingInfo::DirectiveType type = PreprocessingInfo::C_StyleComment;
  llvm::StringRef text_ref(text);
  if (text_ref.starts_with("//")) {
    type = PreprocessingInfo::CplusplusStyleComment;
  } else if (text_ref.starts_with("/*")) {
    type = PreprocessingInfo::C_StyleComment;
  }

  recordDirective(loc, type, text);
  return false;
}

void SagePreprocessorRecord::MacroDefined(const clang::Token &MacroNameTok,
                                          const clang::MacroDirective *MD) {
  clang::SourceLocation loc = MacroNameTok.getLocation();
  bool should_record = shouldRecordDirective(loc);
  if (!should_record) {
    return;
  }

  const clang::MacroInfo *macro_info = nullptr;
  const clang::IdentifierInfo *macro_ident = MacroNameTok.getIdentifierInfo();
  std::string macro_name;
  if (macro_ident != nullptr) {
    macro_name = macro_ident->getName().str();
  }

  if (MD != nullptr) {
    macro_info = MD->getMacroInfo();
    if (macro_info != nullptr) {
      if (macro_info->getDefinitionLoc().isValid()) {
        loc = macro_info->getDefinitionLoc();
        should_record = shouldRecordDirective(loc);
      }
    }
  }

  if (!should_record) {
    return;
  }

  std::string text;
  if (should_record) {
    text = collectDirectiveTextFromSource(p_source_manager, loc);
    if (!text.empty()) {
      size_t first = text.find_first_not_of(" \t\r\n");
      if (first != std::string::npos && text[first] != '#') {
        const std::string keyword = "define";
        if (text.compare(first, keyword.size(), keyword) == 0) {
          text = "#" + text.substr(first);
        } else {
          text = "#define " + text.substr(first);
        }
      }
    }
  }
  if (should_record && text.empty()) {
    std::string name_for_text = macro_name;
    if (name_for_text.empty()) {
      name_for_text = "__macro";
    }
    text = "#define " + name_for_text;
  }

  recordDirective(loc, PreprocessingInfo::CpreprocessorDefineDeclaration, text);
}

void SagePreprocessorRecord::MacroUndefined(
    const clang::Token &MacroNameTok, const clang::MacroDefinition &MD,
    const clang::MacroDirective *Undef) {
  (void)MD;
  const clang::IdentifierInfo *macro_ident = MacroNameTok.getIdentifierInfo();
  clang::SourceLocation loc = MacroNameTok.getLocation();
  if (Undef != nullptr && Undef->getLocation().isValid()) {
    loc = Undef->getLocation();
  }
  const bool should_record = shouldRecordDirective(loc);
  if (!should_record) {
    return;
  }
  std::string text;
  text = collectDirectiveTextFromSource(p_source_manager, loc);
  if (!text.empty()) {
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && text[first] != '#') {
      const std::string keyword = "undef";
      if (text.compare(first, keyword.size(), keyword) == 0) {
        text = "#" + text.substr(first);
      } else {
        text = "#undef " + text.substr(first);
      }
    }
  }
  if (text.empty()) {
    std::string name;
    if (macro_ident != nullptr) {
      name = macro_ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#undef " + name;
  }
  recordDirective(loc, PreprocessingInfo::CpreprocessorUndefDeclaration, text);
}

void SagePreprocessorRecord::Defined(const clang::Token &MacroNameTok,
                                     const clang::MacroDefinition &MD,
                                     clang::SourceRange Range) {
  (void)MacroNameTok;
  (void)MD;
  (void)Range;
}

void SagePreprocessorRecord::SourceRangeSkipped(
    clang::SourceRange Range, clang::SourceLocation EndifLoc) {
  (void)EndifLoc;
  if (p_source_manager == nullptr || p_preprocessor == nullptr ||
      !Range.isValid()) {
    return;
  }
  compactRemovedRecordedDirectives();

  const clang::LangOptions &lang_opts = p_preprocessor->getLangOpts();
  clang::SourceLocation begin = Range.getBegin();
  clang::SourceLocation end = Range.getEnd();
  if (begin.isMacroID()) {
    begin = p_source_manager->getSpellingLoc(begin);
  }
  if (end.isMacroID()) {
    end = p_source_manager->getSpellingLoc(end);
  }
  if (!begin.isValid() || !end.isValid() || !shouldRecordDirective(begin)) {
    return;
  }

  const bool end_at_directive_line_start =
      locationIsAtDirectiveLineStart(p_source_manager, end);
  if (!end_at_directive_line_start) {
    clang::SourceLocation end_of_token =
        clang::Lexer::getLocForEndOfToken(end, 0, *p_source_manager, lang_opts);
    if (end_of_token.isValid()) {
      end = end_of_token;
    }
  }

  clang::CharSourceRange char_range =
      clang::CharSourceRange::getCharRange(begin, end);

  // Preserve the full text of the skipped range so AST-based unparsing can
  // round-trip inactive branches (e.g., `#ifdef` blocks used by tests).
  //
  // Avoid `Lexer::getSourceText()` here because the range can be represented
  // in a variety of forms; use file offsets directly when possible.
  clang::CharSourceRange file_range =
      clang::Lexer::makeFileCharRange(char_range, *p_source_manager, lang_opts);
  clang::SourceLocation file_begin = begin;
  clang::SourceLocation file_end = end;
  if (!file_range.isInvalid()) {
    file_begin = file_range.getBegin();
    file_end = file_range.getEnd();
  } else {
    file_begin = p_source_manager->getFileLoc(begin);
    file_end = p_source_manager->getFileLoc(end);
  }

  if (file_begin.isValid() && file_end.isValid()) {
    clang::FileID file_id = p_source_manager->getFileID(file_begin);
    clang::FileID file_id_end = p_source_manager->getFileID(file_end);
    if (file_id.isValid() && file_id == file_id_end) {
      if (!end_at_directive_line_start) {
        if (clang::SourceLocation after_token =
                clang::Lexer::getLocForEndOfToken(file_end, 0,
                                                  *p_source_manager, lang_opts);
            after_token.isValid()) {
          file_end = after_token;
        }
      }

      auto buffer = p_source_manager->getBufferDataOrNone(file_id);
      if (buffer) {
        unsigned begin_offset = p_source_manager->getFileOffset(file_begin);
        unsigned end_offset = p_source_manager->getFileOffset(file_end);
        if (begin_offset < end_offset && end_offset <= buffer->size()) {
          llvm::StringRef slice =
              buffer->substr(begin_offset, end_offset - begin_offset);
          if (!slice.empty()) {
            p_skipped_ranges.push_back({file_id, begin_offset, end_offset});

            // Prune entries owned by the inactive body, but keep structural
            // conditionals as separate attachments so token ordering preserves
            // `#elif`/`#else`/`#endif` instead of collapsing them into one
            // skipped blob.
            std::vector<std::pair<unsigned, unsigned>> preserved_spans;
            for (auto it = p_preprocessor_record_list.begin();
                 it != p_preprocessor_record_list.end();) {
              Sg_File_Info *existing_info = it->first;
              PreprocessingInfo *existing_pp = it->second;
              if (existing_info == nullptr || existing_pp == nullptr ||
                  p_removed_preprocessor_records.find(existing_pp) !=
                      p_removed_preprocessor_records.end()) {
                ++it;
                continue;
              }
              auto position_it =
                  p_preprocessor_record_positions.find(existing_pp);
              if (position_it == p_preprocessor_record_positions.end() ||
                  position_it->second.file_id != file_id) {
                ++it;
                continue;
              }
              const unsigned existing_offset = position_it->second.file_offset;
              if (begin_offset <= existing_offset &&
                  existing_offset < end_offset) {
                if (isStructuralConditionalDirective(
                        existing_pp->getTypeOfDirective())) {
                  unsigned span_end = lineDirectiveEndOffset(
                      p_source_manager, file_id, existing_offset);
                  preserved_spans.emplace_back(existing_offset,
                                               std::min(span_end, end_offset));
                  ++it;
                  continue;
                }
                releaseRecordedDirective(*it);
                it = p_preprocessor_record_list.erase(it);
                continue;
              }
              ++it;
            }

            std::sort(preserved_spans.begin(), preserved_spans.end());
            std::vector<std::pair<unsigned, unsigned>> merged_spans;
            for (const auto &span : preserved_spans) {
              if (span.first >= span.second) {
                continue;
              }
              if (merged_spans.empty() ||
                  merged_spans.back().second < span.first) {
                merged_spans.push_back(span);
              } else {
                merged_spans.back().second =
                    std::max(merged_spans.back().second, span.second);
              }
            }

            auto record_skipped_gap = [&](unsigned gap_begin,
                                          unsigned gap_end) {
              if (gap_begin >= gap_end) {
                return;
              }
              llvm::StringRef gap_slice =
                  buffer->substr(gap_begin, gap_end - gap_begin);
              if (gap_slice.empty()) {
                return;
              }
              clang::SourceLocation gap_loc =
                  p_source_manager->getLocForStartOfFile(file_id)
                      .getLocWithOffset(gap_begin);
              recordDirective(gap_loc, PreprocessingInfo::CSkippedToken,
                              gap_slice.str());
            };

            unsigned gap_begin = begin_offset;
            for (const auto &span : merged_spans) {
              record_skipped_gap(gap_begin, span.first);
              gap_begin = std::max(gap_begin, span.second);
            }
            record_skipped_gap(gap_begin, end_offset);
            return;
          }
        }
      }
    }
  }

  begin = file_begin;
  end = file_end;
  if (!begin.isValid() || !end.isValid() ||
      !p_source_manager->isBeforeInTranslationUnit(begin, end)) {
    return;
  }

  clang::FileID file_id = p_source_manager->getFileID(begin);
  if (!file_id.isValid()) {
    return;
  }

  clang::SourceLocation cursor = begin;
  unsigned current_line = 0;
  bool line_has_content = false;
  bool expecting_undef = false;
  clang::SourceLocation hash_loc;

  auto is_identifier_named_undef = [&](const clang::Token &token) -> bool {
    if (!token.isAnyIdentifier()) {
      return false;
    }
    if (token.is(clang::tok::identifier)) {
      const clang::IdentifierInfo *ident = token.getIdentifierInfo();
      return ident != nullptr && ident->getName() == "undef";
    }
    bool invalid_spelling = false;
    std::string spelling = clang::Lexer::getSpelling(
        token, *p_source_manager, lang_opts, &invalid_spelling);
    return !invalid_spelling && spelling == "undef";
  };

  while (cursor.isValid() &&
         p_source_manager->isBeforeInTranslationUnit(cursor, end)) {
    clang::Token token;
    if (clang::Lexer::getRawToken(cursor, token, *p_source_manager, lang_opts,
                                  /*IgnoreWhiteSpace=*/true)) {
      break;
    }

    if (token.is(clang::tok::eof) || token.getLength() == 0) {
      break;
    }

    clang::SourceLocation token_loc = token.getLocation();
    if (!token_loc.isValid() ||
        !p_source_manager->isBeforeInTranslationUnit(token_loc, end)) {
      break;
    }

    clang::FileID token_file_id = p_source_manager->getFileID(token_loc);
    if (token_file_id != file_id) {
      cursor = token_loc.getLocWithOffset(token.getLength());
      continue;
    }

    bool invalid_line = false;
    unsigned token_line =
        p_source_manager->getSpellingLineNumber(token_loc, &invalid_line);
    if (invalid_line || token_line == 0) {
      cursor = token_loc.getLocWithOffset(token.getLength());
      continue;
    }

    if (token_line != current_line) {
      current_line = token_line;
      line_has_content = false;
      expecting_undef = false;
      hash_loc = clang::SourceLocation();
    }

    if (token.is(clang::tok::comment)) {
      cursor = token_loc.getLocWithOffset(token.getLength());
      continue;
    }

    if (!line_has_content) {
      line_has_content = true;
      if (token.is(clang::tok::hash)) {
        expecting_undef = true;
        hash_loc = token_loc;
      }
    } else if (expecting_undef) {
      if (is_identifier_named_undef(token)) {
        std::string directive_text =
            collectDirectiveTextFromSource(p_source_manager, hash_loc);
        if (!directive_text.empty()) {
          recordDirective(hash_loc,
                          PreprocessingInfo::CpreprocessorUndefDeclaration,
                          directive_text);
        }
      }
      expecting_undef = false;
    }

    cursor = token_loc.getLocWithOffset(token.getLength());
  }
}

void SagePreprocessorRecord::If(
    clang::SourceLocation Loc, clang::SourceRange ConditionRange,
    clang::PPCallbacks::ConditionValueKind ConditionValue) {
  (void)ConditionRange;
  (void)ConditionValue;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, Loc);
  if (text.empty()) {
    text = "#if 0";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorIfDeclaration, text);
}

void SagePreprocessorRecord::Elif(
    clang::SourceLocation Loc, clang::SourceRange ConditionRange,
    clang::PPCallbacks::ConditionValueKind ConditionValue,
    clang::SourceLocation IfLoc) {
  (void)ConditionRange;
  (void)ConditionValue;
  (void)IfLoc;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, Loc);
  if (text.empty()) {
    text = "#elif 0";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorElifDeclaration, text);
}

void SagePreprocessorRecord::Ifdef(clang::SourceLocation Loc,
                                   const clang::Token &MacroNameTok,
                                   const clang::MacroDefinition &MD) {
  (void)MD;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, Loc);
  if (text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#ifdef " + name;
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorIfdefDeclaration, text);
}

void SagePreprocessorRecord::Ifndef(clang::SourceLocation Loc,
                                    const clang::Token &MacroNameTok,
                                    const clang::MacroDefinition &MD) {
  (void)MD;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, Loc);
  if (text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#ifndef " + name;
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorIfndefDeclaration, text);
}

void SagePreprocessorRecord::Else(clang::SourceLocation Loc,
                                  clang::SourceLocation IfLoc) {
  (void)IfLoc;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, Loc);
  if (text.empty()) {
    text = "#else";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorElseDeclaration, text);
}

void SagePreprocessorRecord::Endif(clang::SourceLocation Loc,
                                   clang::SourceLocation IfLoc) {
  (void)IfLoc;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveTextFromSource(p_source_manager, Loc);
  if (text.empty()) {
    text = "#endif";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorEndifDeclaration, text);
}

std::pair<Sg_File_Info *, PreprocessingInfo *> SagePreprocessorRecord::top() {
  sortRecordedDirectives();
  ROSE_ASSERT(p_preprocessor_record_cursor < p_preprocessor_record_list.size());
  return p_preprocessor_record_list[p_preprocessor_record_cursor];
}

std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
SagePreprocessorRecord::remainingRecords() const {
  if (p_preprocessor_record_cursor > p_preprocessor_record_list.size()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[preprocessing-recorder]: residual "
                    "record cursor exceeds the recorded directive list\n");
    ROSE_ABORT();
  }
  std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>> records;
  records.reserve(p_preprocessor_record_list.size() -
                  p_preprocessor_record_cursor);
  for (size_t i = p_preprocessor_record_cursor;
       i < p_preprocessor_record_list.size(); ++i) {
    const auto &record = p_preprocessor_record_list[i];
    if (record.first == nullptr || record.second == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[preprocessing-recorder]: residual "
              "record has no exact location and directive pair\n");
      ROSE_ABORT();
    }
    records.push_back(record);
  }
  return records;
}

bool SagePreprocessorRecord::pop() {
  if (p_preprocessor_record_cursor < p_preprocessor_record_list.size()) {
    releaseRecordedDirective(
        p_preprocessor_record_list[p_preprocessor_record_cursor]);
    ++p_preprocessor_record_cursor;
  }
  return size() > 0;
}
