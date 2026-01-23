
#include <algorithm>

#include <cctype>

#include <iostream>

#include <memory>

#include <llvm/Support/MemoryBuffer.h>

#include "clang-frontend-private.hpp"

#include "clang-frontend-utils.hpp"

#include "sage3basic.h"

#include "clang-to-dot.hpp"

#include "clang_paths.h"

#include "ompAstConstruction.h"

#include <clang/Lex/Lexer.h>

#include "rose_config.h"

namespace {

struct TokenWithOffset {
  clang::Token token;
  unsigned offset;
};

// Preserve legacy frontend acceptance of explicit member specializations that
// omit the required template<> header.
std::unique_ptr<llvm::MemoryBuffer>
maybeFixMissingTemplateHeader(clang::SourceManager &source_manager,
                              clang::FileID file_id,
                              const clang::LangOptions &lang_opts,
                              std::vector<unsigned> *insert_offsets_out) {
  auto buffer_or = source_manager.getBufferOrNone(file_id);
  if (!buffer_or) {
    return nullptr;
  }
  clang::Lexer lexer(file_id, *buffer_or, source_manager, lang_opts);
  std::vector<TokenWithOffset> tokens;
  tokens.reserve(256);

  clang::Token token;
  while (true) {
    lexer.LexFromRawLexer(token);
    if (token.is(clang::tok::eof)) {
      break;
    }
    if (source_manager.getFileID(token.getLocation()) != file_id) {
      continue;
    }
    tokens.push_back(
        {token, source_manager.getFileOffset(token.getLocation())});
  }

  if (tokens.empty()) {
    return nullptr;
  }

  auto is_boundary = [](const clang::Token &tok) {
    return tok.isOneOf(clang::tok::semi, clang::tok::l_brace,
                       clang::tok::r_brace);
  };
  auto is_identifier_like = [](const clang::Token &tok) {
    return tok.isOneOf(clang::tok::identifier, clang::tok::raw_identifier);
  };
  auto is_template_keyword = [&](const clang::Token &tok) -> bool {
    if (tok.is(clang::tok::kw_template)) {
      return true;
    }
    if (!is_identifier_like(tok)) {
      return false;
    }
    std::string spelling =
        clang::Lexer::getSpelling(tok, source_manager, lang_opts);
    return spelling == "template";
  };

  std::vector<unsigned> insert_offsets;
  insert_offsets.reserve(8);

  for (size_t i = 1; i < tokens.size(); ++i) {
    if (!tokens[i].token.is(clang::tok::coloncolon)) {
      continue;
    }
    if (!tokens[i - 1].token.isOneOf(clang::tok::greater,
                                     clang::tok::greatergreater)) {
      continue;
    }

    bool has_template_id = false;
    for (size_t j = i; j-- > 0;) {
      if (is_boundary(tokens[j].token)) {
        break;
      }
      if (tokens[j].token.is(clang::tok::less)) {
        has_template_id = true;
        break;
      }
    }
    if (!has_template_id) {
      continue;
    }

    size_t name_index = i + 1;
    if (name_index >= tokens.size()) {
      continue;
    }
    if (tokens[name_index].token.is(clang::tok::tilde)) {
      ++name_index;
      if (name_index >= tokens.size()) {
        continue;
      }
    }
    if (!(is_identifier_like(tokens[name_index].token) ||
          tokens[name_index].token.is(clang::tok::kw_operator))) {
      continue;
    }

    bool matched = false;
    bool saw_extra_identifier = false;
    for (size_t k = name_index + 1; k < tokens.size(); ++k) {
      if (is_identifier_like(tokens[k].token)) {
        saw_extra_identifier = true;
        break;
      }
      if (tokens[k].token.is(clang::tok::l_paren)) {
        matched = !saw_extra_identifier;
        break;
      }
      if (is_boundary(tokens[k].token)) {
        break;
      }
    }
    if (!matched) {
      continue;
    }

    size_t boundary = i;
    while (boundary > 0 && !is_boundary(tokens[boundary - 1].token)) {
      --boundary;
    }

    bool has_template_keyword = false;
    for (size_t j = boundary; j < i; ++j) {
      if (is_template_keyword(tokens[j].token)) {
        has_template_keyword = true;
        break;
      }
    }
    if (has_template_keyword) {
      continue;
    }

    insert_offsets.push_back(tokens[boundary].offset);
  }

  std::sort(insert_offsets.begin(), insert_offsets.end());
  insert_offsets.erase(
      std::unique(insert_offsets.begin(), insert_offsets.end()),
      insert_offsets.end());
  if (insert_offsets_out != nullptr) {
    *insert_offsets_out = insert_offsets;
  }

  if (insert_offsets.empty()) {
    return nullptr;
  }

  std::string updated = buffer_or->getBuffer().str();
  const std::string insertion = "template<> ";
  size_t shift = 0;
  for (unsigned offset : insert_offsets) {
    updated.insert(offset + shift, insertion);
    shift += insertion.size();
  }

  return llvm::MemoryBuffer::getMemBufferCopy(updated,
                                              buffer_or->getBufferIdentifier());
}

} // namespace

// DQ (11/28/2020): Use this for testing the DOT graph generator.
#define EXIT_AFTER_BUILDING_DOT_FILE 0

