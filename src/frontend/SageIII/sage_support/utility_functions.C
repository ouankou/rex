#include "sage3basic.h"

#if ROSE_WITH_LIBHARU
#include "AstPDFGeneration.h"
#endif

#include "AstDOTGeneration.h"

#include "processSupport.h"

#include "sageInterface/sageInterface.h"

#include "wholeAST_API.h"

#include "tokenStreamMapping.h"

#include "plugin.h"

#include <filesystem>
#include <time.h>

#include "rex-backend-link-options.h"
#include "rose_config.h"
#include "rose_path_resolver.h"
#include <signal.h>

using namespace std;
using namespace Rose;

namespace {

[[noreturn]] void tokenContractFailure(const SgSourceFile *sourceFile,
                                       const SgNode *node,
                                       const std::string &detail) {
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[token-map]: token-unparse contract "
          "violation: file=%s node=%s address=%p %s\n",
          sourceFile != NULL ? sourceFile->getFileName().c_str() : "<null>",
          node != NULL ? node->class_name().c_str() : "<null>",
          static_cast<const void *>(node), detail.c_str());
  ROSE_ABORT();
}

void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

int reportBackendFilesystemError(
    SgProject *project, const std::filesystem::filesystem_error &error) {
  // Filesystem failures terminate the requested operation through the public
  // backend status API. Normal unwinding releases the exception and AST state.
  fprintf(stderr, "REX_BACKEND_ERROR[filesystem]: %s\n", error.what());
  project->set_backendErrorCode(1);
  rosePhaseTrace("backend.end");
  return 1;
}
} // namespace

// global variable for turning on and off internal debugging.
int ROSE_DEBUG = 0;

Rose::Options Rose::global_options;
Rose::Options::Options() = default;

Rose::Options::Options(const Options &) = default;

Options &Rose::Options::operator=(const Options &) = default;

bool Rose::Options::get_backend_warnings() { return backend_warnings; }

void Rose::Options::set_backend_warnings(bool flag) { backend_warnings = flag; }

// Nonowning index of the token maps owned by live source-file nodes.
std::map<SgSourceFile *,
         std::map<SgNode *, TokenStreamSequenceToNodeMapping *> *>
    Rose::tokenSubsequenceMapOfMapsBySourceFile;

std::map<int, std::map<SgScopeStatement *, SgStatement *> *>
    Rose::representativeWhitespaceStatementMapOfMaps;

std::map<int, std::map<SgStatement *, MacroExpansion *> *>
    Rose::macroExpansionMapOfMaps;

bool Rose::is_C_language = false;
bool Rose::is_OpenMP_language = false;
bool Rose::is_C99_language = false;
bool Rose::is_Cxx_language = false;
bool Rose::is_Fortran_language = false;
bool Rose::is_CAF_language = false;
bool Rose::is_Cuda_language = false;
bool Rose::is_OpenCL_language = false;

std::string frontendVersionString() {
#ifdef ROSE_BUILD_CXX_LANGUAGE_SUPPORT
  string frontend_version = "Clang/LLVM 23 (experimental)";
#else
  string frontend_version = "unknown (C/C++ is disabled)";
#endif
  return frontend_version;
}

std::string version_message() {
  std::ostringstream ss;

  // NOTE: In the output below,
  //
  //   * a feature defined within ROSE, such as an analysis capability, is
  //   either "enabled" or "disabled".
  //
  //   * a library used by ROSE is either "used" (in which case we show the
  //   version number) or "unused".

  // Use the same version string as outut by the --version switch. This string
  // is usually "ROSE 0.x.y.z" but can be changed at runtime. Tools often change
  // this to be a tool version number followed by the ROSE version number.
  ss << version_number() << " (configured " << ROSE_CONFIGURE_DATE << ")\n";

  //-----------------------------------------------------------------------
  // GLobal information regardless of what languages ROSE is configured to
  // analyze
  //-----------------------------------------------------------------------

  // Show some indication of how optimized the ROSE library is. There's no
  // portable way to determine what compiler optimization flags are being used
  // to compile ROSE or even if that set constitutes "full" optimization,
  // whatever that might mean.  But we do know that assertions can prevent
  // certain types of optimization, not to mention that there are enough
  // assertions in ROSE that simply checking them at runtime takes measurably
  // significant time.
#ifdef NDEBUG
  ss << "  --- logic assertions:           disabled\n";
  // full optimizations *might* be enabled; we just don't know for sure
#else
  ss << "  --- logic assertions:           enabled\n";
  ss << "  --- full optimization:          disabled\n";
#endif

  //-----------------------------------------------------------------------
  // Information related to source language analysis.
  //-----------------------------------------------------------------------

#ifdef ROSE_ENABLE_SOURCE_ANALYSIS
  static const RosePathRoots roots = resolveRosePaths(nullptr);
  string build_tree_path =
      roots.build_root.empty() ? "not available" : roots.build_root;
  string install_path;
  if (!roots.install_prefix.empty()) {
    install_path = roots.install_prefix;
  } else if (!ROSE_INSTALL_PREFIX.empty()) {
    install_path = ROSE_INSTALL_PREFIX;
  } else {
    install_path = "not available";
  }
  ss << "  --- library build path:         " << build_tree_path << "\n";
  ss << "  --- original installation path: " << install_path << "\n";
#endif

  //-----------------------------------------------------------------------
  // Information related to C/C++ analysis.
  //-----------------------------------------------------------------------

#ifdef ROSE_BUILD_C_LANGUAGE_SUPPORT
  ss << "  --- C/C++ analysis:             enabled\n";
  extern string frontendVersionString();
  ss << "  ---   C/C++ front-end:          " << frontendVersionString() << "\n";

  ss << "  ---   C back-end:               "
     << BACKEND_C_COMPILER_MAJOR_VERSION_NUMBER << "."
     << BACKEND_C_COMPILER_MINOR_VERSION_NUMBER << " ("
     << BACKEND_C_COMPILER_NAME_WITH_PATH << ")\n";

  ss << "  ---   C++ back-end:             "
     << BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER << "."
     << BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER << " ("
     << BACKEND_CXX_COMPILER_NAME_WITH_PATH << ")\n";

#else
  ss << "  --- C/C++ analysis:             disabled\n";
#endif

  //-----------------------------------------------------------------------
  // Information related to Fortran analysis.
  //-----------------------------------------------------------------------

#ifdef ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT
  ss << "  --- Fortran analysis:           enabled\n";

  ss << "  ---   Fortran frontend:         Flang\n";

  ss << "  ---   Fortran back-end:         "
     << BACKEND_FORTRAN_COMPILER_MAJOR_VERSION_NUMBER << "."
     << BACKEND_FORTRAN_COMPILER_MINOR_VERSION_NUMBER << " ("
     << BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH << ")\n";

#else
  ss << "  --- Fortran analysis:           disabled\n";
#endif

  //-----------------------------------------------------------------------
  // Information related to other language analysis.
  //-----------------------------------------------------------------------

#ifdef ROSE_BUILD_CUDA_LANGUAGE_SUPPORT
  ss << "  --- CUDA analysis:              enabled\n";
#else
  ss << "  --- CUDA analysis:              disabled\n";
#endif

  return ss.str();
}

std::string version_number() { return ROSE_PACKAGE_VERSION; }

