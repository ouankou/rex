// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "checkIsModifiedFlag.h"

#if ROSE_WITH_LIBHARU
#include "AstPDFGeneration.h"
#endif

#include "AstDOTGeneration.h"

#include "processSupport.h"

#include "sageInterface/sageInterface.h"

#include "wholeAST_API.h"

#include "tokenStreamMapping.h"

#include "plugin.h"

#include <time.h>

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
#include "rose_path_resolver.h"
// DQ (9/8/2017): Debugging ROSE_ASSERT. Call sighandler_t signal(int signum,
// sighandler_t handler);
#include <signal.h>

// DQ (12/31/2005): This is OK if not declared in a header file
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
} // namespace

// global variable for turning on and off internal debugging.
int ROSE_DEBUG = 0;

// DQ (3/6/2017): Adding ROSE options data structure to support frontend and
// backend options (see header file for details).
Rose::Options Rose::global_options;
Rose::Options::Options() {
  // DQ (3/6/2017): Default option value to minimize the chattyness of ROSE
  // based tools.
  frontend_notes = false;
  frontend_warnings = false;
  backend_notes = false;
  backend_warnings = false;
}

// DQ (3/6/2017): Adding ROSE options data structure to support frontend and
// backend options (see header file for details).
Rose::Options::Options(const Options &X) {
  // DQ (3/6/2017): Default option value to minimize the chattyness of ROSE
  // based tools.
  frontend_notes = X.frontend_notes;
  frontend_warnings = X.frontend_warnings;
  backend_notes = X.backend_notes;
  backend_warnings = X.backend_warnings;
}

// DQ (3/6/2017): Adding ROSE options data structure to support frontend and
// backend options (see header file for details).
Options &Rose::Options::operator=(const Options &X) {
  // DQ (3/6/2017): Default option value to minimize the chattyness of ROSE
  // based tools.
  frontend_notes = X.frontend_notes;
  frontend_warnings = X.frontend_warnings;
  backend_notes = X.backend_notes;
  backend_warnings = X.backend_warnings;

  return *this;
}

bool Rose::Options::get_frontend_notes() { return frontend_notes; }

void Rose::Options::set_frontend_notes(bool flag) { frontend_notes = flag; }

bool Rose::Options::get_frontend_warnings() { return frontend_warnings; }

void Rose::Options::set_frontend_warnings(bool flag) {
  frontend_warnings = flag;
}

bool Rose::Options::get_backend_notes() { return backend_notes; }

void Rose::Options::set_backend_notes(bool flag) { backend_notes = flag; }

bool Rose::Options::get_backend_warnings() { return backend_warnings; }

void Rose::Options::set_backend_warnings(bool flag) { backend_warnings = flag; }

#define OUTPUT_TO_FILE true
#define DEBUG_COPY_EDIT false

// DQ (9/27/2018): We need to build multiple maps, one for each file (to support
// token based unparsing for multiple files, such as what is required when using
// the unparsing header files feature). DQ (10/28/2013): Put the token sequence
// map here, it is set and accessed via member functions on the SgSourceFile IR
// node. std::map<SgNode*,TokenStreamSequenceToNodeMapping*>
// Rose::tokenSubsequenceMap;
// std::set<int,std::map<SgNode*,TokenStreamSequenceToNodeMapping*> >
// Rose::tokenSubsequenceMapSet;
std::map<int, std::map<SgNode *, TokenStreamSequenceToNodeMapping *> *>
    Rose::tokenSubsequenceMapOfMaps;

// DQ (1/19/2021): This is part of moving to a new map that uses the
// SgSourceFile pointer instead of the file_id.
std::map<SgSourceFile *,
         std::map<SgNode *, TokenStreamSequenceToNodeMapping *> *>
    Rose::tokenSubsequenceMapOfMapsBySourceFile;

// DQ (11/20/2015): Provide a statement to use as a key in the token sequence
// map to get representative whitespace.
// std::map<SgScopeStatement*,SgStatement*>
// Rose::representativeWhitespaceStatementMap;
std::map<int, std::map<SgScopeStatement *, SgStatement *> *>
    Rose::representativeWhitespaceStatementMapOfMaps;

// DQ (11/30/2015): Provide a statement to use as a key in the macro expansion
// map to get info about macro expansions.
// std::map<SgStatement*,MacroExpansion*> Rose::macroExpansionMap;
std::map<int, std::map<SgStatement *, MacroExpansion *> *>
    Rose::macroExpansionMapOfMaps;

// DQ (10/29/2018): Build a map for the unparser to use to locate SgIncludeFile
// IR nodes.

