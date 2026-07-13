
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

static bool isSameLineNamespaceClosingComment(SgLocatedNode *node,
                                              Sg_File_Info *comment_location,
                                              const PreprocessingInfo *info) {
  if (!isCommentPreprocessingInfo(info) || comment_location == nullptr) {
    return false;
  }

  SgNamespaceDeclarationStatement *namespace_decl =
      isSgNamespaceDeclarationStatement(node);
  if (namespace_decl == nullptr) {
    return false;
  }

  Sg_File_Info *namespace_end = namespaceDeclarationSyntaxEnd(namespace_decl);
  if (namespace_end == nullptr || namespace_end->get_line() <= 0 ||
      namespace_end->get_col() <= 0 || comment_location->get_line() <= 0 ||
      comment_location->get_col() <= 0 ||
      namespace_end->get_physical_file_id() < 0 ||
      comment_location->get_physical_file_id() < 0 ||
      !namespace_end->isSameFile(*comment_location)) {
    return false;
  }

  return namespace_end->get_line() == comment_location->get_line() &&
         namespace_end->get_col() < comment_location->get_col();
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
          !definition_end->isSameFile(*record_start)) {
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

static bool isSemanticNonLexicalDeclarationSubtree(SgNode *node) {
  if (node == nullptr) {
    return false;
  }

  std::unordered_set<SgNode *> visited;
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
      return true;
    }
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
      !start->isSameFile(*end)) {
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
    llvm::StringRef dir_ref(llvm_dir);
    if (dir_ref.ends_with(cmake_suffix) ||
        dir_ref.ends_with(cmake_suffix_version)) {
      llvm::StringRef root_ref = llvm::sys::path::parent_path(dir_ref);
      root_ref = llvm::sys::path::parent_path(root_ref);
      root_ref = llvm::sys::path::parent_path(root_ref);
      return root_ref.str();
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
  return 0; // Success - AST was built
}

void finishSageAST(ClangToSageTranslator &translator) {
  SgGlobal *global_scope = translator.getGlobalScope();

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
    roseClangPhaseTrace("clang_main.finishSageAST.preprocessorTraverse.begin");
    NextPreprocessorToInsert npp(translator);
    std::pair<Sg_File_Info *, PreprocessingInfo *> top =
        translator.preprocessor_top();
    if (top.first == nullptr || top.second == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: recorder "
                   "published an incomplete preprocessing record\n";
      ROSE_ABORT();
    }
    npp.cursor = top.first;
    npp.next_to_insert = top.second;
    npp.candidat = NULL;

    PreprocessorInserter preprocessor_inserter;
    preprocessor_inserter.traverse(global_scope, &npp);
  }

  if (translator.preprocessor_list_size() > 0) {
    if (global_scope == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: residual "
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
      return a->isSameFile(*b);
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
    auto source_file_key = [](Sg_File_Info *info) -> std::string {
      if (info == nullptr || info->get_physical_file_id() < 0) {
        return std::string();
      }
      return std::string("#physical-file-id:") +
             std::to_string(info->get_physical_file_id());
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
    std::map<std::string, FileAnchorIndex> anchor_index;
    std::unordered_set<SgLocatedNode *> indexed_nodes;
    std::unordered_set<SgLocatedNode *> active_nodes;
    size_t next_anchor_order = 0;
    std::function<void(SgLocatedNode *, size_t)> index_anchor_node;
    index_anchor_node = [&](SgLocatedNode *node, size_t depth) {
      if (node == nullptr) {
        return;
      }
      if (isSemanticNonLexicalDeclarationSubtree(node)) {
        return;
      }
      if (!active_nodes.insert(node).second) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-anchor-index]: "
                     "structural cycle at "
                  << node->class_name() << "\n";
        ROSE_ABORT();
      }
      if (!indexed_nodes.insert(node).second) {
        active_nodes.erase(node);
        return;
      }

      Sg_File_Info *start_info = node_start(node);
      Sg_File_Info *end_info = node_end(node);
      SgNamespaceDeclarationStatement *namespace_declaration =
          isSgNamespaceDeclarationStatement(node);
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
          const std::string key = source_file_key(fragment_start);
          if (key.empty() || fragment_start->get_line() <= 0 ||
              fragment_start->get_col() <= 0 || fragment_end->get_line() <= 0 ||
              fragment_end->get_col() <= 0 ||
              !fragment_start->isSameFile(*fragment_end)) {
            std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
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
              {node, start, end, depth, next_anchor_order++});
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
        const std::string key = source_file_key(start_info);
        if (key.empty() || !is_same_file(start_info, end_info) ||
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
            {node, start, end, depth, next_anchor_order++});
      }

      auto index_node = [&](SgLocatedNode *child) {
        index_anchor_node(child, depth + 1);
      };
      auto index_stmt_list = [&](const SgStatementPtrList &statements) {
        for (SgStatement *statement : statements) {
          index_node(isSgLocatedNode(statement));
        }
      };
      auto index_decl_list =
          [&](const SgDeclarationStatementPtrList &declarations) {
            for (SgDeclarationStatement *declaration : declarations) {
              index_node(isSgLocatedNode(declaration));
            }
          };

      if (SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node)) {
        index_node(isSgLocatedNode(declaration->get_definition()));
      }
      if (SgFunctionDefinition *definition = isSgFunctionDefinition(node)) {
        index_node(isSgLocatedNode(definition->get_body()));
      }
      if (SgBasicBlock *block = isSgBasicBlock(node)) {
        index_stmt_list(block->get_statements());
      }
      if (SgForStatement *statement = isSgForStatement(node)) {
        index_node(isSgLocatedNode(statement->get_loop_body()));
      }
      if (SgRangeBasedForStatement *statement =
              isSgRangeBasedForStatement(node)) {
        index_node(isSgLocatedNode(statement->get_loop_body()));
      }
      if (SgWhileStmt *statement = isSgWhileStmt(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
      }
      if (SgDoWhileStmt *statement = isSgDoWhileStmt(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
      }
      if (SgIfStmt *statement = isSgIfStmt(node)) {
        index_node(isSgLocatedNode(statement->get_true_body()));
        index_node(isSgLocatedNode(statement->get_false_body()));
      }
      if (SgSwitchStatement *statement = isSgSwitchStatement(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
      }
      if (SgCaseOptionStmt *statement = isSgCaseOptionStmt(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
      }
      if (SgDefaultOptionStmt *statement = isSgDefaultOptionStmt(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
      }
      if (SgTryStmt *statement = isSgTryStmt(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
        index_stmt_list(statement->get_catch_statement_seq());
      }
      if (SgCatchOptionStmt *statement = isSgCatchOptionStmt(node)) {
        index_node(isSgLocatedNode(statement->get_body()));
      }
      if (SgLabelStatement *statement = isSgLabelStatement(node)) {
        index_node(isSgLocatedNode(statement->get_statement()));
      }
      if (SgNamespaceDeclarationStatement *declaration =
              isSgNamespaceDeclarationStatement(node)) {
        index_node(isSgLocatedNode(declaration->get_definition()));
      }
      if (SgNamespaceDefinitionStatement *definition =
              isSgNamespaceDefinitionStatement(node)) {
        index_decl_list(definition->get_declarations());
      }
      if (SgClassDeclaration *declaration = isSgClassDeclaration(node)) {
        index_node(isSgLocatedNode(declaration->get_definition()));
      }
      if (SgTemplateClassDeclaration *declaration =
              isSgTemplateClassDeclaration(node)) {
        index_node(isSgLocatedNode(declaration->get_definition()));
      }
      if (SgClassDefinition *definition = isSgClassDefinition(node)) {
        index_decl_list(definition->get_members());
      }
      if (SgTemplateClassDefinition *definition =
              isSgTemplateClassDefinition(node)) {
        index_decl_list(definition->get_members());
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
    std::map<std::string, FileBoundaryIndex> boundary_index;
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
        prior = std::prev(prior_end)->node;
      }
      if (prior != nullptr &&
          isSameLineNamespaceClosingComment(prior, cursor, info)) {
        relative_position = PreprocessingInfo::after_syntax;
        return prior;
      }

      SgLocatedNode *following = nullptr;
      auto following_begin = std::lower_bound(
          index.by_start.begin(), index.by_start.end(), position,
          [](const BoundaryCandidate &candidate, uint64_t value) {
            return candidate.start < value;
          });
      if (following_begin != index.by_start.end()) {
        auto following_end = std::upper_bound(
            following_begin, index.by_start.end(), following_begin->start,
            [](uint64_t value, const BoundaryCandidate &candidate) {
              return value < candidate.start;
            });
        following = std::prev(following_end)->node;
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

      auto consider = [&](SgLocatedNode *candidate, SgLocatedNode *&best) {
        if (candidate == nullptr ||
            isSemanticNonLexicalDeclarationSubtree(candidate)) {
          return;
        }
        Sg_File_Info *candidate_start = node_start(candidate);
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

      SgLocatedNode *best = nullptr;
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
          if (isSemanticNonLexicalDeclarationSubtree(target)) {
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
                closing_start->isSameFile(*info->get_file_info())) {
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
                    fragment_start->isSameFile(*info->get_file_info())) {
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
          if (isSemanticNonLexicalDeclarationSubtree(target) ||
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
        if (anchor == nullptr) {
          anchor = find_file_boundary_anchor(cursor, forced_relative_position,
                                             entry.second);
          has_forced_anchor = anchor != nullptr;
        }
        if (anchor == nullptr && cursor != nullptr) {
          const std::string cursor_key = source_file_key(cursor);
          const auto boundaries = boundary_index.find(cursor_key);
          const bool has_lexical_boundary =
              boundaries != boundary_index.end() &&
              !boundaries->second.by_start.empty();
          if (cursor_key.empty() || cursor->get_line() <= 0 ||
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
        if (is_include && anchor != nullptr && cursor != nullptr &&
            isSgScopeStatement(anchor) != nullptr) {
          if (SgLocatedNode *following =
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
                     isSgTemplateClassDeclaration(anchor) != nullptr) {
            if (start != nullptr && end != nullptr &&
                cursor_inside_node(anchor, entry.first) &&
                !location_leq(entry.first, start) &&
                !location_leq(end, entry.first)) {
              entry.second->setRelativePosition(PreprocessingInfo::inside);
            } else if (start != nullptr && location_leq(entry.first, start)) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
            } else if (isSameLineNamespaceClosingComment(anchor, entry.first,
                                                         entry.second)) {
              entry.second->setRelativePosition(
                  PreprocessingInfo::after_syntax);
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

// struct NextPreprocessorToInsert

// NextPreprocessorToInsert::NextPreprocessorToInsert(ClangToSageTranslator &
// translator_) :
NextPreprocessorToInsert::NextPreprocessorToInsert(
    ClangToSageTranslator &translator_)
    : cursor(NULL), candidat(NULL), next_to_insert(NULL),
      translator(translator_) {}

bool NextPreprocessorToInsert::advance() {
  if (!translator.preprocessor_pop()) {
    cursor = NULL;
    next_to_insert = NULL;
    candidat = NULL;
    return false;
  }

  std::pair<Sg_File_Info *, PreprocessingInfo *> next =
      translator.preprocessor_top();
  if (next.first == nullptr || next.second == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: recorder "
                 "published an incomplete successor record\n";
    ROSE_ABORT();
  }
  cursor = next.first;
  next_to_insert = next.second;
  candidat = NULL;
  return true;
}

namespace {

std::string lexicalBoundaryFileKey(Sg_File_Info *info) {
  return info != nullptr && info->get_physical_file_id() >= 0
             ? std::string("#physical-file-id:") +
                   std::to_string(info->get_physical_file_id())
             : std::string();
}

uint64_t lexicalBoundaryCoordinate(Sg_File_Info *info) {
  if (info == nullptr || info->get_line() <= 0 || info->get_col() <= 0) {
    return 0;
  }
  return (static_cast<uint64_t>(info->get_line()) << 32) |
         static_cast<uint64_t>(info->get_col());
}

} // namespace

void PreprocessorInserter::cacheLexicalBoundaries(SgGlobal *global_scope) {
  if (global_scope == cached_lexical_boundary_scope_) {
    return;
  }
  cached_lexical_boundary_scope_ = global_scope;
  cached_lexical_boundaries_.clear();
  if (global_scope == nullptr) {
    std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-boundary]: lexical "
                 "boundary cache requires a global scope\n";
    ROSE_ABORT();
  }

  Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(global_scope, V_SgStatement);
  for (SgNode *node : statements) {
    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == nullptr) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-boundary]: "
                   "statement query returned a non-located node\n";
      ROSE_ABORT();
    }
    if (isSgGlobal(located) != nullptr) {
      continue;
    }
    if (isSemanticNonLexicalDeclarationSubtree(located)) {
      continue;
    }

    SgNamespaceDeclarationStatement *namespace_declaration =
        isSgNamespaceDeclarationStatement(located);
    if (namespace_declaration != nullptr) {
      if (!namespace_declaration->has_source_fragments()) {
        std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                     "lexical namespace preprocessing owner has no typed "
                     "source fragments\n";
        ROSE_ABORT();
      }
      namespace_declaration->validate_source_fragments();
      SgNamespaceSourceFragment *closing =
          namespace_declaration->get_closing_source_fragment();
      if (closing == nullptr ||
          closing->get_parent() != namespace_declaration) {
        std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                     "preprocessing boundary has no exact closing owner\n";
        ROSE_ABORT();
      }
      closing->validate();
      Sg_File_Info *closing_end = closing->get_endOfConstruct();
      const std::string key = lexicalBoundaryFileKey(closing_end);
      const uint64_t coordinate = lexicalBoundaryCoordinate(closing_end);
      if (key.empty() || coordinate == 0) {
        std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                     "preprocessing boundary has no exact source identity\n";
        ROSE_ABORT();
      }
      cached_lexical_boundaries_[key].push_back(
          {namespace_declaration, coordinate});
      continue;
    }

    Sg_File_Info *start = located->get_startOfConstruct();
    Sg_File_Info *end = located->get_endOfConstruct();
    const bool has_start = start != nullptr && start->get_line() > 0;
    const bool has_end = end != nullptr && end->get_line() > 0;
    if (!has_start && !has_end) {
      continue;
    }
    if (!hasExactPhysicalSourceInterval(located)) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-boundary]: source "
                   "node="
                << located->class_name()
                << " has no exact ordered physical interval\n";
      ROSE_ABORT();
    }
    const std::string key = lexicalBoundaryFileKey(end);
    const uint64_t coordinate = lexicalBoundaryCoordinate(end);
    if (key.empty() || coordinate == 0) {
      std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-boundary]: source "
                   "node="
                << located->class_name()
                << " has no exact physical file identity\n";
      ROSE_ABORT();
    }
    cached_lexical_boundaries_[key].push_back({nullptr, coordinate});
  }

  for (auto &[key, boundaries] : cached_lexical_boundaries_) {
    (void)key;
    std::stable_sort(
        boundaries.begin(), boundaries.end(),
        [](const LexicalBoundaryInfo &lhs, const LexicalBoundaryInfo &rhs) {
          return lhs.end < rhs.end;
        });
  }
}

SgNamespaceDeclarationStatement *
PreprocessorInserter::findExactNamespaceClosingCommentOwner(
    SgGlobal *global_scope, Sg_File_Info *comment_location,
    const PreprocessingInfo *info) {
  if (!isCommentPreprocessingInfo(info) || comment_location == nullptr) {
    return nullptr;
  }
  cacheLexicalBoundaries(global_scope);

  const std::string key = lexicalBoundaryFileKey(comment_location);
  const uint64_t comment_coordinate =
      lexicalBoundaryCoordinate(comment_location);
  const auto found = cached_lexical_boundaries_.find(key);
  if (key.empty() || comment_coordinate == 0 ||
      found == cached_lexical_boundaries_.end()) {
    return nullptr;
  }

  const std::vector<LexicalBoundaryInfo> &boundaries = found->second;
  auto prior = std::lower_bound(
      boundaries.begin(), boundaries.end(), comment_coordinate,
      [](const LexicalBoundaryInfo &candidate, uint64_t coordinate) {
        return candidate.end < coordinate;
      });
  if (prior == boundaries.begin()) {
    return nullptr;
  }
  const uint64_t nearest_end = std::prev(prior)->end;
  auto nearest_begin = std::lower_bound(
      boundaries.begin(), prior, nearest_end,
      [](const LexicalBoundaryInfo &candidate, uint64_t coordinate) {
        return candidate.end < coordinate;
      });

  SgNamespaceDeclarationStatement *exact_owner = nullptr;
  for (auto candidate = nearest_begin; candidate != prior; ++candidate) {
    if (candidate->namespace_closing_owner == nullptr ||
        !isSameLineNamespaceClosingComment(candidate->namespace_closing_owner,
                                           comment_location, info)) {
      continue;
    }
    if (exact_owner != nullptr &&
        exact_owner != candidate->namespace_closing_owner) {
      std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                   "one closing comment has multiple namespace fragments at "
                   "the same source coordinate\n";
      ROSE_ABORT();
    }
    exact_owner = candidate->namespace_closing_owner;
  }
  return exact_owner;
}

// class

NextPreprocessorToInsert *PreprocessorInserter::evaluateInheritedAttribute(
    SgNode *astNode, NextPreprocessorToInsert *inheritedValue) {
  // Guard against null after final preprocessor insertion
  if (inheritedValue == NULL)
    return NULL;
  if (inheritedValue->cursor == NULL)
    return NULL;

  SgLocatedNode *position_node = isSgLocatedNode(astNode);
  if (position_node == NULL)
    return inheritedValue;
  if (isSemanticNonLexicalDeclarationSubtree(position_node)) {
    return inheritedValue;
  }

  Sg_File_Info *current_pos = position_node->get_startOfConstruct();
  if (current_pos == NULL) {
    return inheritedValue;
  }

  // Source fragments participate in lexical traversal, but preprocessing is
  // owned by the namespace declaration that emits the fragment.  Preserve the
  // fragment's exact traversal coordinate while using that declaration as the
  // attachment target.
  SgLocatedNode *loc_node = position_node;
  if (SgNamespaceSourceFragment *fragment =
          isSgNamespaceSourceFragment(position_node)) {
    fragment->validate();
    SgNamespaceDeclarationStatement *owner =
        isSgNamespaceDeclarationStatement(fragment->get_parent());
    if (owner == nullptr || !owner->has_source_fragments() ||
        (fragment->get_kind() ==
             SgNamespaceSourceFragment::
                 e_namespace_source_fragment_opening_introducer &&
         owner->get_opening_introducer_source_fragment() != fragment) ||
        (fragment->get_kind() ==
             SgNamespaceSourceFragment::e_namespace_source_fragment_opening &&
         owner->get_opening_source_fragment() != fragment) ||
        (fragment->get_kind() ==
             SgNamespaceSourceFragment::e_namespace_source_fragment_closing &&
         owner->get_closing_source_fragment() != fragment)) {
      std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                   "preprocessing traversal reached fragment without its "
                   "exact namespace owner\n";
      ROSE_ABORT();
    }
    owner->validate_source_fragments();
    // The closing fragment is a structural child of the declaration, so AST
    // preorder reaches it before the namespace definition and its lexical
    // contents.  Treating that early visit as a source cursor boundary steals
    // comments/directives from declarations inside the namespace.  The next
    // real lexical node (or the residual exact-boundary index at EOF) consumes
    // closing-boundary records after the contents have been traversed.
    if (fragment->get_kind() ==
        SgNamespaceSourceFragment::e_namespace_source_fragment_closing) {
      return inheritedValue;
    }
    loc_node = owner;
  }

  auto exact_source_start = [](SgLocatedNode *node) -> Sg_File_Info * {
    return hasExactPhysicalSourceInterval(node) ? node->get_startOfConstruct()
                                                : nullptr;
  };
  auto is_same_file = [](Sg_File_Info *a, Sg_File_Info *b) -> bool {
    if (a == nullptr || b == nullptr || a->get_physical_file_id() < 0 ||
        b->get_physical_file_id() < 0 ||
        a->get_physical_file_occurrence_id() == 0 ||
        b->get_physical_file_occurrence_id() == 0) {
      return false;
    }
    return a->isSameFile(*b) && a->get_physical_file_occurrence_id() ==
                                    b->get_physical_file_occurrence_id();
  };
  auto should_attach_preproc = [&](SgLocatedNode *node) -> bool {
    // Keep in sync with SgLocatedNode::addToAttachedPreprocessingInfo() hard
    // restrictions: these node kinds are not valid preprocessing anchors.
    // SgFunctionParameterList must also be excluded: top-level directives that
    // precede a function declaration belong to the enclosing declaration, not
    // inside the declarator's parameter-list syntax.
    if (node == nullptr || isSgGlobal(node) != nullptr ||
        isSemanticNonLexicalDeclarationSubtree(node) ||
        isSgTypedefSeq(node) != nullptr ||
        isSgCatchStatementSeq(node) != nullptr ||
        isSgCtorInitializerList(node) != nullptr ||
        isSgFunctionParameterList(node) != nullptr) {
      return false;
    }
    if (SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(node)) {
      if (!namespace_declaration->has_source_fragments()) {
        return false;
      }
      namespace_declaration->validate_source_fragments();
      return true;
    }
    return hasExactPhysicalSourceInterval(node);
  };
  // Non-statement nodes may appear earlier in traversal order than later
  // directives/comments. Limit those attachments to the node's own lexical
  // range so conditionals do not migrate into preceding expressions.
  auto should_attach_preproc_target =
      [&](SgLocatedNode *node, Sg_File_Info *directive_info) -> bool {
    auto location_leq_local = [](Sg_File_Info *lhs, Sg_File_Info *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };

    if (node == nullptr || isSemanticNonLexicalDeclarationSubtree(node) ||
        !should_attach_preproc(node)) {
      return false;
    }
    if (directive_info == nullptr) {
      return true;
    }

    Sg_File_Info *info = nullptr;
    if (SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(node)) {
      if (!namespace_declaration->has_source_fragments()) {
        std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                     "namespace preprocessing target has no typed source "
                     "fragments\n";
        ROSE_ABORT();
      }
      namespace_declaration->validate_source_fragments();
      SgNamespaceSourceFragment *introducer =
          namespace_declaration->get_opening_introducer_source_fragment();
      SgNamespaceSourceFragment *opening =
          namespace_declaration->get_opening_source_fragment();
      SgNamespaceSourceFragment *closing =
          namespace_declaration->get_closing_source_fragment();
      ROSE_ASSERT(opening != nullptr && closing != nullptr);
      Sg_File_Info *opening_start = opening->get_startOfConstruct();
      Sg_File_Info *closing_start = closing->get_startOfConstruct();
      Sg_File_Info *introducer_start =
          introducer != nullptr ? introducer->get_startOfConstruct() : nullptr;
      if (introducer_start != nullptr &&
          introducer_start->get_physical_file_id() >= 0 &&
          directive_info->get_physical_file_id() >= 0 &&
          is_same_file(introducer_start, directive_info)) {
        info = introducer_start;
      } else if (opening_start != nullptr &&
                 opening_start->get_physical_file_id() >= 0 &&
                 directive_info->get_physical_file_id() >= 0 &&
                 is_same_file(opening_start, directive_info)) {
        info = opening_start;
      } else if (closing_start != nullptr &&
                 closing_start->get_physical_file_id() >= 0 &&
                 directive_info->get_physical_file_id() >= 0 &&
                 is_same_file(closing_start, directive_info)) {
        info = closing_start;
      } else {
        return false;
      }
    } else {
      info = exact_source_start(node);
    }
    if (info == nullptr || info->get_line() <= 0 || info->get_col() <= 0) {
      return false;
    }
    if (!is_same_file(info, directive_info)) {
      return false;
    }

    if (isSgStatement(node) != nullptr) {
      return true;
    }
    Sg_File_Info *start = node->get_startOfConstruct();
    Sg_File_Info *end = node->get_endOfConstruct();
    return start != nullptr && end != nullptr &&
           is_same_file(start, directive_info) &&
           is_same_file(end, directive_info) &&
           location_leq_local(start, directive_info) &&
           location_leq_local(directive_info, end);
  };
  auto is_include_directive = [](const PreprocessingInfo *info) -> bool {
    if (info == nullptr) {
      return false;
    }
    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
  };
  auto is_comment_directive = [](const PreprocessingInfo *info) -> bool {
    if (info == nullptr) {
      return false;
    }
    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    return type == PreprocessingInfo::C_StyleComment ||
           type == PreprocessingInfo::CplusplusStyleComment ||
           type == PreprocessingInfo::FortranStyleComment ||
           type == PreprocessingInfo::F90StyleComment;
  };
  auto promote_include_target =
      [&](SgLocatedNode *node,
          PreprocessingInfo *directive) -> SgLocatedNode * {
    if (node == nullptr || !is_include_directive(directive)) {
      return node;
    }
    Sg_File_Info *directive_info = directive->get_file_info();
    if (directive_info == nullptr) {
      return node;
    }

    SgLocatedNode *best = node;
    for (SgNode *cursor = node; cursor != nullptr;
         cursor = cursor->get_parent()) {
      SgLocatedNode *located = isSgLocatedNode(cursor);
      if (located == nullptr || isSgStatement(located) == nullptr) {
        continue;
      }
      Sg_File_Info *located_info = exact_source_start(located);
      if (located_info == nullptr || located_info->get_line() == 0) {
        continue;
      }
      if (!is_same_file(located_info, directive_info)) {
        continue;
      }
      if (!should_attach_preproc(located)) {
        continue;
      }
      best = located;
      break;
    }

    return best;
  };
  auto should_attach_include_target =
      [&](SgLocatedNode *node, Sg_File_Info *directive_info) -> bool {
    if (node == nullptr || isSemanticNonLexicalDeclarationSubtree(node) ||
        !should_attach_preproc(node)) {
      return false;
    }
    if (directive_info == nullptr) {
      return true;
    }

    if (SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(node)) {
      namespace_declaration->validate_source_fragments();
      SgNamespaceSourceFragment *fragments[] = {
          namespace_declaration->get_opening_introducer_source_fragment(),
          namespace_declaration->get_opening_source_fragment(),
          namespace_declaration->get_closing_source_fragment()};
      for (SgNamespaceSourceFragment *fragment : fragments) {
        if (fragment == nullptr) {
          continue;
        }
        Sg_File_Info *start = fragment->get_startOfConstruct();
        Sg_File_Info *end = fragment->get_endOfConstruct();
        if (start != nullptr && end != nullptr &&
            is_same_file(start, directive_info) &&
            is_same_file(end, directive_info)) {
          return true;
        }
      }
      return false;
    }
    return is_same_file(node->get_startOfConstruct(), directive_info) &&
           is_same_file(node->get_endOfConstruct(), directive_info);
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
  auto find_exact_enum_body_anchor =
      [&](SgLocatedNode *seed, Sg_File_Info *directive_info,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgLocatedNode * {
    if (seed == nullptr || directive_info == nullptr) {
      return nullptr;
    }

    SgEnumDeclaration *enum_owner = nullptr;
    std::unordered_set<SgNode *> visited;
    for (SgNode *cursor = seed; cursor != nullptr;
         cursor = cursor->get_parent()) {
      if (!visited.insert(cursor).second) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[enum-source-ownership]: parent cycle "
               "while locating a preprocessing owner\n";
        ROSE_ABORT();
      }
      if ((enum_owner = isSgEnumDeclaration(cursor)) != nullptr) {
        break;
      }
    }
    if (enum_owner == nullptr || !hasExactPhysicalSourceInterval(enum_owner)) {
      return nullptr;
    }

    Sg_File_Info *enum_start = enum_owner->get_startOfConstruct();
    Sg_File_Info *enum_end = enum_owner->get_endOfConstruct();
    if (!is_same_file(enum_start, directive_info) ||
        !is_same_file(enum_end, directive_info) ||
        !location_leq(enum_start, directive_info) ||
        !location_leq(directive_info, enum_end)) {
      return nullptr;
    }

    enum_owner->validate_enumerator_source_ownership();
    for (SgInitializedName *enumerator : enum_owner->get_enumerators()) {
      ROSE_ASSERT(enumerator != nullptr);
      if (enumerator->get_enum_constant_source_ownership() !=
          SgInitializedName::e_enum_constant_source_body) {
        continue;
      }
      Sg_File_Info *enumerator_start = enumerator->get_startOfConstruct();
      Sg_File_Info *enumerator_end = enumerator->get_endOfConstruct();
      if (enumerator_start == nullptr || enumerator_end == nullptr ||
          enumerator_start->isCompilerGenerated()) {
        continue;
      }
      if (!is_same_file(enum_start, enumerator_start) ||
          !is_same_file(enum_start, enumerator_end)) {
        std::cerr
            << "REX_FRONTEND_INVARIANT[enum-source-ownership]: direct enum "
               "constant does not share its owner's exact physical source\n";
        ROSE_ABORT();
      }
      if (location_leq(enumerator_start, directive_info) &&
          location_leq(directive_info, enumerator_end)) {
        // This is not an enum-list boundary.  A nested expression/source
        // producer must claim preprocessing that lies inside one constant's
        // own exact syntax range.
        return nullptr;
      }
      const bool follows_directive =
          location_leq(directive_info, enumerator_start) &&
          !location_leq(enumerator_start, directive_info);
      if (follows_directive) {
        relative_position = PreprocessingInfo::before;
        return enumerator;
      }
    }

    // A directive after the last source-spelled constant still belongs inside
    // the exact enum body.  The defining declaration is its typed terminal
    // boundary; canonicalPreprocessingOwner preserves that owner.
    relative_position = PreprocessingInfo::inside;
    return enum_owner;
  };
  auto declaration_group_comment_anchor =
      [&](SgLocatedNode *seed, Sg_File_Info *comment_position,
          PreprocessingInfo *comment,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgDeclarationStatement * {
    if (seed == nullptr || comment_position == nullptr ||
        !is_comment_directive(comment)) {
      return nullptr;
    }

    SgDeclarationGroupStatement *group = nullptr;
    for (SgNode *node = seed; node != nullptr; node = node->get_parent()) {
      if ((group = isSgDeclarationGroupStatement(node)) != nullptr) {
        break;
      }
    }
    if (group == nullptr) {
      return nullptr;
    }
    group->validate();

    Sg_File_Info *group_start = group->get_startOfConstruct();
    Sg_File_Info *group_end = group->get_endOfConstruct();
    if (group_start == nullptr || group_end == nullptr ||
        group_start->get_line() <= 0 || group_end->get_line() <= 0 ||
        !is_same_file(group_start, group_end) ||
        !is_same_file(group_start, comment_position) ||
        !location_leq(group_start, group_end)) {
      std::cerr << "REX_CFE_PREPROCESSING_INVARIANT[declaration-group-owner]: "
                   "typed declaration group has no exact ordered single-file "
                   "range\n";
      ROSE_ABORT();
    }
    const bool comment_strictly_inside_group =
        location_leq(group_start, comment_position) &&
        !location_leq(comment_position, group_start) &&
        location_leq(comment_position, group_end) &&
        !location_leq(group_end, comment_position);
    if (!comment_strictly_inside_group) {
      return nullptr;
    }

    Sg_File_Info *previous_member_end = nullptr;
    const SgDeclarationStatementPtrList &members = group->get_declarations();
    for (size_t index = 0; index < members.size(); ++index) {
      SgDeclarationStatement *member = members[index];
      Sg_File_Info *member_start =
          member != nullptr ? member->get_startOfConstruct() : nullptr;
      Sg_File_Info *member_end =
          member != nullptr ? member->get_endOfConstruct() : nullptr;
      if (member == nullptr || member->get_parent() != group ||
          member_start == nullptr || member_end == nullptr ||
          member_start->get_line() <= 0 || member_end->get_line() <= 0 ||
          !is_same_file(group_start, member_start) ||
          !is_same_file(group_start, member_end) ||
          !location_leq(group_start, member_start) ||
          !location_leq(member_start, member_end) ||
          !location_leq(member_end, group_end) ||
          (previous_member_end != nullptr &&
           !location_leq(previous_member_end, member_end))) {
        std::cerr
            << "REX_CFE_PREPROCESSING_INVARIANT[declaration-group-owner]: "
               "typed declaration group member index="
            << index << " has no exact monotonic source range\n";
        ROSE_ABORT();
      }

      if (previous_member_end != nullptr &&
          location_leq(previous_member_end, comment_position) &&
          !location_leq(comment_position, previous_member_end) &&
          location_leq(comment_position, member_end)) {
        relative_position = PreprocessingInfo::before;
        return member;
      }
      previous_member_end = member_end;
    }

    ROSE_ASSERT(previous_member_end != nullptr);
    if (location_leq(previous_member_end, comment_position) &&
        !location_leq(comment_position, previous_member_end)) {
      relative_position = PreprocessingInfo::after;
      return members.back();
    }
    return nullptr;
  };
  auto directive_inside_node = [&](SgLocatedNode *node,
                                   Sg_File_Info *directive_info) -> bool {
    if (node == nullptr || directive_info == nullptr ||
        !hasExactPhysicalSourceInterval(node)) {
      return false;
    }
    Sg_File_Info *start = node->get_startOfConstruct();
    Sg_File_Info *end = node->get_endOfConstruct();
    if (!is_same_file(start, directive_info) ||
        !is_same_file(end, directive_info)) {
      return false;
    }

    return location_leq(start, directive_info) &&
           location_leq(directive_info, end);
  };
  std::function<SgLocatedNode *(SgLocatedNode *, Sg_File_Info *)>
      include_target_for_inside;
  include_target_for_inside =
      [&](SgLocatedNode *node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (node == nullptr || directive_info == nullptr) {
      return node;
    }

    auto choose_inside = [&](SgLocatedNode *candidate) -> SgLocatedNode * {
      if (candidate == nullptr) {
        return nullptr;
      }
      if (!should_attach_include_target(candidate, directive_info)) {
        return nullptr;
      }
      if (!directive_inside_node(candidate, directive_info)) {
        return nullptr;
      }
      return candidate;
    };

    auto descend_statements =
        [&](const SgStatementPtrList &stmts) -> SgLocatedNode * {
      for (SgStatement *stmt : stmts) {
        SgLocatedNode *candidate = isSgLocatedNode(stmt);
        if (candidate == nullptr) {
          continue;
        }
        if (!directive_inside_node(candidate, directive_info)) {
          continue;
        }
        if (SgLocatedNode *target =
                include_target_for_inside(candidate, directive_info)) {
          return target;
        }
        return candidate;
      }
      return nullptr;
    };

    auto descend_declarations =
        [&](const SgDeclarationStatementPtrList &decls) -> SgLocatedNode * {
      for (SgDeclarationStatement *decl : decls) {
        SgLocatedNode *candidate = isSgLocatedNode(decl);
        if (candidate == nullptr) {
          continue;
        }
        if (!directive_inside_node(candidate, directive_info)) {
          continue;
        }
        if (SgLocatedNode *target =
                include_target_for_inside(candidate, directive_info)) {
          return target;
        }
        return candidate;
      }
      return nullptr;
    };

    if (SgBasicBlock *block = isSgBasicBlock(node)) {
      if (SgLocatedNode *target = descend_statements(block->get_statements())) {
        return target;
      }
    }
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(node)) {
      if (SgLocatedNode *target =
              descend_declarations(ns_def->get_declarations())) {
        return target;
      }
    }
    if (SgClassDefinition *class_def = isSgClassDefinition(node)) {
      if (SgLocatedNode *target =
              descend_declarations(class_def->get_members())) {
        return target;
      }
    }
    if (SgTemplateClassDefinition *class_def =
            isSgTemplateClassDefinition(node)) {
      if (SgLocatedNode *target =
              descend_declarations(class_def->get_members())) {
        return target;
      }
    }

    if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(node)) {
      if (SgFunctionDefinition *defn = func_decl->get_definition()) {
        if (SgBasicBlock *body = defn->get_body()) {
          if (SgLocatedNode *target =
                  include_target_for_inside(body, directive_info)) {
            return target;
          }
          if (SgLocatedNode *target = choose_inside(body)) {
            return target;
          }
        }
        if (SgLocatedNode *target =
                include_target_for_inside(defn, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(defn)) {
          return target;
        }
      }
    }
    if (SgIfStmt *if_stmt = isSgIfStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(if_stmt->get_true_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(if_stmt->get_true_body()))) {
        return target;
      }
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(if_stmt->get_false_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(if_stmt->get_false_body()))) {
        return target;
      }
    }
    if (SgForStatement *for_stmt = isSgForStatement(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(for_stmt->get_loop_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(for_stmt->get_loop_body()))) {
        return target;
      }
    }
    if (SgRangeBasedForStatement *for_stmt = isSgRangeBasedForStatement(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(for_stmt->get_loop_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(for_stmt->get_loop_body()))) {
        return target;
      }
    }
    if (SgWhileStmt *while_stmt = isSgWhileStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(while_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(while_stmt->get_body()))) {
        return target;
      }
    }
    if (SgDoWhileStmt *do_stmt = isSgDoWhileStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(do_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(do_stmt->get_body()))) {
        return target;
      }
    }
    if (SgSwitchStatement *switch_stmt = isSgSwitchStatement(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(switch_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(switch_stmt->get_body()))) {
        return target;
      }
    }
    if (SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(case_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(case_stmt->get_body()))) {
        return target;
      }
    }
    if (SgDefaultOptionStmt *default_stmt = isSgDefaultOptionStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(default_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(default_stmt->get_body()))) {
        return target;
      }
    }
    if (SgCatchOptionStmt *catch_stmt = isSgCatchOptionStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(catch_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(catch_stmt->get_body()))) {
        return target;
      }
    }
    if (SgNamespaceDeclarationStatement *ns_decl =
            isSgNamespaceDeclarationStatement(node)) {
      if (SgNamespaceDefinitionStatement *ns_def = ns_decl->get_definition()) {
        if (SgLocatedNode *target =
                include_target_for_inside(ns_def, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(ns_def)) {
          return target;
        }
      }
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(node)) {
      if (SgClassDefinition *class_def = class_decl->get_definition()) {
        if (SgLocatedNode *target =
                include_target_for_inside(class_def, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(class_def)) {
          return target;
        }
      }
    }
    if (SgTemplateClassDeclaration *class_decl =
            isSgTemplateClassDeclaration(node)) {
      if (SgTemplateClassDefinition *class_def =
              isSgTemplateClassDefinition(class_decl->get_definition())) {
        if (SgLocatedNode *target =
                include_target_for_inside(class_def, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(class_def)) {
          return target;
        }
      }
    }

    return choose_inside(node);
  };
  auto find_prior_include_anchor =
      [&](SgLocatedNode *node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (node == nullptr || directive_info == nullptr) {
      return nullptr;
    }

    auto choose_best = [&](SgLocatedNode *candidate,
                           SgLocatedNode *best) -> SgLocatedNode * {
      if (candidate == nullptr) {
        return best;
      }
      if (!should_attach_include_target(candidate, directive_info)) {
        return best;
      }
      if (!directive_inside_node(candidate, directive_info)) {
        return best;
      }
      return candidate;
    };

    SgLocatedNode *best = nullptr;
    SgNode *parent = node->get_parent();
    if (SgBasicBlock *block = isSgBasicBlock(parent)) {
      for (SgStatement *stmt : block->get_statements()) {
        if (stmt == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(stmt), best);
      }
    } else if (SgNamespaceDefinitionStatement *ns_def =
                   isSgNamespaceDefinitionStatement(parent)) {
      for (SgDeclarationStatement *decl : ns_def->get_declarations()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    } else if (SgGlobal *global = isSgGlobal(parent)) {
      for (SgDeclarationStatement *decl : global->get_declarations()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    } else if (SgClassDefinition *class_def = isSgClassDefinition(parent)) {
      for (SgDeclarationStatement *decl : class_def->get_members()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    } else if (SgTemplateClassDefinition *class_def =
                   isSgTemplateClassDefinition(parent)) {
      for (SgDeclarationStatement *decl : class_def->get_members()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    }

    if (best == nullptr) {
      best = choose_best(isSgLocatedNode(parent), best);
    }
    return best;
  };

  auto find_global_preprocessing_gap_anchor =
      [&](Sg_File_Info *directive_info,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgLocatedNode * {
    SgGlobal *global_scope = inheritedValue->translator.getGlobalScope();
    if (directive_info == nullptr || global_scope == nullptr) {
      return nullptr;
    }

    SgLocatedNode *prior = nullptr;
    Sg_File_Info *prior_end = nullptr;
    SgLocatedNode *following = nullptr;
    Sg_File_Info *following_start = nullptr;
    SgNamespaceDefinitionStatement *following_namespace_closing_owner = nullptr;
    for (SgDeclarationStatement *decl : global_scope->get_declarations()) {
      SgLocatedNode *candidate = isSgLocatedNode(decl);
      if (candidate == nullptr ||
          !should_attach_include_target(candidate, directive_info)) {
        continue;
      }
      Sg_File_Info *start = candidate->get_startOfConstruct();
      Sg_File_Info *end = candidate->get_endOfConstruct();
      bool matched_namespace_closing_fragment = false;
      if ((start == nullptr || !is_same_file(start, directive_info)) &&
          isSgNamespaceDeclarationStatement(candidate) != nullptr) {
        SgNamespaceDeclarationStatement *namespace_declaration =
            isSgNamespaceDeclarationStatement(candidate);
        if (!namespace_declaration->has_source_fragments()) {
          std::cerr << "REX_FRONTEND_INVARIANT[namespace-source-fragment]: "
                       "global preprocessing anchor has no typed source "
                       "fragments\n";
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
          if (fragment_start != nullptr && fragment_end != nullptr &&
              fragment_start->get_physical_file_id() >= 0 &&
              directive_info->get_physical_file_id() >= 0 &&
              fragment_start->isSameFile(*directive_info)) {
            start = fragment_start;
            end = fragment_end;
            matched_namespace_closing_fragment =
                fragment->get_kind() ==
                SgNamespaceSourceFragment::e_namespace_source_fragment_closing;
            break;
          }
        }
      }
      if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
          end->get_line() <= 0 || !is_same_file(start, directive_info) ||
          !is_same_file(end, directive_info)) {
        continue;
      }
      if (!location_leq(start, end)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-gap-owner]: node="
                  << candidate->class_name()
                  << " has a retrograde source interval\n";
        ROSE_ABORT();
      }

      if (location_leq(start, directive_info) &&
          location_leq(directive_info, end)) {
        // The directive is lexically inside this declaration; a nested source
        // owner must claim it.
        return nullptr;
      }

      const bool candidate_precedes = location_leq(end, directive_info) &&
                                      !location_leq(directive_info, end);
      if (candidate_precedes &&
          (prior_end == nullptr || location_leq(prior_end, end))) {
        prior = candidate;
        prior_end = end;
      }

      const bool candidate_follows = location_leq(directive_info, start) &&
                                     !location_leq(start, directive_info);
      if (candidate_follows && (following_start == nullptr ||
                                location_leq(start, following_start))) {
        following = candidate;
        following_start = start;
        following_namespace_closing_owner =
            matched_namespace_closing_fragment
                ? isSgNamespaceDeclarationStatement(candidate)->get_definition()
                : nullptr;
      }
    }

    if (prior != nullptr) {
      relative_position =
          isSameLineNamespaceClosingComment(prior, directive_info,
                                            inheritedValue->next_to_insert)
              ? PreprocessingInfo::after_syntax
              : PreprocessingInfo::after;
      return prior;
    }
    if (following != nullptr) {
      if (following_namespace_closing_owner != nullptr) {
        relative_position = PreprocessingInfo::inside;
        return following_namespace_closing_owner;
      }
      relative_position = PreprocessingInfo::before;
      return following;
    }
    return nullptr;
  };
  auto is_conditional_directive = [](PreprocessingInfo::DirectiveType type) {
    return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
           type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
           type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
           type == PreprocessingInfo::CpreprocessorElifDeclaration ||
           type == PreprocessingInfo::CpreprocessorElseDeclaration ||
           type == PreprocessingInfo::CpreprocessorEndifDeclaration;
  };
  auto is_gap_preserved_directive = [&](PreprocessingInfo::DirectiveType type) {
    // Preserve only trailing conditional structure across statement gaps.
    // Opening directives must stay anchored on the following declaration or
    // scope so later rewrites cannot strand an `#endif` without its `#if`.
    return type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
           type == PreprocessingInfo::CpreprocessorElifDeclaration ||
           type == PreprocessingInfo::CpreprocessorElseDeclaration ||
           type == PreprocessingInfo::CpreprocessorEndifDeclaration;
  };
  auto is_opening_conditional_directive =
      [](PreprocessingInfo::DirectiveType type) {
        return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
               type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
               type == PreprocessingInfo::CpreprocessorIfndefDeclaration;
      };
  auto is_structural_conditional_scope = [](SgLocatedNode *node) {
    return node != nullptr &&
           (isSgBasicBlock(node) != nullptr || isSgIfStmt(node) != nullptr ||
            isSgSwitchStatement(node) != nullptr ||
            isSgCaseOptionStmt(node) != nullptr ||
            isSgDefaultOptionStmt(node) != nullptr ||
            isSgForStatement(node) != nullptr ||
            isSgRangeBasedForStatement(node) != nullptr ||
            isSgWhileStmt(node) != nullptr ||
            isSgDoWhileStmt(node) != nullptr ||
            isSgCatchOptionStmt(node) != nullptr);
  };
  auto find_structural_conditional_anchor =
      [&](SgLocatedNode *candidate, SgLocatedNode *current_node,
          Sg_File_Info *directive_info,
          PreprocessingInfo::DirectiveType directive_type,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgLocatedNode * {
    if (candidate == nullptr || current_node == nullptr ||
        directive_info == nullptr || current_pos == nullptr ||
        !is_conditional_directive(directive_type)) {
      return nullptr;
    }

    const bool is_opening = is_opening_conditional_directive(directive_type);
    for (SgNode *cursor_node = candidate; cursor_node != nullptr;
         cursor_node = cursor_node->get_parent()) {
      SgLocatedNode *anchor = isSgLocatedNode(cursor_node);
      if (anchor == nullptr || !is_structural_conditional_scope(anchor) ||
          !should_attach_preproc_target(anchor, directive_info)) {
        continue;
      }
      if (anchor == current_node ||
          SageInterface::isAncestor(anchor, current_node)) {
        continue;
      }

      Sg_File_Info *start = anchor->get_startOfConstruct();
      Sg_File_Info *end = anchor->get_endOfConstruct();
      if (start == nullptr || end == nullptr ||
          !is_same_file(start, directive_info) ||
          !is_same_file(end, directive_info)) {
        continue;
      }

      if (is_opening) {
        if (location_leq(start, directive_info) &&
            location_leq(directive_info, end)) {
          relative_position = PreprocessingInfo::inside;
          return anchor;
        }
      } else {
        if (location_leq(end, directive_info) &&
            location_leq(directive_info, current_pos) &&
            !location_leq(current_pos, directive_info)) {
          relative_position = PreprocessingInfo::after;
          return anchor;
        }
      }
    }

    return nullptr;
  };
  auto find_exact_gap_anchor_after_candidate =
      [&](SgLocatedNode *candidate, SgLocatedNode *current_node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (candidate == nullptr || current_node == nullptr ||
        directive_info == nullptr || current_pos == nullptr) {
      return nullptr;
    }

    auto get_effective_end = [&](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      if (SgNamespaceDeclarationStatement *namespace_declaration =
              isSgNamespaceDeclarationStatement(node)) {
        return namespaceDeclarationSyntaxEnd(namespace_declaration);
      }
      return hasExactPhysicalSourceInterval(node) ? node->get_endOfConstruct()
                                                  : nullptr;
    };

    SgLocatedNode *exact_anchor = nullptr;
    for (SgNode *cursor_node = candidate; cursor_node != nullptr;
         cursor_node = cursor_node->get_parent()) {
      SgLocatedNode *anchor = isSgLocatedNode(cursor_node);
      if (anchor == nullptr || anchor == current_node ||
          SageInterface::isAncestor(anchor, current_node) ||
          !should_attach_preproc_target(anchor, directive_info)) {
        continue;
      }

      Sg_File_Info *anchor_end = get_effective_end(anchor);
      if (anchor_end == nullptr || anchor_end->get_line() <= 0 ||
          !is_same_file(anchor_end, directive_info) ||
          !is_same_file(current_pos, directive_info)) {
        continue;
      }

      const bool after_anchor = location_leq(anchor_end, directive_info) &&
                                !location_leq(directive_info, anchor_end);
      const bool before_current = location_leq(directive_info, current_pos) &&
                                  !location_leq(current_pos, directive_info);
      if (after_anchor && before_current) {
        if (SgStatement *anchor_stmt = isSgStatement(anchor)) {
          auto next_sibling_statement = [](SgStatement *stmt) -> SgStatement * {
            if (stmt == nullptr) {
              return nullptr;
            }
            if (SgBasicBlock *bb = isSgBasicBlock(stmt->get_parent())) {
              const SgStatementPtrList &stmts = bb->get_statements();
              for (size_t i = 0; i < stmts.size(); ++i) {
                if (stmts[i] == stmt) {
                  return (i + 1 < stmts.size()) ? stmts[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgGlobal *global = isSgGlobal(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &decls =
                  global->get_declarations();
              for (size_t i = 0; i < decls.size(); ++i) {
                if (decls[i] == stmt) {
                  return (i + 1 < decls.size()) ? decls[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgNamespaceDefinitionStatement *ns_def =
                    isSgNamespaceDefinitionStatement(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &decls =
                  ns_def->get_declarations();
              for (size_t i = 0; i < decls.size(); ++i) {
                if (decls[i] == stmt) {
                  return (i + 1 < decls.size()) ? decls[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgClassDefinition *class_def =
                    isSgClassDefinition(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &members =
                  class_def->get_members();
              for (size_t i = 0; i < members.size(); ++i) {
                if (members[i] == stmt) {
                  return (i + 1 < members.size()) ? members[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgTemplateClassDefinition *class_def =
                    isSgTemplateClassDefinition(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &members =
                  class_def->get_members();
              for (size_t i = 0; i < members.size(); ++i) {
                if (members[i] == stmt) {
                  return (i + 1 < members.size()) ? members[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            return nullptr;
          };

          if (SgStatement *next_stmt = next_sibling_statement(anchor_stmt)) {
            Sg_File_Info *next_start = next_stmt->get_startOfConstruct();
            if (hasExactPhysicalSourceInterval(next_stmt) &&
                next_start != nullptr && next_start->get_line() > 0 &&
                is_same_file(next_start, directive_info) &&
                location_leq(directive_info, next_start) &&
                !location_leq(next_start, directive_info)) {
              exact_anchor = anchor;
            }
          } else if (SgLocatedNode *parent =
                         isSgLocatedNode(anchor_stmt->get_parent())) {
            Sg_File_Info *parent_end = get_effective_end(parent);
            if (parent_end != nullptr && parent_end->get_line() > 0 &&
                is_same_file(parent_end, directive_info) &&
                location_leq(directive_info, parent_end)) {
              exact_anchor = anchor;
            }
          }
        }
      }
    }

    return exact_anchor;
  };
  auto find_exact_enclosing_gap_anchor =
      [&](SgLocatedNode *candidate, SgLocatedNode *current_node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (candidate == nullptr || current_node == nullptr ||
        directive_info == nullptr || current_pos == nullptr) {
      return nullptr;
    }

    for (SgNode *cursor_node = candidate; cursor_node != nullptr;
         cursor_node = cursor_node->get_parent()) {
      SgLocatedNode *anchor = isSgLocatedNode(cursor_node);
      if (anchor == nullptr || anchor == current_node ||
          isSgGlobal(anchor) != nullptr ||
          !SageInterface::isAncestor(anchor, current_node) ||
          !should_attach_preproc_target(anchor, directive_info)) {
        continue;
      }

      Sg_File_Info *start = anchor->get_startOfConstruct();
      Sg_File_Info *end = anchor->get_endOfConstruct();
      if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
          end->get_line() <= 0 || !is_same_file(start, directive_info) ||
          !is_same_file(end, directive_info) ||
          !is_same_file(current_pos, directive_info)) {
        continue;
      }
      if (!location_leq(start, directive_info) ||
          location_leq(directive_info, start) ||
          !location_leq(directive_info, end) ||
          !location_leq(directive_info, current_pos) ||
          location_leq(current_pos, directive_info)) {
        continue;
      }

      return anchor;
    }
    return nullptr;
  };
  auto find_exact_traversal_boundary_anchor =
      [&](SgLocatedNode *candidate, SgLocatedNode *current_node,
          Sg_File_Info *directive_info,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgLocatedNode * {
    if (directive_info == nullptr || current_pos == nullptr ||
        !is_same_file(current_pos, directive_info)) {
      return nullptr;
    }

    if (candidate != nullptr &&
        should_attach_preproc_target(candidate, directive_info)) {
      Sg_File_Info *candidate_start = candidate->get_startOfConstruct();
      Sg_File_Info *candidate_end = candidate->get_endOfConstruct();
      if (candidate_start != nullptr && candidate_end != nullptr &&
          candidate_start->get_line() > 0 && candidate_end->get_line() > 0 &&
          is_same_file(candidate_start, directive_info) &&
          is_same_file(candidate_end, directive_info)) {
        if (!location_leq(candidate_start, candidate_end)) {
          std::swap(candidate_start, candidate_end);
        }
        const bool strictly_inside =
            location_leq(candidate_start, directive_info) &&
            !location_leq(directive_info, candidate_start) &&
            location_leq(directive_info, candidate_end) &&
            !location_leq(candidate_end, directive_info);
        if (strictly_inside) {
          relative_position = PreprocessingInfo::inside;
          return candidate;
        }
        const bool strictly_after_candidate =
            location_leq(candidate_end, directive_info) &&
            !location_leq(directive_info, candidate_end);
        const bool strictly_before_current =
            location_leq(directive_info, current_pos) &&
            !location_leq(current_pos, directive_info);
        if (strictly_after_candidate && strictly_before_current) {
          relative_position = PreprocessingInfo::after;
          return candidate;
        }
      }
    }

    if (current_node != nullptr &&
        should_attach_preproc_target(current_node, directive_info)) {
      Sg_File_Info *current_start = current_node->get_startOfConstruct();
      if (current_start != nullptr && current_start->get_line() > 0 &&
          is_same_file(current_start, directive_info) &&
          location_leq(directive_info, current_start) &&
          !location_leq(current_start, directive_info)) {
        relative_position = PreprocessingInfo::before;
        return current_node;
      }
    }
    return nullptr;
  };
  auto find_exact_function_body_boundary =
      [&](SgLocatedNode *seed, Sg_File_Info *directive_info) -> SgBasicBlock * {
    if (seed == nullptr || directive_info == nullptr) {
      return nullptr;
    }
    for (SgNode *cursor = seed; cursor != nullptr;
         cursor = cursor->get_parent()) {
      SgFunctionDeclaration *function = isSgFunctionDeclaration(cursor);
      if (function == nullptr || function->get_definition() == nullptr ||
          function->get_definition()->get_body() == nullptr) {
        continue;
      }
      Sg_File_Info *function_start = function->get_startOfConstruct();
      SgBasicBlock *body = function->get_definition()->get_body();
      Sg_File_Info *body_start = body->get_startOfConstruct();
      if (function_start == nullptr || body_start == nullptr ||
          function_start->get_line() <= 0 || body_start->get_line() <= 0 ||
          !is_same_file(function_start, directive_info) ||
          !is_same_file(body_start, directive_info)) {
        continue;
      }
      const bool after_function_start =
          location_leq(function_start, directive_info) &&
          !location_leq(directive_info, function_start);
      const bool before_body = location_leq(directive_info, body_start) &&
                               !location_leq(body_start, directive_info);
      if (after_function_start && before_body) {
        return body;
      }
    }
    return nullptr;
  };
  auto find_exact_lexical_scope_boundary =
      [&](SgLocatedNode *seed, Sg_File_Info *directive_info,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgLocatedNode * {
    if (seed == nullptr || directive_info == nullptr) {
      return nullptr;
    }

    SgLocatedNode *best_scope = nullptr;
    const SgStatementPtrList *best_statements = nullptr;
    const SgDeclarationStatementPtrList *best_declarations = nullptr;
    std::optional<unsigned int> bestSpan;
    for (SgNode *cursor = seed; cursor != nullptr;
         cursor = cursor->get_parent()) {
      SgLocatedNode *scope = isSgLocatedNode(cursor);
      const SgStatementPtrList *statements = nullptr;
      const SgDeclarationStatementPtrList *declarations = nullptr;
      if (SgBasicBlock *block = isSgBasicBlock(cursor)) {
        statements = &block->get_statements();
      } else if (SgNamespaceDefinitionStatement *ns =
                     isSgNamespaceDefinitionStatement(cursor)) {
        declarations = &ns->get_declarations();
      } else if (SgClassDefinition *class_definition =
                     isSgClassDefinition(cursor)) {
        declarations = &class_definition->get_members();
      } else if (SgTemplateClassDefinition *class_definition =
                     isSgTemplateClassDefinition(cursor)) {
        declarations = &class_definition->get_members();
      } else {
        continue;
      }
      Sg_File_Info *start = scope->get_startOfConstruct();
      Sg_File_Info *end = scope->get_endOfConstruct();
      if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
          end->get_line() <= 0 || !is_same_file(start, directive_info) ||
          !is_same_file(end, directive_info) ||
          !location_leq(start, directive_info) ||
          !location_leq(directive_info, end)) {
        continue;
      }
      if (end->get_line() < start->get_line()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[preprocessing-scope-span]: "
                "scope=%p/%s has retrograde source lines %d..%d\n",
                static_cast<void *>(scope), scope->class_name().c_str(),
                start->get_line(), end->get_line());
        ROSE_ABORT();
      }
      const unsigned int span =
          static_cast<unsigned int>(end->get_line() - start->get_line());
      if (!bestSpan.has_value() || span < *bestSpan) {
        best_scope = scope;
        best_statements = statements;
        best_declarations = declarations;
        bestSpan = span;
      }
    }

    SgGlobal *global_scope = inheritedValue->translator.getGlobalScope();
    if (global_scope != nullptr) {
      if (cached_lexical_conditional_scope_ != global_scope) {
        cached_lexical_conditional_scope_ = global_scope;
        cached_lexical_conditional_scopes_.clear();
        auto add_scope =
            [&](SgLocatedNode *scope, const SgStatementPtrList *statements,
                const SgDeclarationStatementPtrList *declarations) {
              if (scope == nullptr) {
                return;
              }
              Sg_File_Info *start = scope->get_startOfConstruct();
              Sg_File_Info *end = scope->get_endOfConstruct();
              if (start == nullptr || end == nullptr ||
                  start->get_line() <= 0 || end->get_line() <= 0) {
                return;
              }
              cached_lexical_conditional_scopes_.push_back(
                  {scope, statements, declarations, start, end});
            };
        for (SgNode *node :
             NodeQuery::querySubTree(global_scope, V_SgBasicBlock)) {
          if (SgBasicBlock *block = isSgBasicBlock(node)) {
            add_scope(block, &block->get_statements(), nullptr);
          }
        }
        for (SgNode *node : NodeQuery::querySubTree(
                 global_scope, V_SgNamespaceDefinitionStatement)) {
          if (SgNamespaceDefinitionStatement *ns =
                  isSgNamespaceDefinitionStatement(node)) {
            add_scope(ns, nullptr, &ns->get_declarations());
          }
        }
        for (SgNode *node :
             NodeQuery::querySubTree(global_scope, V_SgClassDefinition)) {
          if (SgClassDefinition *class_definition = isSgClassDefinition(node)) {
            add_scope(class_definition, nullptr,
                      &class_definition->get_members());
          }
        }
        for (SgNode *node : NodeQuery::querySubTree(
                 global_scope, V_SgTemplateClassDefinition)) {
          if (SgTemplateClassDefinition *class_definition =
                  isSgTemplateClassDefinition(node)) {
            add_scope(class_definition, nullptr,
                      &class_definition->get_members());
          }
        }
      }

      for (const LexicalConditionalScopeInfo &candidate :
           cached_lexical_conditional_scopes_) {
        if (!is_same_file(candidate.start, directive_info) ||
            !is_same_file(candidate.end, directive_info) ||
            !location_leq(candidate.start, directive_info) ||
            !location_leq(directive_info, candidate.end)) {
          continue;
        }
        if (candidate.end->get_line() < candidate.start->get_line()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[preprocessing-scope-span]: "
                  "cached scope=%p/%s has retrograde source lines %d..%d\n",
                  static_cast<void *>(candidate.scope),
                  candidate.scope->class_name().c_str(),
                  candidate.start->get_line(), candidate.end->get_line());
          ROSE_ABORT();
        }
        const unsigned int span = static_cast<unsigned int>(
            candidate.end->get_line() - candidate.start->get_line());
        if (!bestSpan.has_value() || span < *bestSpan) {
          best_scope = candidate.scope;
          best_statements = candidate.statements;
          best_declarations = candidate.declarations;
          bestSpan = span;
        }
      }
    }
    if (best_scope == nullptr) {
      return nullptr;
    }

    SgLocatedNode *following = nullptr;
    Sg_File_Info *following_start = nullptr;
    bool directive_inside_child = false;
    auto consider_child = [&](SgLocatedNode *child) {
      if (directive_inside_child || child == nullptr ||
          !should_attach_preproc_target(child, directive_info)) {
        return;
      }
      Sg_File_Info *start = child->get_startOfConstruct();
      Sg_File_Info *end = child->get_endOfConstruct();
      if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
          end->get_line() <= 0 || !is_same_file(start, directive_info) ||
          !is_same_file(end, directive_info)) {
        return;
      }
      if (!location_leq(start, end)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-gap-owner]: node="
                  << child->class_name()
                  << " has a retrograde physical source interval\n";
        ROSE_ABORT();
      }
      const bool strictly_inside = location_leq(start, directive_info) &&
                                   !location_leq(directive_info, start) &&
                                   location_leq(directive_info, end) &&
                                   !location_leq(end, directive_info);
      if (strictly_inside) {
        directive_inside_child = true;
        return;
      }
      const bool child_after = location_leq(directive_info, start) &&
                               !location_leq(start, directive_info);
      if (child_after && (following_start == nullptr ||
                          location_leq(start, following_start))) {
        following = child;
        following_start = start;
      }
    };
    if (best_statements != nullptr) {
      for (SgStatement *statement : *best_statements) {
        consider_child(isSgLocatedNode(statement));
      }
    } else {
      for (SgDeclarationStatement *declaration : *best_declarations) {
        consider_child(isSgLocatedNode(declaration));
      }
    }

    if (directive_inside_child) {
      return nullptr;
    }
    if (following != nullptr) {
      relative_position = PreprocessingInfo::before;
      return following;
    }
    // A lexical gap is a boundary in the containing child list, not part of
    // the syntax of the preceding child.  In particular, attaching a trailing
    // conditional to a switch/loop with `after` can emit it before that
    // construct's closing brace.  Terminal gaps therefore belong inside the
    // exact containing scope; non-terminal gaps belong before the following
    // child.  All preprocessing records in one gap consequently share one
    // owner and retain recorder order.
    relative_position = PreprocessingInfo::inside;
    return best_scope;
  };

  if (inheritedValue->cursor != nullptr &&
      *current_pos <= *(inheritedValue->cursor)) {
    bool is_include = is_include_directive(inheritedValue->next_to_insert);
    bool record_candidate = false;
    if (is_include) {
      record_candidate =
          should_attach_include_target(loc_node, inheritedValue->cursor);
    } else {
      record_candidate =
          should_attach_preproc_target(loc_node, inheritedValue->cursor);
    }
    if (record_candidate) {
      inheritedValue->candidat = loc_node;
    }
  }

  bool passed_cursor = *current_pos >= *(inheritedValue->cursor);

  while (passed_cursor) {
    if (inheritedValue->next_to_insert != NULL) {
      SgLocatedNode *attach_node = loc_node;
      PreprocessingInfo::DirectiveType directive_type =
          inheritedValue->next_to_insert->getTypeOfDirective();
      attach_node =
          promote_include_target(attach_node, inheritedValue->next_to_insert);
      bool has_exact_namespace_closing_comment_anchor = false;
      if (SgNamespaceDeclarationStatement *exact_owner =
              findExactNamespaceClosingCommentOwner(
                  inheritedValue->translator.getGlobalScope(),
                  inheritedValue->cursor, inheritedValue->next_to_insert)) {
        attach_node = exact_owner;
        inheritedValue->next_to_insert->setRelativePosition(
            PreprocessingInfo::after_syntax);
        has_exact_namespace_closing_comment_anchor = true;
      }
      bool has_global_lexical_preprocessing_anchor = false;
      bool has_structural_conditional_anchor = false;
      bool has_lexical_conditional_scope_anchor = false;
      bool has_function_body_conditional_anchor = false;
      bool has_lexical_include_scope_anchor = false;
      bool has_declaration_group_comment_anchor = false;
      if (!has_exact_namespace_closing_comment_anchor &&
          inheritedValue->cursor != nullptr) {
        PreprocessingInfo::RelativePositionType global_position =
            PreprocessingInfo::before;
        if (SgLocatedNode *global_anchor = find_global_preprocessing_gap_anchor(
                inheritedValue->cursor, global_position)) {
          // File-level preprocessing is owned by the adjacent declaration in
          // the global lexical list. Semantic traversal can otherwise consume
          // it on a non-output instantiated descendant.
          attach_node = global_anchor;
          inheritedValue->next_to_insert->setRelativePosition(global_position);
          has_global_lexical_preprocessing_anchor = true;
        }
      }
      if (!has_global_lexical_preprocessing_anchor &&
          is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr) {
        PreprocessingInfo::RelativePositionType boundary_position =
            inheritedValue->next_to_insert->getRelativePosition();
        SgLocatedNode *boundary_anchor = find_exact_lexical_scope_boundary(
            inheritedValue->candidat != nullptr ? inheritedValue->candidat
                                                : loc_node,
            inheritedValue->cursor, boundary_position);
        if (boundary_anchor == nullptr &&
            inheritedValue->candidat != loc_node) {
          boundary_anchor = find_exact_lexical_scope_boundary(
              loc_node, inheritedValue->cursor, boundary_position);
        }
        if (boundary_anchor != nullptr) {
          attach_node = boundary_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              boundary_position);
          has_lexical_include_scope_anchor = true;
        }
      }
      if (!has_global_lexical_preprocessing_anchor &&
          !has_lexical_include_scope_anchor &&
          is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node &&
          should_attach_include_target(inheritedValue->candidat,
                                       inheritedValue->cursor)) {
        if (directive_inside_node(inheritedValue->candidat,
                                  inheritedValue->cursor)) {
          attach_node = inheritedValue->candidat;
        }
      }
      const bool is_lexical_conditional_directive =
          is_conditional_directive(directive_type) ||
          directive_type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
          directive_type == PreprocessingInfo::CSkippedToken;
      if (!has_global_lexical_preprocessing_anchor &&
          !has_structural_conditional_anchor &&
          is_lexical_conditional_directive &&
          inheritedValue->cursor != nullptr) {
        SgBasicBlock *body_anchor = find_exact_function_body_boundary(
            inheritedValue->candidat, inheritedValue->cursor);
        if (body_anchor == nullptr) {
          body_anchor = find_exact_function_body_boundary(
              loc_node, inheritedValue->cursor);
        }
        if (body_anchor != nullptr) {
          attach_node = body_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              PreprocessingInfo::before);
          has_function_body_conditional_anchor = true;
        }
      }
      if (!has_global_lexical_preprocessing_anchor &&
          !has_structural_conditional_anchor &&
          !has_function_body_conditional_anchor &&
          is_lexical_conditional_directive &&
          inheritedValue->cursor != nullptr) {
        PreprocessingInfo::RelativePositionType boundary_position =
            inheritedValue->next_to_insert->getRelativePosition();
        SgLocatedNode *boundary_anchor = find_exact_lexical_scope_boundary(
            inheritedValue->candidat != nullptr ? inheritedValue->candidat
                                                : loc_node,
            inheritedValue->cursor, boundary_position);
        if (boundary_anchor == nullptr &&
            inheritedValue->candidat != loc_node) {
          boundary_anchor = find_exact_lexical_scope_boundary(
              loc_node, inheritedValue->cursor, boundary_position);
        }
        if (boundary_anchor != nullptr) {
          attach_node = boundary_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              boundary_position);
          has_lexical_conditional_scope_anchor = true;
        }
      }
      if (!has_exact_namespace_closing_comment_anchor &&
          !has_global_lexical_preprocessing_anchor &&
          is_comment_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr) {
        PreprocessingInfo::RelativePositionType boundary_position =
            inheritedValue->next_to_insert->getRelativePosition();
        SgLocatedNode *boundary_anchor = find_exact_lexical_scope_boundary(
            inheritedValue->candidat != nullptr ? inheritedValue->candidat
                                                : loc_node,
            inheritedValue->cursor, boundary_position);
        if (boundary_anchor == nullptr &&
            inheritedValue->candidat != loc_node) {
          boundary_anchor = find_exact_lexical_scope_boundary(
              loc_node, inheritedValue->cursor, boundary_position);
        }
        if (boundary_anchor != nullptr) {
          // Top-down traversal sees a scope only at its opening token.  A
          // terminal comment before the closing token therefore remains
          // pending until a later source coordinate is visited.  Bind it to
          // the smallest exact lexical scope now; declaration-group,
          // same-line trailing, enum-body, and namespace-closing rules below
          // may still select a more specific typed surface.
          attach_node = boundary_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              boundary_position);
        }
      }
      if (!has_global_lexical_preprocessing_anchor &&
          !has_function_body_conditional_anchor &&
          !has_lexical_conditional_scope_anchor &&
          !is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node) {
        PreprocessingInfo::RelativePositionType relative_position =
            inheritedValue->next_to_insert->getRelativePosition();
        if (SgLocatedNode *boundary_anchor = find_structural_conditional_anchor(
                inheritedValue->candidat, loc_node, inheritedValue->cursor,
                directive_type, relative_position)) {
          attach_node = boundary_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              relative_position);
          has_structural_conditional_anchor = true;
        }
      }
      if (!has_global_lexical_preprocessing_anchor &&
          !has_structural_conditional_anchor &&
          !has_function_body_conditional_anchor &&
          !has_lexical_conditional_scope_anchor &&
          !is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node &&
          is_gap_preserved_directive(directive_type)) {
        SgLocatedNode *gap_anchor = find_exact_gap_anchor_after_candidate(
            inheritedValue->candidat, loc_node, inheritedValue->cursor);
        PreprocessingInfo::RelativePositionType gap_position =
            PreprocessingInfo::after;
        if (gap_anchor == nullptr) {
          gap_anchor = find_exact_traversal_boundary_anchor(
              inheritedValue->candidat, loc_node, inheritedValue->cursor,
              gap_position);
        }
        if (gap_anchor == nullptr) {
          gap_anchor = find_exact_enclosing_gap_anchor(
              inheritedValue->candidat, loc_node, inheritedValue->cursor);
          gap_position = PreprocessingInfo::inside;
        }
        if (gap_anchor == nullptr) {
          std::cerr << "REX_CFE_PREPROCESSING_INVARIANT[gap-owner]: trailing "
                       "conditional directive between AST nodes has no exact "
                       "adjacent owner; directive="
                    << inheritedValue->cursor->get_filenameString() << ":"
                    << inheritedValue->cursor->get_line() << ":"
                    << inheritedValue->cursor->get_col()
                    << " candidate=" << inheritedValue->candidat->class_name()
                    << " current=" << loc_node->class_name() << "\n";
          ROSE_ABORT();
        }
        attach_node = gap_anchor;
        inheritedValue->next_to_insert->setRelativePosition(gap_position);
      }
      if (!has_global_lexical_preprocessing_anchor &&
          !has_lexical_include_scope_anchor &&
          is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr) {
        if (!directive_inside_node(attach_node, inheritedValue->cursor)) {
          if (SgLocatedNode *prior = find_prior_include_anchor(
                  attach_node, inheritedValue->cursor)) {
            attach_node = prior;
          }
        }
        if (SgLocatedNode *inside_target = include_target_for_inside(
                attach_node, inheritedValue->cursor)) {
          attach_node = inside_target;
        }
        if (isSgBasicBlock(attach_node) != nullptr ||
            isSgClassDefinition(attach_node) != nullptr ||
            isSgTemplateClassDefinition(attach_node) != nullptr ||
            isSgNamespaceDefinitionStatement(attach_node) != nullptr) {
          inheritedValue->next_to_insert->setRelativePosition(
              PreprocessingInfo::inside);
        } else if (isSgNamespaceDeclarationStatement(attach_node) != nullptr ||
                   isSgClassDeclaration(attach_node) != nullptr ||
                   isSgTemplateClassDeclaration(attach_node) != nullptr) {
          Sg_File_Info *start = attach_node->get_startOfConstruct();
          Sg_File_Info *end = attach_node->get_endOfConstruct();
          if (start != nullptr && end != nullptr &&
              directive_inside_node(attach_node, inheritedValue->cursor) &&
              location_leq(start, inheritedValue->cursor) &&
              location_leq(inheritedValue->cursor, end) &&
              !location_leq(inheritedValue->cursor, start) &&
              !location_leq(end, inheritedValue->cursor)) {
            inheritedValue->next_to_insert->setRelativePosition(
                PreprocessingInfo::inside);
          }
        }
      }
      if (!has_exact_namespace_closing_comment_anchor &&
          !has_global_lexical_preprocessing_anchor &&
          !is_include_directive(inheritedValue->next_to_insert) &&
          is_comment_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr) {
        PreprocessingInfo::RelativePositionType group_position =
            inheritedValue->next_to_insert->getRelativePosition();
        SgDeclarationStatement *group_anchor = declaration_group_comment_anchor(
            inheritedValue->candidat != nullptr ? inheritedValue->candidat
                                                : loc_node,
            inheritedValue->cursor, inheritedValue->next_to_insert,
            group_position);
        if (group_anchor == nullptr && inheritedValue->candidat != loc_node) {
          group_anchor = declaration_group_comment_anchor(
              loc_node, inheritedValue->cursor, inheritedValue->next_to_insert,
              group_position);
        }
        if (group_anchor != nullptr) {
          attach_node = group_anchor;
          inheritedValue->next_to_insert->setRelativePosition(group_position);
          has_declaration_group_comment_anchor = true;
        }
      }
      if (!has_exact_namespace_closing_comment_anchor &&
          !has_global_lexical_preprocessing_anchor &&
          !has_declaration_group_comment_anchor &&
          !is_include_directive(inheritedValue->next_to_insert) &&
          is_comment_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node) {
        SgLocatedNode *candidate_node = inheritedValue->candidat;
        Sg_File_Info *candidate_end =
            isSgNamespaceDeclarationStatement(candidate_node) != nullptr
                ? namespaceDeclarationSyntaxEnd(
                      isSgNamespaceDeclarationStatement(candidate_node))
                : candidate_node->get_endOfConstruct();
        if (candidate_end != nullptr && candidate_end->get_line() > 0 &&
            is_same_file(candidate_end, inheritedValue->cursor) &&
            candidate_end->get_line() == inheritedValue->cursor->get_line() &&
            candidate_end->get_col() > 0 &&
            candidate_end->get_col() < inheritedValue->cursor->get_col()) {
          attach_node = candidate_node;
          const PreprocessingInfo::RelativePositionType relative_position =
              isSameLineNamespaceClosingComment(candidate_node,
                                                inheritedValue->cursor,
                                                inheritedValue->next_to_insert)
                  ? PreprocessingInfo::after_syntax
                  : PreprocessingInfo::after;
          inheritedValue->next_to_insert->setRelativePosition(
              relative_position);
        }
      }
      bool has_exact_enum_body_anchor = false;
      if (inheritedValue->cursor != nullptr) {
        PreprocessingInfo::RelativePositionType enum_position =
            inheritedValue->next_to_insert->getRelativePosition();
        SgLocatedNode *enum_anchor = find_exact_enum_body_anchor(
            attach_node, inheritedValue->cursor, enum_position);
        if (enum_anchor == nullptr && inheritedValue->candidat != nullptr &&
            inheritedValue->candidat != attach_node) {
          enum_anchor = find_exact_enum_body_anchor(
              inheritedValue->candidat, inheritedValue->cursor, enum_position);
        }
        if (enum_anchor == nullptr && loc_node != attach_node &&
            inheritedValue->candidat != loc_node) {
          enum_anchor = find_exact_enum_body_anchor(
              loc_node, inheritedValue->cursor, enum_position);
        }
        if (enum_anchor != nullptr) {
          attach_node = enum_anchor;
          inheritedValue->next_to_insert->setRelativePosition(enum_position);
          has_exact_enum_body_anchor = true;
        }
      }
      bool is_include = is_include_directive(inheritedValue->next_to_insert);
      bool can_attach = has_exact_enum_body_anchor ||
                        (is_include ? should_attach_include_target(
                                          attach_node, inheritedValue->cursor)
                                    : should_attach_preproc_target(
                                          attach_node, inheritedValue->cursor));
      if (!can_attach) {
        return inheritedValue;
      }
      attach_node = canonicalPreprocessingOwner(attach_node,
                                                inheritedValue->next_to_insert);
      if (isSemanticNonLexicalDeclarationSubtree(attach_node) ||
          !hasExactPhysicalSourceInterval(attach_node)) {
        std::cerr << "REX_FRONTEND_INVARIANT[preprocessing-owner]: canonical "
                     "traversal owner="
                  << attach_node->class_name()
                  << " has no exact lexical source interval\n";
        ROSE_ABORT();
      }
      inheritedValue->translator.preprocessor_mark_attached(
          inheritedValue->next_to_insert);
      attach_node->addToAttachedPreprocessingInfo(
          inheritedValue->next_to_insert);
    }
    if (!inheritedValue->advance()) {
      return NULL;
    }

    if (inheritedValue->cursor != nullptr &&
        *current_pos <= *(inheritedValue->cursor)) {
      bool next_is_include =
          is_include_directive(inheritedValue->next_to_insert);
      bool record_candidate = false;
      if (next_is_include) {
        record_candidate =
            should_attach_include_target(loc_node, inheritedValue->cursor);
      } else {
        record_candidate =
            should_attach_preproc_target(loc_node, inheritedValue->cursor);
      }
      if (record_candidate) {
        inheritedValue->candidat = loc_node;
      }
    }

    passed_cursor = *current_pos >= *(inheritedValue->cursor);
  }

  return inheritedValue;
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
    bool record_directive_stream, bool record_application_header_directives)
    : p_source_manager(source_manager), p_preprocessor(preprocessor),
      p_preprocessor_record_list(), p_preprocessor_records_by_location(),
      p_preprocessor_records_by_line(), p_preprocessor_record_positions(),
      p_removed_preprocessor_records(), p_attached_preprocessor_records(),
      p_preprocessor_record_cursor(0), p_preprocessor_record_list_sorted(true),
      p_record_directive_stream(record_directive_stream),
      p_record_application_header_directives(
          record_application_header_directives),
      p_include_ownership_paths(), p_normalized_physical_paths_by_file_id(),
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

  std::vector<std::pair<unsigned, clang::SourceLocation>> included_locations;
  for (const auto &record : p_preprocessor_record_list) {
    PreprocessingInfo *info = record.second;
    if (info == nullptr) {
      continue;
    }
    const PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    if (type != PreprocessingInfo::CpreprocessorIncludeDeclaration &&
        type != PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
      continue;
    }
    const auto position = p_preprocessor_record_positions.find(info);
    if (position == p_preprocessor_record_positions.end()) {
      if (p_removed_preprocessor_records.find(info) !=
          p_removed_preprocessor_records.end()) {
        continue;
      }
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[initializer-include-range]: recorded "
              "include has no exact source position\n");
      ROSE_ABORT();
    }
    if (position->second.file_id != file_id ||
        position->second.file_offset < begin_offset ||
        (!allow_at_interval_start &&
         position->second.file_offset == begin_offset) ||
        position->second.file_offset >= end_offset) {
      continue;
    }
    included_locations.emplace_back(position->second.file_offset,
                                    position->second.file_location);
  }

  std::sort(
      included_locations.begin(), included_locations.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });
  for (size_t index = 1; index < included_locations.size(); ++index) {
    if (included_locations[index - 1].first ==
        included_locations[index].first) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[initializer-include-range]: multiple "
              "include directives occupy one physical source offset\n");
      ROSE_ABORT();
    }
  }

  std::vector<clang::SourceLocation> result;
  result.reserve(included_locations.size());
  for (const auto &entry : included_locations) {
    result.push_back(entry.second);
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

  std::vector<RecordedDirectiveRef> matches;
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
      matches.push_back({file_info, info});
    }
  }
  if (matches.size() != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s expected one "
            "include at physical offset %u, found %zu\n",
            contract, file_offset, matches.size());
    ROSE_ABORT();
  }

  Sg_File_Info *interval_start = source_interval_owner->get_startOfConstruct();
  Sg_File_Info *interval_end = source_interval_owner->get_endOfConstruct();
  Sg_File_Info *include_info = matches.front().file_info;
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
      (!source_less(interval_start, include_info) &&
       !(allow_at_interval_start &&
         interval_start->get_line() == include_info->get_line() &&
         interval_start->get_col() == include_info->get_col())) ||
      !source_less(include_info, interval_end)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s include is "
            "not strictly inside its source-owner interval\n",
            contract);
    ROSE_ABORT();
  }

  PreprocessingInfo *info = matches.front().preprocessing_info;
  if (p_attached_preprocessor_records.find(info) !=
          p_attached_preprocessor_records.end() ||
      p_removed_preprocessor_records.find(info) !=
          p_removed_preprocessor_records.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s include was "
            "already claimed by another AST owner\n",
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
            "REX_FRONTEND_INVARIANT[include-boundary-owner]: %s include "
            "transfer did not publish one exact typed boundary\n",
            contract);
    ROSE_ABORT();
  }
  markRecordedDirectiveRemoved(matches.front().file_info, info);
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
  auto by_location =
      [&](const std::pair<Sg_File_Info *, PreprocessingInfo *> &lhs,
          const std::pair<Sg_File_Info *, PreprocessingInfo *> &rhs) {
        ROSE_ASSERT(lhs.first != nullptr && lhs.second != nullptr);
        ROSE_ASSERT(rhs.first != nullptr && rhs.second != nullptr);
        auto lhs_position = p_preprocessor_record_positions.find(lhs.second);
        auto rhs_position = p_preprocessor_record_positions.find(rhs.second);
        ROSE_ASSERT(lhs_position != p_preprocessor_record_positions.end());
        ROSE_ASSERT(rhs_position != p_preprocessor_record_positions.end());

        const bool lhs_is_main = lhs_position->second.file_id == main_file_id;
        const bool rhs_is_main = rhs_position->second.file_id == main_file_id;
        if (lhs_is_main != rhs_is_main) {
          return lhs_is_main;
        }

        const std::string lhs_path = FileHelper::normalizePathIfPossible(
            lhs.first->get_filenameString());
        const std::string rhs_path = FileHelper::normalizePathIfPossible(
            rhs.first->get_filenameString());
        if (lhs_path != rhs_path) {
          return lhs_path < rhs_path;
        }

        return lhs_position->second.file_offset <
               rhs_position->second.file_offset;
      };

  std::stable_sort(p_preprocessor_record_list.begin() +
                       p_preprocessor_record_cursor,
                   p_preprocessor_record_list.end(), by_location);
  p_preprocessor_record_list_sorted = true;
}

SagePreprocessorRecord::IncludeOwnership
SagePreprocessorRecord::includeOwnershipForPath(const std::string &path) const {
  const std::string normalized =
      path.empty() ? std::string() : FileHelper::normalizePathIfPossible(path);
  if (normalized.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: cannot classify an "
            "empty physical path\n");
    ROSE_ABORT();
  }
  auto found = p_include_ownership_paths.find(normalized);
  if (found == p_include_ownership_paths.end()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-ownership]: physical path=%s has "
            "no exact preprocessor ownership producer\n",
            normalized.c_str());
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
    resolved = p_source_manager->getSpellingLoc(resolved);
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
  if (!p_record_application_header_directives) {
    return false;
  }
  return ownership == IncludeOwnership::application_textual;
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

void SagePreprocessorRecord::recordDirective(
    clang::SourceLocation loc, PreprocessingInfo::DirectiveType directive_type,
    const std::string &text) {
  if (!shouldRecordDirective(loc)) {
    return;
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
    return;
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
        return;
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
        return;
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
        return;
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

  recordDirective(HashLoc, directive_type, include_text);
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

bool SagePreprocessorRecord::pop() {
  if (p_preprocessor_record_cursor < p_preprocessor_record_list.size()) {
    releaseRecordedDirective(
        p_preprocessor_record_list[p_preprocessor_record_cursor]);
    ++p_preprocessor_record_cursor;
  }
  return size() > 0;
}