/*! \brief Call to frontend, processes commandline and generates a SgProject
   object.

    This function represents a simple interface to the use of ROSE as a library.
    The commandline is processed and the return parameter is the generate
   SgProject object.
 */

static SgProject::constant_folding_enum
legacyConstantFoldingChoice(bool frontendConstantFolding) {
  return frontendConstantFolding ? SgProject::e_folded_values_only
                                 : SgProject::e_original_expressions_only;
}

SgProject *frontend(int argc, char **argv, bool frontendConstantFolding) {
  return frontend(std::vector<std::string>(argv, argv + argc),
                  legacyConstantFoldingChoice(frontendConstantFolding));
}

SgProject *frontend(const std::vector<std::string> &argv,
                    bool frontendConstantFolding) {
  return frontend(argv, legacyConstantFoldingChoice(frontendConstantFolding));
}

SgProject *frontend(int argc, char **argv,
                    SgProject::constant_folding_enum frontendConstantFolding) {
  return frontend(std::vector<std::string>(argv, argv + argc),
                  frontendConstantFolding);
}

SgProject *frontend(const std::vector<std::string> &argv,
                    SgProject::constant_folding_enum frontendConstantFolding) {
  SgProject *project = nullptr;
  bool skip_postprocessing = false;
  {
    TimingPerformance timer("ROSE frontend():");

    // Syncs C++ and C I/O subsystems!
    ios::sync_with_stdio();

    // We parse plugin related command line options before calling project();
    std::vector<std::string> argv2 = argv; // workaround const argv
    Rose::processPluginCommandLine(argv2);

    {
      SgNode::SourceStateConstruction construction;
      project = new SgProject;
      ASSERT_not_null(project);
      project->set_frontendConstantFolding(frontendConstantFolding);
      SageInterface::registerAstTeardownProject(project);
      SageInterface::registerAstTeardownAtExit();
      rosePhaseTrace("project.parse.begin");
      project->parse(argv2);
      rosePhaseTrace("project.parse.end");
    }
    int frontend_status = project->get_frontendErrorCode();
    if (frontend_status != 0) {
      skip_postprocessing = true;
    }

    if (!skip_postprocessing) {
      // Set the mode to be transformation, mostly for Fortran. Liao 8/1/2013
      if (SageBuilder::SourcePositionClassificationMode ==
          SageBuilder::e_sourcePositionFrontendConstruction) {
        SageBuilder::setSourcePositionClassificationMode(
            SageBuilder::e_sourcePositionTransformation);
      }

      // Connect to Ast Plugin Mechanism
      rosePhaseTrace("plugins.begin");
      Rose::obtainAndExecuteActions(project);
      rosePhaseTrace("plugins.end");

      if (SageInterface::isAstTeardownEnabled()) {
        rosePhaseTrace("validateSymbolOwnership.begin");
        SageInterface::validateSymbolOwnership(project);
        rosePhaseTrace("validateSymbolOwnership.end");
      }
    }
  }
  return project;
}

int frontendExitStatus(const SgProject *project) {
  if (project == nullptr) {
    return 1;
  }

  int status = project->get_frontendErrorCode();
  if (status == 0) {
    return 0;
  }

  const SgFilePtrList &files = project->get_fileList();
  bool sawFile = false;
  bool allNegative = true;
  for (SgFile *file : files) {
    if (file != nullptr) {
      sawFile = true;
      if (!file->get_negative_test()) {
        allNegative = false;
        break;
      }
    }
  }
  if (sawFile && allNegative) {
    return 0;
  }

  return status;
}

/*! \brief Call to build SgProject with empty SgFiles.

    This function represents a simple interface to build a SgProject with all
   the SgFiles in place any initialized properly but containing a SgGlobal with
   no declarations. The purpose of this function is to build a SgProject with
   SgFile object for each file on the command line, and allow for the frontend
   to be called separately for each file. The commandline is completely
   processed so that all information is in each SgFile of the SgProject. This
   way specific files can be processed conditionally.  This mechanism can be
   used to build support for exclusion of specific files or for exclusion of
   files in a specific subdirectory.  Or to support specialized handling of
   files with a specific extension, etc.

 */

SgProject *frontendShell(int argc, char **argv) {
  return frontendShell(std::vector<std::string>(argv, argv + argc));
}

SgProject *frontendShell(const std::vector<std::string> &argv) {
  // Convert this to a list of strings to simplify editing (adding new option)
  Rose_STL_Container<string> commandLineList = argv;
  printf("frontendShell (top): argv = \n%s \n",
         StringUtility::listToString(commandLineList).c_str());

  // Invoke ROSE commandline option to skip internal frontend processing (we
  // will call the fronend explicitly for selected files, after construction of
  // SgProject).
  commandLineList.push_back("-rose:skip_rose");

  // Build the SgProject, but if the above option was used this will only build
  // empty SgFile nodes
  SgProject *project = frontend(commandLineList);
  ASSERT_not_null(project);

  project->display("In frontendShell(), after frontend()");

  for (auto file : project->get_fileList()) {
    // Get the local command line so that we can remove the "-rose:skip_rose"
    // option
    vector<string> local_argv = file->get_originalCommandLineArgumentList();

    // Note that we have to remove the "-rose:skip_rose" option that was saved
    CommandlineProcessing::removeArgs(local_argv, "-rose:skip_rose");

    // Set the new commandline (without the "-rose:skip_rose" option)
    file->get_originalCommandLineArgumentList() = local_argv;

    // Things set by "-rose:skip_rose" option, which must be unset (reset to
    // default valees)!
    file->set_skip_transformation(false);
    file->set_useBackendOnly(false);
    file->set_skipfinalCompileStep(false);

    // Skip all processing of comments
    file->set_skip_commentsAndDirectives(false);
  }

  return project;
}

namespace {

bool isLocatedNodeInSourceFile(const SgLocatedNode *node,
                               const SgSourceFile *sourceFile) {
  if (node == NULL || sourceFile == NULL) {
    return false;
  }

  const Sg_File_Info *nodeInfo = node->get_file_info();
  const Sg_File_Info *sourceInfo = sourceFile->get_file_info();
  if (nodeInfo != NULL && sourceInfo != NULL && nodeInfo->get_file_id() >= 0 &&
      sourceInfo->get_file_id() >= 0) {
    return nodeInfo->get_file_id() == sourceInfo->get_file_id();
  }

  if (nodeInfo != NULL) {
    const std::string nodeFilename = nodeInfo->get_filenameString();
    if (!nodeFilename.empty()) {
      return nodeFilename == sourceFile->getFileName();
    }
  }

  // getEnclosingSourceFile is legacy and not const-correct; it only walks
  // parent/source-file links and does not mutate the node, so this cast is
  // safe for read-only ownership lookup. Use it only when file-info ownership
  // is unavailable so header declarations attached under a translation unit do
  // not get charged to the wrong token stream.
  SgSourceFile *owner =
      SageInterface::getEnclosingSourceFile(const_cast<SgLocatedNode *>(node));
  return owner != NULL && owner == sourceFile;
}

void assertTokenSubsequenceWithinBounds(
    const SgSourceFile *sourceFile, const SgNode *node,
    const TokenStreamSequenceToNodeMapping *mapping, size_t tokenCount) {
  if (mapping == NULL) {
    tokenContractFailure(sourceFile, node, "null mapping");
  }

  const TokenStreamHalfOpenInterval &core =
      mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);