// DQ (11/25/2020): These are the boolean variables that are computed in the
// function compute_language_kind() and inlined via the
// SageInterface::is_<language kind>_language() functions.  See more details
// comment in the header file.
bool Rose::is_C_language = false;
bool Rose::is_OpenMP_language = false;
bool Rose::is_C99_language = false;
bool Rose::is_Cxx_language = false;
bool Rose::is_Fortran_language = false;
bool Rose::is_CAF_language = false;
bool Rose::is_Cuda_language = false;
bool Rose::is_OpenCL_language = false;

// DQ (3/24/2016): Adding Robb's message logging mechanism to contrl output
// debug message from the legacy frontend/ROSE connection code.

// DQ (4/17/2010): This function must be defined if C++ support in ROSE is
// disabled. REX: legacy frontend has been replaced with Clang/LLVM frontend
std::string frontendVersionString() {
#ifdef ROSE_BUILD_CXX_LANGUAGE_SUPPORT
  string frontend_version = "Clang/LLVM 22 (experimental)";
#else
  string frontend_version = "unknown (C/C++ is disabled)";
#endif
  return frontend_version;
}

// DQ (11/1/2009): replaced "version()" with separate "version_number()" and
// "version_message()" functions.
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

#if defined(ROSE_BUILD_CPP_LANGUAGE_SUPPORT) ||                                \
    defined(ROSE_BUILD_C_LANGUAGE_SUPPORT)
  ss << "  --- C/C++ analysis:             enabled\n";
  extern string frontendVersionString();
  ss << "  ---   C/C++ front-end:          " << frontendVersionString() << "\n";

  // This prints (originally and still now) the version of the C++ compiler
  // instead of the C compiler. This is fine for the normal case where both
  // compilers come from the same compiler collection, but would be wrong, for
  // instance, if the user configured ROSE to use a C compiler from GCC and a
  // C++ compiler from LLVM. [Robb Matzke 2021-08-18]
  ss << "  ---   C back-end:               "
     << BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER << "."
     << BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER << " ("
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
  // Information related to other language analysis, alphabetically. These
  // CPP symbols with weird and inconsistent names come from ROSE's
  // configure-time checks. If you remove them from this list because they're
  // not supported anymore, then kindly also remove them from the rest of the
  // ROSE library source code and tests!
  //-----------------------------------------------------------------------

#ifdef ROSE_BUILD_CUDA_LANGUAGE_SUPPORT
  ss << "  --- CUDA analysis:              enabled\n";
#else
  ss << "  --- CUDA analysis:              disabled\n";
#endif

#ifdef ROSE_BUILD_OPENCL_LANGUAGE_SUPPORT
  ss << "  --- OpenCL analysis:            enabled\n";
#else
  ss << "  --- OpenCL analysis:            disabled\n";
#endif

  return ss.str();
}

// DQ (11/1/2009): replaced "version()" with separate "version_number()" and
// "version_message()" functions.
std::string version_number() {
#ifdef VERSION
  // returns a string containing the current version number
  // the VERSION macro is defined at compile time on the
  // compile command line by the build system
  return VERSION;
#else
  ROSE_ASSERT(!"Expected CPP macro VERSION to be defined");
#endif
}