int clang_main(int argc, char **argv, SgSourceFile &sageFile,
               const char *driver_argv0) {

  // CLANG FRONTEND FIX: Enable template unparsing from AST
  // The Clang frontend doesn't save template strings like the legacy frontend
  // did, so we must unparse templates from the AST instead of from saved
  // strings. This ensures template declarations like "template <class T>" are
  // output correctly.
  sageFile.set_unparse_template_ast(true);
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
    // DQ (10/23/2020): Calling clang-to-dot generator (I don't think this
    // modifies the argv list).
    int clang_to_dot_status = clang_to_dot_main(argc, argv, driver_argv0);
    if (clang_to_dot_status != 0) {
      printf("Error in generation of dot file of Clang IR: returing from top "
             "of clang_main(): clang_to_dot_status = %d \n",
             clang_to_dot_status);
      return clang_to_dot_status;
    } else {
    }

#if EXIT_AFTER_BUILDING_DOT_FILE
    return 0;
#endif
  }

  // 0 - Analyse Cmd Line

  std::vector<std::string> sys_dirs_list;
  std::vector<std::string> inc_dirs_list;
  std::vector<std::string> define_list;
  std::vector<std::string> openmp_define_list;
  std::vector<std::string> inc_list;
  std::string input_file;
  std::vector<std::string> passthrough_args;
  bool enable_openmp = false;
  bool enable_openmp_simd = false;
  bool disable_openmp_via_flag = false;
  bool continue_on_error = false;
  bool disable_access_control = false;
  enum class ExceptionMode { Unspecified, Enabled, Disabled };
  ExceptionMode exception_mode = ExceptionMode::Unspecified;
  enum class RttiMode { Unspecified, Enabled, Disabled };
  RttiMode rtti_mode = RttiMode::Unspecified;

  for (int i = 0; i < argc; i++) {
    std::string current_arg(argv[i]);
    if (current_arg.find("-I") == 0) {
      if (current_arg.length() > 2) {
        inc_dirs_list.push_back(current_arg.substr(2));
      } else {
        i++;
        if (i < argc)
          inc_dirs_list.push_back(current_arg);
        else
          break;
      }
    } else if (current_arg == "-isystem") {
      ++i;
      if (i < argc) {
        sys_dirs_list.push_back(argv[i]);
      } else {
        break;
      }
    } else if (current_arg.rfind("-isystem", 0) == 0) {
      if (current_arg.size() > 8 && current_arg[8] != '-') {
        sys_dirs_list.push_back(current_arg.substr(8));
      }
    } else if (current_arg.find("-D") == 0) {
      std::string define_value;
      if (current_arg.length() > 2) {
        define_value = current_arg.substr(2);
      } else {
        ++i;
        if (i < argc)
          define_value = argv[i];
        else
          break;
      }
      const bool is_openmp_define = (define_value == "_OPENMP") ||
                                    (define_value.rfind("_OPENMP=", 0) == 0);
      if (is_openmp_define)
        openmp_define_list.push_back(define_value);
      else
        define_list.push_back(define_value);
    }
    // Note: -fopenmp is processed by ROSE's command line processor before
    // reaching here ROSE sets sageFile.set_openmp(true) and related flags
    // automatically We don't need to parse it again here
    else if (current_arg.find("-c") == 0) {
    } else if (current_arg == "-std") {
      passthrough_args.push_back(current_arg);
      ++i;
      if (i < argc)
        passthrough_args.push_back(argv[i]);
      else
        break;
    } else if (current_arg.rfind("-std=", 0) == 0) {
      passthrough_args.push_back(current_arg);
    } else if (current_arg.find("-o") == 0) {
      if (current_arg.length() == 2) {
        i++;
        if (i >= argc)
          break;
      }
    } else if (current_arg.rfind("-fopenmp", 0) == 0) {
      // Don't pass -fopenmp to Clang - REX captures OpenMP pragmas as plain
      // text
      bool explicitly_disabled = false;
      if (current_arg.size() > 8 && current_arg[8] == '=') {
        std::string value = current_arg.substr(9);
        std::string lower_value = value;
        std::transform(
            lower_value.begin(), lower_value.end(), lower_value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower_value == "0" || lower_value == "false" ||
            lower_value == "disabled") {
          explicitly_disabled = true;
        }
      }

      if (explicitly_disabled) {
        disable_openmp_via_flag = true;
        enable_openmp = false;
      } else if (!disable_openmp_via_flag) {
        enable_openmp = true;
      }
    } else if (current_arg == "-fopenmp-simd") {
      enable_openmp_simd = true;
      enable_openmp =
          true; // SIMD is a subset of OpenMP, enable full pragma capture
    } else if (current_arg == "-fexceptions" ||
               current_arg == "-fcxx-exceptions") {
      exception_mode = ExceptionMode::Enabled;
      passthrough_args.push_back(current_arg);
    } else if (current_arg == "-fno-exceptions" ||
               current_arg == "-fno-cxx-exceptions") {
      exception_mode = ExceptionMode::Disabled;
      passthrough_args.push_back(current_arg);
    } else if (current_arg == "-frtti") {
      rtti_mode = RttiMode::Enabled;
    } else if (current_arg == "-fno-rtti") {
      rtti_mode = RttiMode::Disabled;
    } else if (current_arg == "-rex:clang:continue-on-error") {
      continue_on_error = true;
    } else if (current_arg == "-rex:clang:disable-access-control") {
      disable_access_control = true;
    } else if (!current_arg.empty() && current_arg[0] == '-') {
      // TODO -include
#if DEBUG_ARGS
      std::cerr << "argv[" << i << "] = " << current_arg
                << " preserved as passthrough option." << std::endl;
#endif
      passthrough_args.push_back(current_arg);
    } else {
#if DEBUG_ARGS
      std::cerr << "argv[" << i << "] = " << current_arg
                << " is neither define or include dir. Use it as input file."
                << std::endl;
#endif
      input_file = current_arg;
    }
  }

  // Detect if this is a secondary parse during lowering/outlining
  // Check if this file is being re-parsed (project already has a file with same
  // source)
  bool is_secondary_parse = false;
  if (sageFile.get_parent() != NULL) {
    SgProject *project = isSgProject(sageFile.get_parent()->get_parent());
    if (project != NULL) {
      // Check if project already has a file with the same source filename
      // This indicates we're in a secondary parse (e.g., during
      // outlining/lowering)
      SgFilePtrList &file_list = project->get_fileList();

      // Normalize input_file to absolute path for comparison
      std::unique_ptr<char, decltype(&free)> abs_input(
          realpath(input_file.c_str(), NULL), &free);
      std::string normalized_input =
          abs_input ? std::string(abs_input.get()) : input_file;

      for (SgFilePtrList::iterator it = file_list.begin();
           it != file_list.end(); ++it) {
        SgSourceFile *existing_file = isSgSourceFile(*it);
        if (existing_file != NULL && existing_file != &sageFile) {
          std::string existing_filename =
              existing_file->get_sourceFileNameWithPath();

          // Normalize existing filename to absolute path
          std::unique_ptr<char, decltype(&free)> abs_existing(
              realpath(existing_filename.c_str(), NULL), &free);
          std::string normalized_existing =
              abs_existing ? std::string(abs_existing.get())
                           : existing_filename;

          // Use exact path comparison only (no substring matching)
          if (normalized_existing == normalized_input) {
            is_secondary_parse = true;
            break;
          }
        }
      }
    }
  }

  // Enable pragma capture if ROSE has OpenMP enabled (don't pass flag to Clang)
  if (sageFile.get_openmp() && !enable_openmp && !disable_openmp_via_flag &&
      !is_secondary_parse) {
    enable_openmp = true;
  }

  if (sageFile.get_openmp_parse_only() && !enable_openmp_simd &&
      !is_secondary_parse) {
    enable_openmp_simd = true;
    enable_openmp = true; // SIMD requires pragma capture
  }

  ClangToSageTranslator::Language language = ClangToSageTranslator::unknown;

  size_t last_period = input_file.find_last_of(".");
  std::string extention(input_file.substr(last_period + 1));

  if (extention == "c") {
    language = ClangToSageTranslator::C;
  } else if (extention == "C" || extention == "cxx" || extention == "cpp" ||
             extention == "cc") {
    language = ClangToSageTranslator::CPLUSPLUS;
  } else if (extention == "objc") {
    language = ClangToSageTranslator::OBJC;
  } else if (extention == "cu") {
    language = ClangToSageTranslator::CUDA;
  } else if (extention == "ocl" || extention == "cl") {
    language = ClangToSageTranslator::OPENCL;
  }

  ROSE_ASSERT(language != ClangToSageTranslator::unknown);

  std::string language_arg;
  switch (language) {
  case ClangToSageTranslator::C:
    language_arg = "-xc";
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    language_arg = "-xc++";
    break;
  case ClangToSageTranslator::CUDA:
    language_arg = "-xcuda";
    break;
  case ClangToSageTranslator::OPENCL:
    language_arg = "-xcl";
    break;
  case ClangToSageTranslator::OBJC:
    language_arg = "-xobjective-c";
    break;
  default:
    ROSE_ABORT();
  }

  if (exception_mode == ExceptionMode::Unspecified &&
      (language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA)) {
    exception_mode = ExceptionMode::Enabled;
    passthrough_args.push_back("-fexceptions");
  }

  if (rtti_mode == RttiMode::Unspecified &&
      (language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA)) {
    rtti_mode = RttiMode::Enabled;
  }

  const char *cxx_config_include_dirs_array[] = CXX_INCLUDE_STRING;
  const char *c_config_include_dirs_array[] = C_INCLUDE_STRING;

  std::vector<std::string> cxx_config_include_dirs(
      cxx_config_include_dirs_array,
      cxx_config_include_dirs_array +
          sizeof(cxx_config_include_dirs_array) / sizeof(const char *));
  std::vector<std::string> c_config_include_dirs(
      c_config_include_dirs_array,
      c_config_include_dirs_array +
          sizeof(c_config_include_dirs_array) / sizeof(const char *));

  RoseClangPathRoots clang_paths = resolveRoseClangPaths(driver_argv0);
  std::string builtin_header_root = clang_paths.builtin_header_root;

  dropRelativeIncludeDirs(c_config_include_dirs);
  dropRelativeIncludeDirs(cxx_config_include_dirs);

  sys_dirs_list.push_back(builtin_header_root);

  switch (language) {
  case ClangToSageTranslator::C:
    sys_dirs_list.insert(sys_dirs_list.begin(), c_config_include_dirs.begin(),
                         c_config_include_dirs.end());
    inc_list.push_back("clang-builtin-c.h");
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    // Use configuration-driven cxx_config_include_dirs for portability
    // across different platforms, architectures, and compiler versions
    sys_dirs_list.insert(sys_dirs_list.begin(), cxx_config_include_dirs.begin(),
                         cxx_config_include_dirs.end());
    inc_list.push_back("clang-builtin-cpp.hpp");
    break;
  case ClangToSageTranslator::CUDA:
    sys_dirs_list.insert(sys_dirs_list.begin(), cxx_config_include_dirs.begin(),
                         cxx_config_include_dirs.end());
    inc_list.push_back("clang-builtin-cuda.hpp");
    break;
  case ClangToSageTranslator::OPENCL:
    sys_dirs_list.insert(sys_dirs_list.begin(), c_config_include_dirs.begin(),
                         c_config_include_dirs.end());
    inc_list.push_back("clang-builtin-opencl.h");
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

  // FIXME should be handle by Clang ?
  define_list.push_back("__I__=_Complex_I");

  // If user explicitly provided -D_OPENMP=value on command line, honor it
  // Otherwise, when -fopenmp is passed to Clang (enable_openmp=true), Clang
  // will automatically define _OPENMP with the correct version for its OpenMP
  // runtime. We should NOT override Clang's built-in _OPENMP macro with a
  // hardcoded value.
  if (!openmp_define_list.empty()) {
    define_list.insert(define_list.end(), openmp_define_list.begin(),
                       openmp_define_list.end());
  }

  const size_t estimated_argc = 1 + define_list.size() + inc_dirs_list.size() +
                                sys_dirs_list.size() + inc_list.size() +
                                passthrough_args.size();
  std::vector<std::string> args_storage;
  args_storage.reserve(estimated_argc);
  args_storage.push_back(language_arg);
  for (const auto &define : define_list) {
    args_storage.push_back("-D" + define);
  }
  for (const auto &inc_dir : inc_dirs_list) {
    args_storage.push_back("-I" + inc_dir);
  }
  for (const auto &sys_dir : sys_dirs_list) {
    args_storage.push_back("-isystem" + sys_dir);
  }
  for (const auto &inc : inc_list) {
    args_storage.push_back("-include" + inc);
  }
  for (const auto &pass : passthrough_args) {
    args_storage.push_back(pass);
  }

  std::vector<const char *> args;
  args.reserve(args_storage.size());
  for (const auto &arg : args_storage) {
    args.push_back(arg.c_str());
  }
#if DEBUG_ARGS
  for (size_t index = 0; index < args.size(); ++index) {
    std::cerr << "args[" << index << "] = " << args[index] << std::endl;
  }
#endif

  // 2 - Create a compiler instance

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

  // Use IntrusiveRefCntPtr for DiagnosticOptions to manage lifetime properly.
  // TextDiagnosticPrinter stores a pointer to DiagOpts, so it must outlive the
  // function scope. IntrusiveRefCntPtr ensures proper reference counting and
  // cleanup.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts =
      llvm::makeIntrusiveRefCnt<clang::DiagnosticOptions>();
  clang::TextDiagnosticPrinter *diag_printer =
      new clang::TextDiagnosticPrinter(llvm::errs(), DiagOpts.get());

  // LLVM 20+ API - requires VFS as first parameter
  compiler_instance->createDiagnostics(*vfs, diag_printer, true);

  clang::CompilerInvocation &invocation = compiler_instance->getInvocation();

  // Parse command-line arguments to populate invocation (including
  // FileSystemOptions like -working-directory, -sysroot)
  llvm::ArrayRef<const char *> argsArrayRef(args.data(), args.size());
  clang::CompilerInvocation::CreateFromArgs(
      invocation, argsArrayRef, compiler_instance->getDiagnostics());
  clang::TargetOptions &target_opts = invocation.getTargetOpts();
  ensureX86BaselineTargetFeatures(target_opts);
  llvm::Triple target_triple(target_opts.Triple);

  // CLANG FRONTEND FIX: Configure header search paths properly
  // WHY: Default paths may point to incorrect locations (e.g.,
  // /usr/include/newlib/ on ARM) CLANG FRONTEND FIX #22: Use Clang's automatic
  // header detection instead of hard-coded paths This ensures portability
  // across different systems, LLVM versions, and distributions.
  //
  // The CompilerInvocation::CreateFromArgs() call above already sets up correct
  // paths:
  // - ResourceDir is auto-detected from the Clang installation
  // - System include paths are computed by Clang's Driver based on the target
  // triple
  // - No need to override with hard-coded paths that break on other systems
  //
  // We only need to enable the relevant flags to use Clang's built-in
  // detection:
  clang::HeaderSearchOptions &headerSearchOpts =
      invocation.getHeaderSearchOpts();

  // Enable Clang's builtin includes (provides compiler-specific headers like
  // stddef.h)
  headerSearchOpts.UseBuiltinIncludes = true;
  headerSearchOpts.UseStandardSystemIncludes = true;
  headerSearchOpts.UseStandardCXXIncludes = true;

  // ResourceDir and system include paths are already correctly set by
  // CreateFromArgs() based on the Clang installation and target triple. Do NOT
  // override them with hard-coded paths.

  clang::LangOptions &lang_opts = compiler_instance->getLangOpts();
  std::vector<std::string> lang_specific_includes;
  clang::LangStandard::Kind requested_std = lang_opts.LangStd;
  clang::LangStandard std_info =
      clang::LangStandard::getLangStandardForKind(requested_std);
  clang::Language clang_lang = clang::Language::C;
  bool enable_cuda = false;
  bool enable_opencl = false;

  switch (language) {
  case ClangToSageTranslator::C:
    clang_lang = clang::Language::C;
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    if (!std_info.isCPlusPlus()) {
      requested_std = clang::LangStandard::lang_gnucxx17;
    }
    clang_lang = clang::Language::CXX;
    break;
  case ClangToSageTranslator::CUDA:
    if (!std_info.isCPlusPlus()) {
      requested_std = clang::LangStandard::lang_gnucxx17;
    }
    clang_lang = clang::Language::CUDA;
    enable_cuda = true;
    break;
  case ClangToSageTranslator::OPENCL:
    if (!std_info.isOpenCL()) {
      requested_std = clang::LangStandard::lang_opencl30;
    }
    clang_lang = clang::Language::OpenCL;
    enable_opencl = true;
    break;
  case ClangToSageTranslator::OBJC:
    ROSE_ASSERT(!"Objective-C is not supported by ROSE Compiler.");
  default:
    ROSE_ABORT();
  }

  clang::InputKind input_kind(clang_lang);
  clang::FrontendOptions &fe_opts = invocation.getFrontendOpts();
  fe_opts.Inputs.clear();
  fe_opts.Inputs.emplace_back(input_file, input_kind);

  clang::LangOptions::setLangDefaults(lang_opts, clang_lang, target_triple,
                                      lang_specific_includes, requested_std);
  if (lang_opts.GNUCVersion == 0) {
    // Clang's driver normally sets a GCC compatibility version; mirror that
    // here so GNU built-in macros (e.g., __GNUC__) are defined.
    lang_opts.GNUCVersion = 40201; // GCC 4.2.1 (Clang default)
  }

  if (language == ClangToSageTranslator::CPLUSPLUS) {
    ROSE_ASSERT(lang_opts.CPlusPlus &&
                "Expected C++ mode after setting language defaults");
    lang_opts.Bool = 1;
  }

  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    if (exception_mode == ExceptionMode::Disabled) {
      lang_opts.CXXExceptions = 0;
      lang_opts.Exceptions = 0;
    } else {
      lang_opts.CXXExceptions = 1;
      lang_opts.Exceptions = 1;
    }
  }

  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    if (rtti_mode == RttiMode::Disabled) {
      lang_opts.RTTI = 0;
      lang_opts.RTTIData = 0;
    } else {
      lang_opts.RTTI = 1;
      lang_opts.RTTIData = 1;
    }
  }

  if (enable_cuda) {
    lang_opts.CUDA = 1;
  }
  if (enable_opencl) {
    lang_opts.OpenCL = 1;
  }
  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    if (disable_access_control) {
      lang_opts.AccessControl = 0;
    }
  }

  // Now create file manager with FileSystemOptions from the parsed invocation
  compiler_instance->createFileManager(vfs);

  clang::PreprocessorOptions &pp_opts =
      compiler_instance->getInvocation().getPreprocessorOpts();
  pp_opts.UsePredefines = true;
  if (!lang_specific_includes.empty()) {
    pp_opts.Includes.insert(pp_opts.Includes.end(),
                            lang_specific_includes.begin(),
                            lang_specific_includes.end());
  }

  // LLVM 20 requires shared_ptr, LLVM 21+ requires reference