  if (tokenCount == 0) {
    if (core.begin != 0 || core.end != 0) {
      tokenContractFailure(sourceFile, node,
                           "non-empty interval for an empty token stream");
    }
    return;
  }

  if (core.begin < 0 || core.end <= core.begin ||
      static_cast<size_t>(core.end) > tokenCount) {
    std::ostringstream detail;
    detail << "invalid core interval [" << core.begin << "," << core.end
           << ") for " << tokenCount << " tokens";
    tokenContractFailure(sourceFile, node, detail.str());
  }
}

void enforceTokenUnparseContractForFile(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);

  const bool isCOrCxx =
      sourceFile->get_C_only() || sourceFile->get_Cxx_only() ||
      sourceFile->get_Cuda_only() || sourceFile->get_OpenCL_only();
  if (!isCOrCxx || sourceFile->get_unparse_tokens() == false) {
    return;
  }

  SgGlobal *globalScope = isSgGlobal(sourceFile->get_globalScope());
  ASSERT_not_null(globalScope);

  std::map<SgSourceFile *, std::map<SgNode *, TokenStreamSequenceToNodeMapping
                                                  *> *>::const_iterator mapIt =
      Rose::tokenSubsequenceMapOfMapsBySourceFile.find(sourceFile);
  if (mapIt == Rose::tokenSubsequenceMapOfMapsBySourceFile.end() ||
      mapIt->second == NULL) {
    tokenContractFailure(sourceFile, sourceFile,
                         "missing per-file token-map entry");
  }

  const std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      sourceFile->get_tokenSubsequenceMap();

  std::vector<stream_element *> tokenVector = getTokenStream(sourceFile);
  const size_t tokenCount = tokenVector.size();

  if (tokenCount == 0) {
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::const_iterator
        globalIt = tokenMap.find(globalScope);
    if (globalIt != tokenMap.end() && globalIt->second != NULL) {
      assertTokenSubsequenceWithinBounds(sourceFile, globalScope,
                                         globalIt->second, tokenCount);
    }
    return;
  }

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::const_iterator
      globalIt = tokenMap.find(globalScope);
  if (globalIt == tokenMap.end() || globalIt->second == NULL) {
    tokenContractFailure(sourceFile, globalScope,
                         "missing global-scope mapping");
  }

  assertTokenSubsequenceWithinBounds(sourceFile, globalScope, globalIt->second,
                                     tokenCount);

  const SgDeclarationStatementPtrList &declarations =
      globalScope->get_declarations();
  size_t requiredTopLevelMappings = 0;
  struct RequiredMapping {
    const SgDeclarationStatement *declaration;
    const TokenStreamSequenceToNodeMapping *mapping;
  };
  std::vector<RequiredMapping> requiredMappings;

  for (SgDeclarationStatement *decl : declarations) {
    if (decl == NULL) {
      continue;
    }

    Sg_File_Info *declInfo = decl->get_file_info();
    if (declInfo == NULL || declInfo->isCompilerGenerated() ||
        declInfo->isTransformation() ||
        declInfo->isOutputInCodeGeneration() == false) {
      continue;
    }

    if (!isLocatedNodeInSourceFile(decl, sourceFile)) {
      continue;
    }

    // A declaration proven by the frontend to be only one semantic fragment
    // of a macro replacement has no independent lexical token surface.  The
    // token-mapping builder therefore rejects an entry for it; the final
    // unparse contract must enforce the same ownership rule instead of
    // requiring the mapping that construction correctly prohibited.
    if (decl->get_source_range_is_macro_expansion_fragment()) {
      continue;
    }

    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      if (!class_decl->get_isAutonomousDeclaration() ||
          class_decl->get_isUnNamed()) {
        continue;
      }
    }

    if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl)) {
      if (!enum_decl->get_isAutonomousDeclaration() ||
          enum_decl->get_isUnNamed()) {
        continue;
      }
    }

    // Template-instantiation directives are structural wrappers around the
    // declaration that owns the source interval. Linkage markers, by contrast,
    // are source-backed declarations with exact token ownership and must
    // satisfy the same mapping contract as every other emitted top-level
    // declaration.
    if (isSgTemplateInstantiationDirectiveStatement(decl) != NULL) {
      continue;
    }

    // Declarations synthesized or structurally changed by ROSE are emitted from
    // the AST instead of replaying an original token subsequence.
    SgFunctionDeclaration *functionDecl = isSgFunctionDeclaration(decl);
    if (decl->get_isModified() || decl->isTransformation() ||
        (functionDecl != NULL && functionDecl->get_is_implicit_function())) {
      continue;
    }

    requiredTopLevelMappings += 1;

    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::const_iterator
        declIt = tokenMap.find(decl);
    if (declIt == tokenMap.end() || declIt->second == NULL) {
      std::ostringstream detail;
      detail << " parent="
             << (decl->get_parent() != NULL ? decl->get_parent()->class_name()
                                            : std::string("<null>"));
      if (SgScopeStatement *scope = decl->get_scope()) {
        detail << " scope=" << scope->class_name();
      } else {
        detail << " scope=<null>";
      }
      if (Sg_File_Info *fi = decl->get_file_info()) {
        detail << " file=" << fi->get_filenameString() << ":" << fi->get_line()
               << ":" << fi->get_col();
      }
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
        detail << " name=" << class_decl->get_name().getString();
      }
      MLOG_ERROR_CXX("sageSupport")
          << "token-unparse contract violation: token-map missing top-level "
          << "declaration entry " << decl << " (" << decl->class_name()
          << ") in file " << sourceFile->getFileName() << detail.str()
          << std::endl;
      tokenContractFailure(sourceFile, decl,
                           "missing required top-level mapping" + detail.str());
    }

    assertTokenSubsequenceWithinBounds(sourceFile, decl, declIt->second,
                                       tokenCount);
    requiredMappings.push_back({decl, declIt->second});
  }

  std::sort(requiredMappings.begin(), requiredMappings.end(),
            [](const RequiredMapping &lhs, const RequiredMapping &rhs) {
              const TokenStreamHalfOpenInterval &lhs_core =
                  lhs.mapping->halfOpenInterval(
                      TokenStreamIntervalKind::token_subsequence);
              const TokenStreamHalfOpenInterval &rhs_core =
                  rhs.mapping->halfOpenInterval(
                      TokenStreamIntervalKind::token_subsequence);
              if (lhs_core.begin != rhs_core.begin) {
                return lhs_core.begin < rhs_core.begin;
              }
              return lhs_core.end < rhs_core.end;
            });
  for (size_t index = 1; index < requiredMappings.size(); ++index) {
    const RequiredMapping &previous = requiredMappings[index - 1];
    const RequiredMapping &current = requiredMappings[index];
    if (current.mapping == previous.mapping) {
      continue;
    }
    const TokenStreamHalfOpenInterval &previous_core =
        previous.mapping->halfOpenInterval(
            TokenStreamIntervalKind::token_subsequence);
    const TokenStreamHalfOpenInterval &current_core =
        current.mapping->halfOpenInterval(
            TokenStreamIntervalKind::token_subsequence);
    if (current_core.begin < previous_core.end) {
      std::ostringstream detail;
      detail << "overlapping top-level intervals [" << previous_core.begin
             << "," << previous_core.end << ") owned by "
             << previous.declaration->class_name();
      if (Sg_File_Info *start = previous.declaration->get_startOfConstruct()) {
        detail << " source=" << start->get_physical_line() << ":"
               << start->get_col();
      }
      if (Sg_File_Info *end = previous.declaration->get_endOfConstruct()) {
        detail << "-" << end->get_physical_line() << ":" << end->get_col();
      }
      detail << " macro-ended="
             << previous.declaration->get_source_range_ends_in_macro_expansion()
             << " and [" << current_core.begin << "," << current_core.end
             << ") owned by " << current.declaration->class_name();
      if (Sg_File_Info *start = current.declaration->get_startOfConstruct()) {
        detail << " source=" << start->get_physical_line() << ":"
               << start->get_col();
      }
      if (Sg_File_Info *end = current.declaration->get_endOfConstruct()) {
        detail << "-" << end->get_physical_line() << ":" << end->get_col();
      }
      tokenContractFailure(sourceFile, current.declaration, detail.str());
    }
  }

  if (requiredTopLevelMappings > 0 && tokenMap.size() <= 1) {
    std::ostringstream detail;
    detail << "map contains only the global mapping but "
           << requiredTopLevelMappings
           << " top-level declarations require coverage";
    tokenContractFailure(sourceFile, globalScope, detail.str());
  }
}