// DQ (4/17/2010): This function must be defined if C++ support in ROSE is
// disabled.
void outputPredefinedMacros() {
  printf("Output of relevant pre-defined macros in ROSE: \n");
#ifdef __GNUC__
  printf("   macro: __GNUC__ = %d \n", __GNUC__);
#endif
#ifdef __GNUC_MINOR__
  printf("   macro: __GNUC_MINOR__ = %d \n", __GNUC_MINOR__);
#endif
#ifdef __VERSION__
  printf("   macro: __VERSION__ = %s \n", __VERSION__);
#endif
#ifdef __GNUC_PATCHLEVEL__
  printf("   macro: __GNUC_PATCHLEVEL__ = %d \n", __GNUC_PATCHLEVEL__);
#endif
#ifdef __GNUC__
  printf("   macro: __GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__ "
         "= %d \n",
         __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__);
#endif
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  printf("   macro: __GXX_EXPERIMENTAL_CXX0X__ = %d \n",
         __GXX_EXPERIMENTAL_CXX0X__);
#endif

#ifdef XXX
  printf("   macro: XXX = %d \n", XXX);
#endif
}

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
  // DQ (6/14/2007): Added support for timing of high level frontend function.
  SgProject *project = nullptr;
  bool skip_postprocessing = false;
  {
    TimingPerformance timer("ROSE frontend():");

    // Syncs C++ and C I/O subsystems!
    ios::sync_with_stdio();

    // We parse plugin related command line options before calling project();
    std::vector<std::string> argv2 = argv; // workaround const argv
    Rose::processPluginCommandLine(argv2);

    // Separate the creation of a new project with building the AST. The default
    // constructor initializes the object and then parse is used to build the
    // AST (in a separate step). The current constructors that conflate object
    // creation with parsing should be deprecated. [Rasmussen, 2024.04.03]
    project = new SgProject;
    ASSERT_not_null(project);
    project->set_frontendConstantFolding(frontendConstantFolding);
    SageInterface::registerAstTeardownProject(project);

    // Ensure teardown hooks are registered before parsing so early exits get
    // cleanup.
    SageInterface::registerAstTeardownAtExit();

    // Create the AST by setting command-line options and then parsing all files
    // from the command line
    rosePhaseTrace("project.parse.begin");
    project->parse(argv2);
    rosePhaseTrace("project.parse.end");
    int frontend_status = project->get_frontendErrorCode();
    if (frontend_status != 0) {
      skip_postprocessing = true;
    }

    if (!skip_postprocessing) {
      // DQ (1/27/2017): Comment this out so that we can generate the dot graph
      // to debug symbol with null basis.
      unsetNodesMarkedAsModified(project);

      // Set the mode to be transformation, mostly for Fortran. Liao 8/1/2013
      if (SageBuilder::SourcePositionClassificationMode ==
          SageBuilder::e_sourcePositionFrontendConstruction) {
        SageBuilder::setSourcePositionClassificationMode(
            SageBuilder::e_sourcePositionTransformation);
      }

      // Rose::AST::cmdline::graphviz.frontend.exec(project);
      // Rose::AST::cmdline::checker.frontend.exec(project);

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

/*! \brief Call to backend, generates either object file or executable.

    This function operates in two modes:
        1) If source files were specified on the command line, then it calls
           unparser and compiles generated file.
        2) If no source files are present then it operates as a linker
   processing all specified object files. If no source files or object files
   are specified then we return a error.

    This function represents a simple interface to the use of ROSE as a
   library.

     At this point in the control flow we have returned from the processing
   via the legacy frontend frontend (or skipped it if that option was
   specified). The following has been done or explicitly skipped if such
   options were specified on the commandline: 1) The application program has
   been parsed 2) All AST's have been build (one for each grammar) 3) The
   transformations have been edited into the C++ AST 4) The C++ AST has been
   unparsed to form the final output file (all code has been generated into a
   different filename "rose_<original file name>.C")

    \internal The error code is returned, but it might be appropriate to make
              it more similar to the frontend() function and its handling of
              the error code.
 */
int backend(SgProject *project, UnparseFormatHelp *unparseFormatHelp,
            UnparseDelegate *unparseDelegate) {
  // DQ (7/12/2005): Introduce tracking of performance of ROSE.
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

  // Rose::AST::cmdline::graphviz.backend.exec(project);
  // Rose::AST::cmdline::checker.backend.exec(project);

  // printf ("   project->get_useBackendOnly() = %s
  // \n",project->get_useBackendOnly() ? "true" : "false");
  if (project->get_useBackendOnly() == false) {
    // Add forward references for instantiated template functions and member
    // functions (which are by default defined at the bottom of the file (but
    // should be declared at the top once we know what instantiations should be
    // built)).  They must be defined at the bottom since they could call other
    // functions not yet declared in the file.  Note that this fixup is required
    // since we have skipped the class template definitions which would contain
    // the declarations that we are generating.  We might need that as a
    // solution at some point if this fails to be sufficently robust. if (
    // SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL-2 )
    //      printf ("Calling fixupInstantiatedTemplates() \n");
    // generate C++ source code
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

    // DQ (5/20/2005): If we have not permitted templates to be
    // instantiated during initial compilation then we have to do the
    // prelink step (this is however still new and somewhat problematic
    // (buggy?)).  It relies upon the legacy frontend mechansisms which
    // are not well understood.
    bool callTemplateInstantation =
        (project->get_template_instantiation_mode() == SgProject::e_none);

    if (callTemplateInstantation == true) {
      // DQ (9/6/2005): I think that this is no longer needed
      printf(
          "I don't think we need to call instantiateTemplates() any more! \n");
      ROSE_ABORT();

      // The instantiation of templates can cause new projects (sets of source
      // files) to be generated, but since the object files are already
      // processed this is not an issue here.  A project might, additionally,
      // keep track of the ASTs associated with the different phases of
      // instantions of templates.
      if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
        printf("Calling instantiateTemplates() \n");

      printf("Skipping template support in backend(SgProject*) \n");
      // instantiateTemplates (project);
    }

    if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
      printf("Calling project->link() \n");

    // DQ (10/15/2005): Trap out case of C programs where we want to make sure
    // that we don't use the C++ compiler to do our linking! This could be done
    // in the
    if (project->get_C_only() == true) {
      printf(
          "Link using the C language linker (when handling C programs) = %s \n",
          BACKEND_C_COMPILER_NAME_WITH_PATH);
      // finalCombinedExitStatus = project->link("gcc");
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
      // Use the default name for C++ compiler (defined at configure time)
      if (SgProject::get_verbose() >= BACKEND_VERBOSE_LEVEL)
        printf("Link using the default linker (when handling non-C programs) = "
               "%s \n",
               BACKEND_CXX_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.begin");
      finalCombinedExitStatus =
          project->link(BACKEND_CXX_COMPILER_NAME_WITH_PATH);
      rosePhaseTrace("backend.link.end");
    }

    // printf ("DONE with link! \n");
  }

  // Message from backend to user.
  // Avoid all I/O to stdout if useBackendOnly == true.
  // if (project->get_useBackendOnly() == false)
  if (SgProject::get_verbose() >= 1)
    cout << "source file(s) compiled with vendor compiler. (exit status = "
         << finalCombinedExitStatus << ").\n"
         << endl;

  // Set the final error code to be returned to the user.
  project->set_backendErrorCode(finalCombinedExitStatus);
  int backendStatus = project->get_backendErrorCode();

  rosePhaseTrace("backend.end");
  return backendStatus;
}

int backendCompilesUsingOriginalInputFile(SgProject *project,
                                          bool compile_with_USE_ROSE_macro) {
  // DQ (8/24/2009):
  // To work with existing makefile systems, we want to force an object file to
  // be generated. So we want to call the backend compiler on the original input
  // file (instead of generating an new file from the AST and running it through
  // the backend).  The whole point is to gnerate a object file.  The effect is
  // that this does a less agressive test of ROSE but test only the parts
  // required for analysis tools instead of transformation tools. This avoids
  // some programs in the name qualification support that is called by the
  // backend and permits testing of the parts of ROSE relevant for analysis
  // tools (e.g. Compass). Of course we eventually want everything to work, but
  // I want to test the compilation of ROSE using ROSE as part of test to get
  // Compass running regularly on ROSE.

  // Note that the command generated may have to be fixup later to include more
  // subtle details required to link libraries, etc.  At present this function
  // only handles the support required to build an object file.
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
    commandLineToGenerateObjectFile.push_back(
        BACKEND_C_COMPILER_NAME_WITH_PATH);
    break;
  case e_cxx:
    commandLineToGenerateObjectFile.push_back(
        BACKEND_CXX_COMPILER_NAME_WITH_PATH);
    break;
  case e_fortran:
    commandLineToGenerateObjectFile.push_back(
        BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH);
    break;

  default: {
    printf("Default reached in switch in "
           "backendCompilesUsingOriginalInputFile() \n");
    ROSE_ABORT();
  }
  }

  if (compile_with_USE_ROSE_macro == true) {
    // DQ (11/3/2011): Mark this as being called from ROSE (even though the
    // backend compiler is being used). This will help us detect where strings
    // handed in using -D options may have lost some outer quotes. There may
    // also be a better fix to detect quoted strings and requote them, so this
    // should be considered also.
    commandLineToGenerateObjectFile.push_back("-DUSE_ROSE");
  }

  int finalCombinedExitStatus = 0;
  if (project->numberOfFiles() > 0) {
    SgStringList originalCommandLineArgumentList =
        project->get_originalCommandLineArgumentList();

    // DQ (2/20/2010): Added filtering of options that should not be passed to
    // the vendor compiler.
    SgFile::stripRoseCommandLineOptions(originalCommandLineArgumentList);
    if (language == e_fortran) {
      SgFile::stripFortranCommandLineOptions(originalCommandLineArgumentList);
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
            StringUtility::getAbsolutePathFromRelativePath(file->getFileName(),
                                                           true);
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
          (afterDoubleDash || argument.empty() || argument.front() != '-')) {
        const std::string operandPath =
            StringUtility::getAbsolutePathFromRelativePath(argument, true);
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

    // DQ (12/28/2010): If we specified to NOT compile the input code then don't
    // do so even when it is the original code. This is important for Fortran
    // 2003 test codes that are not compiled by the backend compiler and for
    // which the tests/nonsmoke/functional/testTokenGeneration.C translator
    // uses this function to generate object files. finalCombinedExitStatus =
    // system (commandLineToGenerateObjectFile.c_str());
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

  return finalCombinedExitStatus;
}

int backendGeneratesSourceCodeButCompilesUsingOriginalInputFile(
    SgProject *project) {
  // DQ (2/6/2010): This function is a step between calling the backend()
  // and calling backendCompilesUsingOriginalInputFile().  It it used
  // the test the generation of the source code, but not the compilation of
  // it using the backend (vendor) compiler.  This is used to test ROSE.

  // Users are likely to either want to use backend() to generate the source
  // code for there project and it compiled (e.g. for optimization) or call
  // backendCompilesUsingOriginalInputFile() to process the input code and
  // then generate object files or executables from the original code
  // (e.g for analysis).

  // This instance of complexity is why this needs to be a separate backend
  // function. Note that file->get_skip_unparse() will be false when the "-E"
  // option, and the unparse() function will properly assert that it should be
  // true.
  if (project->get_skip_unparse() == false) {
    project->unparse();
  }

  return backendCompilesUsingOriginalInputFile(project);
}

void generatePDF(const SgProject &project) {
  // DQ (6/14/2007): Added support for timing of the generatePDF() function.
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
  // DQ (7/4/2008): Added default parameter to support the filenamePostfix
  // mechanism in AstDOTGeneration

  // DQ (6/14/2007): Added support for timing of the generateDOT() function.
  TimingPerformance timer("ROSE generateDOT():");

  AstDOTGeneration astdotgen;
  SgProject &nonconstProject = (SgProject &)project;

  // DQ (12/14/2018): The number of nodes is computed globally, but the graph is
  // genereated only for the input file. So this can suppress the generation of
  // the graph when there are a large number of IR nodes from header files.
  // Multiplied the previous value by 10 to support building the smaller graph
  // of the input file. DQ (2/18/2013): Generating a DOT file of over a million
  // IR nodes is too much. int maxSize = 1000000;
  int maxSize = 10000000;

  int numberOfASTnodes = numberOfNodes();

  if (SgProject::get_verbose() >= 1)
    printf("In generateDOT(): numberOfASTnodes = %d maxSize = %d \n",
           numberOfASTnodes, maxSize);

  // DQ (2/18/2013): Compute the number of IR nodes for the AST and limit the
  // size of these graphs (take too long to generate and the graphs are not
  // useful).
  if (numberOfASTnodes < maxSize) {
    // Note that the use of generateInputFiles causes the graph to be generated
    // for only the input source file and not any included header files. The
    // result is a much smaller file (and generally a more useful one).
    // DQ (9/1/2008): This is the default for the last long while, but the
    // SgProject IR nodes is not being processed (which appears to be a bug).
    // This is because in the implementation of the generateInputFiles the
    // function traverseInputFiles is called.
    // astdotgen.generateInputFiles(&nonconstProject,DOTGeneration<SgNode*>::TOPDOWNBOTTOMUP,filenamePostfix);
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
  // DQ (9/22/2017): This function is being provided to support the generation
  // of a dot file from any subtree. The more imediate use for this function is
  // to support generation of dot files from trees built using the ROSE Untyped
  // nodes.

  // DQ (6/14/2007): Added support for timing of the generateDOT() function.
  TimingPerformance timer("ROSE generateDOT():");

  AstDOTGeneration astdotgen;

  // This used to be the default, but it would output too much data (from
  // include files). std::string filenamePostfix = ".dot";
  std::string filenamePostfix = "";
  astdotgen.generate(node, filename, DOTGeneration<SgNode *>::TOPDOWNBOTTOMUP,
                     filenamePostfix);
}

void generateDOT_withIncludes(const SgProject &project,
                              std::string filenamePostfix) {
  TimingPerformance timer("ROSE generateDOT_withIncludes():");

  AstDOTGeneration astdotgen;
  SgProject &nonconstProject = (SgProject &)project;

  // Note that the use of generateInputFiles causes the graph to be generated
  // for only the input source file and not any included header files. The
  // result is a much smaller file (and generally a more useful one).
  // This used to be the default, but it would output too much data (from
  // include files). It is particularly useful when handling multiple files on
  // the command line and traversing the files included from each file.
  // astdotgen.generate(&nonconstProject);
  // DOTGeneration::traversalType tt = TOPDOWNBOTTOMUP;
  AstDOTGeneration::traversalType tt = AstDOTGeneration::TOPDOWNBOTTOMUP;
  astdotgen.generate(&nonconstProject, tt, filenamePostfix);
}

void generateDOTforMultipleFile(const SgProject &project,
                                std::string filenamePostfix) {
  TimingPerformance timer("ROSE generateDOTforMultipleFile():");

  // This is the best way to handle generation of DOT files where multiple files
  // are specified on the command line.  Later we may be able to filter out the
  // include files (but this is a bit difficult until generateInputFiles() can
  // be implemetned to call the evaluation of inherited and synthesized
  // attributes.
  generateDOT_withIncludes(project, filenamePostfix);
}

void generateAstGraph(const SgProject *project, int maxSize,
                      std::string filenameSuffix)
// void generateAstGraph ( const SgProject* project, int maxSize, std::string
// filenameSuffix, CustomMemoryPoolDOTGeneration::s_Filter_Flags* filter_flags)
{
  // DQ (6/14/2007): Added support for timing of the generateAstGraph()
  // function.
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

  // Compute the number of IR nodes for the AST
  if (numberOfASTnodes < maxSize) {
    // generateWholeGraphOfAST(filename);

    // Added support to handle options to control filtering of Whole AST graphs.
    // std::vector<std::string>  argvList (argv, argv+ argc);
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

int Rose::getLineNumber(SgLocatedNode *locatedNodePointer) {
  // Get the line number from the Sage II statement object
  ROSE_ASSERT(locatedNodePointer != NULL);
  int lineNumber = -1;
  // Sometimes the locatedNode doesn't have a SgFile object
  // (likely because it is part of a parent statement object)
  if (locatedNodePointer->get_file_info() != NULL) {
    ROSE_ASSERT(locatedNodePointer->get_file_info() != NULL);
    ROSE_ASSERT(locatedNodePointer->get_file_info()->get_filename() != NULL);
    Sg_File_Info *fileInfo = locatedNodePointer->get_file_info();
    lineNumber = fileInfo->get_line();
  }

  return lineNumber;
}
int Rose::getColumnNumber(SgLocatedNode *locatedNodePointer) {
  // Get the line number from the Sage II statement object
  ROSE_ASSERT(locatedNodePointer != NULL);
  int columnNumber = -1;
  // Sometimes the locatedNode doesn't have a SgFile object
  // (likely because it is part of a parent statement object)
  if (locatedNodePointer->get_file_info() != NULL) {
    ROSE_ASSERT(locatedNodePointer->get_file_info() != NULL);
    ROSE_ASSERT(locatedNodePointer->get_file_info()->get_filename() != NULL);
    Sg_File_Info *fileInfo = locatedNodePointer->get_file_info();
    columnNumber = fileInfo->get_col();
  }

  return columnNumber;
}
std::string Rose::getFileName(SgLocatedNode *locatedNodePointer) {
  // Get the filename from the Sage II statement object
  ROSE_ASSERT(locatedNodePointer != NULL);
  std::string fileName = "NO NAME FILE";
  // Sometimes the locatedNode doesn't have a SgFile object
  // (likely because it is part of a parent statement object)
  if (locatedNodePointer->get_file_info() != NULL) {
    ROSE_ASSERT(locatedNodePointer->get_file_info() != NULL);
    // printf ("In Rose::getFileName(): locatedNodePointer->get_file_info() = %p
    // \n",locatedNodePointer->get_file_info());
    Sg_File_Info *fileInfo = locatedNodePointer->get_file_info();
    fileName = fileInfo->get_filenameString();
  }

  return fileName;
}
bool Rose::isPartOfTransformation(SgLocatedNode *locatedNodePointer) {
  bool result = false;
  Sg_File_Info *fileInfo = locatedNodePointer->get_file_info();
  if (fileInfo != 0)
    result = fileInfo->get_isPartOfTransformation();
  return result;
}

std::string Rose::getFileNameWithoutPath(SgStatement *statementPointer) {
  // Get the filename from the Sage III statement object
  ROSE_ASSERT(statementPointer != NULL);
  ROSE_ASSERT(statementPointer->get_file_info() != NULL);

  // char* fileName = getFileName(statementPointer);
  std::string fileName =
      statementPointer->get_file_info()->get_filenameString();

  return utility_stripPathFromFileName(fileName);
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

// DQ (3/15/2005): New, simpler and better implementation suggested function
// from Tom, thanks Tom!
string Rose::getPathFromFileName(const string fileName) {
  size_t pos = fileName.rfind('/');
  if (pos == std::string::npos) {
    return ".";
  } else {
    return fileName.substr(0, pos);
  }
}

// Later I expect we will move these functions to be SgFile member functions

//! get the source directory (requires an input string currently)
string Rose::getSourceDirectory(string fileNameWithPath) {
  return getPathFromFileName(fileNameWithPath);
}

//! get the current directory
string Rose::getWorkingDirectory() {
  // DQ (9/5/2006): Increase the buffer size
  // const int maxPathNameLength = 1024;
  const unsigned int maxPathNameLength = 10000;
  char *currentDirectory = new char[maxPathNameLength + 1];
  const char *getcwdResult = getcwd(currentDirectory, maxPathNameLength);

  if (!getcwdResult) {
    perror("getcwd: ");
    ROSE_ABORT();
  }
  string returnString = getcwdResult;
  delete[] currentDirectory;
  currentDirectory = NULL;
  return returnString;
}

SgName Rose::concatenate(const SgName &X, const SgName &Y) { return X + Y; }

// DQ (9/5/2008): Try to remove this function!
string Rose::getFileNameByTraversalBackToFileNode(const SgNode *astNode) {
  string returnString;

  ROSE_ASSERT(astNode != NULL);

  // Make sure this is not a project node (since the SgFile exists below
  // the project and could not be found by a traversal of the parent list)
  if (isSgProject(astNode) == NULL) {
    const SgNode *parent = astNode;
    while ((parent != NULL) && (isSgFile(parent) == NULL)) {
      // printf ("In getFileNameByTraversalBackToFileNode(): parent = %p = %s
      // \n",parent,parent->class_name().c_str());
      parent = parent->get_parent();
    }

    if (!parent) {
      const SgLocatedNode *ln = isSgLocatedNode(astNode);
      ROSE_ASSERT(ln);
      return ln->get_file_info()->get_filenameString();
    }
    // ROSE_ASSERT (parent != NULL);
    const SgFile *file = isSgFile(parent);
    ROSE_ASSERT(file != NULL);
    if (file != NULL) {
      // returnString = Rose::getFileName(file);
      returnString = file->getFileName();
    }

    // ROSE_ASSERT (returnString.length() > 0);
    ROSE_ASSERT(returnString.empty() == false);
  }

  return returnString;
}

void Rose::usage() { SgFile::usage(); }

int Rose::containsString(const std::string &masterString,
                         const std::string &targetString) {
  return masterString.find(targetString) != string::npos;
}

void Rose::filterInputFile(const string inputFileName,
                           const string outputFileName) {
  // This function filters the input file to remove ^M characters and expand
  // tabs etc. Any possible processing of the input file, before being compiled,
  // should be done by this function.

  // This function is implemented in the ROSE/dqDevelopmentDirectory directory.
}

SgStatement *Rose::getNextStatement(SgStatement *currentStatement) {
  ROSE_ASSERT(currentStatement != NULL);
  // CI (1/3/2007): This used to be not implemented ,,, here is my try
  //! get next statement will return the next statement in a function or method.
  //! if at the end or outside, it WILL return NULL

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
        ROSE_ASSERT("!ROSE::getNextStatement label not found");
      }

      ++i;
      return (i == statementList.end()) ? nullptr : *i;
    }
  }

  // DQ (9/18/2010): If we try to get the next statement from SgGlobal, then
  // return NULL.
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
  // case V_SgBasicBlock: // Liao 10/20/2010, We should allow users to get a
  // statement immediately AFTER a block.
  case V_SgClassDefinition:
  case V_SgFunctionDefinition:
  case V_SgStatement:
  case V_SgFunctionParameterList: {
    ROSE_ABORT();
    // not specified
  }

    // DQ (11/8/2015): Added support for SgLabelStatement (see testcode
    // tests/nonsmoke/functional/roseTests/astInterfaceTests/inputmoveDeclarationToInnermostScope_test2015_134.C)
  case V_SgLabelStatement: {
    SgLabelStatement *lableStatement = isSgLabelStatement(currentStatement);
    nextStatement = lableStatement->get_statement();
    ROSE_ASSERT(nextStatement != NULL);
    break;
  }

  default: {
    // We have to handle the cases of a SgStatementPtrList and a
    // SgDeclarationStatementPtrList separately
    if (isSgDeclarationScope(scope))
      return nullptr;
    if (scope->containsOnlyDeclarations() == true ||
        (isSgDeclarationScope(scope))) {
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
      // Liao, 11/18/2009, Handle the rare case that current statement is not
      // found in its scope's statement list
      for (i = statementList.begin();
           i != statementList.end() && (*i) != currentStatement; i++) {
        //  SgStatement* cur_stmt = *i;
        //  cout<<"Skipping current statement: "<<cur_stmt->class_name()<<endl;
        //  cout<<cur_stmt->get_file_info()->displayString()<<endl;
      }

      // currentStatement is not found in the list
      if (i == statementList.end()) {
        MLOG_FATAL_CXX("sageSupport")
            << "fatal error: ROSE::getNextStatement(): current statement is "
               "not found within its scope's statement list"
            << endl;
        MLOG_FATAL_CXX("sageSupport") << "current statement is "
                                      << currentStatement->class_name() << endl;
        //~ MLOG_FATAL_CXX("sageSupport") <<"code: " <<
        // currentStatement->unparseToString()<<endl;
        MLOG_FATAL_CXX("sageSupport")
            << currentStatement->get_file_info()->displayString() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << "Its scope is " << scope->class_name() << endl;
        MLOG_FATAL_CXX("sageSupport")
            << scope->get_file_info()->displayString() << endl;
        ROSE_ASSERT("!ROSE::getNextStatement not found");
      }

      // now i == currentStatement
      ROSE_ASSERT(*i == currentStatement);

      // DQ (7/19/2015): Added assertion that should be true, else i++ is not
      // defined.
      ROSE_ASSERT(i != statementList.end());

      i++;
      if (statementList.end() == i)
        nextStatement = NULL;
      else
        nextStatement = *i;
    }

    // If the target statement was the last statement in a scope then
    if (nextStatement == NULL) {
      // Someone might think of a better answer than NULL
    }

    break;
  }
  }

  // This assertion does not make sense.
  // Since a trailing statement within a scope can have null next statement,
  // and  the statement can be not global scope statement, Liao 3/12/2009
  // ROSE_ASSERT (isSgGlobal(currentStatement) != NULL || nextStatement !=
  // NULL);

  return nextStatement;
}

#define DEBUG_PREVIOUS_STATEMENT 0

static SgStatement *getPreviousStatement_support_for_declaration_list(
    SgScopeStatement *parent_scope, SgStatement *targetStatement,
    bool climbOutScope /*= true*/) {
  // This supports scopes that contain declaration statement lists (SgGlobal,
  // SgClassDefinition, etc.)

  ROSE_ASSERT(parent_scope != NULL);
  ROSE_ASSERT(targetStatement != NULL);

  ROSE_ASSERT(parent_scope->containsOnlyDeclarations() == true);

  SgStatement *previousStatement = NULL;

#if DEBUG_PREVIOUS_STATEMENT
  printf("In getPreviousStatement_support_for_declaration_list(): "
         "targetStatement = %p = %s \n",
         targetStatement, targetStatement->class_name().c_str());
#endif

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

    // DQ (3/12/2024): This should always be true.
    ROSE_ASSERT(previousStatement != targetStatement);
  }

#if DEBUG_PREVIOUS_STATEMENT
  printf("@@@@@ previousStatement = %p \n", previousStatement);
  if (previousStatement != NULL) {
    printf("@@@@@ previousStatement = %p = %s \n", previousStatement,
           previousStatement->class_name().c_str());
    // printf ("@@@@@ previousStatement->unparseToString() = %s
    // \n",previousStatement->unparseToString().c_str());
  }
#endif

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

#if DEBUG_PREVIOUS_STATEMENT
  printf("In getPreviousStatement_support_for_statement_list(): "
         "targetStatement = %p = %s \n",
         targetStatement, targetStatement->class_name().c_str());
#endif

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

    // DQ (3/12/2024): This should always be true.
    ROSE_ASSERT(previousStatement != targetStatement);
  }

#if DEBUG_PREVIOUS_STATEMENT
  printf("@@@@@ previousStatement = %p \n", previousStatement);
  if (previousStatement != NULL) {
    printf("@@@@@ previousStatement = %p = %s \n", previousStatement,
           previousStatement->class_name().c_str());
    // printf ("@@@@@ previousStatement->unparseToString() = %s
    // \n",previousStatement->unparseToString().c_str());
  }
#endif

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

  // DQ (3/15/2024): We don't need this variable now.
  // SgScopeStatement *scope             = targetStatement->get_scope();
  // ROSE_ASSERT (scope != NULL);

  // DQ (9/18/2010): If we try to get the previous statement from SgGlobal, then
  // return NULL.
  if (isSgGlobal(targetStatement) != NULL) {
    return NULL;
  }

#if DEBUG_PREVIOUS_STATEMENT
  printf("@@@@@ In Rose::getPreviousStatement(): targetStatement = %p = %s "
         "climbOutScope = %s \n",
         targetStatement, targetStatement->class_name().c_str(),
         climbOutScope ? "true" : "false");
  // printf ("@@@@@ In Rose::getPreviousStatement(): targetStatement = %s
  // \n",targetStatement->class_name().c_str());
  printf("@@@@@ In Rose::getPreviousStatement(): "
         "targetStatement->unparseToString() = %s \n",
         targetStatement->unparseToString().c_str());
  printf("@@@@@ In Rose::getPreviousStatement(): scope = %s \n",
         scope->class_name().c_str());
  // printf ("@@@@@ In Rose::getPreviousStatement(): scope->unparseToString() =
  // %s \n",scope->unparseToString().c_str());
#endif

  // DQ (3/15/2024): This is the new version of this function.

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

#if DEBUG_PREVIOUS_STATEMENT
  printf("@@@@@ previousStatement = %p \n", previousStatement);
  if (previousStatement != NULL) {
    printf("@@@@@ previousStatement = %p = %s \n", previousStatement,
           previousStatement->class_name().c_str());
    // printf ("@@@@@ previousStatement->unparseToString() = %s
    // \n",previousStatement->unparseToString().c_str());
  }
#endif

  if (climbOutScope) {
    ROSE_ASSERT(isSgGlobal(targetStatement) != NULL ||
                previousStatement != NULL);
  }

  return previousStatement;
}