#if LLVM_VERSION_MAJOR >= 21
  clang::TargetInfo *target_info = clang::TargetInfo::CreateTargetInfo(
      compiler_instance->getDiagnostics(), invocation.getTargetOpts());
#else
  auto target_options =
      std::make_shared<clang::TargetOptions>(invocation.getTargetOpts());
  clang::TargetInfo *target_info = clang::TargetInfo::CreateTargetInfo(
      compiler_instance->getDiagnostics(), target_options);
#endif
  compiler_instance->setTarget(target_info);

  compiler_instance->createSourceManager(compiler_instance->getFileManager());

  // In LLVM 20, getFileRef returns Expected<FileEntryRef> instead of ErrorOr
  llvm::Expected<clang::FileEntryRef> ret =
      compiler_instance->getFileManager().getFileRef(input_file);
  if (!ret) {
    llvm::errs() << "Error opening file: " << input_file << "\n";
    ROSE_ABORT();
  }
  clang::FileEntryRef input_file_entry = *ret;
  // In LLVM 20, createFileID takes FileEntryRef instead of const FileEntry*
  clang::FileID mainFileID = compiler_instance->getSourceManager().createFileID(
      input_file_entry, clang::SourceLocation(), clang::SrcMgr::C_User);
  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    std::vector<unsigned> missing_template_offsets;
    if (auto fixed_buffer = maybeFixMissingTemplateHeader(
            compiler_instance->getSourceManager(), mainFileID, lang_opts,
            &missing_template_offsets)) {
      compiler_instance->getSourceManager().overrideFileContents(
          input_file_entry, std::move(fixed_buffer));
      // The token stream no longer matches the on-disk source; unparse from
      // the AST to preserve the normalized template<> specialization headers.
      sageFile.set_unparse_tokens(false);
    }
    if (!missing_template_offsets.empty()) {
      std::string file_name = input_file;
      if (auto entry =
              compiler_instance->getSourceManager().getFileEntryRefForID(
                  mainFileID)) {
        file_name = entry->getName().str();
      }
      sageFile.addNewAttribute(
          kMissingTemplateHeaderFixupAttributeName,
          new MissingTemplateHeaderFixupAttribute(
              std::move(file_name), std::move(missing_template_offsets)));
    }
  }

  compiler_instance->getSourceManager().setMainFileID(mainFileID);

  if (!compiler_instance->hasPreprocessor())
    compiler_instance->createPreprocessor(clang::TU_Complete);

  // Register pragma callback to capture pragmas as plain text (OpenMP and
  // others)
  RoseOpenMPPragmaCallback *omp_callback = nullptr;
  {
    clang::Preprocessor &PP = compiler_instance->getPreprocessor();
    auto omp_callback_owner = std::make_unique<RoseOpenMPPragmaCallback>(
        compiler_instance->getSourceManager(), PP);
    omp_callback = omp_callback_owner.get();
    PP.addPPCallbacks(std::move(omp_callback_owner));
  }

  if (!compiler_instance->hasASTContext())
    compiler_instance->createASTContext();

  compiler_instance->getPreprocessor().getBuiltinInfo().initializeBuiltins(
      compiler_instance->getPreprocessor().getIdentifierTable(), lang_opts);

  auto translator_ptr = std::make_unique<ClangToSageTranslator>(
      compiler_instance.get(), language, &sageFile);
  ClangToSageTranslator *translator = translator_ptr.get();

  // Pass pragma callback to translator
  if (omp_callback) {
    translator->setOpenMPPragmaCallback(omp_callback);
  }

  compiler_instance->setASTConsumer(std::move(translator_ptr));

  if (!compiler_instance->hasSema())
    compiler_instance->createSema(clang::TU_Complete, NULL);

  ROSE_ASSERT(compiler_instance->hasDiagnostics());
  ROSE_ASSERT(compiler_instance->hasTarget());
  ROSE_ASSERT(compiler_instance->hasFileManager());
  ROSE_ASSERT(compiler_instance->hasSourceManager());
  ROSE_ASSERT(compiler_instance->hasPreprocessor());
  ROSE_ASSERT(compiler_instance->hasASTContext());
  ROSE_ASSERT(compiler_instance->hasSema());

  // 3 - Translate

  //  printf ("Calling clang::ParseAST()\n");

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

  compiler_instance->getDiagnosticClient().BeginSourceFile(
      compiler_instance->getLangOpts(),
      &(compiler_instance->getPreprocessor()));
  if (language == ClangToSageTranslator::CPLUSPLUS) {
    clang::IdentifierInfo &builtin_id =
        compiler_instance->getPreprocessor().getIdentifierTable().get(
            "__builtin_clzll");
    ROSE_ASSERT(builtin_id.getBuiltinID() !=
                    static_cast<unsigned>(clang::Builtin::NotBuiltin) &&
                "Expected Clang builtins to be initialised for C++ mode");
  }
  clang::ParseAST(compiler_instance->getPreprocessor(), translator,
                  compiler_instance->getASTContext());
  compiler_instance->getDiagnosticClient().EndSourceFile();

  // In LLVM 20, get error count from diagnostics directly
  unsigned numErrors = compiler_instance->getDiagnostics().getNumErrors();
  if (numErrors > 0) {
    printf("Clang found %d diagnostic errors during parsing\n", numErrors);
  }

  SgGlobal *global_scope = translator->getGlobalScope();

  // 4 - Attach to the file

  if (sageFile.get_globalScope() != NULL) {
    SgGlobal *old_global_scope = sageFile.get_globalScope();
    sageFile.set_globalScope(nullptr);
    old_global_scope->set_parent(nullptr);
    SageInterface::deleteAST(old_global_scope);
    auto map_it = Rose::tokenSubsequenceMapOfMapsBySourceFile.find(&sageFile);
    if (map_it != Rose::tokenSubsequenceMapOfMapsBySourceFile.end() &&
        map_it->second != NULL) {
      // Clear stale token mappings from the previous AST to avoid
      // dangling SgNode* references on re-parse.
      map_it->second->clear();
    }
  }

  sageFile.set_globalScope(global_scope);

  // Check if OpenMP was enabled by ROSE's command line processor
  // ROSE processes -fopenmp before clang_main is called and sets these flags
  if (sageFile.get_openmp()) {
    // By default, ROSE sets parse_only mode. For Clang frontend, we want to
    // default to ast_only. Only override if no explicit processing flag was set
    // by user
    bool has_explicit_processing_flag = sageFile.get_openmp_ast_only() ||
                                        sageFile.get_openmp_lowering() ||
                                        sageFile.get_openmp_analyzing();

    if (!has_explicit_processing_flag && sageFile.get_openmp_parse_only()) {
      sageFile.set_openmp_parse_only(false);
      sageFile.set_openmp_ast_only(true);
    }
  }

  // Parent relationship already set up during global scope creation

  std::string file_name(input_file);

  Sg_File_Info *start_fi = new Sg_File_Info(file_name, 0, 0);
  Sg_File_Info *end_fi = new Sg_File_Info(file_name, 0, 0);

  global_scope->set_startOfConstruct(start_fi);

  global_scope->set_endOfConstruct(end_fi);

  // 6 - Finish the AST (fixup phase)

  finishSageAST(*translator);

  // 7 - OpenMP Processing
  //
  // NOTE: processOpenMP() is called automatically by sage_support.cpp after
  // this function returns. If -fopenmp was specified, it will convert
  // SgPragmaDeclaration nodes to OpenMP-specific AST nodes. The OpenMP flags
  // have been set correctly earlier in this function.

  // 8 - Cleanup LLVM objects
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

  // REX: By default, treat Clang diagnostic errors as fatal.
  // Use -rex:clang:continue-on-error to allow backend to run with a partial
  // AST.
  if (global_scope == NULL) {
    printf("Error: Failed to build AST - global_scope is NULL\n");
    return (numErrors > 0) ? numErrors : 1; // Failure - no AST
  }

  if (numErrors > 0) {
    if (continue_on_error) {
      sageFile.set_skipfinalCompileStep(true);
      printf("Note: Proceeding to backend despite %d Clang diagnostic "
             "error(s) because AST was successfully constructed\n",
             numErrors);
      return 0; // Success - AST was built
    }
    printf("Error: Clang reported %d diagnostic error(s); refusing to run "
           "backend (use -rex:clang:continue-on-error to override)\n",
           numErrors);
    return numErrors;
  }

  return 0; // Success - AST was built
}