void enforceTokenUnparseContractImpl(SgProject *project) {
  ASSERT_not_null(project);

  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile == NULL) {
      continue;
    }

    enforceTokenUnparseContractForFile(sourceFile);
  }
}

} // namespace

void enforceTokenUnparseContract(SgProject *project) {
  enforceTokenUnparseContractImpl(project);
}

/** Generate source from the AST and compile it, or link an object-only project.
 * Frontend failures are returned before any output is generated. Filesystem
 * failures stop the operation and are reported as a nonzero backend status.
 */
int backend(SgProject *project, UnparseFormatHelp *unparseFormatHelp,
            UnparseDelegate *unparseDelegate) try {
  ROSE_ASSERT(project != nullptr);
  TimingPerformance timer("AST Object Code Generation (backend):");
  rosePhaseTrace("backend.begin");

  int finalCombinedExitStatus = 0;

  const int frontendStatus = frontendExitStatus(project);
  if (frontendStatus != 0) {
    project->set_backendErrorCode(frontendStatus);
    rosePhaseTrace("backend.end");
    return frontendStatus;
  }

  if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL) {
    printf("Inside of backend(SgProject*) \n");
  }

  if (project->get_useBackendOnly() == false) {
    if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL) {
      cout << "Calling project->unparse()\n";
    }

    rosePhaseTrace("backend.unparse.begin");
    project->unparse(unparseFormatHelp, unparseDelegate);
    rosePhaseTrace("backend.unparse.end");

    if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL) {
      cout << "source file(s) generated. (from AST)\n" << endl;
    }
  }

  if (project->numberOfFiles() > 0 || project->numberOfDirectories() > 0) {
    // Compile generated C++ source code with vendor compiler.
    // Generate object file (required for further template processing
    // if templates exist).
    if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL) {
      cout << "Calling project->compileOutput()\n";
    }

    rosePhaseTrace("backend.compileOutput.begin");
    finalCombinedExitStatus = project->compileOutput();
    rosePhaseTrace("backend.compileOutput.end");
  } else {
    if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
      printf("   project->get_compileOnly() = %s \n",
             project->get_compileOnly() ? "true" : "false");

    if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
      printf("Calling project->link() \n");

    if (project->get_C_only() == true) {
      printf(
          "Link using the C language linker (when handling C programs) = %s \n",
          BACKEND_C_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.begin");
      finalCombinedExitStatus =
          project->link(BACKEND_C_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.end");
    } else if (project->get_Fortran_only() == true) {
      printf("Link using the Fortran language linker (when handling Fortran "
             "programs) = %s \n",
             BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.begin");
      finalCombinedExitStatus =
          project->link(BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.end");
    } else {
      if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
        printf("Link using the default linker (when handling non-C programs) = "
               "%s \n",
               BACKEND_CXX_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.begin");
      finalCombinedExitStatus =
          project->link(BACKEND_CXX_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.end");
    }
  }

  if (SgProject::get_verbose() >= 1)
    cout << "source file(s) compiled with vendor compiler. (exit status = "
         << finalCombinedExitStatus << ").\n"
         << endl;

  // Set the final error code to be returned to the user.
  project->set_backendErrorCode(finalCombinedExitStatus);
  int backendStatus = project->get_backendErrorCode();

  rosePhaseTrace("backend.end");
  return backendStatus;
} catch (const std::filesystem::filesystem_error &error) {
  return reportBackendFilesystemError(project, error);
}

int backendCompilesUsingOriginalInputFile(
    SgProject *project, bool compile_with_USE_ROSE_macro) try {
  ROSE_ASSERT(project != nullptr);
  const int frontendStatus = frontendExitStatus(project);
  if (frontendStatus != 0) {
    project->set_backendErrorCode(frontendStatus);
    return frontendStatus;
  }
  // Analysis tools compile the original sources without unparsing the AST.
  SgStringList commandLineToGenerateObjectFile;

  enum language_enum {
    e_none = 0,
    e_c = 1,
    e_cxx = 2,
    e_fortran = 3,
    e_last_language
  };

  bool hasCInput = false;
  bool hasCxxInput = false;
  bool hasFortranInput = false;
  auto recordExactFileLanguage = [&](const SgFile *file) {
    if (file == nullptr) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[project-language]: project contains a "
              "null file\n");
      ROSE_ABORT();
    }

    if (file->get_Cuda_only()) {
      hasCxxInput = true;
      return;
    }
    if (file->get_OpenCL_only()) {
      hasCInput = true;
      return;
    }

    switch (file->get_inputLanguage()) {
    case SgFile::e_C_language:
      hasCInput = true;
      return;
    case SgFile::e_Cxx_language:
      hasCxxInput = true;
      return;
    case SgFile::e_Fortran_language:
      hasFortranInput = true;
      return;
    default:
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[project-language]: file '%s' has no "
              "supported exact input language (value=%d)\n",
              file->getFileName().c_str(),
              static_cast<int>(file->get_inputLanguage()));
      ROSE_ABORT();
    }
  };

  if (project->numberOfFiles() > 0) {
    for (const SgFile *file : project->get_fileList()) {
      recordExactFileLanguage(file);
    }
  } else {
    hasCInput = project->get_C_only();
    hasCxxInput = project->get_Cxx_only();
    hasFortranInput = project->get_Fortran_only();
  }

  if (hasFortranInput && (hasCInput || hasCxxInput)) {
    fprintf(stderr,
            "REX_BACKEND_INVARIANT[project-language]: one original-input "
            "backend command cannot compile a mixed Fortran and C/C++ "
            "project\n");
    ROSE_ABORT();
  }

  // Clang selects each translation unit's language from its exact -x setting
  // or suffix.  A mixed C/C++ link must use the C++ driver so that the C++
  // runtime is present; this does not change any SgProject/SgFile language
  // metadata.
  const language_enum language = hasFortranInput ? e_fortran
                                 : hasCxxInput   ? e_cxx
                                 : hasCInput     ? e_c
                                                 : e_none;

  if (language == e_none) {
    fprintf(stderr,
            "REX_BACKEND_INVARIANT[project-language]: object generation "
            "requires an exact project language\n");
    ROSE_ABORT();
  }

  switch (language) {
  case e_c:
    commandLineToGenerateObjectFile.insert(
        commandLineToGenerateObjectFile.end(),
        RexBackend::cCompilerCommand.begin(),
        RexBackend::cCompilerCommand.end());
    break;
  case e_cxx:
    commandLineToGenerateObjectFile.insert(
        commandLineToGenerateObjectFile.end(),
        RexBackend::cxxCompilerCommand.begin(),
        RexBackend::cxxCompilerCommand.end());
    break;
  case e_fortran:
    commandLineToGenerateObjectFile.insert(
        commandLineToGenerateObjectFile.end(),
        RexBackend::fortranCompilerCommand.begin(),
        RexBackend::fortranCompilerCommand.end());
    break;

  default: {
    printf("Default reached in switch in "
           "backendCompilesUsingOriginalInputFile() \n");
    ROSE_ABORT();
  }
  }

  if (compile_with_USE_ROSE_macro == true) {
    commandLineToGenerateObjectFile.push_back("-DUSE_ROSE");
  }

  int finalCombinedExitStatus = 0;
  if (project->numberOfFiles() > 0) {
    SgStringList originalCommandLineArgumentList =
        project->get_originalCommandLineArgumentList();

    SgFile::stripRoseCommandLineOptions(originalCommandLineArgumentList);
    if (language == e_fortran) {
      SgFile::validateFortranCommandLineOptions(
          originalCommandLineArgumentList);
    }

    struct MixedSourceLanguageState {
      std::string selectedDriverName;
      std::string restoredDriverName;
    };
    std::map<std::string, MixedSourceLanguageState> mixedSourceLanguages;
    if (hasCInput && hasCxxInput) {
      for (const SgFile *file : project->get_fileList()) {
        ASSERT_not_null(file);
        const std::string sourcePath =
            StringUtility::getAbsolutePathFromRelativePath(file->getFileName());
        using CommandlineProcessing::ClangLanguageFamily;
        const CommandlineProcessing::ClangLanguageSelection selection =
            CommandlineProcessing::clangLanguageSelectionForSource(
                originalCommandLineArgumentList, file->getFileName());
        ClangLanguageFamily expectedFamily;
        if (file->get_Cuda_only()) {
          expectedFamily = ClangLanguageFamily::Cuda;
        } else if (file->get_OpenCL_only()) {
          expectedFamily = ClangLanguageFamily::OpenCL;
        } else if (file->get_inputLanguage() == SgFile::e_C_language) {
          expectedFamily = ClangLanguageFamily::C;
        } else if (file->get_inputLanguage() == SgFile::e_Cxx_language) {
          expectedFamily = ClangLanguageFamily::Cxx;
        } else {
          fprintf(stderr,
                  "REX_BACKEND_INVARIANT[project-language]: mixed C/C++ "
                  "project contains non-Clang source '%s'\n",
                  file->getFileName().c_str());
          ROSE_ABORT();
        }
        if (selection.isExplicit() && selection.family != expectedFamily) {
          fprintf(stderr,
                  "REX_BACKEND_INVARIANT[project-language]: source '%s' has "
                  "Clang -x language '%s' inconsistent with its exact AST "
                  "language\n",
                  file->getFileName().c_str(), selection.driverName.c_str());
          ROSE_ABORT();
        }

        CommandlineProcessing::ClangLanguageSelection suffixSelection;
        if (!selection.isExplicit()) {
          suffixSelection =
              CommandlineProcessing::clangLanguageSelectionForSuffix(
                  file->getFileName());
          if (suffixSelection.family != expectedFamily ||
              suffixSelection.driverName.empty()) {
            fprintf(stderr,
                    "REX_BACKEND_INVARIANT[project-language]: source '%s' "
                    "has no exact Clang suffix language consistent with its "
                    "AST language\n",
                    file->getFileName().c_str());
            ROSE_ABORT();
          }
        }

        // The temporary selection must preserve exact modes such as
        // c-header and cpp-output. Restore the state active before this source
        // immediately afterwards so a following object, archive, or source is
        // classified exactly as it was on the original driver command line.
        MixedSourceLanguageState languageState;
        languageState.selectedDriverName = selection.isExplicit()
                                               ? selection.driverName
                                               : suffixSelection.driverName;
        languageState.restoredDriverName =
            selection.isExplicit() ? selection.driverName : "none";
        if (!mixedSourceLanguages.emplace(sourcePath, languageState).second) {
          fprintf(stderr,
                  "REX_BACKEND_INVARIANT[project-language]: mixed C/C++ "
                  "project contains duplicate source identity '%s'\n",
                  sourcePath.c_str());
          ROSE_ABORT();
        }
      }
    }

    size_t mixedSourcesPublished = 0;
    bool afterDoubleDash = false;
    for (size_t index = 1; index < originalCommandLineArgumentList.size();
         ++index) {
      const std::string &argument = originalCommandLineArgumentList[index];
      if (!afterDoubleDash && argument == "--") {
        afterDoubleDash = true;
        commandLineToGenerateObjectFile.push_back(argument);
        continue;
      }

      if (!afterDoubleDash &&
          CommandlineProcessing::isOptionTakingSecondParameter(argument)) {
        const size_t operandCount =
            CommandlineProcessing::isOptionTakingThirdParameter(argument) ? 2
                                                                          : 1;
        if (operandCount >= originalCommandLineArgumentList.size() - index) {
          fprintf(stderr,
                  "REX_BACKEND_INVARIANT[backend-option-operand]: option '%s' "
                  "requires %zu argument(s)\n",
                  argument.c_str(), operandCount);
          ROSE_ABORT();
        }
        commandLineToGenerateObjectFile.push_back(argument);
        for (size_t operand = 0; operand < operandCount; ++operand) {
          commandLineToGenerateObjectFile.push_back(
              originalCommandLineArgumentList[++index]);
        }
        continue;
      }

      if (!mixedSourceLanguages.empty() &&
          (afterDoubleDash || argument.empty() || argument.front() != '-') &&
          std::filesystem::exists(argument)) {
        const std::string operandPath =
            StringUtility::getAbsolutePathFromRelativePath(argument);
        const auto languageIt = mixedSourceLanguages.find(operandPath);
        if (languageIt != mixedSourceLanguages.end()) {
          if (afterDoubleDash) {
            fprintf(stderr,
                    "REX_BACKEND_INVARIANT[project-language]: cannot publish "
                    "the exact language of mixed source '%s' after '--'\n",
                    argument.c_str());
            ROSE_ABORT();
          }
          commandLineToGenerateObjectFile.push_back("-x");
          commandLineToGenerateObjectFile.push_back(
              languageIt->second.selectedDriverName);
          commandLineToGenerateObjectFile.push_back(argument);
          commandLineToGenerateObjectFile.push_back("-x");
          commandLineToGenerateObjectFile.push_back(
              languageIt->second.restoredDriverName);
          ++mixedSourcesPublished;
          continue;
        }
      }

      commandLineToGenerateObjectFile.push_back(argument);
    }
    if (mixedSourcesPublished != mixedSourceLanguages.size()) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[project-language]: published %zu of %zu "
              "mixed C/C++ source language identities\n",
              mixedSourcesPublished, mixedSourceLanguages.size());
      ROSE_ABORT();
    }

    if (SgProject::get_verbose() >= 1) {
      printf("Compile Line: ");
      for (const std::string &argument : commandLineToGenerateObjectFile) {
        printf("%s ", argument.c_str());
      }
      printf("\n");
    }

    if (project->get_compileOnly() == true) {
      bool addCompileOnlyFlag = true;
      for (const auto &arg : commandLineToGenerateObjectFile) {
        if (arg == "-c") {
          addCompileOnlyFlag = false;
          break;
        }
      }
      if (addCompileOnlyFlag) {
        commandLineToGenerateObjectFile.push_back("-c");
      }
    }

    if (project->get_skipfinalCompileStep() == false) {
      finalCombinedExitStatus =
          systemFromVector(commandLineToGenerateObjectFile);
    }
  } else {
    // Note that in general it is not possible to tell whether to use the C,
    // C++, or Fortran backend compiler to do the linking. When we just have a
    // list of object files then we can't assume anything (and
    // project->get_C_only() will be false). Note that
    // commandLineToGenerateObjectFile is just the name of the backend compiler
    // to use! JL (03/15/2018) Put in ROSE_ASSERT to verify command line is just
    // the linker Thats all link is supposed to take
    ROSE_ASSERT(commandLineToGenerateObjectFile.size() == 1);
    finalCombinedExitStatus = project->link(commandLineToGenerateObjectFile[0]);
  }

  project->set_backendErrorCode(finalCombinedExitStatus);
  return finalCombinedExitStatus;
} catch (const std::filesystem::filesystem_error &error) {
  return reportBackendFilesystemError(project, error);
}

