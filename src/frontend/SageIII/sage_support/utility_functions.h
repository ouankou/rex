// utility_functions.h -- header file for the ROSE Optimizing Preprocessor

#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H

#include "Cxx_Grammar.h"

class UnparseDelegate;

#define BACKEND_VERBOSE_LEVEL 2

std::string version_message();

std::string version_number();

//! Simple APIs for ROSE (error codes are in SgProject.frontendErrorCode(),
//! backendErrorCode()).
//! @{
ROSE_DLL_API SgProject *frontend(int argc, char **argv,
                                 bool frontendConstantFolding);
ROSE_DLL_API SgProject *frontend(const std::vector<std::string> &argv,
                                 bool frontendConstantFolding);
ROSE_DLL_API SgProject *
frontend(int argc, char **argv,
         SgProject::constant_folding_enum frontendConstantFolding =
             SgProject::e_original_expressions_only);
ROSE_DLL_API SgProject *
frontend(const std::vector<std::string> &argv,
         SgProject::constant_folding_enum frontendConstantFolding =
             SgProject::e_original_expressions_only);
//! @}
ROSE_DLL_API int frontendExitStatus(const SgProject *project);

// This builds a shell of a frontend SgProject with associated SgFile objects
// (but with empty SgGlobal objects) supporting only commandline processing and
// requiring the frontend to be called explicitly for each SgFile object.  See
// tutorial/selectedFileTranslation.C for example.
SgProject *frontendShell(int argc, char **argv);
ROSE_DLL_API SgProject *frontendShell(const std::vector<std::string> &argv);

// The caller retains ownership of unparseFormatHelp and unparseDelegate.
ROSE_DLL_API int backend(SgProject *project,
                         UnparseFormatHelp *unparseFormatHelp = NULL,
                         UnparseDelegate *unparseDelegate = NULL);

// Validate every token-backed source file before the unparser can create or
// replace an output file. Contract violations terminate immediately.
ROSE_DLL_API void enforceTokenUnparseContract(SgProject *project);

// Compile the original inputs without unparsing transformed source.
ROSE_DLL_API int
backendCompilesUsingOriginalInputFile(SgProject *project,
                                      bool compile_with_USE_ROSE_macro = false);

// Generate transformed source, then compile the original inputs.
ROSE_DLL_API int
backendGeneratesSourceCodeButCompilesUsingOriginalInputFile(SgProject *project);

ROSE_DLL_API void generatePDF(const SgProject &project);

ROSE_DLL_API void generateDOT(const SgProject &project,
                              std::string filenamePostfix = "",
                              bool excludeTemplateInstantiations = false);

ROSE_DLL_API void generateDOT(SgNode *node, std::string filename);

ROSE_DLL_API void generateDOT_withIncludes(const SgProject &project,
                                           std::string filenamePostfix = "");
ROSE_DLL_API void generateDOTforMultipleFile(const SgProject &project,
                                             std::string filenamePostfix = "");

ROSE_DLL_API void generateAstGraph(const SgProject *project, int maxSize = 2000,
                                   std::string filenameSuffix = "");

#ifndef SWIG

namespace AstDOTGenerationExtended_Defaults {
struct DefaultExtraNodeInfo;
struct DefaultExtraNodeOptions;
struct DefaultExtraEdgeInfo;
struct DefaultExtraEdgeOptions;
} // namespace AstDOTGenerationExtended_Defaults

template <typename ExtraNodeInfo_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraNodeInfo,
          typename ExtraNodeOptions_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraNodeOptions,
          typename ExtraEdgeInfo_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraEdgeInfo,
          typename ExtraEdgeOptions_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraEdgeOptions>
struct generateDOTExtended;

template <typename ExtraNodeInfo_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraNodeInfo,
          typename ExtraNodeOptions_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraNodeOptions,
          typename ExtraEdgeInfo_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraEdgeInfo,
          typename ExtraEdgeOptions_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraEdgeOptions>
struct generateDOTExtended_withIncludes;

template <typename ExtraNodeInfo_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraNodeInfo,
          typename ExtraNodeOptions_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraNodeOptions,
          typename ExtraEdgeInfo_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraEdgeInfo,
          typename ExtraEdgeOptions_t =
              AstDOTGenerationExtended_Defaults::DefaultExtraEdgeOptions>
struct generateDOTExtendedforMultipleFile;

#endif

#ifndef SWIG
namespace Rose {
class Options {
private:
  bool backend_warnings = false;

public:
  ROSE_DLL_API Options();
  ROSE_DLL_API Options(const Options &X);
  ROSE_DLL_API Options &operator=(const Options &X);

  ROSE_DLL_API bool get_backend_warnings();
  ROSE_DLL_API void set_backend_warnings(bool flag);
};

ROSE_DLL_API extern Options global_options;

ROSE_DLL_API std::string getWorkingDirectory();
ROSE_DLL_API std::string utility_stripPathFromFileName(
    const std::string
        &fileNameWithPath); //! get the filename from the full filename

ROSE_DLL_API std::string getPathFromFileName(
    std::string fileNameWithPath); //! get the path from the full filename

ROSE_DLL_API void usage();

SgStatement *getPreviousStatement(SgStatement *targetStatement,
                                  bool climbOutScope = true);
SgStatement *getNextStatement(SgStatement *targetStatement);

// Nonowning index of the token maps owned by live source-file nodes.
extern std::map<SgSourceFile *,
                std::map<SgNode *, TokenStreamSequenceToNodeMapping *> *>
    tokenSubsequenceMapOfMapsBySourceFile;

extern std::map<int, std::map<SgScopeStatement *, SgStatement *> *>
    representativeWhitespaceStatementMapOfMaps;

extern std::map<int, std::map<SgStatement *, MacroExpansion *> *>
    macroExpansionMapOfMaps;

// Note: the semantics is that there is at least one of the language kind
// files processed by ROSE, across all SgFile objects across all SgProject
// objects.
extern bool is_C_language;
extern bool is_OpenMP_language;
extern bool is_C99_language;
extern bool is_Cxx_language;
extern bool is_Fortran_language;
extern bool is_CAF_language;
extern bool is_Cuda_language;
extern bool is_OpenCL_language;
}; // namespace Rose

#endif

#endif // ifndef UTILITY_FUNCTIONS_H