void finishSageAST(ClangToSageTranslator &translator) {
  SgGlobal *global_scope = translator.getGlobalScope();

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
              translator.setCompilerGeneratedFileInfo(null_stmt);
              labelStmt->set_statement(null_stmt);
              null_stmt->set_parent(labelStmt);
              SageInterface::insertStatementAfter(labelStmt, sub_stmt);
          }
      }
  */
  // 2 - Place Preprocessor informations

  if (translator.preprocessor_list_size() > 0) {
    NextPreprocessorToInsert npp(translator);
    std::pair<Sg_File_Info *, PreprocessingInfo *> top =
        translator.preprocessor_top();
    npp.cursor = top.first;
    npp.next_to_insert = top.second;
    npp.candidat = NULL;

    PreprocessorInserter preprocessor_inserter;
    preprocessor_inserter.traverse(global_scope, &npp);
  }
}

SgGlobal *ClangToSageTranslator::getGlobalScope() const {
  return p_global_scope;
}

ClangToSageTranslator::ClangToSageTranslator(
    clang::CompilerInstance *compiler_instance, Language language_,
    SgSourceFile *sage_source_file)
    : clang::ASTConsumer(), p_decl_translation_map(), p_stmt_translation_map(),
      p_type_translation_map(), p_template_decl_cache(),
      p_template_inst_cache(), p_global_scope(NULL),
      p_class_type_decl_first_see_in_type(),
      p_enum_type_decl_first_see_in_type(),
      p_compiler_instance(compiler_instance),
      p_sage_preprocessor_recorder(std::make_unique<SagePreprocessorRecord>(
          &(p_compiler_instance->getSourceManager()))),
      p_sage_source_file(sage_source_file), language(language_),
      p_openmp_pragma_callback(nullptr) {}