int backendGeneratesSourceCodeButCompilesUsingOriginalInputFile(
    SgProject *project) try {
  ROSE_ASSERT(project != nullptr);
  const int frontendStatus = frontendExitStatus(project);
  if (frontendStatus != 0) {
    project->set_backendErrorCode(frontendStatus);
    return frontendStatus;
  }
  // Exercise source generation, then compile the original input for tools
  // whose transformations are not intended to produce executable output.
  if (project->get_skip_unparse() == false) {
    project->unparse();
  }

  return backendCompilesUsingOriginalInputFile(project);
} catch (const std::filesystem::filesystem_error &error) {
  return reportBackendFilesystemError(project, error);
}

void generatePDF(const SgProject &project) {
  TimingPerformance timer("ROSE generatePDF():");

  if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
    printf("Inside of generatePDF \n");

  // Output the source code file (as represented by the SAGE AST) as a PDF file
  // (with bookmarks)
#if ROSE_WITH_LIBHARU
  AstPDFGeneration pdftest;
  SgProject &nonconstProject = (SgProject &)project;
  pdftest.generateInputFiles(&nonconstProject);
#else
  printf("Warning: libharu support is not enabled\n");
#endif
}

void generateDOT(const SgProject &project, std::string filenamePostfix,
                 bool excludeTemplateInstantiations) {
  TimingPerformance timer("ROSE generateDOT():");

  AstDOTGeneration astdotgen;
  SgProject &nonconstProject = (SgProject &)project;

  int maxSize = 10000000;

  int numberOfASTnodes = numberOfNodes();

  if (SgProject::get_verbose() >= 1)
    printf("In generateDOT(): numberOfASTnodes = %d maxSize = %d \n",
           numberOfASTnodes, maxSize);

  if (numberOfASTnodes < maxSize) {
    astdotgen.generateInputFiles(
        &nonconstProject, DOTGeneration<SgNode *>::TOPDOWNBOTTOMUP,
        filenamePostfix, excludeTemplateInstantiations);
  } else {
    if (SgProject::get_verbose() >= 0)
      printf("In generateDOT(): AST graph too large to generate. "
             "(numberOfASTnodes=%d) > (maxSize=%d) \n",
             numberOfASTnodes, maxSize);
  }
}

void generateDOT(SgNode *node, std::string filename) {
  TimingPerformance timer("ROSE generateDOT():");

  AstDOTGeneration astdotgen;
  astdotgen.generate(node, filename, DOTGeneration<SgNode *>::TOPDOWNBOTTOMUP,
                     "");
}

void generateDOT_withIncludes(const SgProject &project,
                              std::string filenamePostfix) {
  TimingPerformance timer("ROSE generateDOT_withIncludes():");

  AstDOTGeneration astdotgen;
  SgProject &nonconstProject = (SgProject &)project;

  astdotgen.generate(&nonconstProject, AstDOTGeneration::TOPDOWNBOTTOMUP,
                     filenamePostfix);
}

void generateDOTforMultipleFile(const SgProject &project,
                                std::string filenamePostfix) {
  TimingPerformance timer("ROSE generateDOTforMultipleFile():");

  generateDOT_withIncludes(project, filenamePostfix);
}

void generateAstGraph(const SgProject *project, int maxSize,
                      std::string filenameSuffix) {
  TimingPerformance timer("ROSE generateAstGraph():");

  // Generate a name from all the files on the command line
  string filename =
      SageInterface::generateProjectName(project, /* supressSuffix = */ true);

  filename += "_WholeAST";

  filename += filenameSuffix;

  int numberOfASTnodes = numberOfNodes();

  if (SgProject::get_verbose() >= 1)
    printf("In generateAstGraph(): numberOfASTnodes = %d maxSize = %d filename "
           "= %s \n",
           numberOfASTnodes, maxSize, filename.c_str());

  if (numberOfASTnodes < maxSize) {
    std::vector<std::string> argvList =
        project->get_originalCommandLineArgumentList();
    CustomMemoryPoolDOTGeneration::s_Filter_Flags filter_flags(argvList);
    generateWholeGraphOfAST(filename, &filter_flags);
  } else {
    if (SgProject::get_verbose() >= 1)
      printf("In generateAstGraph(): WHOLE AST graph too large to generate. "
             "(numberOfASTnodes=%d) > (maxSize=%d) \n",
             numberOfASTnodes, maxSize);
  }
}

std::string
Rose::utility_stripPathFromFileName(const std::string &fileNameWithPath) {
  size_t pos = fileNameWithPath.rfind('/');
  if (pos == std::string::npos || pos == fileNameWithPath.size() - 1) {
    return fileNameWithPath;
  } else {
    return fileNameWithPath.substr(pos + 1);
  }
}

string Rose::getPathFromFileName(const string fileName) {
  size_t pos = fileName.rfind('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0) {
    return "/";
  }
  return fileName.substr(0, pos);
}

string Rose::getWorkingDirectory() {
  return std::filesystem::current_path().string();
}

void Rose::usage() { SgFile::usage(); }