ClangToSageTranslator::~ClangToSageTranslator() {}

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

void ClangToSageTranslator::applySourceRange(SgNode *node,
                                             clang::SourceRange source_range) {
  SgLocatedNode *located_node = isSgLocatedNode(node);
  SgInitializedName *init_name = isSgInitializedName(node);

#if DEBUG_SOURCE_LOCATION
  std::cerr << "Set File_Info for " << node << " of type " << node->class_name()
            << std::endl;
#endif

  if (located_node == NULL && init_name == NULL) {
    // ROOT CAUSE FIX: Some generated nodes (like SgVarRefExp placeholders for
    // unresolved members) may not be located nodes. Just skip them instead of
    // fatal error.
#if DEBUG_SOURCE_LOCATION
    std::cerr << "Warning: Skipping source range application for non-located "
                 "node of type "
              << node->class_name() << std::endl;
#endif
    return;
  } else {
    if (located_node != NULL) {
      Sg_File_Info *fi = located_node->get_startOfConstruct();
      if (fi != NULL)
        delete fi;
      fi = located_node->get_endOfConstruct();
      if (fi != NULL)
        delete fi;
    } else {
      if (init_name != NULL) {
        Sg_File_Info *fi = init_name->get_startOfConstruct();
        if (fi != NULL)
          delete fi;
        fi = init_name->get_endOfConstruct();
        if (fi != NULL)
          delete fi;
      }
    }
  }

  Sg_File_Info *start_fi = NULL;
  Sg_File_Info *end_fi = NULL;

  if (source_range.isValid()) {
    clang::SourceLocation begin = source_range.getBegin();
    clang::SourceLocation end = source_range.getEnd();

    if (begin.isValid() && end.isValid()) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();

      bool begin_is_macro = begin.isMacroID();
      bool end_is_macro = end.isMacroID();
      clang::SourceLocation spelling_begin = sm.getSpellingLoc(begin);
      clang::SourceLocation spelling_end = sm.getSpellingLoc(end);

      if (begin_is_macro) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation begin as it is a MacroID: ";
        begin.dump(sm);
        std::cerr << std::endl;
#endif
        begin = sm.getExpansionLoc(begin);
        ROSE_ASSERT(begin.isValid());
      }

      if (end_is_macro) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation end as it is a MacroID: ";
        end.dump(sm);
        std::cerr << std::endl;
#endif
        end = sm.getExpansionLoc(end);
        ROSE_ASSERT(end.isValid());
      }

      clang::FileID file_begin = sm.getFileID(begin);
      clang::FileID file_end = sm.getFileID(end);

      if (!sm.isWrittenInMainFile(begin) &&
          sm.isWrittenInMainFile(spelling_begin)) {
        begin = spelling_begin;
        end = spelling_end;
        file_begin = sm.getFileID(begin);
        file_end = sm.getFileID(end);
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

        // In LLVM 20, getFileEntryForID still returns const FileEntry*.
        const clang::FileEntry *fileEntry = sm.getFileEntryForID(file_begin);
        std::string file;
        if (fileEntry) {
          // In LLVM 20, FileEntry uses tryGetRealPathName() instead of
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
          // Mark nodes for code generation only when they originate
          // from the main file being compiled. This preserves
          // ROSE's default behavior of not inlining declarations
          // from included headers into the generated output file
          // (e.g. copyAST_copytest2007_40), while still allowing
          // template default-argument logic to reason about which
          // declarations will be unparsed in the main file (Issue
          // 69).
          if (file.find("clang-builtin-c.h") != std::string::npos) {
            start_fi = new Sg_File_Info(file, ls, cs);
            end_fi = new Sg_File_Info(file, le, ce);

            // DQ (11/29/2020): This is not doing what I had hoped
            // it would do. I think the solution is to use the
            // -DSKIP_ROSE_BUILTIN_DECLARATIONS option, but that is
            // not working as I expected either.  Time to go home.
            // start_fi =
            // Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
            // end_fi   =
            // Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
            start_fi->set_classificationBitField(
                Sg_File_Info::e_frontend_specific);
            end_fi->set_classificationBitField(
                Sg_File_Info::e_frontend_specific);
            if (in_main_file) {
              start_fi->setOutputInCodeGeneration();
              end_fi->setOutputInCodeGeneration();
            }
          } else {
            start_fi = new Sg_File_Info(file, ls, cs);
            end_fi = new Sg_File_Info(file, le, ce);
            if (in_main_file) {
              start_fi->setOutputInCodeGeneration();
              end_fi->setOutputInCodeGeneration();
            }
          }

          if (begin_is_macro || end_is_macro) {
            auto set_physical_info = [&](Sg_File_Info *fi,
                                         const clang::SourceLocation &loc) {
              if (fi == NULL || !loc.isValid())
                return;
              clang::FileID spelling_id = sm.getFileID(loc);
              if (!spelling_id.isInvalid()) {
                const clang::FileEntry *spelling_entry =
                    sm.getFileEntryForID(spelling_id);
                if (spelling_entry) {
                  std::string spelling_file =
                      spelling_entry->tryGetRealPathName().str();
                  if (!spelling_file.empty())
                    fi->set_physical_filename(spelling_file);
                }
              }
              bool inv_line = false;
              unsigned spelling_line = sm.getSpellingLineNumber(loc, &inv_line);
              if (!inv_line && spelling_line > 0)
                fi->set_physical_line(spelling_line);
            };

            set_physical_info(start_fi, spelling_begin);
            set_physical_info(end_fi, spelling_end);
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
    start_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();

    start_fi->setCompilerGenerated();
    end_fi->setCompilerGenerated();
#if DEBUG_SOURCE_LOCATION
    std::cerr << "Create FI for compiler generated node" << std::endl;
#endif
  } else {
    if (start_fi == NULL || end_fi == NULL) {
      ROSE_ASSERT(!"start_fi == NULL xor end_fi == NULL");
    }
  }

  if (located_node != NULL) {
    setFileInfosWithParent(located_node, start_fi, end_fi);

    // Pei-Hung (09/29/2022) SgExpression::get_file_info() checks and
    // returns get_operatorPosition(), so ensure operatorPosition is set.
    SgExpression *expr = isSgExpression(located_node);
    if (expr != NULL) {
      if (expr->get_operatorPosition() == NULL) {
        expr->set_operatorPosition(start_fi);
      } else {
        // CFE FIX: If operatorPosition was already created by
        // setSourcePositionToDefault, update it to match the real source
        // location.
        delete expr->get_operatorPosition();
        Sg_File_Info *op_fi = new Sg_File_Info(*start_fi);
        expr->set_operatorPosition(op_fi);
        op_fi->set_parent(expr);
      }
    }
  } else {
    if (init_name != NULL) {
      setFileInfosWithParent(init_name, start_fi, end_fi);
    }
  }
}

void ClangToSageTranslator::setCompilerGeneratedFileInfo(SgNode *node,
                                                         bool to_be_unparse) {
  Sg_File_Info *start_fi =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  Sg_File_Info *end_fi =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();

  start_fi->setCompilerGenerated();
  end_fi->setCompilerGenerated();

  if (to_be_unparse) {
    start_fi->setOutputInCodeGeneration();
    end_fi->setOutputInCodeGeneration();
  }

  ROSE_ASSERT(start_fi != NULL && end_fi != NULL);

#if DEBUG_SOURCE_LOCATION
  std::cerr << "Set File_Info for " << node << " of type " << node->class_name()
            << std::endl;
#endif

  SgLocatedNode *located_node = isSgLocatedNode(node);
  SgInitializedName *init_name = isSgInitializedName(node);

  if (located_node == NULL && init_name == NULL) {
    std::cerr << "Consistency error: try to set a Sage node which is not a "
                 "SgLocatedNode or a SgInitializedName as compiler generated"
              << std::endl;
    exit(-1);
  } else if (located_node != NULL) {
    Sg_File_Info *fi = located_node->get_startOfConstruct();
    if (fi != NULL)
      delete fi;
    fi = located_node->get_endOfConstruct();
    if (fi != NULL)
      delete fi;

    setFileInfosWithParent(located_node, start_fi, end_fi);

    // Pei-Hung (07/12/2023) Ensure SgExpression file info is set so
    // get_file_info() doesn't return null for compiler-generated nodes.
    if (SgExpression *expr = isSgExpression(located_node)) {
      Sg_File_Info *expr_fi = expr->get_file_info();
      if (expr_fi != NULL && expr_fi != start_fi && expr_fi != end_fi) {
        delete expr_fi;
      }
      expr->set_file_info(start_fi);
    }

    // CFE FIX: If operatorPosition exists, update it to match
    // compiler-generated flags
    SgExpression *expr = isSgExpression(located_node);
    if (expr != NULL && expr->get_operatorPosition() != NULL) {
      delete expr->get_operatorPosition();
      Sg_File_Info *op_fi =
          Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
      op_fi->setCompilerGenerated();
      if (to_be_unparse) {
        op_fi->setOutputInCodeGeneration();
      }
      expr->set_operatorPosition(op_fi);
      op_fi->set_parent(expr);
    }
  } else if (init_name != NULL) {
    Sg_File_Info *fi = init_name->get_startOfConstruct();
    if (fi != NULL)
      delete fi;
    fi = init_name->get_endOfConstruct();
    if (fi != NULL)
      delete fi;

    setFileInfosWithParent(init_name, start_fi, end_fi);
  }
}

/* Overload of ASTConsumer::HandleTranslationUnit, it is the "entry point" */

void ClangToSageTranslator::HandleTranslationUnit(
    clang::ASTContext &ast_context) {
  Traverse(ast_context.getTranslationUnitDecl());
}

/* Preprocessor Stack */

std::pair<Sg_File_Info *, PreprocessingInfo *>
ClangToSageTranslator::preprocessor_top() {
  return p_sage_preprocessor_recorder->top();
}

bool ClangToSageTranslator::preprocessor_pop() {
  return p_sage_preprocessor_recorder->pop();
}

size_t ClangToSageTranslator::preprocessor_list_size() {
  return p_sage_preprocessor_recorder->size();
}

// struct NextPreprocessorToInsert

// NextPreprocessorToInsert::NextPreprocessorToInsert(ClangToSageTranslator &
// translator_) :
NextPreprocessorToInsert::NextPreprocessorToInsert(
    ClangToSageTranslator &translator_)
    : cursor(NULL), candidat(NULL), next_to_insert(NULL),
      translator(translator_) {}

NextPreprocessorToInsert *NextPreprocessorToInsert::next() {
  if (!translator.preprocessor_pop())
    return NULL;

  NextPreprocessorToInsert *res = new NextPreprocessorToInsert(translator);

  std::pair<Sg_File_Info *, PreprocessingInfo *> next =
      translator.preprocessor_top();
  res->cursor = next.first;
  res->next_to_insert = next.second;
  res->candidat = candidat;
  return res;
}

// class

NextPreprocessorToInsert *PreprocessorInserter::evaluateInheritedAttribute(
    SgNode *astNode, NextPreprocessorToInsert *inheritedValue) {
  // Guard against null after final preprocessor insertion
  if (inheritedValue == NULL)
    return NULL;

  SgLocatedNode *loc_node = isSgLocatedNode(astNode);
  if (loc_node == NULL)
    return inheritedValue;

  Sg_File_Info *current_pos = loc_node->get_startOfConstruct();

  bool passed_cursor = *current_pos > *(inheritedValue->cursor);

  if (passed_cursor) {
    if (inheritedValue->next_to_insert != NULL) {
      loc_node->addToAttachedPreprocessingInfo(inheritedValue->next_to_insert);
    }
    NextPreprocessorToInsert *next_value = inheritedValue->next();
    if (next_value != NULL) {
      owned_inherited_.emplace_back(next_value);
    }
    return next_value;
  }

  return inheritedValue;
}

// class SagePreprocessorRecord

SagePreprocessorRecord::SagePreprocessorRecord(
    clang::SourceManager *source_manager)
    : p_source_manager(source_manager), p_preprocessor_record_list() {}

void SagePreprocessorRecord::InclusionDirective(
    clang::SourceLocation HashLoc, const clang::Token &IncludeTok,
    llvm::StringRef FileName, bool IsAngled, const clang::FileEntry *File,
    clang::SourceLocation EndLoc, llvm::StringRef SearchPath,
    llvm::StringRef RelativePath) {
  std::cerr << "InclusionDirective" << std::endl;

  bool inv_begin_line;
  bool inv_begin_col;

  unsigned ls =
      p_source_manager->getSpellingLineNumber(HashLoc, &inv_begin_line);
  unsigned cs =
      p_source_manager->getSpellingColumnNumber(HashLoc, &inv_begin_col);

  // In LLVM 20, getFileEntryForID still returns const FileEntry*
  std::string file = "";
  const clang::FileEntry *fileEntry =
      p_source_manager->getFileEntryForID(p_source_manager->getFileID(HashLoc));
  if (fileEntry) {
    // In LLVM 20, FileEntry uses tryGetRealPathName() instead of getName()
    file = fileEntry->tryGetRealPathName().str();
  }

  std::cerr << "    In file  : " << file << std::endl;
  std::cerr << "    From     : " << ls << ":" << cs << std::endl;
  std::cerr << "    Included : " << FileName.str() << std::endl;
  std::cerr << "    Is angled: " << (IsAngled ? "T" : "F") << std::endl;

  Sg_File_Info *file_info = new Sg_File_Info(file, ls, cs);
  PreprocessingInfo *preproc_info = new PreprocessingInfo(
      PreprocessingInfo::CpreprocessorIncludeDeclaration, FileName.str(), file,
      ls, cs, 0, PreprocessingInfo::before);

  p_preprocessor_record_list.push_back(
      std::pair<Sg_File_Info *, PreprocessingInfo *>(file_info, preproc_info));
}

void SagePreprocessorRecord::EndOfMainFile() {
  std::cerr << "EndOfMainFile" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Ident(clang::SourceLocation Loc,
                                   const std::string &str) {
  std::cerr << "Ident" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::PragmaComment(clang::SourceLocation Loc,
                                           const clang::IdentifierInfo *Kind,
                                           const std::string &Str) {
  std::cerr << "PragmaComment" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::PragmaMessage(clang::SourceLocation Loc,
                                           llvm::StringRef Str) {
  std::cerr << "PragmaMessage" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::PragmaDiagnosticPush(clang::SourceLocation Loc,
                                                  llvm::StringRef Namespace) {
  std::cerr << "PragmaDiagnosticPush" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::PragmaDiagnosticPop(clang::SourceLocation Loc,
                                                 llvm::StringRef Namespace) {
  std::cerr << "PragmaDiagnosticPop" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::PragmaDiagnostic(clang::SourceLocation Loc,
                                              llvm::StringRef Namespace,
                                              clang::diag::Severity Severity,
                                              llvm::StringRef Str) {
  std::cerr << "PragmaDiagnostic" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::MacroExpands(const clang::Token &MacroNameTok,
                                          const clang::MacroInfo *MI,
                                          clang::SourceRange Range) {
  std::cerr << "MacroExpands" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::MacroDefined(const clang::Token &MacroNameTok,
                                          const clang::MacroInfo *MI) {
  std::cerr << "" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::MacroUndefined(const clang::Token &MacroNameTok,
                                            const clang::MacroInfo *MI) {
  std::cerr << "MacroUndefined" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Defined(const clang::Token &MacroNameTok) {
  std::cerr << "Defined" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::SourceRangeSkipped(clang::SourceRange Range) {
  std::cerr << "SourceRangeSkipped" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::If(clang::SourceRange Range) {
  std::cerr << "If" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Elif(clang::SourceRange Range) {
  std::cerr << "Elif" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Ifdef(const clang::Token &MacroNameTok) {
  std::cerr << "Ifdef" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Ifndef(const clang::Token &MacroNameTok) {
  std::cerr << "Ifndef" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Else() {
  std::cerr << "Else" << std::endl;
  ROSE_ABORT();
}

void SagePreprocessorRecord::Endif() {
  std::cerr << "Endif" << std::endl;
  ROSE_ABORT();
}

std::pair<Sg_File_Info *, PreprocessingInfo *> SagePreprocessorRecord::top() {
  return p_preprocessor_record_list.front();
}

bool SagePreprocessorRecord::pop() {
  p_preprocessor_record_list.erase(p_preprocessor_record_list.begin());
  return !p_preprocessor_record_list.empty();
}