SgStatement *Rose::getNextStatement(SgStatement *currentStatement) {
  ROSE_ASSERT(currentStatement != NULL);
  // A source declaration group is the lexical owner of an ordered sequence of
  // semantic declarations.  Its members are deliberately absent from the
  // surrounding scope list, so iteration must first advance within that exact
  // typed owner and then continue from the group wrapper.
  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(currentStatement->get_parent())) {
    group->validate();
    const SgDeclarationStatementPtrList &members = group->get_declarations();
    auto position = std::find(members.begin(), members.end(), currentStatement);
    if (position == members.end() ||
        std::count(members.begin(), members.end(), currentStatement) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[statement-iteration]: declaration=%p "
              "type=%s is not owned exactly once by group=%p\n",
              static_cast<void *>(currentStatement),
              currentStatement->class_name().c_str(),
              static_cast<void *>(group));
      ROSE_ABORT();
    }
    ++position;
    if (position != members.end()) {
      return *position;
    }
    currentStatement = group;
  }

  // SgForInitStatement owns a real ordered statement list but is not a scope.
  // Handle that list directly instead of trying to find its children in their
  // semantic scope's unrelated statement list.
  if (SgForInitStatement *for_init =
          isSgForInitStatement(currentStatement->get_parent())) {
    const SgStatementPtrList &initializers = for_init->get_init_stmt();
    auto position =
        std::find(initializers.begin(), initializers.end(), currentStatement);
    if (position == initializers.end() ||
        std::count(initializers.begin(), initializers.end(),
                   currentStatement) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[statement-iteration]: initializer=%p "
              "type=%s is not owned exactly once by for-init=%p\n",
              static_cast<void *>(currentStatement),
              currentStatement->class_name().c_str(),
              static_cast<void *>(for_init));
      ROSE_ABORT();
    }
    ++position;
    return position == initializers.end() ? nullptr : *position;
  }

  SgStatement *nextStatement = NULL;
  SgScopeStatement *scope = currentStatement->get_scope();
  ROSE_ASSERT(scope != NULL);

  // If this statement is nested under a label, it is not directly present in
  // the enclosing scope's statement list. Advance from the label statement
  // itself (which *is* in that list) to preserve linear statement iteration.
  if (SgLabelStatement *label_parent =
          isSgLabelStatement(currentStatement->get_parent())) {
    SgScopeStatement *label_scope =
        isSgScopeStatement(label_parent->get_parent());
    if (label_scope == nullptr ||
        label_scope->containsOnlyDeclarations() == true ||
        isSgDeclarationScope(label_scope) != nullptr) {
      label_scope = scope;
    }
    ROSE_ASSERT(label_scope != NULL);

    // C/C++ labels have function scope semantically, but the label statement is
    // threaded through its enclosing statement-list scope.
    if (!isSgDeclarationScope(label_scope) &&
        label_scope->containsOnlyDeclarations() == false) {
      SgStatementPtrList &statementList = label_scope->getStatementList();
      Rose_STL_Container<SgStatement *>::iterator i;
      for (i = statementList.begin();
           i != statementList.end() && (*i) != label_parent; ++i) {
      }

      if (i == statementList.end()) {
        MLOG_FATAL_CXX("sageSupport")
            << "fatal error: ROSE::getNextStatement(): label statement is not "
               "found within its scope's statement list"
            << endl;
        MLOG_FATAL_CXX("sageSupport") << "current statement is "
                                      << currentStatement->class_name() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << currentStatement->get_file_info()->displayString() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << "label statement is " << label_parent->class_name() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << label_parent->get_file_info()->displayString() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << "Its scope is " << label_scope->class_name() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << label_scope->get_file_info()->displayString() << endl;
        ROSE_ABORT();
      }

      ++i;
      return (i == statementList.end()) ? nullptr : *i;
    }
  }

  if (isSgGlobal(currentStatement) != NULL)
    return NULL;

  // Make sure that we didn't get ourselves back from the get_scope()
  // function (previous bug fixed, but tested here).
  ROSE_ASSERT(scope != currentStatement);

  if (SgSwitchStatement *switch_scope = isSgSwitchStatement(scope)) {
    if (switch_scope->get_item_selector() == currentStatement) {
      SgBasicBlock *switch_body =
          SageInterface::ensureBasicBlockAsBodyOfSwitch(switch_scope);
      SgStatementPtrList &body_statements = switch_body->getStatementList();
      return body_statements.empty() ? nullptr : body_statements.front();
    }
  }

  switch (currentStatement->variantT()) {
  case V_SgForInitStatement:
  case V_SgClassDefinition:
  case V_SgFunctionDefinition:
  case V_SgStatement:
  case V_SgFunctionParameterList: {
    ROSE_ABORT();
  }

  case V_SgLabelStatement: {
    SgLabelStatement *labelStatement = isSgLabelStatement(currentStatement);
    nextStatement = labelStatement->get_statement();
    ROSE_ASSERT(nextStatement != NULL);
    break;
  }

  default: {
    // We have to handle the cases of a SgStatementPtrList and a
    // SgDeclarationStatementPtrList separately
    if (isSgDeclarationScope(scope))
      return nullptr;
    if (scope->containsOnlyDeclarations()) {
      // Usually a global scope or class declaration scope
      SgDeclarationStatementPtrList &declarationList =
          scope->getDeclarationList();
      Rose_STL_Container<SgDeclarationStatement *>::iterator i;
      for (i = declarationList.begin();
           (i != declarationList.end() && (*i) != currentStatement); i++) {
      }
      if (i == declarationList.end()) {
        fprintf(stderr,
                "REX_AST_INVARIANT[statement-iteration]: declaration=%p "
                "type=%s is absent from scope=%p type=%s declaration list\n",
                static_cast<void *>(currentStatement),
                currentStatement->class_name().c_str(),
                static_cast<void *>(scope), scope->class_name().c_str());
        ROSE_ABORT();
      } else {
        i++;
        if (declarationList.end() == i)
          nextStatement = NULL;
        else
          nextStatement = *i;
      }
    } else {
      SgStatementPtrList &statementList = scope->getStatementList();
      Rose_STL_Container<SgStatement *>::iterator i;
      for (i = statementList.begin();
           i != statementList.end() && (*i) != currentStatement; i++) {
      }

      // currentStatement is not found in the list
      if (i == statementList.end()) {
        MLOG_FATAL_CXX("sageSupport")
            << "fatal error: ROSE::getNextStatement(): current statement is "
               "not found within its scope's statement list"
            << endl;
        MLOG_FATAL_CXX("sageSupport") << "current statement is "
                                      << currentStatement->class_name() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << currentStatement->get_file_info()->displayString() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << "Its scope is " << scope->class_name() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << scope->get_file_info()->displayString() << endl;
        ROSE_ABORT();
      }

      // now i == currentStatement
      ROSE_ASSERT(*i == currentStatement);

      ROSE_ASSERT(i != statementList.end());

      i++;
      if (statementList.end() == i)
        nextStatement = NULL;
      else
        nextStatement = *i;
    }

    break;
  }
  }

  return nextStatement;
}

static SgStatement *getPreviousStatement_support_for_declaration_list(
    SgScopeStatement *parent_scope, SgStatement *targetStatement,
    bool climbOutScope /*= true*/) {
  // This supports scopes that contain declaration statement lists (SgGlobal,
  // SgClassDefinition, etc.)

  ROSE_ASSERT(parent_scope != NULL);
  ROSE_ASSERT(targetStatement != NULL);

  ROSE_ASSERT(parent_scope->containsOnlyDeclarations() == true);

  SgStatement *previousStatement = NULL;

  // Usually a global scope or class declaration scope
  SgDeclarationStatementPtrList &declarationList =
      parent_scope->getDeclarationList();

  Rose_STL_Container<SgDeclarationStatement *>::iterator targetIterator =
      find(declarationList.begin(), declarationList.end(), targetStatement);

  if (targetIterator == declarationList.end()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[statement-iteration]: declaration=%p type=%s "
            "is absent from scope=%p type=%s declaration list\n",
            static_cast<void *>(targetStatement),
            targetStatement->class_name().c_str(),
            static_cast<void *>(parent_scope),
            parent_scope->class_name().c_str());
    ROSE_ABORT();
  }

  if (targetIterator == declarationList.begin()) {
    if (climbOutScope) {
      previousStatement = isSgStatement(targetStatement->get_parent());
      ROSE_ASSERT(previousStatement != NULL);
    }
  } else {
    Rose_STL_Container<SgDeclarationStatement *>::iterator
        previousStatementIterator = --targetIterator;
    previousStatement = *previousStatementIterator;

    ROSE_ASSERT(previousStatement != targetStatement);
  }

  if (climbOutScope) {
    ROSE_ASSERT(isSgGlobal(targetStatement) != NULL ||
                previousStatement != NULL);
  }

  return previousStatement;
}

static SgStatement *
getPreviousStatement_support_for_statement_list(SgScopeStatement *parent_scope,
                                                SgStatement *targetStatement,
                                                bool climbOutScope /*= true*/) {
  // This supports scopes that contain statement lists (SgBasicBlock, etc.)

  ROSE_ASSERT(parent_scope != NULL);
  ROSE_ASSERT(targetStatement != NULL);

  ROSE_ASSERT(parent_scope->containsOnlyDeclarations() == false);

  SgStatement *previousStatement = NULL;

  // Usually a SgBasicBlock scope
  SgStatementPtrList &statementList = parent_scope->getStatementList();

  Rose_STL_Container<SgStatement *>::iterator targetIterator =
      find(statementList.begin(), statementList.end(), targetStatement);

  if (targetIterator == statementList.end()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[statement-iteration]: statement=%p type=%s is "
            "absent from scope=%p type=%s statement list\n",
            static_cast<void *>(targetStatement),
            targetStatement->class_name().c_str(),
            static_cast<void *>(parent_scope),
            parent_scope->class_name().c_str());
    ROSE_ABORT();
  }

  if (targetIterator == statementList.begin()) {
    if (climbOutScope) {
      previousStatement = isSgStatement(targetStatement->get_parent());
      ROSE_ASSERT(previousStatement != NULL);
    }
  } else {
    Rose_STL_Container<SgStatement *>::iterator previousStatementIterator =
        --targetIterator;
    previousStatement = *previousStatementIterator;

    ROSE_ASSERT(previousStatement != targetStatement);
  }

  if (climbOutScope) {
    ROSE_ASSERT(isSgGlobal(targetStatement) != NULL ||
                previousStatement != NULL);
  }

  return previousStatement;
}

SgStatement *Rose::getPreviousStatement(SgStatement *targetStatement,
                                        bool climbOutScope /*= true*/) {
  // Note that the option to specify climbOutScope is only used in Liao's
  // arithmeticIntensity tool (specifically in
  // midend/programAnalysis/arithmeticIntensity/ai_measurement.cpp).

  ROSE_ASSERT(targetStatement != NULL);

  SgStatement *previousStatement = NULL;

  // Mirror getNextStatement's exact lexical ordering for typed source
  // declaration groups.  The first member continues from the wrapper; later
  // members return their preceding semantic declaration directly.
  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(targetStatement->get_parent())) {
    group->validate();
    const SgDeclarationStatementPtrList &members = group->get_declarations();
    auto position = std::find(members.begin(), members.end(), targetStatement);
    if (position == members.end() ||
        std::count(members.begin(), members.end(), targetStatement) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[statement-iteration]: declaration=%p "
              "type=%s is not owned exactly once by group=%p\n",
              static_cast<void *>(targetStatement),
              targetStatement->class_name().c_str(),
              static_cast<void *>(group));
      ROSE_ABORT();
    }
    if (position != members.begin()) {
      return *std::prev(position);
    }
    targetStatement = group;
  }

  if (SgForInitStatement *for_init =
          isSgForInitStatement(targetStatement->get_parent())) {
    const SgStatementPtrList &initializers = for_init->get_init_stmt();
    auto position =
        std::find(initializers.begin(), initializers.end(), targetStatement);
    if (position == initializers.end() ||
        std::count(initializers.begin(), initializers.end(), targetStatement) !=
            1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[statement-iteration]: initializer=%p "
              "type=%s is not owned exactly once by for-init=%p\n",
              static_cast<void *>(targetStatement),
              targetStatement->class_name().c_str(),
              static_cast<void *>(for_init));
      ROSE_ABORT();
    }
    if (position != initializers.begin()) {
      return *std::prev(position);
    }
    return climbOutScope ? for_init : nullptr;
  }

  if (isSgGlobal(targetStatement) != NULL) {
    return NULL;
  }

  SgStatement *const parentStatement =
      isSgStatement(targetStatement->get_parent());
  ROSE_ASSERT(parentStatement != NULL);

  // Liao 5/10/2010, special case when a true/false body of a if statement is
  // not a basic block since getStatementList() is not defined for a if
  // statement. We define the previous statement of the true/false body to be
  // the if statement This is consistent with the later handling that when a
  // statement is the first in a parent, treat the parent as the previous
  // statement PP 5/22/2024, generalize for a number of scope statements with
  // similar property

  const bool isSpecialScopeStatement =
      (isSgIfStmt(parentStatement) || isSgWhileStmt(parentStatement) ||
       isSgForStatement(parentStatement) || isSgDoWhileStmt(parentStatement) ||
       isSgSwitchStatement(parentStatement));

  if (isSpecialScopeStatement) {
    // the target statement is a child of a special statement
    //   => previousStatement = parentStatement
    //   unless climbOutScope is provided, in which case there is none.
    if (climbOutScope)
      previousStatement = parentStatement;
    else
      ROSE_ASSERT(previousStatement == NULL);
  } else if (SgScopeStatement *parent_scope =
                 isSgScopeStatement(parentStatement)) {
    if (parent_scope->containsOnlyDeclarations() == true) {
      // Examples of this case would be a SgGlobal, SgClassDefinition, and some
      // other scopes.
      previousStatement = getPreviousStatement_support_for_declaration_list(
          parent_scope, targetStatement, climbOutScope);
    } else {
      // Examples of this case would be a SgBasicBlock, and some other scopes.
      ROSE_ASSERT(parent_scope->containsOnlyDeclarations() == false);

      previousStatement = getPreviousStatement_support_for_statement_list(
          parent_scope, targetStatement, climbOutScope);
    }
  } else {
    // Not clear what kinds of statements these should be.
    if (climbOutScope) {
      previousStatement = parentStatement;
      ROSE_ASSERT(previousStatement != NULL);
    }
  }

  if (climbOutScope) {
    ROSE_ASSERT(isSgGlobal(targetStatement) != NULL ||
                previousStatement != NULL);
  }

  return previousStatement;
}
