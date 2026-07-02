/**
 * \file    sage_support.C
 * \author  Justin Too <too1@llnl.gov>
 * \date    April 4, 2012
 */

/*-----------------------------------------------------------------------------
 *  Dependencies
 *---------------------------------------------------------------------------*/
#include "ROSE_UNUSED.h"

#include "Rose/FileSystem.h"

#include "Unique.h"

#include "cmdline.h"

#include "FileHelper.h"

#include "keep_going.h"

#include "processSupport.h"

#include "sage3basic.h"

#include "sage_support.h"

#include "astJson/sageAstJson.h"

#include "rose_path_resolver.h"

#include "rose_paths.h"

#if defined(ROSE_FLANG_FRONTEND)
#include "FlangModuleInfo.h"
#include "fortran_flang_support.h"
#include "unparseFortran_modfile.h"
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}
} // namespace
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <system_error>

// DQ (12/22/2019): I don't need this now, and it is an issue for some compilers
// (e.g. GNU 4.9.4). DQ (12/21/2019): Require hash table support for determining
// the shared nodes in the ASTs. #include <unordered_map>

#ifdef __INSURE__
// Provide a dummy function definition to support linking with Insure++.
// We have not identified why this is required.  This fixes the problem of
// a link error for the "std::ostream & operator<<()" used with
// "std::vector<bool>".
std::ostream &operator<<(std::basic_ostream<char, std::char_traits<char>> &os,
                         std::vector<bool, std::allocator<bool>> &m) {
  printf("Inside of std::ostream & operator<<(std::basic_ostream<char, "
         "std::char_traits<char> >& os, std::vector<bool, std::allocator<bool> "
         ">& m): A test! \n");
  // ROSE_ABORT();
  return os;
}
#endif

// DQ (9/26/2018): Added so that we can call the display function for
// TokenStreamSequenceToNodeMapping (for debugging).
#include "attachPreprocessingInfo.h"

#include "tokenStreamMapping.h"

using namespace std;
using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

const string FileHelper::pathDelimiter = "/";

/* These symbols are defined when we include sage_support.h above - ZG
(4/5/2013)
extern const std::string ROSE_GFORTRAN_PATH;
*/

// DQ (12/6/2014): Moved this from the unparser.C fle to here so that it can
// be called before any processing of the AST (so that it relates to the
// original AST before transformations). void
// buildTokenStreamMapping(SgSourceFile* sourceFile); void
// buildTokenStreamMapping(SgSourceFile* sourceFile, vector<stream_element*> &
// tokenVector);

// DQ (11/30/2015): Adding general support fo the detection of macro expansions
// and include file expansions.
void detectMacroOrIncludeFileExpansions(SgSourceFile *sourceFile);

static bool shouldBuildTokenMapping(const SgSourceFile *sourceFile) {
  if (sourceFile == NULL) {
    return false;
  }

  return (sourceFile->get_unparse_tokens() == true ||
          sourceFile->get_use_token_stream_to_improve_source_position_info() ==
              true);
}

static bool hasAttachedPreprocessingInfo(SgNode *root) {
  if (root == nullptr) {
    return false;
  }

  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (SgNode *node : nodes) {
    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == nullptr) {
      continue;
    }
    AttachedPreprocessingInfoType *info =
        located->getAttachedPreprocessingInfo();
    if (info != nullptr && !info->empty()) {
      return true;
    }
  }
  return false;
}

static std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

static std::string getPathSuffix(const std::string &path) {
  std::string::size_type dot = path.rfind('.');
  if (dot == std::string::npos || dot + 1 >= path.size()) {
    return std::string();
  }
  return path.substr(dot + 1);
}

static bool isEscapedDoubleQuote(const std::string &text, size_t pos) {
  size_t backslash_count = 0;
  while (pos > backslash_count && text[pos - backslash_count - 1] == '\\') {
    ++backslash_count;
  }

  return (backslash_count % 2) != 0;
}

static std::string::size_type
findUnescapedDoubleQuote(const std::string &text,
                         std::string::size_type start) {
  std::string::size_type quote_pos = text.find('"', start);
  while (quote_pos != std::string::npos &&
         isEscapedDoubleQuote(text, quote_pos)) {
    quote_pos = text.find('"', quote_pos + 1);
  }

  return quote_pos;
}

static bool fixQuotedArgumentForWrapperScript(std::string &arg) {
  bool changed = false;
  std::string::size_type search_start = 0;

  while (true) {
    const std::string::size_type startingQuote =
        findUnescapedDoubleQuote(arg, search_start);
    if (startingQuote == std::string::npos) {
      break;
    }

    const std::string::size_type endingQuote =
        findUnescapedDoubleQuote(arg, startingQuote + 1);
    if (endingQuote == std::string::npos) {
      break;
    }

    const std::string::size_type quotedLength = endingQuote - startingQuote + 1;
    const std::string quotedSubstring = arg.substr(startingQuote, quotedLength);
    const std::string fixedQuotedSubstring =
        std::string("\\\"") + quotedSubstring + std::string("\\\"");

    arg.replace(startingQuote, quotedLength, fixedQuotedSubstring);
    search_start = startingQuote + fixedQuotedSubstring.size();
    changed = true;
  }

  return changed;
}

namespace {
#if defined(ROSE_FLANG_FRONTEND)
bool isFortranSourcePath(const std::string &path) {
  std::string ext = getPathSuffix(path);
  if (ext.empty())
    return false;
  ext = toLowerCopy(ext);
  return ext == "f" || ext == "f77" || ext == "f90" || ext == "f95" ||
         ext == "f03" || ext == "f08" || ext == "f18";
}

bool isIntrinsicModuleUse(const SgUseStatement *use_stmt) {
  if (use_stmt == NULL)
    return false;
  std::string nature = toLowerCopy(use_stmt->get_module_nature());
  return nature == "intrinsic";
}

void collectModuleSourcesFromFile(SgSourceFile *file,
                                  std::vector<std::string> &order,
                                  std::set<std::string> &seen,
                                  std::set<std::string> &visiting);

void collectModuleSourcesFromUse(const SgUseStatement *use_stmt,
                                 std::vector<std::string> &order,
                                 std::set<std::string> &seen,
                                 std::set<std::string> &visiting) {
  if (use_stmt == NULL || isIntrinsicModuleUse(use_stmt))
    return;

  std::string name = use_stmt->get_name().str();
  if (name.empty())
    return;

  SgModuleStatement *mod_stmt = FlangModuleInfo::getModule(name);
  if (mod_stmt == NULL)
    return;

  SgSourceFile *mod_file = SageInterface::getEnclosingSourceFile(mod_stmt);
  if (mod_file == NULL)
    return;

  collectModuleSourcesFromFile(mod_file, order, seen, visiting);
}

void collectModuleSourcesFromFile(SgSourceFile *file,
                                  std::vector<std::string> &order,
                                  std::set<std::string> &seen,
                                  std::set<std::string> &visiting) {
  if (file == NULL)
    return;
  std::string path = file->getFileName();
  if (!isFortranSourcePath(path))
    return;
  if (seen.count(path) != 0)
    return;
  if (visiting.count(path) != 0)
    return;

  visiting.insert(path);

  Rose_STL_Container<SgNode *> uses =
      NodeQuery::querySubTree(file, V_SgUseStatement);
  for (Rose_STL_Container<SgNode *>::iterator it = uses.begin();
       it != uses.end(); ++it) {
    collectModuleSourcesFromUse(isSgUseStatement(*it), order, seen, visiting);
  }

  visiting.erase(path);
  seen.insert(path);
  order.push_back(path);
}
#endif
} // namespace

static bool isFlangFortranSourceSuffix(const std::string &suffix) {
  const std::string lower = toLowerCopy(suffix);
  return lower == "f" || lower == "ff" || lower == "f90" || lower == "ff90" ||
         lower == "f95" || lower == "ff95" || lower == "f18" ||
         lower == "ff18" || lower == "cuf" || lower == "rmod" ||
         lower == "rcmp" || lower == "mod";
}

static bool needsFlangFortranExtensionFix(const std::string &path) {
  const std::string suffix = getPathSuffix(path);
  if (suffix.empty()) {
    return false;
  }
  if (!CommandlineProcessing::isFortranFileNameSuffix(suffix)) {
    return false;
  }
  return !isFlangFortranSourceSuffix(suffix);
}

static bool commandLineSelectsFortran(const std::vector<std::string> &argv) {
  for (size_t i = 0; i < argv.size(); ++i) {
    const std::string &arg = argv[i];
    if (arg == "-rose:fortran" || arg == "--rose:fortran" ||
        arg == "-rose:CoArrayFortran" || arg == "--rose:CoArrayFortran" ||
        arg == "-std=f2018" || arg == "--std=f2018" ||
        arg.rfind("-rose:fortran_std=", 0) == 0 ||
        arg.rfind("--rose:fortran_std=", 0) == 0) {
      return true;
    }
    if ((arg == "-rose:fortran_std" || arg == "--rose:fortran_std") &&
        i + 1 < argv.size()) {
      return true;
    }
  }
  return false;
}

static bool sourceContainsPreprocessorDirectives(const std::string &path) {
  std::ifstream input(path.c_str());
  if (!input) {
    return false;
  }

  std::string line;
  while (std::getline(input, line)) {
    const size_t first_non_space = line.find_first_not_of(" \t\f\v\r");
    if (first_non_space != std::string::npos && line[first_non_space] == '#') {
      return true;
    }
  }

  return false;
}

static bool isFixedFormFortranSource(const SgSourceFile *source,
                                     const std::string &suffix) {
  if (source != nullptr) {
    if (source->get_inputFormat() == SgFile::e_fixed_form_output_format) {
      return true;
    }
    if (source->get_inputFormat() == SgFile::e_unknown_output_format &&
        source->get_F77_only()) {
      return true;
    }
  }
  const std::string lower = toLowerCopy(suffix);
  return lower == "f" || lower == "f77";
}

static std::string findOutputDirArg(const std::vector<std::string> &argv) {
  for (size_t i = 0; i + 1 < argv.size(); ++i) {
    if (argv[i] == "-outputdir") {
      return argv[i + 1];
    }
  }
  return std::string();
}

static bool hasIncludeDir(const std::vector<std::string> &args,
                          const std::string &dir) {
  if (dir.empty()) {
    return false;
  }
  const std::string normalized_dir = FileHelper::normalizePathIfPossible(dir);
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];
    if (arg == "-I") {
      if (i + 1 < args.size()) {
        if (FileHelper::normalizePathIfPossible(args[i + 1]) ==
            normalized_dir) {
          return true;
        }
      }
      continue;
    }
    if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
      if (FileHelper::normalizePathIfPossible(arg.substr(2)) ==
          normalized_dir) {
        return true;
      }
    }
  }
  return false;
}

static void addFlangIntrinsicIncludeDir(std::vector<std::string> &args) {
#if defined(ROSE_FLANG_INTRINSIC_INCLUDEDIR)
  const std::string include_dir = ROSE_FLANG_INTRINSIC_INCLUDEDIR;
  std::error_code ec;
  if (!include_dir.empty() && std::filesystem::exists(include_dir, ec) && !ec &&
      !hasIncludeDir(args, include_dir)) {
    args.push_back("-I");
    args.push_back(include_dir);
  }
#endif
}

static void copyLanguageSettings(SgSourceFile *target,
                                 const SgSourceFile *reference) {
  ROSE_ASSERT(target != NULL);
  ROSE_ASSERT(reference != NULL);

  target->set_outputLanguage(reference->get_outputLanguage());
  target->set_inputLanguage(reference->get_inputLanguage());

  target->set_C_only(reference->get_C_only());
  if (reference->get_C99_only()) {
    target->set_C99_only();
  }
  if (reference->get_C11_only()) {
    target->set_C11_only();
  }
  target->set_Cxx_only(reference->get_Cxx_only());

  target->set_Cuda_only(reference->get_Cuda_only());
  target->set_OpenCL_only(reference->get_OpenCL_only());
  target->set_Fortran_only(reference->get_Fortran_only());
}

static SgSourceFile *buildHeaderSourceFile(SgProject *project,
                                           const std::string &headerPath,
                                           const SgSourceFile *referenceFile) {
  ROSE_ASSERT(project != NULL);
  ROSE_ASSERT(referenceFile != NULL);

  vector<string> argv = project->get_originalCommandLineArgumentList();
  Rose_STL_Container<string> fileList =
      CommandlineProcessing::generateSourceFilenames(
          argv, project->get_binary_only());
  CommandlineProcessing::removeAllFileNamesExcept(argv, fileList, headerPath);
  if (std::find(argv.begin(), argv.end(), headerPath) == argv.end()) {
    argv.push_back(headerPath);
  }

  SgSourceFile *headerFile = new SgSourceFile(argv, project);
  ROSE_ASSERT(headerFile != NULL);

  headerFile->set_isHeaderFile(true);
  headerFile->set_unparseHeaderFiles(referenceFile->get_unparseHeaderFiles());
  headerFile->set_unparse_tokens(referenceFile->get_unparse_tokens());
  headerFile->set_use_token_stream_to_improve_source_position_info(
      referenceFile->get_use_token_stream_to_improve_source_position_info());
  headerFile->set_unparse_using_leading_and_trailing_token_mappings(
      referenceFile->get_unparse_using_leading_and_trailing_token_mappings());

  copyLanguageSettings(headerFile, referenceFile);

  headerFile->set_requires_C_preprocessor(false);
  headerFile->initializeGlobalScope();

  if (headerFile->get_preprocessorDirectivesAndCommentsList() == NULL) {
    headerFile->set_preprocessorDirectivesAndCommentsList(
        new ROSEAttributesListContainer());
  }

  return headerFile;
}

static SgIncludeFile *findIncludeFileByPath(SgIncludeFile *includeRoot,
                                            const std::string &headerPath) {
  if (includeRoot == NULL) {
    return NULL;
  }

  const std::string normalizedHeaderPath =
      FileHelper::normalizePathIfPossible(headerPath);

  std::vector<SgIncludeFile *> worklist;
  std::set<SgIncludeFile *> visited;
  worklist.push_back(includeRoot);

  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == NULL || !visited.insert(includeFile).second) {
      continue;
    }

    if (FileHelper::normalizePathIfPossible(includeFile->get_filename()) ==
        normalizedHeaderPath) {
      return includeFile;
    }

    const SgIncludeFilePtrList &children = includeFile->get_include_file_list();
    for (SgIncludeFile *child : children) {
      worklist.push_back(child);
    }
  }

  return NULL;
}

static PreprocessingInfo *findIncludingPreprocessingInfo(
    SgProject *project,
    const map<string, set<PreprocessingInfo *>> &includingPreprocessingInfosMap,
    SgSourceFile *includingSourceFile, const std::string &includedPath) {
  ROSE_ASSERT(project != NULL);
  if (includingSourceFile == NULL) {
    return NULL;
  }

  const std::string normalizedIncludedPath =
      FileHelper::normalizePathIfPossible(includedPath);
  const std::string normalizedIncludingPath =
      FileHelper::normalizePathIfPossible(includingSourceFile->getFileName());

  auto matchesInclude = [&](PreprocessingInfo *preprocessingInfo) {
    if (preprocessingInfo == NULL) {
      return false;
    }

    if (FileHelper::getNormalizedContainingFileName(preprocessingInfo) !=
        normalizedIncludingPath) {
      return false;
    }

    const std::string resolvedIncludedPath =
        FileHelper::normalizePathIfPossible(
            project->findIncludedFile(preprocessingInfo));
    return !resolvedIncludedPath.empty() &&
           resolvedIncludedPath == normalizedIncludedPath;
  };

  map<string, set<PreprocessingInfo *>>::const_iterator mapIt =
      includingPreprocessingInfosMap.find(normalizedIncludedPath);
  if (mapIt == includingPreprocessingInfosMap.end()) {
    mapIt = includingPreprocessingInfosMap.find(includedPath);
  }
  if (mapIt != includingPreprocessingInfosMap.end()) {
    const set<PreprocessingInfo *> &includingInfos = mapIt->second;
    for (set<PreprocessingInfo *>::const_iterator infoIt =
             includingInfos.begin();
         infoIt != includingInfos.end(); ++infoIt) {
      if (matchesInclude(*infoIt)) {
        return *infoIt;
      }
    }
  }

  ROSEAttributesListContainerPtr filePreprocInfo =
      includingSourceFile->get_preprocessorDirectivesAndCommentsList();
  if (filePreprocInfo == NULL) {
    return NULL;
  }

  std::map<std::string, ROSEAttributesList *> &attributeLists =
      filePreprocInfo->getList();
  for (std::map<std::string, ROSEAttributesList *>::iterator listIt =
           attributeLists.begin();
       listIt != attributeLists.end(); ++listIt) {
    ROSEAttributesList *attributeList = listIt->second;
    if (attributeList == NULL) {
      continue;
    }

    std::vector<PreprocessingInfo *> &preprocessingInfos =
        attributeList->getList();
    for (std::vector<PreprocessingInfo *>::iterator infoIt =
             preprocessingInfos.begin();
         infoIt != preprocessingInfos.end(); ++infoIt) {
      if (matchesInclude(*infoIt)) {
        return *infoIt;
      }
    }
  }

  return NULL;
}

static SgIncludeFile *ensureSourceFileIncludeRoot(SgProject *project,
                                                  SgSourceFile *sourceFile) {
  ROSE_ASSERT(project != NULL);
  if (sourceFile == NULL) {
    return NULL;
  }

  SgIncludeFile *includeRoot = sourceFile->get_associated_include_file();
  if (includeRoot != NULL) {
    return includeRoot;
  }

  if (sourceFile->get_isHeaderFile() == true) {
    return NULL;
  }

  const std::string sourcePath =
      FileHelper::normalizePathIfPossible(sourceFile->getFileName());
  includeRoot = new SgIncludeFile(sourcePath);
  includeRoot->set_filename(sourcePath);
  includeRoot->set_name_used_in_include_directive(
      FileHelper::getFileName(sourcePath));
  includeRoot->set_name_without_path(FileHelper::getFileName(sourcePath));

  std::string directoryPrefix = Rose::getPathFromFileName(sourcePath);
  if (directoryPrefix == ".") {
    directoryPrefix.clear();
  }
  includeRoot->set_directory_prefix(directoryPrefix);
  includeRoot->set_applicationRootDirectory(
      project->get_applicationRootDirectory());
  includeRoot->set_isApplicationFile(true);
  includeRoot->set_isRootSourceFile(true);
  includeRoot->set_source_file(sourceFile);
  includeRoot->set_source_file_of_translation_unit(sourceFile);
  includeRoot->set_including_source_file(sourceFile);

  sourceFile->set_associated_include_file(includeRoot);
  return includeRoot;
}

static SgIncludeFile *
synthesizeIncludeFile(SgProject *project, SgSourceFile *includingSourceFile,
                      SgSourceFile *includedSourceFile,
                      const std::string &includedPath,
                      PreprocessingInfo *preprocessingInfo) {
  ROSE_ASSERT(project != NULL);
  if (includingSourceFile == NULL || includedSourceFile == NULL) {
    return NULL;
  }

  SgIncludeFile *parentIncludeFile =
      includingSourceFile->get_associated_include_file();
  if (parentIncludeFile == NULL) {
    parentIncludeFile =
        ensureSourceFileIncludeRoot(project, includingSourceFile);
  }
  if (parentIncludeFile == NULL) {
    return NULL;
  }

  SgIncludeFile *includeFile =
      findIncludeFileByPath(parentIncludeFile, includedPath);
  if (includeFile != NULL) {
    return includeFile;
  }

  const std::string normalizedIncludedPath =
      FileHelper::normalizePathIfPossible(includedPath);
  includeFile = new SgIncludeFile(normalizedIncludedPath);
  includeFile->set_filename(normalizedIncludedPath);
  includeFile->set_name_without_path(
      FileHelper::getFileName(normalizedIncludedPath));

  std::string nameUsedInIncludeDirective =
      FileHelper::getFileName(normalizedIncludedPath);
  bool isSystemInclude =
      parentIncludeFile != NULL && parentIncludeFile->get_isSystemInclude();
  bool includedPathIsUnderApplicationRoot = false;
  const std::string normalizedApplicationRoot =
      FileHelper::normalizePathIfPossible(
          project->get_applicationRootDirectory());
  if (!normalizedApplicationRoot.empty()) {
    std::filesystem::path rootPath(normalizedApplicationRoot);
    std::filesystem::path includedPath(normalizedIncludedPath);
    std::error_code ec;
    rootPath = std::filesystem::weakly_canonical(rootPath, ec);
    if (!ec) {
      includedPath = std::filesystem::weakly_canonical(includedPath, ec);
    }
    if (!ec) {
      std::string root = rootPath.string();
      std::string included = includedPath.string();
      if (!root.empty() &&
          root.back() != std::filesystem::path::preferred_separator) {
        root += std::filesystem::path::preferred_separator;
      }
      includedPathIsUnderApplicationRoot =
          included == rootPath.string() || included.rfind(root, 0) == 0;
    }
  }
  if (preprocessingInfo != NULL) {
    IncludeDirective includeDirective(preprocessingInfo->getString());
    nameUsedInIncludeDirective = includeDirective.getIncludedPath();
    bool includedPathResolvesFromIncludingFile = false;
    if (includeDirective.isQuotedInclude() == false &&
        FileHelper::isAbsolutePath(includeDirective.getIncludedPath()) ==
            false &&
        preprocessingInfo->get_file_info() != NULL) {
      const std::string includingFileName =
          preprocessingInfo->get_file_info()->get_filenameString();
      const std::string includingDirectory =
          FileHelper::getParentFolder(includingFileName);
      const std::string candidatePath =
          FileHelper::normalizePathIfPossible(FileHelper::concatenatePaths(
              includingDirectory, includeDirective.getIncludedPath()));
      includedPathResolvesFromIncludingFile =
          !candidatePath.empty() && candidatePath == normalizedIncludedPath;
    }
    isSystemInclude =
        isSystemInclude || (includeDirective.isQuotedInclude() == false &&
                            !includedPathIsUnderApplicationRoot &&
                            !includedPathResolvesFromIncludingFile);
  }
  includeFile->set_name_used_in_include_directive(nameUsedInIncludeDirective);

  std::string directoryPrefix =
      Rose::getPathFromFileName(nameUsedInIncludeDirective);
  if (directoryPrefix == ".") {
    directoryPrefix.clear();
  }
  includeFile->set_directory_prefix(directoryPrefix);
  includeFile->set_applicationRootDirectory(
      project->get_applicationRootDirectory());
  includeFile->set_isSystemInclude(isSystemInclude);
  includeFile->set_isApplicationFile(isSystemInclude == false);
  includeFile->set_parent_include_file(parentIncludeFile);
  includeFile->set_source_file(includedSourceFile);

  SgSourceFile *translationUnit =
      parentIncludeFile->get_source_file_of_translation_unit();
  if (translationUnit == NULL &&
      includingSourceFile->get_isHeaderFile() == false) {
    translationUnit = includingSourceFile;
  }
  if (translationUnit == NULL) {
    translationUnit = parentIncludeFile->get_including_source_file();
  }
  if (translationUnit == NULL) {
    translationUnit = includingSourceFile;
  }
  includeFile->set_source_file_of_translation_unit(translationUnit);
  includeFile->set_including_source_file(includingSourceFile);

  SgIncludeFilePtrList &childIncludes =
      parentIncludeFile->get_include_file_list();
  childIncludes.push_back(includeFile);

  return includeFile;
}

static void ensureHeaderTokenMapping(SgSourceFile *headerFile,
                                     SgSourceFile *traversalRoot) {
  ROSE_ASSERT(headerFile != NULL);
  ROSE_ASSERT(traversalRoot != NULL);

  ROSEAttributesListContainerPtr filePreprocInfo =
      headerFile->get_preprocessorDirectivesAndCommentsList();
  if (filePreprocInfo == NULL) {
    filePreprocInfo = new ROSEAttributesListContainer();
    headerFile->set_preprocessorDirectivesAndCommentsList(filePreprocInfo);
  }

  const std::string headerPath = headerFile->getFileName();
  std::map<std::string, ROSEAttributesList *> &listMap =
      filePreprocInfo->getList();
  if (listMap.find(headerPath) == listMap.end()) {
    listMap[headerPath] = getPreprocessorDirectives(headerPath);
  }

  std::vector<stream_element *> tokenVector = getTokenStream(headerFile);
  buildTokenStreamMappingForRoot(headerFile, traversalRoot, tokenVector);

  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      headerFile->get_tokenSubsequenceMap();
  const int firstTokenIndex = tokenVector.empty() ? -1 : 0;
  const int lastTokenIndex =
      tokenVector.empty() ? -1 : static_cast<int>(tokenVector.size()) - 1;

  auto ensureWholeFileMapping = [&](SgNode *node) {
    if (node == NULL) {
      return;
    }

    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::iterator mapIt =
        tokenMap.find(node);
    if (mapIt != tokenMap.end() && mapIt->second != NULL) {
      return;
    }

    tokenMap[node] = new TokenStreamSequenceToNodeMapping(
        node, -1, -1, firstTokenIndex, lastTokenIndex, -1, -1, -1, -1);
  };

  ensureWholeFileMapping(headerFile);
  ensureWholeFileMapping(headerFile->get_globalScope());
}

// DQ (2/12/2011): Added const so that this could be called in get_mangled()
// (and more generally). std::string
// SgValueExp::get_constant_folded_value_as_string()
std::string SgValueExp::get_constant_folded_value_as_string() const {
  // DQ (8/18/2009): Added support for generating a string from a SgValueExp.
  // Note that the point is not to call unparse since that would provide the
  // expression tree and we want the constant folded value.

  // DQ (7/24/2012): Added a test.
  ASSERT_not_null(this);

  string s;
  const int max_buffer_size = 500;
  char buffer[max_buffer_size];
  switch (variantT()) {
  case V_SgIntVal: {
    const SgIntVal *integerValueExpression = isSgIntVal(this);
    ASSERT_not_null(integerValueExpression);
    int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %d
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%d", numericValue);
    s = buffer;
    break;
  }

    // DQ (10/4/2010): Added case
  case V_SgLongIntVal: {
    const SgLongIntVal *integerValueExpression = isSgLongIntVal(this);
    ASSERT_not_null(integerValueExpression);
    long int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %ld
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%ld", numericValue);
    s = buffer;
    break;
  }

  case V_SgLongLongIntVal: {
    const SgLongLongIntVal *integerValueExpression = isSgLongLongIntVal(this);
    ASSERT_not_null(integerValueExpression);
    long long int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %ld
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%lld", numericValue);
    s = buffer;
    break;
  }

    // DQ (10/5/2010): Added case
  case V_SgShortVal: {
    const SgShortVal *integerValueExpression = isSgShortVal(this);
    ASSERT_not_null(integerValueExpression);
    short int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %ld
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%d", numericValue);
    s = buffer;
    break;
  }

  case V_SgUnsignedShortVal: {
    const SgUnsignedShortVal *integerValueExpression =
        isSgUnsignedShortVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned short int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %ld
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%u", numericValue);
    s = buffer;
    break;
  }

  case V_SgUnsignedLongLongIntVal: {
    const SgUnsignedLongLongIntVal *integerValueExpression =
        isSgUnsignedLongLongIntVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned long long int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %llu
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%llu", numericValue);
    s = buffer;
    break;
  }

    // DQ (8/19/2009): Added case
  case V_SgUnsignedLongVal: {
    const SgUnsignedLongVal *integerValueExpression = isSgUnsignedLongVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned long int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %llu
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%lu", numericValue);
    s = buffer;
    break;
  }

    // DQ (8/19/2009): Added case
  case V_SgUnsignedIntVal: {
    const SgUnsignedIntVal *integerValueExpression = isSgUnsignedIntVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned int numericValue = integerValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %llu
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%u", numericValue);
    s = buffer;
    break;
  }

    // DQ (8/19/2009): Added case
  case V_SgBoolValExp: {
    const SgBoolValExp *booleanValueExpression = isSgBoolValExp(this);
    ASSERT_not_null(booleanValueExpression);
    bool booleanValue = booleanValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%s",
             booleanValue == true ? "true" : "false");
    s = buffer;
    break;
  }

    // DQ (8/19/2009): Added case
  case V_SgStringVal: {
    const SgStringVal *stringValueExpression = isSgStringVal(this);
    ASSERT_not_null(stringValueExpression);
    s = stringValueExpression->get_value();
    break;
  }

    // DQ (8/19/2009): Added case
  case V_SgCharVal: {
    const SgCharVal *charValueExpression = isSgCharVal(this);
    ASSERT_not_null(charValueExpression);
    // DQ (9/24/2011): Handle case where this is non-printable character (see
    // test2011_140.C, where the bug was the the dot file had a non-printable
    // character and caused zgrviewer to crash). s =
    // charValueExpression->get_value();
    char value = charValueExpression->get_value();
    unsigned char classificationValue = static_cast<unsigned char>(value);
    if (isalnum(classificationValue) == true) {
      // Leave this as a alpha or numeric value where possible.
      s = charValueExpression->get_value();
    } else {
      // Convert this to be a string of the numeric value so that it will print.
      snprintf(buffer, max_buffer_size, "%d", value);
      s = buffer;
    }
    break;
  }

    // PL (3/24/2025): Added case
  case V_SgUnsignedCharVal: {
    const SgUnsignedCharVal *charValueExpression = isSgUnsignedCharVal(this);
    ASSERT_not_null(charValueExpression);

    unsigned char value = charValueExpression->get_value();
    if (isalnum(value) == true) {
      // Leave this as a alpha or numeric value where possible.
      s = charValueExpression->get_value();
    } else {
      // Convert this to be a string of the numeric value so that it will print.
      snprintf(buffer, max_buffer_size, "%d", value);
      s = buffer;
    }
    break;
  }

    // PL (3/24/2025): Added case
  case V_SgSignedCharVal: {
    const SgSignedCharVal *charValueExpression = isSgSignedCharVal(this);
    ASSERT_not_null(charValueExpression);

    signed char value = charValueExpression->get_value();
    unsigned char classificationValue = static_cast<unsigned char>(value);
    if (isalnum(classificationValue) == true) {
      // Leave this as a alpha or numeric value where possible.
      s = charValueExpression->get_value();
    } else {
      // Convert this to be a string of the numeric value so that it will print.
      snprintf(buffer, max_buffer_size, "%d", value);
      s = buffer;
    }
    break;
  }

    // DQ (10/4/2010): Added case
  case V_SgFloatVal: {
    const SgFloatVal *floatValueExpression = isSgFloatVal(this);
    ASSERT_not_null(floatValueExpression);
    float numericValue = floatValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %f
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%f", numericValue);
    s = buffer;
    break;
  }

    // DQ (10/4/2010): Added case
  case V_SgDoubleVal: {
    const SgDoubleVal *floatValueExpression = isSgDoubleVal(this);
    ASSERT_not_null(floatValueExpression);
    double numericValue = floatValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %f
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%lf", numericValue);
    s = buffer;
    break;
  }

    // DQ (10/4/2010): Added case
  case V_SgLongDoubleVal: {
    const SgLongDoubleVal *floatValueExpression = isSgLongDoubleVal(this);
    ASSERT_not_null(floatValueExpression);
    long double numericValue = floatValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %f
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%Lf", numericValue);
    s = buffer;
    break;
  }

  case V_SgFloat128Val: {
    const SgFloat128Val *floatValueExpression = isSgFloat128Val(this);
    ASSERT_not_null(floatValueExpression);
    long double numericValue = floatValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%Lf", numericValue);
    s = buffer;
    break;
  }

    // DQ (10/4/2010): Added case
  case V_SgEnumVal: {
    const SgEnumVal *enumValueExpression = isSgEnumVal(this);
    ASSERT_not_null(enumValueExpression);
    int numericValue = enumValueExpression->get_value();
    // printf ("numericValue of constant folded expression = %d
    // \n",numericValue);
    snprintf(buffer, max_buffer_size, "%d", numericValue);
    s = string("_enum_") + string(buffer);
    break;
  }

    // DQ (9/24/2011): Added support for complex values to be output as strings.
  case V_SgComplexVal: {
    const SgComplexVal *complexValueExpression = isSgComplexVal(this);
    ASSERT_not_null(complexValueExpression);

    string real_string = "null";
    if (complexValueExpression->get_real_value() != nullptr)
      real_string = complexValueExpression->get_real_value()
                        ->get_constant_folded_value_as_string();

    string imaginary_string = "null";
    if (complexValueExpression->get_imaginary_value() != nullptr)
      imaginary_string = complexValueExpression->get_imaginary_value()
                             ->get_constant_folded_value_as_string();

    s = "(" + real_string + "," + imaginary_string + ")";
    break;
  }

    // DQ (11/28/2011): Adding support for template declarations in the AST.
  case V_SgTemplateParameterVal: {
    // Note that constant folding on SgTemplateParameterVal expressions does not
    // make any sense!
    const SgTemplateParameterVal *templateParameterValueExpression =
        isSgTemplateParameterVal(this);
    ASSERT_not_null(templateParameterValueExpression);
    string stringName =
        templateParameterValueExpression->get_template_parameter_name();
    s = stringName;
    break;
  }

    // DQ (11/10/2014): Adding support for C++11 value "nullptr".
  case V_SgNullptrValExp: {
    s = "_nullptr_";
    break;
  }

    // DQ (2/12/2019): Adding support for SgWcharVal.
  case V_SgWcharVal: {
    const SgWcharVal *wideCharValueExpression = isSgWcharVal(this);
    ASSERT_not_null(wideCharValueExpression);
    // DQ (9/24/2011): Handle case where this is non-printable character (see
    // test2011_140.C, where the bug was the the dot file had a non-printable
    // character and caused zgrviewer to crash). s =
    // charValueExpression->get_value();
    char value = wideCharValueExpression->get_value();
    if (isalnum(value) == true) {
      // Leave this as a alpha or numeric value where possible.
      s = wideCharValueExpression->get_value();
    } else {
      // Convert this to be a string of the numeric value so that it will print.
      snprintf(buffer, max_buffer_size, "%d", value);
      s = buffer;
    }
    break;
  }

  default: {
    printf("Error SgValueExp::get_constant_folded_value_as_string(): case of "
           "value = %s not handled \n",
           this->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return s;
}

void whatTypeOfFileIsThis(const string &name) {
  // DQ (2/3/2009): It is helpful to report what type of file this is where
  // possible. Call the Unix "file" command, it would be great if this was an
  // available system call (but Robb thinks it might not always be available).

  vector<string> commandLineVector;
  commandLineVector.push_back("file -b " + name);

  printf("Error: unknown file type: ");
  flush(cout);

  // Use "-b" for brief mode!
  string commandLine = "file " + name;
  if (system(commandLine.c_str()))
    MLOG_ERROR_CXX("sage_support")
        << "command failed: \"" << StringUtility::cEscape(commandLine)
        << "\"\n";
}

void outputTypeOfFileAndExit(const string &name) {
  // DQ (8/20/2008): The code (from Robb) identifies what kind of file this is
  // or more specifically what kind of file most tools would think this file is
  // (using the system file(1) command as a standard way to identify file types
  // using their first few bytes.

  whatTypeOfFileIsThis(name);

  printf("In outputTypeOfFileAndExit(): name = %s \n", name.c_str());
  printf("\n\nExiting: Unknown file Error \n\n");
  ROSE_ABORT();
}

// DQ (1/5/2008): These are functions separated out of the generated
// code in ROSETTA.  These functions don't need to be generated since
// there implementation is not as dependent upon the IR as other functions
// (e.g. IR node member functions).
//
// Switches taking a second parameter need to be added to
// CommandlineProcessing::isOptionTakingSecondParameter().

static std::string resolveRoseSupportPath(const std::string &root,
                                          const std::string &suffix) {
  std::filesystem::path suffix_path(suffix);
  if (suffix_path.is_absolute()) {
    return suffix_path.string();
  }
  return (std::filesystem::path(root) / suffix_path).string();
}

static std::string resolveXompArchivePath() {
  static const RosePathRoots roots = resolveRosePaths(nullptr);
  string xomp_lib_root =
      roots.in_install_tree
          ? resolveRoseSupportPath(roots.install_prefix, ROSE_INSTALL_LIB_DIR)
          : ROSE_BUILD_LIB_DIR;
  ROSE_ASSERT(!xomp_lib_root.empty());
  return (std::filesystem::path(xomp_lib_root) / "libxomp.a").string();
}

string findRoseSupportPathFromSource(const string &sourceTreeLocation,
                                     const string &installTreeLocation) {
  static const RosePathRoots roots = resolveRosePaths(nullptr);
  if (roots.in_install_tree) {
    return resolveRoseSupportPath(roots.install_prefix, installTreeLocation);
  }
  return resolveRoseSupportPath(ROSE_SOURCE_TREE, sourceTreeLocation);
}

string findRoseSupportPathFromBuild(const string &buildTreeLocation,
                                    const string &installTreeLocation) {
  static const RosePathRoots roots = resolveRosePaths(nullptr);
  if (roots.in_install_tree) {
    return resolveRoseSupportPath(roots.install_prefix, installTreeLocation);
  }
  return resolveRoseSupportPath(roots.build_root, buildTreeLocation);
}

//! Resolve the installation prefix if running from an install tree.
bool roseInstallPrefix(std::string &result) {
  static const RosePathRoots roots = resolveRosePaths(nullptr);
  if (roots.in_install_tree) {
    result = roots.install_prefix;
    return true;
  }
  result.clear();
  return false;
}

/* This function suffers from the same problems as
 * CommandlineProcessing::isExecutableFilename(), namely that the list of magic
 * numbers used here needs to be kept in sync with changes to the binary
 * parsers. */
bool isBinaryExecutableFile(string sourceFilename) {
  bool returnValue = false;

  if (SgProject::get_verbose() > 1)
    printf("Inside of isBinaryExecutableFile(%s) \n", sourceFilename.c_str());

  // Open file for reading
  FILE *f = fopen(sourceFilename.c_str(), "rb");
  if (!f)
    return false; // a file that cannot be opened is not a binary file

  int character0 = fgetc(f);
  int character1 = fgetc(f);

  // The first character of an ELF binary is '\127'. Some other executable
  // formats begin with 'M'.

  if (character0 == 127 || character0 == 77) {
    if (character1 == 'E' || character1 == 'Z') {
      returnValue = true;
    }
  }

  fclose(f);

  return returnValue;
}

bool isLibraryArchiveFile(string sourceFilename) {
  // The if this is a "*.a" file, not that "*.so" files
  // will appear as an executable.

  bool returnValue = false;

  if (SgProject::get_verbose() > 1)
    printf("Inside of isLibraryArchiveFile(%s) \n", sourceFilename.c_str());

  // Open file for reading
  FILE *f = fopen(sourceFilename.c_str(), "rb");
  if (!f)
    return false; // a non-existing file is not a library archive

  string magicHeader;
  for (int i = 0; i < 7; i++) {
    magicHeader = magicHeader + (char)getc(f);
  }

  // printf ("magicHeader = %s \n",magicHeader.c_str());
  returnValue = (magicHeader == "!<arch>");

  // printf ("isLibraryArchiveFile() returning %s \n",returnValue ? "true" :
  // "false");

  fclose(f);

  return returnValue;
}

void SgFile::initializeSourcePosition(const std::string &sourceFilename) {
  // printf ("Inside of SgFile::initializeSourcePosition() \n");

  Sg_File_Info *fileInfo = new Sg_File_Info(sourceFilename, 1, 1);
  ASSERT_not_null(fileInfo);

  // set_file_info(fileInfo);
  set_startOfConstruct(fileInfo);
  fileInfo->set_parent(this);
  ASSERT_not_null(get_startOfConstruct());
  ASSERT_not_null(get_file_info());
}

void SgSourceFile::initializeGlobalScope() {
  ASSERT_not_null(this);

  // printf ("Inside of SgSourceFile::initializeGlobalScope() \n");

  // Note that SgFile::initializeSourcePosition() should have already been
  // called.
  ASSERT_not_null(get_startOfConstruct());

  string sourceFilename = get_startOfConstruct()->get_filename();

  // DQ (8/31/2006): Generate a NULL_FILE (instead of SgFile::SgFile) so that we
  // can enforce that the filename is always an absolute path (starting with
  // "/"). Sg_File_Info* globalScopeFileInfo = new
  // Sg_File_Info("SgGlobal::SgGlobal",0,0);
  Sg_File_Info *globalScopeFileInfo = new Sg_File_Info(sourceFilename, 0, 0);
  ASSERT_not_null(globalScopeFileInfo);

  // printf ("&&&&&&&&&& In SgSourceFile::initializeGlobalScope(): Building
  // SgGlobal (with empty filename) &&&&&&&&&& \n");

  set_globalScope(new SgGlobal(globalScopeFileInfo));
  ASSERT_not_null(get_globalScope());

  // Rasmussen (3/22/2020): Fixed setting case insensitivity
  // if (SageBuilder::symbol_table_case_insensitive_semantics == true)
  if (SageInterface::is_language_case_insensitive()) {
    // Rasmussen (3/27/2020): Experimenting with returning to original (using
    // symbol_table_case_insensitive_semantics variable)
    ROSE_ASSERT(SageBuilder::symbol_table_case_insensitive_semantics == true);
    get_globalScope()->setCaseInsensitive(true);
  }

  // DQ (2/15/2006): Set the parent of the SgGlobal IR node
  get_globalScope()->set_parent(this);

  // DQ (8/21/2008): Set the end of the global scope (even if it is updated
  // later) printf ("In SgFile::initialization(): p_root->get_endOfConstruct() =
  // %p \n",p_root->get_endOfConstruct());
  ROSE_ASSERT(get_globalScope()->get_endOfConstruct() == nullptr);
  get_globalScope()->set_endOfConstruct(new Sg_File_Info(sourceFilename, 0, 0));
  ASSERT_not_null(get_globalScope()->get_endOfConstruct());

  // DQ (1/21/2008): Set the filename in the SgGlobal IR node so that the
  // traversal to add CPP directives and comments will succeed.
  ROSE_ASSERT(get_globalScope() != NULL);
  ROSE_ASSERT(get_globalScope()->get_startOfConstruct() != NULL);

  // DQ (8/21/2008): Modified to make endOfConstruct consistant (avoids warning
  // in AST consistancy check). ROSE_ASSERT(p_root->get_endOfConstruct()   ==
  // NULL);
  ROSE_ASSERT(get_globalScope()->get_endOfConstruct() != NULL);

  // p_root->get_file_info()->set_filenameString(p_sourceFileNameWithPath);
  // ROSE_ASSERT(p_root->get_file_info()->get_filenameString().empty() ==
  // false);

  // DQ (12/22/2008): Added to support CPP preprocessing of Fortran files.
  string filename = p_sourceFileNameWithPath;
  if (get_requires_C_preprocessor() == true) {
    // This must be a Fortran source file (requiring the use of CPP to process
    // its directives).
    if (get_experimental_flang_frontend() == false) {
      filename = generate_C_preprocessor_intermediate_filename(filename);
    }
  }

  // printf ("get_requires_C_preprocessor() = %s filename = %s
  // \n",get_requires_C_preprocessor() ? "true" : "false",filename.c_str());

  get_globalScope()->get_startOfConstruct()->set_filenameString(filename);
  ROSE_ASSERT(
      get_globalScope()->get_startOfConstruct()->get_filenameString().empty() ==
      false);

  get_globalScope()->get_endOfConstruct()->set_filenameString(filename);
  ROSE_ASSERT(
      get_globalScope()->get_endOfConstruct()->get_filenameString().empty() ==
      false);

  // DQ (12/23/2008): These should be in the Sg_File_Info map already.
  ROSE_ASSERT(
      Sg_File_Info::getIDFromFilename(get_file_info()->get_filename()) >= 0);
  if (get_requires_C_preprocessor() == true) {
    ROSE_ASSERT(Sg_File_Info::getIDFromFilename(
                    generate_C_preprocessor_intermediate_filename(
                        get_file_info()->get_filename())) >= 0);
  }
}

SgFile *determineFileType(vector<string> argv, int &nextErrorCode,
                          SgProject *project) {
  SgFile *file = nullptr;

  // DQ (2/4/2009): The specification of "-rose:binary" causes filenames to be
  // interpreted differently if they are object files or libary archive files.
  // DQ (4/21/2006): New version of source file name handling (set the source
  // file name early) printf ("In determineFileType(): Calling
  // CommandlineProcessing::generateSourceFilenames(argv) \n");
  // Rose_STL_Container<string> fileList =
  // CommandlineProcessing::generateSourceFilenames(argv);
  ROSE_ASSERT(project != NULL);
  Rose_STL_Container<string> fileList =
      CommandlineProcessing::generateSourceFilenames(
          argv, project->get_binary_only());

  // DQ (2/6/2009): This fails for the build function SageBuilder::buildFile(),
  // so OK to comment it out. DQ (12/23/2008): I think that we may be able to
  // assert this is true, if so then we can simplify the code below.
  ROSE_ASSERT(fileList.empty() == false);

  if (fileList.empty() == false) {
    if (fileList.size() != 1) {
      cout << endl;
      for (Rose_STL_Container<string>::iterator i = fileList.begin();
           i != fileList.end(); i++) {
        cout << (*i) << endl;
      }
      cout << endl;
      cout.flush();
    }
    ROSE_ASSERT(fileList.size() == 1);

    // DQ (8/31/2006): Convert the source file to have a path if it does not
    // already
    string sourceFilename = *(fileList.begin());

    sourceFilename =
        StringUtility::getAbsolutePathFromRelativePath(sourceFilename, true);

    // This should be an absolute path
    string targetSubstring = "/";

    // Rama: 12/06/06: Fixup for problem with file names.
    // Made changes to this file and string utilities function
    // getAbsolutePathFromRelativePath by cloning it with name
    // getAbsolutePathFromRelativePathWithErrors Also refer to script that tests
    // -- reasonably exhaustively -- to various combinarions of input files.

    // [Robb P Matzke 2017-04-21]: Such a low-level utility function as
    // this shouldn't be emitting output at all, especially not on
    // standard output, because it makes it problematic to call this in
    // situations where the file might not exist.

    // DQ (11/29/2006): Even if this is C mode, we have to define the
    // __cplusplus macro if we detect we are processing a source file
    // using a C++ filename extension.
    string filenameExtension = StringUtility::fileNameSuffix(sourceFilename);

    // DQ (1/8/2014): We need to handle the case of "/dev/null" being used as an
    // input filename.
    if (filenameExtension == "/dev/null") {
      printf("Warning: detected use of /dev/null as input filename: not yet "
             "supported (exiting with 0 exit code) \n");
      exit(0);
    }

    // DQ (11/17/2007): Mark this as a file using a Fortran file extension (else
    // this turns off options down stream).
    if (CommandlineProcessing::isFortranFileNameSuffix(filenameExtension) ==
        true) {
      SgSourceFile *sourceFile = new SgSourceFile(argv, project);
      file = sourceFile;

      // printf ("----------- Great location to set the sourceFilename = %s
      // \n",sourceFilename.c_str());

      // DQ (12/23/2008): Moved initialization of source position (call to
      // initializeSourcePosition()) to earliest position in setup of SgFile.

      file->set_sourceFileUsesFortranFileExtension(true);

      // Use the filename suffix as a default means to set this value
      file->set_outputLanguage(SgFile::e_Fortran_language);

      // DQ (29/8/2017): Set the input language as well.
      file->set_inputLanguage(SgFile::e_Fortran_language);

      file->set_Fortran_only(true);

      // DQ (11/25/2020): Add support to set this as a specific language kind
      // file (there is at least one language kind file processed by ROSE).
      Rose::is_Fortran_language = true;

      // DQ (11/30/2010): This variable activates scopes built within the
      // SageBuilder interface to be built to use case insensitive symbol table
      // handling.
      SageBuilder::symbol_table_case_insensitive_semantics = true;

      // determine whether to run this file through the C preprocessor
      const std::string lowerExtension = toLowerCopy(filenameExtension);
      bool is_module_file_suffix =
          (lowerExtension == "rmod" || lowerExtension == "rcmp" ||
           lowerExtension == "mod");
      bool requires_C_preprocessor =
          // Module interface files should never require preprocessing.
          !is_module_file_suffix &&
          (
              // if the file extension implies it
              CommandlineProcessing::isFortranFileNameSuffixRequiringCPP(
                  filenameExtension) ||
              // if the command line includes "-D" options
              !getProject()->get_macroSpecifierList().empty());

      file->set_requires_C_preprocessor(requires_C_preprocessor);

      // DQ (12/23/2008): This needs to be called after the
      // set_requires_C_preprocessor() function is called. If CPP processing is
      // required then the global scope should have a source position using the
      // intermediate file name (generated by
      // generate_C_preprocessor_intermediate_filename()).
      sourceFile->initializeGlobalScope();

      // Now set the specific types of Fortran file extensions
      if (CommandlineProcessing::isFortran77FileNameSuffix(filenameExtension) ==
          true) {
        file->set_sourceFileUsesFortran77FileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_fixed_form_output_format);
        file->set_backendCompileFormat(SgFile::e_fixed_form_output_format);

        file->set_F77_only();
      }

      if (CommandlineProcessing::isFortran90FileNameSuffix(filenameExtension) ==
          true) {
        file->set_sourceFileUsesFortran90FileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_free_form_output_format);
        file->set_backendCompileFormat(SgFile::e_free_form_output_format);

        file->set_F90_only();
      }

      if (CommandlineProcessing::isFortran95FileNameSuffix(filenameExtension) ==
          true) {
        file->set_sourceFileUsesFortran95FileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_free_form_output_format);
        file->set_backendCompileFormat(SgFile::e_free_form_output_format);

        file->set_F95_only();
      }

      if (CommandlineProcessing::isFortran2003FileNameSuffix(
              filenameExtension) == true) {
        file->set_sourceFileUsesFortran2003FileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_free_form_output_format);
        file->set_backendCompileFormat(SgFile::e_free_form_output_format);

        file->set_F2003_only();
      }

      if (CommandlineProcessing::isCoArrayFortranFileNameSuffix(
              filenameExtension) == true) {
        file->set_sourceFileUsesCoArrayFortranFileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_free_form_output_format);
        file->set_backendCompileFormat(SgFile::e_free_form_output_format);

        // DQ (1/23/2009): I think that since CAF is an extension of F2003, we
        // want to mark this as F2003 as well.
        file->set_F2003_only();
        file->set_CoArrayFortran_only(true);
      }

      if (CommandlineProcessing::isFortran2008FileNameSuffix(
              filenameExtension) == true) {
        // printf ("Sorry, Fortran 2008 specific support is not yet implemented
        // in ROSE ... \n"); ROSE_ABORT();

        // This is not yet supported.
        file->set_sourceFileUsesFortran2008FileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_free_form_output_format);
        file->set_backendCompileFormat(SgFile::e_free_form_output_format);

        file->set_F2008_only();
      }
    } else {
      // SG (7/9/2015) When processing multiple files, we need to reset
      // case_insensitive_semantics.  But this only sets it to the last
      // file created.  During AST construction, it will need to be
      // reset for each language.
      SageBuilder::symbol_table_case_insensitive_semantics = false;

      {

        // if (StringUtility::isCppFileNameSuffix(filenameExtension) == true)
        if (CommandlineProcessing::isCppFileNameSuffix(filenameExtension) ==
                true &&
            project->get_C_only() == false) {
          // file = new SgSourceFile ( argv,  project );
          SgSourceFile *sourceFile = new SgSourceFile(argv, project);
          file = sourceFile;

          // This is a C++ file (so define __cplusplus, just like GNU gcc would)
          // file->set_requires_cplusplus_macro(true);
          file->set_sourceFileUsesCppFileExtension(true);

          // Use the filename suffix as a default means to set this value
          file->set_outputLanguage(SgFile::e_Cxx_language);

          // DQ (29/8/2017): Set the input language as well.
          file->set_inputLanguage(SgFile::e_Cxx_language);

          file->set_Cxx_only(true);

          // DQ (11/25/2020): Add support to set this as a specific language
          // kind file (there is at least one language kind file processed by
          // ROSE).
          Rose::is_Cxx_language = true;

          // DQ (12/23/2008): This is the eariliest point where the global scope
          // can be set. Note that file->get_requires_C_preprocessor() should be
          // false.
          ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
          sourceFile->initializeGlobalScope();
        } else {
          if (CommandlineProcessing::isCFileNameSuffix(filenameExtension) ==
                  true ||
              CommandlineProcessing::isAssemblerFileNameSuffix(
                  filenameExtension) == true ||
              project->get_C_only() == true) {
            // file = new SgSourceFile ( argv,  project );
            SgSourceFile *sourceFile = new SgSourceFile(argv, project);
            file = sourceFile;

            // This a not a C++ file (assume it is a C file and don't define the
            // __cplusplus macro, just like GNU gcc would)
            file->set_sourceFileUsesCppFileExtension(false);

            // Use the filename suffix as a default means to set this value
            file->set_outputLanguage(SgFile::e_C_language);

            // DQ (8/29/2017): Set the input language as well.
            file->set_inputLanguage(SgFile::e_C_language);

            file->set_C_only(true);

            // DQ (11/25/2020): Add support to set this as a specific language
            // kind file (there is at least one language kind file processed by
            // ROSE).
            Rose::is_C_language = true;

            // Default C standard is handled in cmdline.C; avoid forcing GNU
            // here.

            // DQ (12/23/2008): This is the eariliest point
            // where the global scope can be set. Note that
            // file->get_requires_C_preprocessor() should be
            // false.
            ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
            sourceFile->initializeGlobalScope();
          } else {
            if (CommandlineProcessing::isCudaFileNameSuffix(
                    filenameExtension) == true) {
              SgSourceFile *sourceFile = new SgSourceFile(argv, project);
              file = sourceFile;

              file->set_outputLanguage(SgFile::e_Cxx_language);

              // DQ (29/8/2017): Set the input language as well.
              file->set_inputLanguage(SgFile::e_Cxx_language);

              file->set_Cuda_only(true);

              // DQ (11/25/2020): Add support to set this as a
              // specific language kind file (there is at least
              // one language kind file processed by ROSE).
              Rose::is_Cuda_language = true;

              // DQ (12/23/2008): This is the eariliest point
              // where the global scope can be set. Note that
              // file->get_requires_C_preprocessor() should be
              // false.
              ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
              sourceFile->initializeGlobalScope();
            } else if (CommandlineProcessing::isOpenCLFileNameSuffix(
                           filenameExtension) == true) {
              SgSourceFile *sourceFile = new SgSourceFile(argv, project);
              file = sourceFile;
              file->set_OpenCL_only(true);

              // DQ (11/25/2020): Add support to set this as a
              // specific language kind file (there is at least
              // one language kind file processed by ROSE).
              Rose::is_OpenCL_language = true;

              // DQ (12/23/2008): This is the eariliest point
              // where the global scope can be set. Note that
              // file->get_requires_C_preprocessor() should be
              // false.
              ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
              sourceFile->initializeGlobalScope();
            } else if (commandLineSelectsFortran(argv)) {
              SgSourceFile *sourceFile = new SgSourceFile(argv, project);
              file = sourceFile;

              file->set_sourceFileUsesFortranFileExtension(false);
              file->set_outputLanguage(SgFile::e_Fortran_language);
              file->set_inputLanguage(SgFile::e_Fortran_language);
              file->set_Fortran_only(true);

              Rose::is_Fortran_language = true;
              SageBuilder::symbol_table_case_insensitive_semantics = true;

              const bool requires_C_preprocessor =
                  sourceContainsPreprocessorDirectives(sourceFilename) ||
                  !project->get_macroSpecifierList().empty();
              file->set_requires_C_preprocessor(requires_C_preprocessor);

              sourceFile->initializeGlobalScope();
            } else {
              file = new SgUnknownFile(argv, project);

              // This should have already been setup!
              // file->initializeSourcePosition();

              ASSERT_not_null(file->get_parent());
              ROSE_ASSERT(file->get_parent() == project);

              // If all else fails, then output the type of file
              // and exit.
              file->set_sourceFileTypeIsUnknown(true);

              file->set_requires_C_preprocessor(false);

              ASSERT_not_null(file->get_file_info());
              // file->set_parent(project);

              // DQ (2/3/2009): Uncommented this to report the file type when we
              // don't process it... outputTypeOfFileAndExit(sourceFilename);
              printf("Warning: This is an unknown file type, not being "
                     "processed by ROSE: sourceFilename = %s \n",
                     sourceFilename.c_str());
              outputTypeOfFileAndExit(sourceFilename);
            }
          }
        }
      }

      file->set_sourceFileUsesFortranFileExtension(false);
    }
  } else {
    // DQ (2/6/2009): This case is used by the build function
    // SageBuilder::buildFile().

    // DQ (12/22/2008): Make any error message from this branch more clear for
    // debugging! AS Is this option possible?
    printf("Is this branch reachable? \n");
    ROSE_ABORT();
    // abort();

    // ROSE_ASSERT (p_numberOfSourceFileNames == 0);
    ROSE_ASSERT(file->get_sourceFileNameWithPath().empty() == true);

    // If no source code file name was found then likely this is:
    //   1) a link command, or
    //   2) called as part of the SageBuilder::buildFile()
    // using the C++ compiler.  In this case skip the legacy frontend
    // processing.
  }

  // Keep the filename stored in the Sg_File_Info consistant.  Later we will
  // want to remove this redundency The reason we have the Sg_File_Info object
  // is so that we can easily support filename matching based on the integer
  // values instead of string comparisions.  Required for the handling co CPP
  // directives and comments.

  ASSERT_not_null(file);

  // DQ (6/13/2013): Added to support error checking (seperation of construction
  // of SgFile IR nodes from calling the fronend on each one).
  ASSERT_not_null(file->get_parent());
  ASSERT_not_null(isSgProject(file->get_parent()));
  ROSE_ASSERT(file->get_parent() == project);

  // DQ (7/2/2020): Added assertion (fails for snippet tests).
  if (file->get_preprocessorDirectivesAndCommentsList() == nullptr) {
    file->set_preprocessorDirectivesAndCommentsList(
        new ROSEAttributesListContainer());
  }
  ASSERT_not_null(file->get_preprocessorDirectivesAndCommentsList());

  return file;
}

void SgFile::runFrontend(int &nextErrorCode) {
  // DQ (6/13/2013):  This function supports the separation of the construction
  // of the SgFile IR nodes from the invocation of the frontend on each SgFile
  // IR node.

  // DQ (11/4/2015): Added assertion.
  ASSERT_not_null(this);

  // DQ (6/13/2013): Added to support error checking (seperation of construction
  // of SgFile IR nodes from calling the fronend on each one).
  ROSE_ASSERT(get_parent() != NULL);

  // DQ (6/13/2013): This is wrong, the parent of the SgFile is the SgFileList
  // IR node. ROSE_ASSERT(isSgProject(get_parent()) != NULL);

  // DQ (6/12/2013): This is the functionality that we need to separate out and
  // run after all of the SgSourceFile IR nodes are constructed.

  // The frontend is called explicitly outside the constructor since that allows
  // for a cleaner control flow. The callFrontEnd() relies on all the "set_"
  // flags to be already called therefore it was placed here. if (
  // isSgUnknownFile(file) == NULL && file != NULL  )

  // DQ (3/25/2017): The NULL check is done above and Clang reports it as a
  // warning that we want to remove. if ( this != NULL && isSgUnknownFile(this)
  // == NULL )
  if (isSgUnknownFile(this) == nullptr) {
    nextErrorCode = this->callFrontEnd();
    this->set_frontendErrorCode(nextErrorCode);
  }
  // Frontends are allowed to return a real diagnostic count for hard errors.
  // SgProject::parse/frontendExitStatus interpret those nonzero statuses; do
  // not treat them as an internal invariant violation here.
}

// The "purpose" as it appears in the man page, uncapitalized and a single,
// short, line.
static const char *purpose =
    "This tool provided basic ROSE source-to-source functionality";

static const char *description =
    "ROSE is a source-to-source compiler infrastructure for building analysis "
    "and/or transformation tools."
    "   --- More info can be found at http:www.RoseCompiler.org ";

// DQ (4/10/2017): Not clear if we want to sue this concept of switch setting in
// ROSE command line handling (implemented as a test). Switches for this tool.
// Tools with lots of switches will probably want these to be in some Settings
// struct mirroring the approach used by some analyses that have lots of
// settings. So we'll do that here too even though it looks funny.
struct RoseSettings {
  bool showRoseSettings; // should we show the outliner settings instead of
                         // running it?
  bool useOldCommandlineParser; // call the old Outliner command-line parser

  RoseSettings() : showRoseSettings(false), useOldCommandlineParser(false) {}
} rose_settings;

//! internal function to invoke the legacy frontend frontend and generate the
//! AST
int SgProject::parse(const vector<string> &argv) {
  // Not sure that if we are just linking that we should call a function called
  // "parse()"!!!

  // DQ (7/6/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST (SgProject::parse(argc,argv)):");

  // TOO1 (2014/01/22): TODO: Consider moving CLI processing out of SgProject
  // constructor. We can't set any error codes on SgProject since
  // SgProject::parse is being called from the SgProject::SgProject constructor,
  // meaning the SgProject object is not properly constructed yet.. The only
  // thing we can do, then, if there is an error here in the commandline
  // handling, is to halt the program.
  if (KEEP_GOING_CAUGHT_COMMANDLINE_SIGNAL) {
    std::cout << "[FATAL] "
              << "Unrecoverable signal generated during commandline processing"
              << std::endl;
    exit(1);
  } else {
    // builds file list (or none if this is a link line)
    processCommandLine(argv);
  }

  // volatile used as a work around for warning: variable might be clobbered by
  // 'longjmp' or 'vfork'
  volatile int errorCode = 0;

  // Normal case without AST Merge: Compiling ...
  // printf ("In SgProject::parse(const vector<string>& argv):
  // get_sourceFileNameList().size() = %" PRIuPTR "
  // \n",get_sourceFileNameList().size());
  if (get_sourceFileNameList().size() > 0) {
    rosePhaseTrace("project.parse.files.begin");
    errorCode = parse();
    rosePhaseTrace("project.parse.files.end");
  }

  // DQ (8/22/2009): We test the parent of SgFunctionTypeTable in the AST post
  // processing, so we need to make sure that it is set.
  SgFunctionTypeTable *functionTypeTable =
      SgNode::get_globalFunctionTypeTable();
  ASSERT_not_null(functionTypeTable);
  if (functionTypeTable->get_parent() == nullptr) {
    // ROSE_ASSERT(numberOfFiles() > 0);
    // printf ("Inside of SgProject::parse(const vector<string>& argv): set the
    // parent of SgFunctionTypeTable \n");
    if (numberOfFiles() > 0)
      functionTypeTable->set_parent(&(get_file(0)));
    else
      functionTypeTable->set_parent(this);
  }
  ROSE_ASSERT(functionTypeTable->get_parent() != NULL);

  ROSE_ASSERT(SgNode::get_globalFunctionTypeTable() != NULL);
  ROSE_ASSERT(SgNode::get_globalFunctionTypeTable()->get_parent() != NULL);
  // DQ (7/25/2010): We test the parent of SgTypeTable in the AST post
  // processing, so we need to make sure that it is set.
  SgTypeTable *typeTable = SgNode::get_globalTypeTable();
  ASSERT_not_null(typeTable);
  if (typeTable->get_parent() == nullptr) {
    // ROSE_ASSERT(numberOfFiles() > 0);
    // printf ("Inside of SgProject::parse(const vector<string>& argv): set the
    // parent of SgTypeTable \n");
    if (numberOfFiles() > 0)
      typeTable->set_parent(&(get_file(0)));
    else
      typeTable->set_parent(this);
  }
  ASSERT_not_null(typeTable->get_parent());

  // DQ (7/30/2010): This test fails in
  // tests/nonsmoke/functional/CompilerOptionsTests/testCpreprocessorOption DQ
  // (7/25/2010): Added new test. printf ("typeTable->get_parent()->class_name()
  // = %s \n",typeTable->get_parent()->class_name().c_str());
  // ROSE_ASSERT(isSgProject(typeTable->get_parent()) != NULL);

  ROSE_ASSERT(SgNode::get_globalTypeTable() != NULL);
  ROSE_ASSERT(SgNode::get_globalTypeTable()->get_parent() != NULL);
  return errorCode;
}

SgSourceFile::SgSourceFile(vector<string> &argv, SgProject *project)
// : SgFile (argv,errorCode,fileNameIndex,project)
{
  // printf ("In the SgSourceFile constructor \n");

  this->initialization();

  set_globalScope(nullptr);

  // DQ (6/15/2011): Added scope to hold unhandled declarations (see
  // test2011_80.C).
  set_temp_holding_scope(nullptr);

  // This constructor makes the frontend call to build the AST (via
  // callFrontEnd()). printf ("In SgSourceFile::SgSourceFile(): Calling
  // doSetupForConstructor() \n");
  doSetupForConstructor(argv, project);
}

int SgSourceFile::callFrontEnd() {
  int frontendErrorLevel = SgFile::callFrontEnd();
  if (get_experimental_flang_frontend() == true &&
      get_requires_C_preprocessor() == true) {
    ASSERT_not_null(get_globalScope());
    ASSERT_not_null(get_globalScope()->get_startOfConstruct());
    ASSERT_not_null(get_globalScope()->get_endOfConstruct());
    const std::string filename = get_sourceFileNameWithPath();
    get_globalScope()->get_startOfConstruct()->set_filenameString(filename);
    get_globalScope()->get_endOfConstruct()->set_filenameString(filename);
  }
  // DQ (1/21/2008): This must be set for all languages
  ASSERT_not_null(get_globalScope());
  ASSERT_not_null(get_globalScope()->get_file_info());
  ROSE_ASSERT(
      get_globalScope()->get_file_info()->get_filenameString().empty() ==
      false);
  // printf ("p_root->get_file_info()->get_filenameString() = %s
  // \n",p_root->get_file_info()->get_filenameString().c_str());

  // DQ (8/21/2008): Added assertion.
  ASSERT_not_null(get_globalScope()->get_startOfConstruct());
  ASSERT_not_null(get_globalScope()->get_endOfConstruct());

  return frontendErrorLevel;
}

int SgUnknownFile::callFrontEnd() {
  // DQ (2/3/2009): This function is defined, but should never be called.
  printf("Error: calling SgUnknownFile::callFrontEnd() \n");
  ROSE_ABORT();
}

int SgProject::RunFrontend() {
  TimingPerformance timer("AST (SgProject::RunFrontend()):");

  int status_of_function = Rose::Frontend::Run(this);
  this->set_frontendErrorCode(status_of_function);

  return status_of_function;
}

int SgProject::parse() {
  int errorCode = 0;

#define DEBUG_PARSE 0

  // DQ (7/6/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST (SgProject::parse()):");

  // ROSE_ASSERT (p_fileList != NULL);

#if defined(ROSE_FLANG_FRONTEND)
  FlangModuleInfo::setCurrentProject(this);
  FlangModuleInfo::set_inputDirs(this);
#endif

  // Simplify multi-file handling so that a single file is just the trivial
  // case and not a special separate case.
#if DEBUG_PARSE
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
         "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
  printf("In SgProject::parse(): Loop through the source files on the command "
         "line! p_sourceFileNameList = %" PRIuPTR " \n",
         p_sourceFileNameList.size());
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
         "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
#endif

  Rose_STL_Container<string>::iterator nameIterator =
      p_sourceFileNameList.begin();
  unsigned int i = 0;

  // The goal in this version of the code is to seperate the construction of
  // the SgFile objects from the invocation of the frontend on each of the
  // SgFile objects.  In general this allows the compilation to reference the
  // other SgFile objects on an as needed basis as part of running the
  // frontend.
  std::vector<SgFile *> vectorOfFiles;
  while (nameIterator != p_sourceFileNameList.end()) {
    int nextErrorCode = 0;

    // DQ (4/20/2006): Exclude other files from list in argc and argv
    vector<string> argv = get_originalCommandLineArgumentList();
    string currentFileName = *nameIterator;
    CommandlineProcessing::removeAllFileNamesExcept(argv, p_sourceFileNameList,
                                                    currentFileName);
    // DQ (11/13/2008): Removed overly complex logic here!
    SgFile *newFile = determineFileType(argv, nextErrorCode, this);
    ASSERT_not_null(newFile);
    ASSERT_not_null(newFile->get_startOfConstruct());
    ASSERT_not_null(newFile->get_parent());

    // DQ (6/13/2013): Added to support error checking (seperation of
    // construction of SgFile IR nodes from calling the fronend on each one).
    ASSERT_not_null(isSgProject(newFile->get_parent()));
    ROSE_ASSERT(newFile->get_parent() == this);

    // This just adds the new file to the list of files stored internally (note:
    // this sets the parent of the newFile).
    set_file(*newFile);

    // DQ (6/13/2013): Added to support error checking (seperation of
    // construction of SgFile IR nodes from calling the fronend on each one).
    ASSERT_not_null(newFile->get_parent());

    // This list of files will be iterated over to call the frontend in the next
    // loop.
    vectorOfFiles.push_back(newFile);

    // newFile->display("Called from SgProject::parse()");

    nameIterator++;
    i++;
  } // end while

  errorCode = this->RunFrontend();
  if (!Rose::KeepGoing::g_keep_going && errorCode != 0) {
    return errorCode;
  }
  if (errorCode > 3) {
    return errorCode;
  }

  // DQ (6/13/2013): Test the new function to lookup the SgFile from the name
  // with full path. This is a simple consistency test for that new function.
  for (size_t i = 0; i < vectorOfFiles.size(); i++) {
    string filename = vectorOfFiles[i]->get_sourceFileNameWithPath();
    SgFile *file = this->operator[](filename);
    ASSERT_not_null(file);

    if (SgProject::get_verbose() > 0) {
      printf("Testing: map of filenames to SgFile IR nodes: filename = %s is "
             "mapped to SgFile = %p \n",
             filename.c_str(), vectorOfFiles[i]);
    }

    ROSE_ASSERT(file == vectorOfFiles[i]);
  }

  // GB (8/19/2009): Moved the AstPostProcessing call from
  // SgFile::callFrontEnd to this point. Thus, it is only called once for
  // the whole project rather than once per file. Repeated calls to
  // AstPostProcessing are slow due to repeated memory pool traversals. The
  // AstPostProcessing is only to be called if there are input files to run
  // it on, and they are meant to be used in some way other than just
  // calling the backend on them. (If only the backend is used, this was
  // never called by SgFile::callFrontEnd either.)
  // if ( !get_fileList().empty() && !get_useBackendOnly() )

  // AstPostProcessing expects a coherent AST. If any frontend failed, the
  // project may contain partial declarations that should be reported back to
  // the caller as a frontend failure rather than pushed through global fixup.
  if ((get_fileList().empty() == false) && (get_useBackendOnly() == false) &&
      errorCode == 0) {
    rosePhaseTrace("AstPostProcessing.begin");
    AstPostProcessing(this);
    rosePhaseTrace("AstPostProcessing.end");

    // Some template-instantiation function declaration chains are finalized
    // only after the broader AST post-processing pipeline completes. Re-run
    // the targeted template-instantiation repair once the project AST is fully
    // assembled so defining/nondefining links remain coherent for later AST
    // copies and consistency checks.
    rosePhaseTrace("fixupTemplateInstantiations.begin");
    fixupTemplateInstantiations(this);
    rosePhaseTrace("fixupTemplateInstantiations.end");
  }

  // negara1 (06/23/2011): Collect information about the included files to
  // support unparsing of those that are modified. In the first step, get the
  // include search paths, which will be used while attaching include
  // preprocessing infos. Proceed only if there are input files and they require
  // header files unparsing.
  if (!get_fileList().empty() &&
      (*get_fileList().begin())->get_unparseHeaderFiles()) {
    if (SgProject::get_verbose() >= 1) {
      cout << endl << "***HEADER FILES ANALYSIS***" << endl << endl;
    }
    CompilerOutputParser compilerOutputParser(this);
    const pair<list<string>, list<string>> &includedFilesSearchPaths =
        compilerOutputParser.collectIncludedFilesSearchPaths();
    set_quotedIncludesSearchPaths(includedFilesSearchPaths.first);
    set_bracketedIncludesSearchPaths(includedFilesSearchPaths.second);

    if (SgProject::get_verbose() >= 1) {
      // DQ (11/7/2018): Output the list of quotedIncludesSearchPaths and
      // bracketedIncludesSearchPaths include paths.
      CollectionHelper::printList(get_quotedIncludesSearchPaths(),
                                  "\nQuoted includes search paths:", "Path:");
      CollectionHelper::printList(
          get_bracketedIncludesSearchPaths(),
          "\nBracketed includes search paths:", "Path:");
    }
  }

  // GB (9/4/2009): Moved the secondary pass over source files (which
  // attaches the preprocessing information) to this point. This way, the
  // secondary pass over each file runs after all fixes have been done. This
  // is relevant where the AstPostProcessing mechanism must first mark nodes
  // to be output before preprocessing information is attached.
  SgFilePtrList &files = get_fileList();

  // volatile used as a work around for warning: variable might be clobbered by
  // 'longjmp' or 'vfork'
  volatile bool unparse_using_tokens = false;

  for (SgFile *file : files) {
    ASSERT_not_null(file);

    SgSourceFile *sourceFile = isSgSourceFile(file);

    // ROSE_ASSERT(sourceFile != NULL);
    if (sourceFile != nullptr) {
      // DQ (4/25/2021): I think this should be a static bool data member.
      if (unparse_using_tokens == false) {
        unparse_using_tokens = sourceFile->get_unparse_tokens();
      }
    }
    if (KEEP_GOING_CAUGHT_FRONTEND_SECONDARY_PASS_SIGNAL) {
      std::cout << "[WARN] "
                << "Configured to keep going after catching a signal in "
                << "SgFile::secondaryPassOverSourceFile()" << std::endl;

      if (file != nullptr) {
        file->set_frontendErrorCode(100);
        int save_volatile_variable = errorCode;
        errorCode = std::max(100, save_volatile_variable);
      } else {
        std::cout
            << "[FATAL] "
            << "Unable to keep going due to an unrecoverable internal error"
            << std::endl;
        // Liao, 4/25/2017. one assertion failure may trigger other assertion
        // failures. We still want to keep going.
        exit(1);
      }
    } else {
      // DQ (1/23/2018): If we are not doing the translation of legacy
      // frontend to ROSE, then we don't want to call this second pass.
      // This will fix the negative test in Plum hall for what should be
      // an error to the C preprocessor.
      // file->secondaryPassOverSourceFile();

      // DQ (8/19/2019): Divide this into two parts, for optimization of
      // header file unparsing, optionally support the main file
      // collection of comments and CPP directives, and seperately the
      // header file collection of comments and CPP directives.
      rosePhaseTrace("secondaryPassOverSourceFile.begin");
      file->secondaryPassOverSourceFile();
      rosePhaseTrace("secondaryPassOverSourceFile.end");
      ROSE_ASSERT(file->get_header_file_unparsing_optimization_source_file() ==
                  false);
      ROSE_ASSERT(file->get_header_file_unparsing_optimization_header_file() ==
                  false);
    }
  }

  if (errorCode != 0) {
    return errorCode;
  }

  // negara1 (06/23/2011): Collect information about the included files to
  // support unparsing of those that are modified. In the second step (after
  // preprocessing infos are already attached), collect the including files map.
  // Proceed only if there are input files and they require header files
  // unparsing.
  if (!get_fileList().empty() &&
      (*get_fileList().begin())->get_unparseHeaderFiles()) {
    CompilerOutputParser compilerOutputParser(this);
    const map<string, set<string>> &includedFilesMap =
        compilerOutputParser.collectIncludedFilesMap();
    IncludingPreprocessingInfosCollector includingPreprocessingInfosCollector(
        this, includedFilesMap);
    const map<string, set<PreprocessingInfo *>>
        &includingPreprocessingInfosMap =
            includingPreprocessingInfosCollector.collect();
    set_includingPreprocessingInfosMap(includingPreprocessingInfosMap);

    std::map<std::string, SgSourceFile *> sourceFilesByPath;
    for (SgFile *file : get_fileList()) {
      SgSourceFile *sourceFile = isSgSourceFile(file);
      if (sourceFile == NULL) {
        continue;
      }
      std::string normalizedPath =
          FileHelper::normalizePathIfPossible(sourceFile->getFileName());
      if (!normalizedPath.empty()) {
        sourceFilesByPath[normalizedPath] = sourceFile;
      }
    }

    std::map<std::string, SgSourceFile *> tokenMapFilesByPath;
    if (unparse_using_tokens == true) {
      for (std::map<SgSourceFile *,
                    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                        *>::const_iterator mapIt =
               Rose::tokenSubsequenceMapOfMapsBySourceFile.begin();
           mapIt != Rose::tokenSubsequenceMapOfMapsBySourceFile.end();
           ++mapIt) {
        SgSourceFile *sourceFile = mapIt->first;
        if (sourceFile == NULL) {
          continue;
        }
        std::string normalizedPath =
            FileHelper::normalizePathIfPossible(sourceFile->getFileName());
        if (!normalizedPath.empty()) {
          tokenMapFilesByPath[normalizedPath] = sourceFile;
        }
      }
    }

    std::set<std::string> processedIncludingFiles;
    bool discoveredNewSourceFile = false;
    do {
      discoveredNewSourceFile = false;

      for (map<string, set<string>>::const_iterator it =
               includedFilesMap.begin();
           it != includedFilesMap.end(); ++it) {
        const std::string includingPath =
            FileHelper::normalizePathIfPossible(it->first);
        std::map<std::string, SgSourceFile *>::const_iterator rootIt =
            sourceFilesByPath.find(includingPath);
        if (rootIt == sourceFilesByPath.end()) {
          continue;
        }

        if (!processedIncludingFiles.insert(includingPath).second) {
          continue;
        }

        SgSourceFile *traversalRoot = rootIt->second;
        if (unparse_using_tokens == true &&
            !shouldBuildTokenMapping(traversalRoot)) {
          continue;
        }

        ensureSourceFileIncludeRoot(this, traversalRoot);

        const set<string> &includedSet = it->second;
        for (set<string>::const_iterator incIt = includedSet.begin();
             incIt != includedSet.end(); ++incIt) {
          std::string includedPath =
              FileHelper::normalizePathIfPossible(*incIt);
          if (!FileHelper::fileExists(includedPath)) {
            continue;
          }

          std::map<std::string, SgSourceFile *>::const_iterator headerIt =
              sourceFilesByPath.find(includedPath);
          SgSourceFile *headerFile =
              headerIt != sourceFilesByPath.end() ? headerIt->second : NULL;
          if (headerFile == NULL && unparse_using_tokens == true) {
            std::map<std::string, SgSourceFile *>::const_iterator tokenMapIt =
                tokenMapFilesByPath.find(includedPath);
            if (tokenMapIt != tokenMapFilesByPath.end()) {
              headerFile = tokenMapIt->second;
            }
          }
          if (headerFile == NULL) {
            // Header-file unparsing needs a file wrapper even when the header
            // AST is emitted from an enclosing scope in another translation
            // unit. Token-based unparsing additionally uses the wrapper for the
            // detached token map.
            headerFile =
                buildHeaderSourceFile(this, includedPath, traversalRoot);
          }
          if (headerFile == NULL) {
            continue;
          }

          if (sourceFilesByPath.insert(std::make_pair(includedPath, headerFile))
                  .second) {
            discoveredNewSourceFile = true;
          }
          if (unparse_using_tokens == true) {
            tokenMapFilesByPath[includedPath] = headerFile;
          }

          SgIncludeFile *includeFile = findIncludeFileByPath(
              traversalRoot->get_associated_include_file(), includedPath);
          if (includeFile == NULL) {
            PreprocessingInfo *preprocessingInfo =
                findIncludingPreprocessingInfo(this,
                                               includingPreprocessingInfosMap,
                                               traversalRoot, includedPath);
            includeFile =
                synthesizeIncludeFile(this, traversalRoot, headerFile,
                                      includedPath, preprocessingInfo);
          }

          if (includeFile != NULL &&
              headerFile->get_associated_include_file() == NULL) {
            headerFile->set_associated_include_file(includeFile);
          }
          if (includeFile != NULL && includeFile->get_source_file() == NULL) {
            includeFile->set_source_file(headerFile);
          }

          if (unparse_using_tokens == true) {
            if (Rose::tokenSubsequenceMapOfMapsBySourceFile.find(headerFile) !=
                Rose::tokenSubsequenceMapOfMapsBySourceFile.end()) {
              std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                  *existingMap =
                      Rose::tokenSubsequenceMapOfMapsBySourceFile[headerFile];
              if (existingMap != NULL && !existingMap->empty()) {
                continue;
              }
            }

            ensureHeaderTokenMapping(headerFile, traversalRoot);
          }
        }
      }
    } while (discoveredNewSourceFile == true);

    if (SgProject::get_verbose() >= 1) {
      printf("\nOutput info for unparse headers support: \n");
      CollectionHelper::printMapOfSets(
          includedFilesMap, "\nIncluded files map:", "File:", "Included file:");
      CollectionHelper::printMapOfSets(
          get_includingPreprocessingInfosMap(),
          "\nIncluding files map:", "File:", "Including file:");
    }

    // ROSE_ASSERT(file->get_header_file_unparsing_optimization() == true);
    // ROSE_ASSERT(file->get_header_file_unparsing_optimization_source_file() ==
    // false);
    // ROSE_ASSERT(file->get_header_file_unparsing_optimization_header_file() ==
    // false);
  }

  if (get_verbose() > 0) {
    // Report the error code if it is non-zero (but only in verbose mode)
    if (errorCode > 0) {
      printf("Frontend Warnings only: errorCode = %d \n", errorCode);
      if (errorCode > 3) {
        printf("Frontend Errors found: errorCode = %d \n", errorCode);
      }
    }
  }

  // warnings from legacy frontend processing are OK but not errors
  ROSE_ASSERT(errorCode <= 3);

  // if (get_useBackendOnly() == false)
  if (SgProject::get_verbose() >= 1) {
    cout << "C++ source(s) parsed. AST generated." << endl;
  }

  if (get_verbose() > 3) {
    printf("In SgProject::parse() (verbose mode ON): \n");
    display("In SgProject::parse()");
  }

#if DEBUG_PARSE
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
  printf("Leaving SgProject::parse(): errorCode = %d \n", errorCode);
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
#endif

  return errorCode;
} // end parse(;

// negara1 (07/29/2011)
// The returned file path is not normalized.
// TODO: Return the normalized path after the bug in ROSE is fixed. The bug
// manifests itself when the same header file is included in multiple places
// using different paths. In such a case, ROSE treats the same file as different
// files and generates different IDs for them.
string SgProject::findIncludedFile(PreprocessingInfo *preprocessingInfo) {
  IncludeDirective includeDirective(preprocessingInfo->getString());
  const string &includedPath = includeDirective.getIncludedPath();
  if (FileHelper::isAbsolutePath(includedPath)) {
    // the path is absolute, so no need to search for the file
    if (FileHelper::fileExists(includedPath)) {
      return includedPath;
    }
    return ""; // file does not exist, so return an empty string
  }
  if (includeDirective.isQuotedInclude()) {
    // start looking from the current folder, then proceed with the quoted
    // includes search paths
    // TODO: Consider the presence of -I- option, which disables looking in the
    // current folder for quoted includes.
    string currentFolder = FileHelper::getParentFolder(
        preprocessingInfo->get_file_info()->get_filenameString());
    p_quotedIncludesSearchPaths.insert(p_quotedIncludesSearchPaths.begin(),
                                       currentFolder);
    string includedFilePath = FileHelper::getIncludedFilePath(
        p_quotedIncludesSearchPaths, includedPath);
    p_quotedIncludesSearchPaths.erase(
        p_quotedIncludesSearchPaths
            .begin()); // remove the previously inserted current folder (for
                       // other files it might be different)
    if (!includedFilePath.empty()) {
      return includedFilePath;
    }
  }
  // For bracketed includes and for not yet found quoted includes proceed with
  // the bracketed includes search paths
  return FileHelper::getIncludedFilePath(p_bracketedIncludesSearchPaths,
                                         includedPath);
}

void SgSourceFile::doSetupForConstructor(const vector<string> &argv,
                                         SgProject *project) {
  // Call the base class implementation!
  SgFile::doSetupForConstructor(argv, project);
}

void SgUnknownFile::doSetupForConstructor(const vector<string> &argv,
                                          SgProject *project) {
  SgFile::doSetupForConstructor(argv, project);
}

void SgFile::doSetupForConstructor(const vector<string> &argv,
                                   SgProject *project) {
  // JJW 10-26-2007 ensure that this object is not on the stack
  preventConstructionOnStack(this);

  // Set the project early in the construction phase so that we can access data
  // in the parent if needed (useful for template handling but also makes sure
  // the parent is set (and avoids fixup (currently done, but too late in the
  // construction process for the template support).
  if (project != nullptr)
    set_parent(project);

  ASSERT_not_null(project);
  ASSERT_not_null(get_parent());

  // initalize all local variables to default values
  initialization();

  if (project->get_compileOnly()) {
    set_compileOnly(true);
  }

  ASSERT_not_null(get_parent());

  // DQ (2/4/2009): The specification of "-rose:binary" causes filenames to
  // be interpreted differently if they are object files or libary archive
  // files. DQ (4/21/2006): Setup the source filename as early as possible
  // setupSourceFilename(argv);
  // Rose_STL_Container<string> fileList =
  // CommandlineProcessing::generateSourceFilenames(argv);
  // Rose_STL_Container<string> fileList =
  // CommandlineProcessing::generateSourceFilenames(argv);
  Rose_STL_Container<string> fileList =
      CommandlineProcessing::generateSourceFilenames(
          argv, project->get_binary_only());

  // DQ (12/23/2008): Use of this assertion will simplify the code below!
  ROSE_ASSERT(fileList.empty() == false);
  string sourceFilename = *(fileList.begin());

  // printf ("Before conversion to absolute path: sourceFilename = %s
  // \n",sourceFilename.c_str()); sourceFilename =
  // StringUtility::getAbsolutePathFromRelativePath(sourceFilename);
  sourceFilename =
      StringUtility::getAbsolutePathFromRelativePath(sourceFilename, true);

  set_sourceFileNameWithPath(sourceFilename);

  // printf ("In SgFile::setupSourceFilename(const vector<string>& argv):
  // p_sourceFileNameWithPath = %s \n",get_sourceFileNameWithPath().c_str());
  // set_sourceFileNameWithoutPath(
  // Rose::utility_stripPathFromFileName(get_sourceFileNameWithPath().c_str())
  // );
  set_sourceFileNameWithoutPath(StringUtility::stripPathFromFileName(
      get_sourceFileNameWithPath().c_str()));

  initializeSourcePosition(sourceFilename);
  ASSERT_not_null(get_file_info());

  // printf ("In SgFile::doSetupForConstructor(): source position set for
  // sourceFilename = %s \n",sourceFilename.c_str());

  // DQ (5/9/2007): Moved this call from above to where the file name is
  // available so that we could include the filename in the label.  This helps
  // to identify the performance data with individual files where multiple
  // source files are specificed on the command line. printf
  // ("p_sourceFileNameWithPath = %s \n",p_sourceFileNameWithPath);
  string timerLabel =
      "AST SgFile Constructor for " + p_sourceFileNameWithPath + ":";
  TimingPerformance timer(timerLabel);

  // Build a DEEP COPY of the input parameters!
  vector<string> local_commandLineArgumentList = argv;

  // Save the commandline as a list of strings (we made a deep copy because the
  // "callFrontEnd()" function might change it!
  get_originalCommandLineArgumentList() = local_commandLineArgumentList;

  // DQ (5/22/2005): Store the file name index in the SgFile object so that it
  // can figure out which file name applies to it.  This helps support functions
  // such as "get_filename()" used elsewhere in Sage III.  Not clear if we
  // really need this! error checking
  ROSE_ASSERT(argv.size() > 1);

  ASSERT_not_null(get_file_info());
  ASSERT_not_null(get_startOfConstruct());

  // DQ (6/13/2013): Added to support error checking (seperation of construction
  // of SgFile IR nodes from calling the fronend on each one).
  ASSERT_not_null(get_parent());
  ROSE_ASSERT(get_parent() == project);
}

string
SgFile::generate_C_preprocessor_intermediate_filename(string sourceFilename) {
  // DQ (9/24/2013): We need this assertion to make sure we are not causing what
  // might be strange errors in memory.
  ROSE_ASSERT(sourceFilename.empty() == false);

  // Note: for "foo.F90" the fileNameSuffix() returns "F90"
  string filenameExtension = StringUtility::fileNameSuffix(sourceFilename);

  // DQ (9/24/2013): We need this assertion to make sure we are not causing what
  // might be strange errors in memory.
  ROSE_ASSERT(filenameExtension.empty() == false);

  string sourceFileNameWithoutExtension =
      StringUtility::stripFileSuffixFromFileName(sourceFilename);

  // DQ (9/24/2013): We need this assertion to make sure we are not causing what
  // might be strange errors in memory.
  ROSE_ASSERT(sourceFileNameWithoutExtension.empty() == false);

  // string sourceFileNameInputToCpp = get_sourceFileNameWithPath();

  // We need to turn on the 5th bit to make the capital a lower case character
  // (assume ASCII)
  filenameExtension[0] = filenameExtension[0] | (1 << 5);

  // Rename the CPP generated intermediate file (strip path to put it in the
  // current directory) string sourceFileNameOutputFromCpp =
  // sourceFileNameWithoutExtension + "_preprocessed." + filenameExtension;
  string sourceFileNameWithoutPathAndWithoutExtension =
      StringUtility::stripPathFromFileName(sourceFileNameWithoutExtension);
  string sourceFileNameOutputFromCpp =
      sourceFileNameWithoutPathAndWithoutExtension + "_postprocessed." +
      filenameExtension;

  return sourceFileNameOutputFromCpp;
}

int SgFile::callFrontEnd() {
  if (SgProject::get_verbose() > 0) {
    std::cout << "[INFO] [SgFile::callFrontEnd]" << std::endl;
  }

  // DQ (1/17/2006): test this
  // ROSE_ASSERT(get_fileInfo() != NULL);

  int fileNameIndex = 0;

  // DQ (4/21/2006): I think we can now assert this!
  ROSE_ASSERT(fileNameIndex == 0);

  // Flags for debug and command line options output.
  bool debugProjectCompileCommandLineWithArgs = false;
  bool showBackendCommandLine = SgProject::get_showBackendCommandLine();

  // DQ (7/6/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST Front End Processing (SgFile):");

  // This function processes the command line and calls the legacy frontend
  // frontend.
  int frontendErrorLevel = 0;

  // Build an argc,argv based C style commandline (we might not really need
  // this)
  vector<string> argv = get_originalCommandLineArgumentList();

#if ROSE_INTERNAL_DEBUG
  if (ROSE_DEBUG > 9) {
    // Print out the input arguments, so we can set them up internally instead
    // of on the command line (which is driving me nuts)
    for (unsigned int i = 0; i < argv.size(); i++)
      printf("argv[%d] = %s \n", i, argv[i]);
  }
#endif

  // printf ("Inside of SgFile::callFrontEnd(): fileNameIndex = %d
  // \n",fileNameIndex);

  // Save this so that it can be used in the template instantiation phase later.
  // This file is later written into the *.ti file so that the compilation can
  // be repeated as required to instantiate all function templates.
  std::string translatorCommandLineString =
      CommandlineProcessing::generateStringFromArgList(argv, false, true);
  set_savedFrontendCommandLine(translatorCommandLineString);

  // display("At TOP of SgFile::callFrontEnd()");

  // local copies of argc and argv variables
  // The purpose of building local copies is to avoid
  // the modification of the command line by SLA
  vector<string> localCopy_argv = argv;
  // printf ("DONE with copy of command line! \n");
  // Process command line options specific to ROSE
  // This leaves all filenames and non-rose specific option in the argv list
  processRoseCommandLineOptions(localCopy_argv);
  // DQ (6/21/2005): Process template specific options so that we can generated
  // code for the backend compiler (this processing is backend specific).
  processBackendSpecificCommandLineOptions(localCopy_argv);
  // display("AFTER processRoseCommandLineOptions in SgFile::callFrontEnd()");

  // Use ROSE buildCommandLine() function
  // int numberOfCommandLineArguments = 24;
  // char** inputCommandLine = new char* [numberOfCommandLineArguments];
  // ROSE_ASSERT (inputCommandLine != NULL);
  vector<string> inputCommandLine;

  // Build the commandline for legacy frontend
  if (get_C_only() || get_Cxx_only() || get_Cuda_only() || get_OpenCL_only()) {
#if defined(BACKEND_CXX_IS_CLANG_COMPILER) && !defined(ROSE_USE_CLANG_FRONTEND)
    // REX: Clang backend is now supported with Clang frontend
#endif

    // REX: Call Clang command line builder (legacy frontend removed)
    build_CLANG_CommandLine(inputCommandLine, localCopy_argv, fileNameIndex);
  } else {
  }

  std::string tmp_translatorCommandLineString =
      CommandlineProcessing::generateStringFromArgList(inputCommandLine, false,
                                                       true);

  // DQ (10/15/2005): This is now a single C++ string (and not a list)
  // Make sure the list of file names is allocated, even if there are no file
  // names in the list. DQ (1/23/2004): I wish that C++ string objects had been
  // used uniformally through out this code!!! ROSE_ASSERT
  // (get_sourceFileNamesWithoutPath() != NULL); ROSE_ASSERT
  // (get_sourceFileNameWithoutPath().empty() == false);

  // display("AFTER build_frontend_command_line in SgFile::callFrontEnd()");

  // Exit if we are to ONLY call the vendor's backend compiler
  if (p_useBackendOnly == true) {
    return 0;
  }

  ROSE_ASSERT(p_useBackendOnly == false);

  // DQ (4/21/2006): If we have called the frontend for this SgFile then mark
  // this file to be unparsed. This will cause code to be generated and the
  // compileOutput() function will then set the name of the file that the
  // backend (vendor) compiler will compile to be the the intermediate file.
  // Else it will be set to be the origianl source file.  In the new design, the
  // frontendShell() can be called to generate just the SgProject and SgFile
  // nodes and we can loop over the SgFile objects and call the frontend
  // separately for each SgFile.  so we have to set the output file name to be
  // compiled late in the processing (at backend compile time since we don't
  // know when or if the frontend will be called for each SgFile). PP
  // (8/23/2022): Experimental: Do not override the flag from the command line.
  // RC-1381
  //   set_skip_unparse(false);

  // DQ (9/2/2008): Factored out the details of building the AST for source
  // code (SgSourceFile IR node).
  // Note that making buildAST() a virtual function does not appear to solve
  // the problems since it is called form the base class.  This is awkward
  // code which is temporary.
  switch (this->variantT()) {
  case V_SgFile:
  case V_SgSourceFile: {
    SgSourceFile *sourceFile = const_cast<SgSourceFile *>(isSgSourceFile(this));
    ASSERT_not_null(sourceFile);
    rosePhaseTrace("SgSourceFile.buildAST.begin");
    frontendErrorLevel = sourceFile->buildAST(localCopy_argv, inputCommandLine);
    rosePhaseTrace("SgSourceFile.buildAST.end");
    break;
  }

  case V_SgUnknownFile: {
    break;
  }

  default: {
    printf("Error: default reached in Rose parser/IR translation processing: "
           "class name = %s \n",
           this->class_name().c_str());
    ROSE_ABORT();
  }
  }

  // if there are warnings report that there are in the verbose mode and
  // continue
  if (frontendErrorLevel > 0) {
    if (get_verbose() >= 1)
      cout << "Warnings in Rose parser/IR translation processing! (continuing "
              "...) "
           << endl;
  }

  // DQ (4/20/2006): This code was moved from the SgFile constructor so that is
  // would permit the separate construction of the SgProject and call to the
  // front-end cleaner.

  // DQ (5/22/2005): This is a older function with a newer more up-to-date
  // comment on why we have it. This function is a repository for minor
  // AST fixups done as a post-processing step in the construction of the
  // Sage III AST from the legacy frontend frontend.  In some cases it
  // fixes specific problems in either legacy frontend or the translation
  // of legacy frontend to Sage III (more the later than the former). In
  // other cases if does post-processing (e.g. setting parent pointers in
  // the AST) can could only done from a more complete global view of the
  // staticly defined AST.  In many cases these AST fixups are not so
  // temporary so the name of the function might change at some point.
  // Notice that all AST fixup is done before attachment of the comments
  // to the AST. temporaryAstFixes(this);

  // GB (8/19/2009): Commented this out and moved it to SgProject::parse().
  // Repeated calls to AstPostProcessing (one per file) can be slow on
  // projects consisting of multiple files due to repeated memory pool
  // traversals.
  // AstPostProcessing(this);

#if defined(ROSE_FLANG_FRONTEND)
  // FMZ: 05/30/2008.  Do not generate module files for nested module loads.
  if (get_Fortran_only() == true && frontendErrorLevel == 0) {
    bool isModuleFile = false;
    if (get_experimental_flang_frontend() == true) {
      isModuleFile = FlangModuleInfo::isModuleFile();
    }
    if (isModuleFile == false) {
      if (get_verbose() > 1)
        printf("Generating a Fortran module file (*.rmod) \n");

      generateModFile(this);

      if (get_verbose() > 1)
        printf("DONE: Generating a Fortran module file (*.rmod) \n");
    }
  }
#endif

  // return the error code associated with the call to the C++ Front-end
  return frontendErrorLevel;
}

void SgFile::secondaryPassOverSourceFile() {
  // DQ (8/19/2019): We want to optionally separate this function out over two
  // phases to optimize the support for header file unparsing. When not
  // optimized, we process all of the header file with the source file. When we
  // are supporting optimization, we handle the collection of comments and CPP
  // directives and their insertion into the AST in two phases:
  //  1) Just the source file (no header files)
  //  2) Just the header files (not the source file)

  // DQ (02/20/2021): Using the performance tracking within ROSE.
  TimingPerformance timer("AST secondaryPassOverSourceFile:");

#define DEBUG_SECONDARY_PASS 0

  // To support initial testing we will call one phase immediately after the
  // other.  Late we will call the second phase, header file processing, from
  // within the unparser when we know what header files are intended to be
  // unparsed.

  // **************************************************************************
  //                      Secondary Pass Over Source File
  // **************************************************************************
  // This pass collects extra information about the source file that may not
  // have been available from previous tools that operated on the file. For
  // example:
  //    1) legacy frontend ignores comments and so we collect the whole token
  //    stream in this phase.
  // For source code (C,C++,Fortran) we collect the whole token stream, for
  // example:
  //    1) Comments
  //    2) Preprocessors directives
  //    3) White space
  //    4) All tokens (each is classified as to what specific type of token
  //    it is)
  //

  // DQ (10/27/2018): Added documentation.
  // Note that this function is called from two locations:
  // 1) From the sage_support.C (for the main source file)
  // 2) From the attachPreprocessingInfoTraversal.C (for any header files
  // when using the unparse headers option)

  // GB (9/4/2009): Factored out the secondary pass. It is now done after
  // the whole project has been constructed and fixed up.

  {
    // This is set in the unparser now so that we can handle the source file
    // plus all header files

    // DQ (8/19/2019): When header file optimization is turned on the this
    // asertion is incorrect. ROSE_ASSERT
    // (p_preprocessorDirectivesAndCommentsList == NULL);

    // DQ (8/19/2019): When header file optimization is turned on the this
    // asertion is incorrect. Build the empty list container so that we can just
    // add lists for new files as they are encountered
    // p_preprocessorDirectivesAndCommentsList = new
    // ROSEAttributesListContainer(); ROSE_ASSERT
    // (p_preprocessorDirectivesAndCommentsList != NULL); DQ (4/24/2021): Trying
    // to debug the header file optimization support. printf ("In
    // SgFile::secondaryPassOverSourceFile():
    // header_file_unparsing_optimization_header_file = %s
    // \n",header_file_unparsing_optimization_header_file ? "true" : "false");

    {
      // DQ (9/23/2019): We need to support calling this function multiple
      // times. ROSE_ASSERT (p_preprocessorDirectivesAndCommentsList == NULL);
      // p_preprocessorDirectivesAndCommentsList = new
      // ROSEAttributesListContainer();
      if (p_preprocessorDirectivesAndCommentsList == nullptr) {
#if DEBUG_SECONDARY_PASS
        printf("Initialize NULL p_preprocessorDirectivesAndCommentsList to "
               "empty ROSEAttributesListContainer \n");
#endif
        p_preprocessorDirectivesAndCommentsList =
            new ROSEAttributesListContainer();
      } else {
#if DEBUG_SECONDARY_PASS
        printf("NOTE: p_preprocessorDirectivesAndCommentsList is already "
               "defined! \n");
        printf(" --- filename = %s \n", this->getFileName().c_str());
        printf(" --- p_preprocessorDirectivesAndCommentsList->getList().size() "
               "= %zu \n",
               p_preprocessorDirectivesAndCommentsList->getList().size());
#endif
      }
      ASSERT_not_null(p_preprocessorDirectivesAndCommentsList);
    }

    // DQ (4/19/2006): since they can take a while and includes substantial
    // file I/O we make this optional (selected from the command line).
    // bool collectAllCommentsAndDirectives =
    // get_collectAllCommentsAndDirectives(); DQ (12/17/2008): The merging of
    // CPP directives and comments from either the source file or including all
    // the include files is not implemented as a single traversal and has been
    // rewritten.
    if (get_skip_commentsAndDirectives() == false) {
      if (get_verbose() >= 1) {
        printf("In SgFile::secondaryPassOverSourceFile(): calling "
               "attachAllPreprocessingInfo() \n");
      }

      // printf ("Secondary pass over source file = %s to comment comments and
      // CPP directives
      // \n",this->get_file_info()->get_filenameString().c_str()); SgSourceFile*
      // sourceFile = const_cast<SgSourceFile*>(this);
      SgSourceFile *sourceFile = isSgSourceFile(this);
      ASSERT_not_null(sourceFile);

      // Save the state of the requirement fo CPP processing (fortran only)
      bool requiresCPP = false;
      if (get_Fortran_only() == true) {
        requiresCPP = get_requires_C_preprocessor();
        if (requiresCPP == true) {
#if DEBUG_SECONDARY_PASS
          printf("@@@@@@@@@@@@@@ Set requires_C_preprocessor to false (test 4) "
                 "\n");
#endif
          set_requires_C_preprocessor(false);
        }
      }
#if DEBUG_SECONDARY_PASS
      printf("In SgFile::secondaryPassOverSourceFile(): requiresCPP = %s \n",
             requiresCPP ? "true" : "false");
#endif
      // Debugging code (eliminate use of CPP directives from source file so
      // that we can debug the insertion of linemarkers from first phase of CPP
      // processing.
      const bool using_flang_frontend =
          sourceFile->get_experimental_flang_frontend() &&
          sourceFile->get_Fortran_only();
      SgNode *preproc_root = sourceFile->get_globalScope();
      if (preproc_root == nullptr) {
        preproc_root = sourceFile;
      }
      const bool preproc_already_attached =
          hasAttachedPreprocessingInfo(preproc_root);
      if (requiresCPP == false) {
        // DQ (10/21/2019): This will be tested below, in
        // attachPreprocessingInfo(), if it is not in place then we need to do
        // it here. ROSEAttributesListContainerPtr filePreprocInfo =
        // sourceFile->get_preprocessorDirectivesAndCommentsList();
        // ROSE_ASSERT(filePreprocInfo->getList().empty() == false);

        // DQ (10/18/2020): This is enforced within attachPreprocessingInfo(),
        // so move the enforcement to be as early as possible.
        ROSE_ASSERT(
            sourceFile->get_processedToIncludeCppDirectivesAndComments() ==
            false);
#if DEBUG_SECONDARY_PASS
        printf(
            "@@@@@@@@@@@@@@ In SgFile::secondaryPassOverSourceFile(): Calling "
            "attachPreprocessingInfo(): sourceFile = %p = %s filename = %s \n",
            sourceFile, sourceFile->class_name().c_str(),
            sourceFile->getFileName().c_str());
#endif
        rosePhaseTrace("secondaryPassOverSourceFile.attachPreprocessing.begin");
        if (!using_flang_frontend) {
          attachPreprocessingInfo(sourceFile, "", !preproc_already_attached);
        } else {
          // Flang already attaches Fortran comments during AST construction.
          sourceFile->set_processedToIncludeCppDirectivesAndComments(true);
        }
        rosePhaseTrace("secondaryPassOverSourceFile.attachPreprocessing.end");
#if DEBUG_SECONDARY_PASS
        printf("@@@@@@@@@@@@@@ DONE: In SgFile::secondaryPassOverSourceFile(): "
               "Calling attachPreprocessingInfo(): sourceFile = %p = %s \n",
               sourceFile, sourceFile->class_name().c_str());
        // printf ("In SgFile::secondaryPassOverSourceFile():
        // sourceFile->get_tokenSubsequenceMap().size() = %zu
        // \n",sourceFile->get_tokenSubsequenceMap().size());
#endif
      }

      // DQ (7/2/2020): Adding support for processing of token stream.
      // Procesing has been moved here since only at this point do we know which
      // header files will be processed (unparsed). And it is the unparsing of
      // the file that drives the requirement for both collection of comments
      // and CPP directives, and when toke-based unparsing is used, drives the
      // processing of the token stream (generation of the token map to the
      // AST).

      {
        // DQ (8/18/2019): Add performance analysis support.
        TimingPerformance timer(
            "legacy frontend-ROSE header file support for tokens:");
        rosePhaseTrace("secondaryPassOverSourceFile.tokenSupport.begin");

        // DQ (7/2/2020): Use this variable for now while debuging this code
        // moved from the parse() function.
        SgFile *file = sourceFile;
        // DQ (10/27/2013): Adding support for token stream use in unparser. We
        // might want to only turn this of when -rose:unparse_tokens is
        // specified. if ( ( (SageInterface::is_C_language() == true) ||
        // (SageInterface::is_Cxx_language() == true) ) &&
        // file->get_unparse_tokens() == true)
        if (((SageInterface::is_C_language() == true) ||
             (SageInterface::is_Cxx_language() == true)) &&
            ((file->get_unparse_tokens() == true) ||
             (file->get_use_token_stream_to_improve_source_position_info() ==
              true))) {
          // This is only currently being tested and evaluated for C language
          // (should also work for C++, but not yet for Fortran).
          if (file->get_translateCommentsAndDirectivesIntoAST() == true) {
            printf("translateCommentsAndDirectivesIntoAST option not yet "
                   "supported! \n");
            ROSE_ABORT();
          }

#if DEBUG_SECONDARY_PASS
          // SgSourceFile* sourceFile = isSgSourceFile(this);
          // printf ("In SgFile::secondaryPassOverSourceFile():
          // sourceFile->get_tokenSubsequenceMap().size() = %zu
          // \n",sourceFile->get_tokenSubsequenceMap().size());
          printf("In SgFile::secondaryPassOverSourceFile(): Building token "
                 "stream mapping map! \n");
#endif

          // DQ (1/18/2021): This is now moved to the
          // buildCommentAndCppDirectiveList() function (closer to where the
          // vector of tokens is generated). This function builds the data base
          // (STL map) for the different subsequences ranges of the token
          // stream. and attaches the toke stream to the SgSourceFile IR node.
          // *** Next we have to attached the data base ***
          // buildTokenStreamMapping(sourceFile);

          // DQ (9/26/2018): We should be able to enforce this for the current
          // header file we have just processed.
          ASSERT_not_null(sourceFile->get_globalScope());

          // DQ (12/2/2018): We can't enforce this for an empty file (see test
          // in roseTests/astTokenStreamTests).
          // ROSE_ASSERT(sourceFile->get_tokenSubsequenceMap().find(sourceFile->get_globalScope())
          // != sourceFile->get_tokenSubsequenceMap().end());

#if DEBUG_SECONDARY_PASS
          // DQ (1/19/2021): This is a test for calling the
          // get_tokenSubsequenceMap() function.
          printf("Testing first use of SgSourceFile::get_tokenSubsequenceMap() "
                 "function:sourceFile = %p = %s \n",
                 sourceFile, sourceFile->getFileName().c_str());
          map<SgNode *, TokenStreamSequenceToNodeMapping *>
              &temp_tokenStreamSequenceMap =
                  sourceFile->get_tokenSubsequenceMap();
          printf("DONE: Testing first use of "
                 "SgSourceFile::get_tokenSubsequenceMap() function:sourceFile "
                 "= %p = %s \n",
                 sourceFile, sourceFile->getFileName().c_str());
#endif
          // DQ (11/30/2015): Add support to detect macro and include file
          // expansions (can use the token sequence mapping if available). Not
          // clear if I want to require the token sequence mapping, it is likely
          // useful to detect macro expansions even without the token sequence,
          // but usig the token sequence permit us to gather more data.
          detectMacroOrIncludeFileExpansions(sourceFile);
        } else {
          // DQ (2/24/2021): Adding debugging support for the token-based
          // unparsing.
        }
        rosePhaseTrace("secondaryPassOverSourceFile.tokenSupport.end");
      }

      // Liao, 3/31/2009 Handle OpenMP here to see macro calls within directives
#ifdef ROSE_BUILD_CPP_LANGUAGE_SUPPORT
      rosePhaseTrace("secondaryPassOverSourceFile.openmp.begin");
      sourceFile = Rose::AstJson::roundTripSourceFile(
          sourceFile, Rose::AstJson::Checkpoint::PreOmpConstruction);
      processOpenMP(sourceFile);
      rosePhaseTrace("secondaryPassOverSourceFile.openmp.end");
#endif
      if (sourceFile->get_openacc())
        printf("OpenACC support is turned on\n");
      // Liao, 1/29/2014, handle failsafe pragmas for resilience work
      // if (sourceFile->get_failsafe())
      //  FailSafe::process_fail_safe_directives (sourceFile);

      // Reset the saved state (might not really be required at this point).
      if (requiresCPP == true) {
        set_requires_C_preprocessor(false);
      }

      if (get_verbose() > 1) {
        printf("In SgFile::secondaryPassOverSourceFile(): Done with "
               "attachAllPreprocessingInfo() \n");
      }

      // DQ (12/13/2012): Insert pass over AST to detect "#line" directives
      // within only the input source file and accoumulate a list of filenames
      // that will have statements incorrectly marked as being from another
      // file. This might be something we want to control via a command line
      // option.  This sort fo processing can be important for generated files
      // (e.g. the sudo application has four generated C language files from lex
      // that are generated into a file toke.c but with #line N "toke.l" CPP
      // directives and this fools ROSE into not unparsing all of the code from
      // the token.c file (because some of it is assigned to the filename
      // "toke.l". For the case of the sudo application code this prevents the
      // generated code from linking properly (undefined symbols). Since many
      // application include generated code (including ROSE itself) we would
      // like to support this better.
      ASSERT_not_null(sourceFile);
      // sourceFile->gatherASTSourcePositionsBasedOnDetectedLineDirectives();
      // sourceFile->fixupASTSourcePositionsBasedOnDetectedLineDirectives();

    } // end if get_skip_commentsAndDirectives() is false
  }

  // DQ (8/22/2009): We test the parent of SgFunctionTypeTable in the AST post
  // processing, so we need to make sure that it is set.
  SgFunctionTypeTable *functionTypeTable =
      SgNode::get_globalFunctionTypeTable();
  // ROSE_ASSERT(functionTypeTable != NULL);
  if (functionTypeTable != nullptr &&
      functionTypeTable->get_parent() == nullptr) {
    // printf ("In SgFile::callFrontEnd(): set the parent of SgFunctionTypeTable
    // \n");
    functionTypeTable->set_parent(this);
  }
  // ROSE_ASSERT(functionTypeTable->get_parent() != NULL);

  // DQ (3/14/2021): Not clear if this needs to be here.
  // SageInterface::translateToUseCppDeclarations(sourceFile);

  // ROSE_ASSERT(SgNode::get_globalFunctionTypeTable() != NULL);
  // ROSE_ASSERT(SgNode::get_globalFunctionTypeTable()->get_parent() != NULL);
}

namespace SgSourceFile_processCppLinemarkers {
class FixupASTSourcePositionsBasedOnDetectedLineDirectives
    : public AstSimpleProcessing {
public:
  set<int> filenameIdList;

  FixupASTSourcePositionsBasedOnDetectedLineDirectives(
      const string &sourceFilename, set<int> &filenameSet);

  void visit(SgNode *astNode);
};
} // namespace SgSourceFile_processCppLinemarkers

SgSourceFile_processCppLinemarkers::
    FixupASTSourcePositionsBasedOnDetectedLineDirectives::
        FixupASTSourcePositionsBasedOnDetectedLineDirectives(
            const string &sourceFilename, set<int> &filenameSet)
    : filenameIdList(filenameSet) {
  if (SgProject::get_verbose() > 1)
    printf("In "
           "FixupASTSourcePositionsBasedOnDetectedLineDirectives::"
           "FixupASTSourcePositionsBasedOnDetectedLineDirectives(): "
           "filenameIdList.size() = %" PRIuPTR " \n",
           filenameIdList.size());
}

void SgSourceFile_processCppLinemarkers::
    FixupASTSourcePositionsBasedOnDetectedLineDirectives::visit(
        SgNode *astNode) {
  // DQ (12/14/2012): This functionality is being added to support applications
  // like sudo which have generated code from tools like lex. This traversal is
  // defined to detect the use of "#line" directived and then accumulate the
  // list of filenames used in the #line directives. When #line directives are
  // used in the input source file then we consider all statements associated
  // with those filenames to be from the input source file and direct them to be
  // unparsed.  The actual way we trigger them to be unparsed is to make the
  // source position to be unparsed in the Sg_File_Info object, this is enough
  // to force the unparsed to output those statements.
  // TODO: It might be that this should operate on SgLocatedNode IR nodes
  // instead of SgStatement IR nodes.

  SgStatement *statement = isSgStatement(astNode);

  if (statement != nullptr) {
    if (SgProject::get_verbose() > 1)
      printf("FixupASTSourcePositionsBasedOnDetectedLineDirectives::visit(): "
             "statement = %p = %s \n",
             statement, statement->class_name().c_str());

    ASSERT_not_null(statement->get_file_info());
    int fileId = statement->get_file_info()->get_file_id();

    // Check if this fileId is associated with set of fileId that we would like
    // to output.
    if (filenameIdList.find(fileId) != filenameIdList.end()) {
      // statement->get_fileInfo()->setOutputInCodeGeneration();
      statement->get_startOfConstruct()->setOutputInCodeGeneration();
      statement->get_endOfConstruct()->setOutputInCodeGeneration();
    }
  }
}

void SgSourceFile::fixupASTSourcePositionsBasedOnDetectedLineDirectives(
    set<int> equivalentFilenames) {

  // SgSourceFile_processCppLinemarkers::FixupASTSourcePositionsBasedOnDetectedLineDirectives
  // linemarkerTraversal(sourceFile->get_sourceFileNameWithPath(),linemarkerTraversal_1.filenameIdList);
  SgSourceFile_processCppLinemarkers::
      FixupASTSourcePositionsBasedOnDetectedLineDirectives linemarkerTraversal(
          this->get_sourceFileNameWithPath(), equivalentFilenames);

  linemarkerTraversal.traverse(this, preorder);
}

int SgSourceFile::build_Fortran_AST(vector<string> argv,
                                    vector<string> inputCommandLine) {
  // Rasmussen (1/24/2022): Transitioning to using Flang as the Fortran parser.
  // The variable ROSE_FLANG_FRONTEND will be defined at
  // configuration but not ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT.  Unfortunately
  // the latter variable is too tightly coupled with JVM usage at the moment.
  // The Flang parser doesn't require the JVM.
  if (get_experimental_flang_frontend() == true) {
    int status{-1};

    vector<string> flangCommandLine;
    flangCommandLine.push_back("f18-parse-demo");
    flangCommandLine.push_back("-fexternal-builder");
    vector<string> flangArgs = argv;
    SgFile::stripRoseCommandLineOptions(flangArgs);
    bool needs_compile_only = false;
    if (SgProject *project = get_project()) {
      needs_compile_only =
          project->get_compileOnly() || project->get_skipfinalCompileStep();
    }
    if (needs_compile_only) {
      bool has_compile_only = false;
      for (const auto &arg : flangArgs) {
        if (arg == "-c") {
          has_compile_only = true;
          break;
        }
      }
      if (!has_compile_only) {
        flangCommandLine.push_back("-c");
      }
    }

    std::string source_path = get_sourceFileNameWithPath();
    if (source_path.empty()) {
      source_path = getFileName();
    }
    const std::string normalized_source =
        FileHelper::normalizePathIfPossible(source_path);
    std::string source_dir;
    if (!source_path.empty()) {
      source_dir = std::filesystem::path(source_path).parent_path().string();
    }

    const std::string output_dir_arg = findOutputDirArg(argv);
    std::filesystem::path temp_dir;
    bool temp_dir_ready = false;
    bool remove_temp_dir = false;
    std::vector<std::string> temp_files;
    bool replaced_source = false;
    bool include_temp_dir_set = false;

    auto ensure_temp_dir = [&]() -> const std::filesystem::path & {
      if (temp_dir_ready) {
        return temp_dir;
      }
      if (!output_dir_arg.empty()) {
        temp_dir = std::filesystem::path(output_dir_arg) / "flang-input";
        std::error_code ec;
        std::filesystem::create_directories(temp_dir, ec);
        remove_temp_dir = false;
      } else {
        temp_dir = Rose::FileSystem::createTemporaryDirectory();
        remove_temp_dir = true;
      }
      temp_dir_ready = true;
      return temp_dir;
    };

    auto rewrite_source_arg = [&](const std::string &arg) -> std::string {
      if (arg.empty() || arg == "-" || arg[0] == '-') {
        return arg;
      }
      if (!normalized_source.empty() &&
          FileHelper::normalizePathIfPossible(arg) != normalized_source) {
        return arg;
      }
      if (!needsFlangFortranExtensionFix(arg)) {
        return arg;
      }
      const std::string suffix = getPathSuffix(arg);
      const bool fixed_form = isFixedFormFortranSource(this, suffix);
      const std::string new_suffix = fixed_form ? "f" : "f90";
      const std::filesystem::path out_dir = ensure_temp_dir();
      const std::filesystem::path original_path(arg);
      const std::string stem = original_path.stem().string();
      const size_t hash_value =
          std::hash<std::string>{}(FileHelper::normalizePathIfPossible(arg));
      const std::string temp_name =
          stem + ".rose_flang_" + std::to_string(hash_value) + "." + new_suffix;
      const std::filesystem::path temp_path = out_dir / temp_name;

      std::error_code ec;
      std::filesystem::copy_file(
          original_path, temp_path,
          std::filesystem::copy_options::overwrite_existing, ec);
      if (ec) {
        std::cerr << "Error: failed to create flang input copy for "
                  << original_path.string() << ": " << ec.message() << "\n";
        ROSE_ABORT();
      }
      temp_files.push_back(temp_path.string());
      replaced_source = true;
      return temp_path.string();
    };

#if defined(ROSE_FLANG_FRONTEND)
    {
      const std::filesystem::path include_temp_dir = ensure_temp_dir();
      set_flang_include_temp_dir(include_temp_dir.string());
      include_temp_dir_set = true;
    }
#endif

    addFlangIntrinsicIncludeDir(flangCommandLine);

    if (flangArgs.size() > 1) {
      for (size_t i = 1; i < flangArgs.size(); ++i) {
        flangCommandLine.push_back(rewrite_source_arg(flangArgs[i]));
      }
    } else if (!source_path.empty()) {
      flangCommandLine.push_back(rewrite_source_arg(source_path));
    }

    if ((replaced_source || include_temp_dir_set) && !source_dir.empty() &&
        !hasIncludeDir(flangCommandLine, source_dir)) {
      flangCommandLine.push_back("-I");
      flangCommandLine.push_back(source_dir);
    }
    CommandlineProcessing::ArgvStorage flangArgvStorage(flangCommandLine);
    int flangArgc = flangArgvStorage.argc();
    char **flangArgv = flangArgvStorage.argv();

    // SG (7/9/2015) In case of a mixed language project, force case sensitivity
    // here.
    SageBuilder::symbol_table_case_insensitive_semantics = true;

#if defined(ROSE_FLANG_FRONTEND)
    status = experimental_fortran_main(flangArgc, flangArgv,
                                       const_cast<SgSourceFile *>(this));
    set_flang_include_temp_dir(std::string());
    for (const auto &path : temp_files) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
    }
    if (remove_temp_dir && temp_dir_ready) {
      std::error_code ec;
      std::filesystem::remove_all(temp_dir, ec);
    }
#else
    ROSE_ASSERT(!"[FATAL] [ROSE] [frontend] [Fortran] "
                 "error: ROSE was not configured to support the Fortran Flang "
                 "frontend.");
#endif
    return status;
  }

  std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] error: Flang frontend must "
               "be enabled for Fortran parsing.\n";
  ROSE_ABORT();
  return -1;
}

//-----------------------------------------------------------------------------
// Rose::Frontend
//-----------------------------------------------------------------------------

int Rose::Frontend::Run(SgProject *project) {
  ASSERT_not_null(project);

  int status = 0;
  {
    status = Rose::Frontend::RunSerial(project);

    project->set_frontendErrorCode(status);
  }

  return status;
} // Rose::Frontend::Run

int Rose::Frontend::RunSerial(SgProject *project) {
  if (SgProject::get_verbose() > 0)
    std::cout << "[INFO] [Frontend] Running in serial mode" << std::endl;

  // volatile used as a work around for warning: variable might be clobbered by
  // 'longjmp' or 'vfork'
  volatile int status_of_function = 0;

  std::vector<SgFile *> all_files = project->get_fileList();
  {
    int status_of_file = 0;
    for (SgFile *file : all_files) {
      ASSERT_not_null(file);
      if (KEEP_GOING_CAUGHT_FRONTEND_SIGNAL) {
        std::cout << "[WARN] "
                  << "Configured to keep going after catching a "
                  << "signal in SgFile::RunFrontend()" << std::endl;

        if (file != nullptr) {
          file->set_frontendErrorCode(100);
          int save_volatile_variable = status_of_function;
          status_of_function = std::max(100, save_volatile_variable);
        } else {
          std::cout
              << "[FATAL] "
              << "Unable to keep going due to an unrecoverable internal error"
              << std::endl;
          exit(1);
        }
      } else {
        //-----------------------------------------------------------
        // Pass File to Frontend. Avoid using try/catch/re-throw if not
        // necessary because it interferes with debugging the exception (it
        // makes it hard to find where the exception was originally thrown).
        // Also, no need to print a fatal message to std::cout(!) if the
        // exception inherits from the STL properly since the C++ runtime will
        // do all that for us. [Robb P. Matzke 2015-01-07]
        //-----------------------------------------------------------
        if (Rose::KeepGoing::g_keep_going) {
          try {
            int save_volatile_variable = status_of_function;
            file->runFrontend(
                status_of_file); // status_of_file is modified as a side effect
            status_of_function = max(status_of_file, save_volatile_variable);
          } catch (...) {
            if (file != nullptr) {
              file->set_frontendErrorCode(100);
            } else {
              std::cout << "[FATAL] "
                        << "Unable to keep going due to an unrecoverable "
                           "internal error"
                        << std::endl;
              exit(1);
            }
            raise(SIGABRT); // catch with signal handling above
          }
        } else {
          // Same thing but without the try/catch because we want the exception
          // to be propagated all the way to the user without us re-throwing it
          // and interfering with debugging.
          int save_volatile_variable = status_of_function;
          file->runFrontend(
              status_of_file); // status_of_file is modified as a side effect
          status_of_function = max(status_of_file, save_volatile_variable);
          if (status_of_file != 0) {
            break;
          }
        }
      }
    }
  } // all_files->callFrontEnd

  ASSERT_not_null(project);

  project->set_frontendErrorCode(status_of_function);

  return status_of_function;
} // Rose::Frontend::RunSerial

namespace SgSourceFile_processCppLinemarkers {
// This class (AST traversal) supports the traversal of the AST required
// to translate the source position using the CPP linemarkers.

class LinemarkerTraversal : public AstSimpleProcessing {
public:
  // list<PreprocessingInfo*> preprocessingInfoStack;
  list<pair<int, int>> sourcePositionStack;
  SgSourceFile *sourceFile;

  LinemarkerTraversal(SgSourceFile *sourceFile);

  void visit(SgNode *astNode);
};
} // namespace SgSourceFile_processCppLinemarkers

SgSourceFile_processCppLinemarkers::LinemarkerTraversal::LinemarkerTraversal(
    SgSourceFile *sourceFile)
    : sourceFile(sourceFile) {
  ASSERT_not_null(sourceFile);
  std::string sourceFilename = sourceFile->get_sourceFileNameWithPath();
  if (sourceFilename.empty()) {
    sourceFilename = sourceFile->getFileName();
  }
  // Build an initial element on the stack so that the original source file name
  // will be used to set the global scope (which will be traversed before we
  // visit any statements that might have CPP directives attached (or which are
  // CPP direcitve IR nodes).

  // Get the fileId of the assocated filename
  int fileId = Sg_File_Info::getIDFromFilename(sourceFilename);

  // Assume this is line 1 (I forget why zero is not a great idea here,
  // I expect that it causes a consstancy test to fail somewhere).
  int line = 1;

  if (SgProject::get_verbose() > 1)
    printf("In LinemarkerTraversal::LinemarkerTraversal(): Push initial stack "
           "entry for line = %d fileId = %d sourceFilename = %s \n",
           line, fileId, sourceFilename.c_str());

  // Push an entry onto the stack before doing the traversal over the whole AST.
  sourcePositionStack.push_front(pair<int, int>(line, fileId));
}

void SgSourceFile_processCppLinemarkers::LinemarkerTraversal::visit(
    SgNode *astNode) {
  (void)astNode;
#ifdef ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT

  // DXN (02/21/2011): Consider the case of SgInterfaceBody.
  // TODO: revise SgInterfaceBody::get_numberOfTraversalSuccessor() to return 1
  // and
  // TODO: revise SgInterfaceBody::get_traversalSuccessorByIndex(int ) to return
  // p_functionDeclaration Such changes will require some re-write of the code
  // to build SgInterfaceBody. With such changes, the patch below to treat the
  // case of SgInterfaceBody will no longer be necessary.
  SgInterfaceBody *interfaceBody = isSgInterfaceBody(astNode);
  if (interfaceBody) {
    AstSimpleProcessing::traverse(interfaceBody->get_functionDeclaration(),
                                  preorder);
  }

  SgStatement *statement = isSgStatement(astNode);
  // printf ("LinemarkerTraversal::visit(): statement = %p = %s
  // \n",statement,(statement != nullptr) ? statement->class_name().c_str() :
  // "NULL");
  if (statement != nullptr) {
    if (SgProject::get_verbose() > 1)
      printf("LinemarkerTraversal::visit(): statement = %p = %s \n", statement,
             statement->class_name().c_str());

    AttachedPreprocessingInfoType *commentOrDirectiveList =
        statement->getAttachedPreprocessingInfo();

    if (SgProject::get_verbose() > 1)
      printf("LinemarkerTraversal::visit(): commentOrDirectiveList = %p (size "
             "= %" PRIuPTR ") \n",
             commentOrDirectiveList,
             (commentOrDirectiveList != NULL) ? commentOrDirectiveList->size()
                                              : 0);

    if (commentOrDirectiveList != nullptr) {
      AttachedPreprocessingInfoType::iterator i =
          commentOrDirectiveList->begin();
      while (i != commentOrDirectiveList->end()) {
        if ((*i)->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker) {
          // This is a CPP linemarker
          int line = (*i)->get_lineNumberForCompilerGeneratedLinemarker();

          // DQ (12/23/2008): Note this is a quoted name, we need the unquoted
          // version!
          std::string quotedFilename =
              (*i)->get_filenameForCompilerGeneratedLinemarker();
          ROSE_ASSERT(quotedFilename[0] == '\"');
          ROSE_ASSERT(quotedFilename[quotedFilename.length() - 1] == '\"');
          std::string filename =
              quotedFilename.substr(1, quotedFilename.length() - 2);

          std::string options =
              (*i)->get_optionalflagsForCompilerGeneratedLinemarker();

          // Add the new filename to the static map stored in the Sg_File_Info
          // (no action if filename is already in the map).
          Sg_File_Info::addFilenameToMap(filename);

          int fileId = Sg_File_Info::getIDFromFilename(filename);

          if (SgProject::get_verbose() > 1)
            printf("line = %d fileId = %d quotedFilename = %s filename = %s "
                   "options = %s \n",
                   line, fileId, quotedFilename.c_str(), filename.c_str(),
                   options.c_str());

          // Just record the first linemarker so that we can test getting the
          // filename correct.
          if (line == 1 && sourcePositionStack.empty() == true) {
            sourcePositionStack.push_front(pair<int, int>(line, fileId));
          }
        }

        i++;
      }
    }

    // ROSE_ASSERT(sourcePositionStack.empty() == false);
    if (sourcePositionStack.empty() == false) {
      int line = sourcePositionStack.front().first;
      int fileId = sourcePositionStack.front().second;

      if (SgProject::get_verbose() > 1)
        printf("Setting the source position of statement = %p = %s to line = "
               "%d fileId = %d \n",
               statement, statement->class_name().c_str(), line, fileId);

      // DXN (02/18/2011): only reset the file id for the node whose file id
      // corresponds to the "_postprocessed" file.
      string sourceFilename = Sg_File_Info::getFilenameFromID(fileId);
      string sourceFileNameOutputFromCpp =
          sourceFile->generate_C_preprocessor_intermediate_filename(
              sourceFilename);
      int cppFileId =
          Sg_File_Info::getIDFromFilename(sourceFileNameOutputFromCpp);
      if (statement->get_file_info()->get_file_id() == cppFileId) {
        statement->get_file_info()->set_file_id(fileId);
      }
      // statement->get_file_info()->set_line(line);

      if (SgProject::get_verbose() > 1)
        Sg_File_Info::display_static_data(
            "Setting the source position of statement");

      string filename = Sg_File_Info::getFilenameFromID(fileId);

      if (SgProject::get_verbose() > 1) {
        printf("filename = %s \n", filename.c_str());
        printf("filename = %s \n",
               statement->get_file_info()->get_filenameString().c_str());
      }

      ASSERT_not_null(statement->get_file_info()->get_filename());
      ROSE_ASSERT(statement->get_file_info()->get_filenameString().empty() ==
                  false);
    }
  }
  // SKW: not called except from Fortran
  printf(">>> SgSourceFile_processCppLinemarkers::LinemarkerTraversal::visit "
         "is not implemented for languages other than Fortran\n");
  ROSE_ABORT();
#endif //   ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT
}

void SgSourceFile::processCppLinemarkers() {
  SgSourceFile *sourceFile = const_cast<SgSourceFile *>(this);

  SgSourceFile_processCppLinemarkers::LinemarkerTraversal linemarkerTraversal(
      sourceFile);

  linemarkerTraversal.traverse(sourceFile, preorder);
}

int SgSourceFile::build_C_and_Cxx_AST(vector<string> argv,
                                      vector<string> inputCommandLine) {
  // SG (7/9/2015) In case of a mixed language project, force case
  // sensitivity here.
  SageBuilder::symbol_table_case_insensitive_semantics = false;

  const std::string filenameExtension =
      StringUtility::fileNameSuffix(get_sourceFileNameWithPath());
  if (CommandlineProcessing::isAssemblerFileNameSuffix(filenameExtension)) {
    // Standalone assembler sources are backend passthrough inputs: ROSE does
    // not build a C/C++ AST for them, but the driver must still accept the
    // suffix and compile the original file in normal compile-only flows.
    set_skip_unparse(true);
    set_skip_commentsAndDirectives(true);
    set_unparse_output_filename(get_sourceFileNameWithPath());
    return 0;
  }

  std::string frontEndCommandLineString;
  frontEndCommandLineString = std::string(argv[0]) + std::string(" ") +
                              CommandlineProcessing::generateStringFromArgList(
                                  inputCommandLine, false, false);

  if (get_verbose() > 1) {
    printf("In build_C_and_Cxx_AST(): Before calling frontend entry point: "
           "frontEndCommandLineString = %s \n",
           frontEndCommandLineString.c_str());
  }

  CommandlineProcessing::ArgvStorage cCxxArgvStorage(inputCommandLine);
  int c_cxx_argc = cCxxArgvStorage.argc();
  char **c_cxx_argv = cCxxArgvStorage.argv();

#ifdef ROSE_BUILD_CXX_LANGUAGE_SUPPORT
  // This is the function call to the legacy frontend front-end (modified in
  // ROSE to pass a SgFile)

  // REX: Always use Clang frontend (legacy frontend removed)
  int clang_main(int, char *[], SgSourceFile &sageFile,
                 const char *driver_argv0);
  const char *driver_argv0 = argv.empty() ? nullptr : argv[0].c_str();
  int frontendErrorLevel =
      clang_main(c_cxx_argc, c_cxx_argv, *this, driver_argv0);

#else
  // DQ (2/21/2016): Added "error: " to allow this to be caught by the ROSE
  // Matrix Testing.
  int frontendErrorLevel = 99;
  ROSE_ASSERT(!"[FATAL] [ROSE] [frontend] [C/C++] "
               "error: ROSE was not configured to support the C/C++ frontend.");
#endif

  return frontendErrorLevel;
}

int SgSourceFile::buildAST(vector<string> argv,
                           vector<string> inputCommandLine) {

  // TV (09/24/2018): Skip actually calling the frontend (used to test the
  // backend with ROSE command line processing)
  if (get_skip_parser())
    return 0;

  // DXN (01/10/2011): except for building C and Cxx AST, frontend fails when
  // frontend error level > 0.
  int frontendErrorLevel = 0;
  bool frontend_failed = false;
  if (get_Fortran_only() == true) {
#if defined(ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT) || defined(ROSE_FLANG_FRONTEND)
    frontendErrorLevel = build_Fortran_AST(argv, inputCommandLine);
    frontend_failed = (frontendErrorLevel != 0);
#else
    // DQ (2/21/2016): Added "error: " to allow this to be caught by the ROSE
    // Matrix Testing.
    ROSE_ASSERT(
        !"[FATAL] [ROSE] [frontend] [Fortran] "
         "error: ROSE was not configured to support the Fortran frontend.");
#endif
  } else {
    {
      // This is the C/C++ case (default).
      frontendErrorLevel = build_C_and_Cxx_AST(argv, inputCommandLine);

      // DQ (12/29/2008): The newer
      // version of legacy frontend
      // (version 3.10 and 4.0) use
      // different return codes for
      // indicating an error. Any
      // non-zero value indicates an
      // error.
      frontend_failed = (frontendErrorLevel != 0);
    }
  }

  // Uniform processing of the error code!

  if (get_verbose() > 1)
    printf("DONE: frontend called (frontendErrorLevel = %d) \n",
           frontendErrorLevel);

  // If we had any errors reported by the frontend then quite now
  if (frontend_failed == true) {
    // cout << "Errors in Processing: (frontendErrorLevel > 3)" << endl;
    if (get_verbose() > 1)
      printf("frontendErrorLevel = %d \n", frontendErrorLevel);

    // DQ (9/22/2006): We need to invert the test result (return code) for
    // negative tests (where failure is expected and success is an error).
    if (get_negative_test() == true) {
      if (get_verbose() > 1) {
        printf("(evaluation of frontend results) This is a negative tests, so "
               "an error in compilation is a PASS but a successful \n");
        printf(
            "compilation is not a FAIL since the failure might happen in the "
            "compilation of the generated code by the vendor compiler. \n");
      }
    } else {
      // DQ (1/28/2016): Tone this down a bit to be less sarcastic.
      // DQ (4/12/2015): Make this a more friendly message than what the OS
      // provides on abort() (which is "Aborted (core dumped)"). Exit because
      // there are errors in the input program cout << "Errors in Processing:
      // (frontend_failed)" << endl; ROSE_ABORT("Errors in Processing:
      // (frontend_failed)"); printf ("Errors in Processing Input File:
      // (throwing an instance of \"frontend_failed\" exception due to errors
      // detected in the input code), have a nice day! \n");
      printf("Errors in Processing Input File: throwing an instance of "
             "\"frontend_failed\" exception due to syntax errors detected in "
             "the input code \n");
    }
    if (Rose::KeepGoing::g_keep_going) {
      raise(SIGABRT); // raise a signal to be handled by the keep going support
                      // , instead of exit. Liao 4/25/2017
    }
    return frontendErrorLevel;
  }

  return frontendErrorLevel;
}

// DQ (10/14/2010): Removing reference to macros defined in rose_config.h
// (defined in the header file as a default parameter). int
// SgFile::compileOutput ( vector<string>& argv, int fileNameIndex, const
// string& compilerNameOrig )
int SgFile::compileOutput(vector<string> &argv, int fileNameIndex) {
  // DQ (7/12/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST Object Code Generation (compile output):");

  // DQ (11/4/2015): Added assertion.
  ROSE_ASSERT(this != NULL);

  // DQ (4/21/2006): I think we can now assert this!
  ROSE_ASSERT(fileNameIndex == 0);

  // Flags for debug and command line options output.
  bool debugProjectCompileCommandLineWithArgs = false;
  bool showBackendCommandLine = SgProject::get_showBackendCommandLine();
// Keep this macro for debugging some parts, if more familiar.
#define DEBUG_PROJECT_COMPILE_COMMAND_LINE_WITH_ARGS 0

#if DEBUG_PROJECT_COMPILE_COMMAND_LINE_WITH_ARGS
  debugProjectCompileCommandLineWithArgs = true;
#endif

  if (debugProjectCompileCommandLineWithArgs || showBackendCommandLine) {
    printf("\n\n***************************************************** \n");
    printf("Inside of SgFile::compileOutput() \n");
    printf("   --- get_unparse_output_filename() = %s \n",
           get_unparse_output_filename().c_str());
    printf("***************************************************** \n\n\n");
  }

  // This function does the final compilation of the unparsed file
  // Remaining arguments from the original compile time are used as well
  // as additional arguments added by the buildCompilerCommandLineOptions()
  // function

  // DQ NOTE: This function has to be modified to compile more than
  //       just one file (as part of the multi-file support)
  // ROSE_ASSERT (sageProject.numberOfFiles() == 1);

  // ******************************************************************************
  // At this point in the control flow (for ROSE) we have returned from the
  // processing via the legacy frontend frontend (or skipped it if that
  // option was specified). The following has been done or explicitly skipped
  // if such options were specified on the commandline:
  //    1) The application program has been parsed
  //    2) All AST's have been build (one for each grammar)
  //    3) The transformations have been edited into the C++ AST
  //    4) The C++ AST has been unparsed to form the final output file (all
  //    code has
  //       been generated into a different filename)
  // ******************************************************************************

  // What remains is to run the specified compiler (typically the C++
  // compiler) using the generated output file (unparsed and transformed
  // application code).
  int returnValueForRose = 0;

  // DQ (1/17/2006): test this
  // ROSE_ASSERT(get_fileInfo() != NULL);

  // DQ (4/2/2011): Added language specific support.
  // const string compilerNameOrig = BACKEND_CXX_COMPILER_NAME_WITH_PATH;
  string compilerNameOrig = BACKEND_CXX_COMPILER_NAME_WITH_PATH;

  if (get_Fortran_only() == true) {
    // printf ("Fortran language support in SgFile::compileOutput() not
    // implemented \n"); ROSE_ABORT();
    compilerNameOrig = BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH;
  }

  // BP : 11/13/2001, checking to see that the compiler name is set
  string compilerName = compilerNameOrig + " ";

  // DQ (4/21/2006): Setup the output file name.
  // Rose_STL_Container<string> fileList =
  // CommandlineProcessing::generateSourceFilenames(argc,argv); ROSE_ASSERT
  // (fileList.size() == 1); p_sourceFileNameWithPath    = *(fileList.begin());
  // p_sourceFileNameWithoutPath =
  // Rose::utility_stripPathFromFileName(p_sourceFileNameWithPath.c_str());

  // ROSE_ASSERT (get_unparse_output_filename().empty() == true);

  // DQ (4/21/2006): If we have not set the unparse_output_filename then we
  // could not have called unparse and we want to compile the original source
  // file as a backup mechanism to generate an object file. printf ("In
  // SgFile::compileOutput(): get_unparse_output_filename() = %s
  // \n",get_unparse_output_filename().c_str());

  bool use_original_input_file =
      Rose::KeepGoing::Backend::UseOriginalInputFile(this);

  // TOO1 (05/14/2013): Handling for -rose:keep_going
  // Replace the unparsed file with the original input file.
  if (use_original_input_file == true) {
    // ROSE_ASSERT(get_skip_unparse() == true);
    string outputFilename = get_sourceFileNameWithPath();

    // DQ (9/15/2013): Added support for generated file to be placed into the
    // same directory as the source file. It's use here is similar to that in
    // the unparse.C file, but less clear here that it is correct since we don't
    // have tests of the -rose:keep_going option (that I know of directly in
    // ROSE).
    SgProject *project = SageInterface::getProject(this);
    if (project != nullptr) {
      if (debugProjectCompileCommandLineWithArgs) {
        printf("In SgFile::compileOutput(): "
               "project->get_unparse_in_same_directory_as_input_file() = %s \n",
               project->get_unparse_in_same_directory_as_input_file()
                   ? "true"
                   : "false");
      }
      if (project->get_unparse_in_same_directory_as_input_file() == true) {
        outputFilename =
            Rose::getPathFromFileName(get_sourceFileNameWithPath()) + "/rose_" +
            get_sourceFileNameWithoutPath();

        printf("In SgFile::compileOutput(): Using filename for unparsed file "
               "into same directory as input file: outputFilename = %s \n",
               outputFilename.c_str());

        set_unparse_output_filename(outputFilename);
      }
    } else {
      printf("WARNING: In SgFile::compileOutput(): file = %p has no associated "
             "project \n",
             this);
    }

    if (debugProjectCompileCommandLineWithArgs) {
      printf("get_unparse_output_filename() = %s \n",
             get_unparse_output_filename().c_str());
    }

    if (get_unparse_output_filename().empty()) {
      if (get_skipfinalCompileStep()) {
        // nothing to do...
      } else if (this->get_frontendErrorCode() == 0) {
        // DQ (7/14/2013): This is the branch taken when processing the -H
        // option (which outputs the header file list, and is required to be
        // supported in ROSE as part of some application specific configuration
        // testing (when configure tests ROSE translators)).

        // TOO1 (9/23/2013): There was never an else branch (or
        // assertion) here before.
        //                   Commenting out for now to allow
        //                   $ROSE/tests/CompilerOptionTests to pass in
        //                   order to expedite the transition from
        //                   ROSE-FRONTEND3 to ROSE-FRONTEND4.
        //   ROSE_ASSERT(! "Not implemented yet");
      }
    } else {
      std::filesystem::path original_file = outputFilename;
      std::filesystem::path unparsed_file = get_unparse_output_filename();

      if (SgProject::get_verbose() >= 2) {
        std::cout << "[DEBUG] "
                  << "unparsed_file "
                  << "'" << unparsed_file << "' "
                  << "exists = " << std::boolalpha
                  << std::filesystem::exists(unparsed_file) << std::endl;
      }
      // Don't replace the original input file with itself
      if (original_file.string() != unparsed_file.string()) {
        if (SgProject::get_verbose() >= 1) {
          std::cout << "[INFO] "
                    << "Replacing "
                    << "'" << unparsed_file << "' "
                    << "with "
                    << "'" << original_file << "'" << std::endl;
        }

        // Remove the existing file first to ensure a complete
        // overwrite.
        if (std::filesystem::exists(unparsed_file)) {
          std::filesystem::remove(unparsed_file);
        }
        if (debugProjectCompileCommandLineWithArgs) {
          printf("NOTE: keep_going option supporting direct copy of original "
                 "input file to overwrite the unparsed file \n");
        }
        Rose::FileSystem::copyFile(original_file, unparsed_file);
      }
    }

    if (debugProjectCompileCommandLineWithArgs) {
      // DQ (11/8/2015): Commented out to avoid output spew.
      printf("In SgFile::compileOutput(): outputFilename = %s \n",
             outputFilename.c_str());
    }
    set_unparse_output_filename(outputFilename);
  }

  ROSE_ASSERT(get_unparse_output_filename().empty() == false);

  // Now call the compiler that rose is replacing
  // if (get_useBackendOnly() == false)
  // DQ (5/27/2022): Temporary debugging...
  // if ( SgProject::get_verbose() >= 1 )
  if (SgProject::get_verbose() >= 1) {
    printf("Now call the backend (vendor's) compiler compilerNameOrig = %s for "
           "file = %s \n",
           compilerNameOrig.c_str(), get_unparse_output_filename().c_str());
  }

  // Build the commandline to hand off to the C++/C compiler
  vector<string> compilerCmdLine =
      buildCompilerCommandLineOptions(argv, fileNameIndex, compilerName);

  // If this is C-only but the suffix indicates C++, force C mode in the
  // backend.
  string sourceFilename = this->getFileName();
  string filenameExtension = StringUtility::fileNameSuffix(sourceFilename);
  const bool isAssemblerSource =
      CommandlineProcessing::isAssemblerFileNameSuffix(filenameExtension);
  if (get_C_only() == true &&
      CommandlineProcessing::isCppFileNameSuffix(filenameExtension) == true) {
    compilerCmdLine.insert(compilerCmdLine.begin() + 1, "-x");
    compilerCmdLine.insert(compilerCmdLine.begin() + 2, "c");
  }

  if (get_Fortran_only() == true && get_experimental_flang_frontend() == true) {
    addFlangIntrinsicIncludeDir(compilerCmdLine);
  }

#if defined(ROSE_USE_CLANG_FRONTEND)
  if (!isAssemblerSource && (get_C_only() || get_Cxx_only() ||
                             get_Cuda_only() || get_OpenCL_only())) {
    std::string clang_include_root = ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR;
    if (!clang_include_root.empty()) {
      std::filesystem::path build_root(clang_include_root);
      std::filesystem::path build_clang_root = build_root / "clang";
      if (std::filesystem::exists(build_clang_root)) {
        clang_include_root = build_clang_root.string();
      }
    }
    if (clang_include_root.empty() ||
        !std::filesystem::exists(clang_include_root)) {
      clang_include_root = ROSE_INSTALL_CLANG_INCLUDE_DIR;
    }
    if (!clang_include_root.empty() &&
        std::filesystem::exists(clang_include_root)) {
      std::string builtin_header;
      if (get_C_only()) {
        builtin_header = "clang-builtin-c.h";
      } else if (get_Cuda_only()) {
        builtin_header = "clang-builtin-cuda.hpp";
      } else if (get_OpenCL_only()) {
        builtin_header = "clang-builtin-opencl.h";
      } else if (get_Cxx_only()) {
        builtin_header = "clang-builtin-cpp.hpp";
      }

      auto has_flag_with_value = [&](const std::string &flag,
                                     const std::string &value) {
        for (size_t i = 0; i + 1 < compilerCmdLine.size(); ++i) {
          if (compilerCmdLine[i] == flag && compilerCmdLine[i + 1] == value) {
            return true;
          }
        }
        return false;
      };
      auto has_include = [&](const std::string &header) {
        for (size_t i = 0; i + 1 < compilerCmdLine.size(); ++i) {
          if (compilerCmdLine[i] == "-include" &&
              compilerCmdLine[i + 1] == header) {
            return true;
          }
        }
        return false;
      };

      if (!has_flag_with_value("-isystem", clang_include_root)) {
        compilerCmdLine.push_back("-isystem");
        compilerCmdLine.push_back(clang_include_root);
      }
      if (!builtin_header.empty() && !has_include(builtin_header)) {
        compilerCmdLine.push_back("-include");
        compilerCmdLine.push_back(builtin_header);
      }
    }
  }
#endif

  int returnValueForCompiler = 0;

  // error checking
  // display("Called from SgFile::compileOutput()");

  // Allow conditional skipping of the final compile step for testing ROSE.
  // printf ("SgFile::compileOutput(): get_skipfinalCompileStep() = %s
  // \n",get_skipfinalCompileStep() ? "true" : "false");
  if (get_skipfinalCompileStep() == false) {
    // Debugging code
    if (get_verbose() >= 1) {
      printf("calling systemFromVector() \n");
      printf("Number of command line arguments: %" PRIuPTR "\n",
             compilerCmdLine.size());
      for (size_t i = 0; i < compilerCmdLine.size(); ++i) {
        printf("Backend compiler arg[%" PRIuPTR "]: = %s\n", i,
               compilerCmdLine[i].c_str());
      }
      printf("End of command line for backend compiler\n");

      // DQ (6/19/2020): Error checking for embedded application name.
      string finalCommandLine =
          CommandlineProcessing::generateStringFromArgList(compilerCmdLine,
                                                           false, false);
      printf("finalCommandLine = %s \n", finalCommandLine.c_str());
      size_t substringPosition = finalCommandLine.find("TestUnparseHeaders");
      printf("substringPosition = %zu \n", substringPosition);
      ROSE_ASSERT(substringPosition == string::npos);

      // I need the exact command line used to compile the generate code with
      // the backendcompiler (so that I can reuse it to test the generated
      // code).
      printf("SgFile::compileOutput(): get_skipfinalCompileStep() == false: "
             "compilerCmdLine = \n%s\n",
             CommandlineProcessing::generateStringFromArgList(compilerCmdLine,
                                                              false, false)
                 .c_str());
    }
    MLOG_TRACE_CXX("sage_support")
        << "SgFile::compileOutput(): compilerCmdLine =\n"
        << CommandlineProcessing::generateStringFromArgList(compilerCmdLine,
                                                            false, false)
        << std::endl;

    // DQ (4/18/2015): Adding support to add compile only mode to the processing
    // of each file when multiple files are processed.
    if (get_compileOnly() == true) {
      bool addCompileOnlyFlag = true;

      for (size_t i = 0; i < compilerCmdLine.size(); ++i) {
        if (compilerCmdLine[i] == "-c") {
          addCompileOnlyFlag = false;
        }
      }

      if (debugProjectCompileCommandLineWithArgs) {
        printf("addCompileOnlyFlag = %s \n",
               addCompileOnlyFlag ? "true" : "false");
      }
      if (addCompileOnlyFlag == true) {
        // We might want to check if "-c" is already present so we don't add it
        // redundantly.
        compilerCmdLine.push_back("-c");
      }

      if (debugProjectCompileCommandLineWithArgs) {
        printf("In SgFile::compileOutput(): get_skipfinalCompileStep() == "
               "false: get_compileOnly() == true: compilerCmdLine = \n%s\n",
               CommandlineProcessing::generateStringFromArgList(compilerCmdLine,
                                                                false, false)
                   .c_str());
      }
    }

    if (debugProjectCompileCommandLineWithArgs || showBackendCommandLine) {
      // DQ (2/6/2022): Set to "1" to output the backend compiler command
      // line.
      printf("In SgFile::compileOutput(): Calling systemFromVector(): "
             "compilerCmdLine = \n%s\n",
             CommandlineProcessing::generateStringFromArgList(compilerCmdLine,
                                                              false, false)
                 .c_str());
    }

#if defined(ROSE_FLANG_FRONTEND)
    if (get_Fortran_only() == true &&
        get_experimental_flang_frontend() == true) {
      std::vector<std::string> module_sources;
      std::set<std::string> seen;
      std::set<std::string> visiting;

      Rose_STL_Container<SgNode *> uses =
          NodeQuery::querySubTree(this, V_SgUseStatement);
      for (Rose_STL_Container<SgNode *>::iterator it = uses.begin();
           it != uses.end(); ++it) {
        collectModuleSourcesFromUse(isSgUseStatement(*it), module_sources, seen,
                                    visiting);
      }

      if (!module_sources.empty()) {
        std::string unparse_path = get_unparse_output_filename();
        std::string original_path = get_sourceFileNameWithPath();
        for (const std::string &module_path : module_sources) {
          if (module_path.empty())
            continue;
          if (module_path == unparse_path || module_path == original_path)
            continue;

          std::vector<std::string> module_cmd = compilerCmdLine;
          bool replaced = false;
          if (!unparse_path.empty()) {
            std::vector<std::string>::iterator it =
                std::find(module_cmd.begin(), module_cmd.end(), unparse_path);
            if (it != module_cmd.end()) {
              *it = module_path;
              replaced = true;
            }
          }
          if (replaced == false && !original_path.empty()) {
            std::vector<std::string>::iterator it =
                std::find(module_cmd.begin(), module_cmd.end(), original_path);
            if (it != module_cmd.end()) {
              *it = module_path;
              replaced = true;
            }
          }
          if (replaced == false)
            continue;

          int module_rc = systemFromVector(module_cmd);
          if (module_rc != 0) {
            printf("Exiting with an error in the backend compilation while "
                   "building Fortran module dependencies! \n");
            ROSE_ASSERT(false);
          }
        }
      }
    }
#endif

    // DQ (2/20/2013): The timer used in TimingPerformance is now fixed to
    // properly record elapsed wall clock time. CAVE3 double check that is
    // correct and shouldn't be compilerCmdLine
    returnValueForCompiler = systemFromVector(compilerCmdLine);

    MLOG_TRACE_CXX("sage_support")
        << "SgFile::compileOutput(): systemFromVector() returnValueForCompiler "
           "= "
        << returnValueForCompiler << std::endl;

    // TOO1 (05/14/2013): Handling for -rose:keep_going
    //
    // Compilation failed =>
    //
    //   1. Unparsed file is invalid => Try compiling the original input file
    //   2. Original input file is invalid => abort
    if (returnValueForCompiler != 0) {
      this->set_backendCompilerErrorCode(-1);
      if (this->get_project()->get_keep_going() == true) {
        // 1. We already failed the compilation of the ROSE unparsed file.
        // 2. Now we tried to compile the original input file --
        //    that was just compiled above -- and failed also.
        if (this->get_unparsedFileFailedCompilation()) {
          this->set_backendCompilerErrorCode(-1);
          // TOO1 (11/16/2013): TODO: Allow user to catch
          // InvalidOriginalInputFileException?
          // throw std::runtime_error("Original input file is invalid");
          std::cout << "[FATAL] "
                    << "Original input file is invalid: "
                    << "'" << this->getFileName() << "'"
                    << "\n\treported by " << __FILE__ << ":" << __LINE__
                    << std::endl;
          if (Rose::KeepGoing::g_keep_going)
            raise(SIGABRT); // raise a signal to be handled by the keep going
                            // support , instead of exit. Liao 4/25/2017
          else
            exit(1);
        } else {
          // The ROSE unparsed file is invalid...
          this->set_frontendErrorCode(-1);
          this->set_unparsedFileFailedCompilation(true);

          returnValueForCompiler = this->compileOutput(argv, fileNameIndex);
        }
      } else {
        // Propagate backend compilation failures as a normal nonzero return
        // value so callers and negative-test harnesses can handle them.
        this->set_backendCompilerErrorCode(-1);
      }
    }
  } // if (get_skipfinalCompileStep() == false)
  else {
    if (get_verbose() > 1)
      printf("COMPILER NOT CALLED: compilerNameString = %s \n",
             "<unknown>" /* compilerNameString.c_str() */);
  }

  // DQ (7/20/2006): Catch errors returned from unix "system" function
  // (commonly "out of memory" errors, suggested by Peter and Jeremiah).
  if (returnValueForCompiler < 0) {
    perror("Serious Error returned from internal systemFromVector command");
  }

  // Assemble an exit status that combines the values for ROSE and the
  // C++/C compiler return an exit status which is the boolean OR of the
  // bits from the legacy frontend/SAGE/ROSE and the compile step
  int finalCompiledExitStatus = returnValueForRose | returnValueForCompiler;

  // It is a strange property of the UNIX $status that it does not map
  // uniformally from the return value of the "exit" command (or "return"
  // statement).  So if the exit status from the compilation stage is
  // nonzero then we just make the exit status 1 (this does seem to be a
  // portable solution). FYI: only the first 8 bits of the exit value are
  // significant.
  if (finalCompiledExitStatus != 0) {
    // If this it is non-zero then make it 1 to be more clear to external
    // tools (e.g. make)
    finalCompiledExitStatus = 1;
  }

  // DQ (9/19/2006): We need to invert the test result (return code) for
  // negative tests (where failure is expected and success is an error).
  if (get_negative_test() == true) {
    if (get_verbose() > 1)
      printf("This is a negative tests, so an error in compilation is a PASS "
             "and successful compilation is a FAIL (vendor compiler return "
             "value = %d) \n",
             returnValueForCompiler);

    finalCompiledExitStatus =
        (finalCompiledExitStatus == 0) ? /* error */ 1 : /* success */ 0;
  }

  // Liao, 4/26/2017. KeepGoingTranslator should keep going no mater what.
  if (Rose::KeepGoing::g_keep_going) {
    finalCompiledExitStatus = 0;
  }

  return finalCompiledExitStatus;
}

//! project level compilation and linking
// three cases: 1. preprocessing only
//              2. compilation:
//              3. linking:
// int SgProject::compileOutput( const std::string& compilerName )
int SgProject::compileOutput() {
  // DQ (7/6/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST Backend Compilation (SgProject):");

  int errorCode = 0;
  int linkingReturnVal = 0;
  int i = 0;

  std::string compilerName;

#define DEBUG_PROJECT_COMPILE_COMMAND_LINE 0

  // DQ (1/19/2014): Adding support for gnu "-S" option.
  if (get_stop_after_compilation_do_not_assemble_file() == true) {
    // DQ (1/19/2014): Handle special case (issue a single compile command for
    // all files using the "-S" option).
    vector<string> argv = get_originalCommandLineArgumentList();

    // strip out any rose options before passing the command line.
    SgFile::stripRoseCommandLineOptions(argv);

    // strip out frontend-specific options that would cause an error in
    // the backend linker (compiler).

    vector<string> originalCommandLine = argv;
    ROSE_ASSERT(!originalCommandLine.empty());

    string &compilerNameString = originalCommandLine[0];
    if (get_C_only() == true) {
      compilerNameString = BACKEND_C_COMPILER_NAME_WITH_PATH;
    } else {
      printf("Error: GNU \"-S\" option is not supported for more than the C "
             "language in ROSE at present! \n");
      ROSE_ABORT();
    }

    if (SgProject::get_verbose() > -1) {
      printf("In SgProject::compileOutput(): listToString(originalCommandLine) "
             "= %s \n",
             StringUtility::listToString(originalCommandLine).c_str());
    }

    errorCode = systemFromVector(originalCommandLine);

    return errorCode + linkingReturnVal;
  }

  if (numberOfFiles() == 0) {
    // printf ("Note in SgProject::compileOutput(%s): numberOfFiles() == 0
    // \n",compilerName.c_str()); printf ("ROSE using %s as backend compiler: no
    // input files \n",compilerName.c_str());

    // DQ (8/24/2008): We can't recreate same behavior on exit as GNU on exit
    // with no files since it causes the test immediately after building
    // librose.so to fail. exit(1);
  }

  // NOTE: that get_C_PreprocessorOnly() is true only if using the "-E" option
  // and not for the " option.

  // case 1: preprocessing only
  if (get_C_PreprocessorOnly() == true) {
    // DQ (10/16/2005): Handle special case (issue a single compile command for
    // all files)
    vector<string> argv = get_originalCommandLineArgumentList();

    // strip out any rose options before passing the command line.
    SgFile::stripRoseCommandLineOptions(argv);

    // strip out frontend-specific options that would cause an error in
    // the backend linker (compiler).

    // Skip the name of the ROSE translator (so that we can insert the
    // backend compiler name, below) bool skipInitialEntry = true;

    // Include all the specified source files
    // bool skipSourceFiles  = false;

    vector<string> originalCommandLine = argv;
    ROSE_ASSERT(!originalCommandLine.empty());

    // DQ (8/13/2006): Use the path to the compiler specified as that backend
    // compiler (should not be specifi to GNU!) DQ (8/6/2006): Test for g++ and
    // use gcc with "-E" option (makes a different for header file processing in
    // ARES configuration) string compilerNameString = compilerName;
    string &compilerNameString = originalCommandLine[0];
    if (get_C_only() == true) {
      compilerNameString = BACKEND_C_COMPILER_NAME_WITH_PATH;
    } else {
      compilerNameString = BACKEND_CXX_COMPILER_NAME_WITH_PATH;
      if (get_Fortran_only() == true) {
        compilerNameString = "f77";
      }
    }

    // DQ (8/13/2006): Add a space to avoid building "g++-E" as output.
    // compilerNameString += " ";

    // Prepend the compiler name to the original command line
    // originalCommandLine = std::string(compilerName) + std::string(" ") +
    // originalCommandLine; originalCommandLine = compilerNameString +
    // originalCommandLine;

    // Prepend the compiler name to the original command line
    // originalCommandLine = std::string(compilerName) + std::string(" ") +
    // originalCommandLine;

    // printf ("originalCommandLine = %s \n",originalCommandLine.c_str());

#ifdef BACKEND_CXX_IS_INTEL_COMPILER
    // DQ (12/18/2016): In the case of using "-E" with the Intel backend
    // compiler we need to add -D__INTEL_CLANG_COMPILER so that we can take a
    // path through the Intel header files that avoids editing header Intel
    // specific header files to handle builtin functions that use types defined
    // in the header files.
    originalCommandLine.push_back("-D__INTEL_CLANG_COMPILER");
#endif

    // DQ (12/18/2016): Add a ROSE specific macro definition that will permit
    // our ROSE specific preinclude header file to skip over the ROSE specific
    // macros and builting functions.  This will allow ROSE to be use to
    // generate CPP output that ROSE could then use as input (without specific
    // declarations being defined twice).  Markus had also requested this
    // behavior.
    originalCommandLine.push_back("-DUSE_ROSE_CPP_PROCESSING");

    // Debug: Output commandline arguments before actually executing
    if (SgProject::get_verbose() > 0)
    // if (SgProject::get_verbose() >= 0)
    {
      for (unsigned int i = 0; i < originalCommandLine.size(); ++i) {
        printf("originalCommandLine[%u] = %s \n", i,
               originalCommandLine[i].c_str());
      }
    }

    errorCode = systemFromVector(originalCommandLine);

    ASSERT_not_null(SgNode::get_globalFunctionTypeTable());
    ASSERT_not_null(SgNode::get_globalFunctionTypeTable()->get_parent());

    // printf ("Exiting after call to compiler using -E option! \n");
    // ROSE_ABORT();
  } else // non-preprocessing-only case
  {
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
    printf("\n\nIn Project::compileOutput(): Compiling numberOfFiles() = %d \n",
           numberOfFiles());
#endif

    // volatile used as a work around for warning: variable might be clobbered
    // by 'longjmp' or 'vfork'
    volatile bool multifile_support_compile_only_flag = false;

    // case 2: compilation  for each file
    // Typical case
    {
      if (numberOfFiles() > 1) {
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
        printf(
            "In Project::compileOutput(): Need to handled multiple files: \n");
        printf(
            "   1) run each one separately through ROSE to generate the "
            "translated source file, and object files (compile only), then \n");
        printf("   2) collect the object files in a final link command \n");
#endif
        multifile_support_compile_only_flag = true;
      }

      for (i = 0; i < numberOfFiles(); i++) {
        int localErrorCode = 0;
        SgFile &file = get_file(i);

#if DEBUG_PROJECT_COMPILE_COMMAND_LINE || 0
        printf("In Project::compileOutput(): Processing file #%d of %d: "
               "filename = %s \n",
               i, numberOfFiles(), file.getFileName().c_str());
#endif
        if (multifile_support_compile_only_flag == true) {
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE || 0
          printf("multifile_support_compile_only_flag == true: Turn ON "
                 "compileOnly flag \n");
#endif
          file.set_compileOnly(true);

#if DEBUG_PROJECT_COMPILE_COMMAND_LINE || 0
          printf("Need to supporess the generation of object file "
                 "specification in backend compiler link line \n");
#endif
          file.set_multifile_support(true);
        }

        if (KEEP_GOING_CAUGHT_BACKEND_COMPILER_SIGNAL) {
          std::cout << "[WARN] "
                    << "Configured to keep going after catching a "
                    << "signal in SgProject::compileOutput()" << std::endl;

          localErrorCode = 100;
          file.set_backendCompilerErrorCode(localErrorCode);
        } else {
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
          printf("\nIn Project::compileOutput(): Calling file.compileOutput(0) "
                 "\n");
#endif
          localErrorCode = file.compileOutput(0);
        }

        if (localErrorCode > errorCode) {
          errorCode = localErrorCode;
        }

        if (multifile_support_compile_only_flag == true) {
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
          printf("In SgProject::compileOutput(): "
                 "multifile_support_compile_only_flag == true: Turn OFF "
                 "compileOnly flag \n");
#endif
          file.set_compileOnly(false);
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
          // Build a link line now that we have processed all of the source
          // files to build the object file.
          printf("Need to build the link line to build the executable using "
                 "the generated object files \n");

          printf("In SgProject::compileOutput(): get_compileOnly() = %s (reset "
                 "to false) \n",
                 get_compileOnly() ? "true" : "false");
#endif
          // I think we should not have set the compileOnly flag to true
          // prevously. set_compileOnly(false);
        }
      }
    }

#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
    printf("In SgProject::compileOutput(): get_compileOnly() = %s \n",
           get_compileOnly() ? "true" : "false");
    printf("In SgProject::compileOutput(): errorCode = %d \n", errorCode);
#endif
    // case 3: linking at the project level

    // DQ (1/9/2017): Only proceed with linking step if the compilation
    // step finished without error.
    if (errorCode == 0) {

      // ROSE_ASSERT(get_compileOnly() == true);

      // Liao, 11/19/2009,
      // I really want to just move the SgFile::compileOutput() to
      // SgProject::compileOutput() and have both compilation and linking
      // finished at the same time, just as the original command line does. Then
      // we don't have to compose compilation command line for each of the input
      // source file or to compose the final linking command line.
      //
      // But there may be some advantages of doing the compilation and linking
      // separately at two levels. I just discussed it with Dan. The two level
      // scheme is needed to support mixed language input, like a C file and a
      // Fortran file In this case, we cannot have a single one level command
      // line to compile and link those two files We have to compile each of
      // them first and finally link the object files.

      // DQ (4/13/2015): Check if the compile line supported the link step.
      // Could this call the linker even we we don't want it called, or skipp
      // calling it when we do want it to be called?
      if (get_compileOnly() == false) {
        // Liao 5/1/2015
        linkingReturnVal = link(BACKEND_CXX_COMPILER_NAME_WITH_PATH);
      } else {
#if DEBUG_PROJECT_COMPILE_COMMAND_LINE
        printf("In SgProject::compileOutput(): Skip calling the linker if the "
               "compile line handled the link step! \n");
#endif
      }
    }
  } // end if preprocessing-only is false

  return errorCode + linkingReturnVal;
}

bool SgFile::isPrelinkPhase() const {
  // This function checks if the "-prelink" option was passed to the ROSE
  // preprocessor It could alternatively just check the commandline and set a
  // flag in the SgFile object. But then there would be a redundent flag in each
  // SgFile object (perhaps the design needs to be better, using a common base
  // class for commandline options (in both the SgProject and the SgFile (would
  // not be a new IR node)).

  bool returnValue = false;

  // DQ (5/9/2004): If the parent is not set then this was compiled as a SgFile
  // directly (likely by the rewrite mechanism). IF so it can't be a prelink
  // phase, which is called only on SgProjects). Not happy with this mechanism!
  if (get_parent() != nullptr) {
    // DQ (1/24/2010): Now that we have directory support, the parent of a
    // SgFile does not have to be a SgProject. SgProject* project =
    // isSgProject(get_parent());
    SgProject *project = SageInterface::getProject(this);

    ASSERT_not_null(project);
    returnValue = project->get_prelink();
  }

  return returnValue;

  // Note that project can be false if this is called on an intermediate file
  // generated by the rewrite system.
  // return (project == NULL) ? false : project->get_prelink();
}

// DQ (10/14/2010): Removing reference to macros defined in rose_config.h
// (defined in the header file as a default parameter).
//! Preprocessing command line and pass it to generate the final linking command
//! line
// int SgProject::link ()
int SgProject::link(std::string linkerName) {

  // DQ (1/25/2010): We have to now test for both numberOfFiles() and
  // numberOfDirectories(), or perhaps define a more simple function to use more
  // directly. Liao, 11/20/2009 translator test1.o will have ZERO SgFile
  // attached with SgProject Special handling for this case if (numberOfFiles()
  // == 0)
  if (numberOfFiles() == 0 && numberOfDirectories() == 0) {
    if (get_verbose() > 0)
      cout << "SgProject::link maybe encountering an object file ..." << endl;

    // DQ (1/24/2010): support for directories not in place yet.
    if (numberOfDirectories() > 0) {
      printf("Directory support for linking not implemented... (unclear what "
             "this means...)\n");
      return 0;
    }
  } else {
    // normal cases that rose translators will actually do something about the
    // input files and we have SgFile for each of the files. if
    // ((numberOfFiles()== 0) || get_compileOnly() ||
    // get_file(0).get_skipfinalCompileStep()
    if (get_compileOnly() || get_file(0).get_skipfinalCompileStep() ||
        get_file(0).get_skip_unparse()) {
      if (get_verbose() > 0)
        cout << "Skipping SgProject::link ..." << endl;
      return 0;
    }
  }

  // Compile the output file from the unparsing
  vector<string> argcArgvList = get_originalCommandLineArgumentList();

  // error checking
  if (numberOfFiles() != 0)
    ROSE_ASSERT(argcArgvList.size() > 1);

  ROSE_ASSERT(linkerName != "");

  // strip out any rose options before passing the command line.
  SgFile::stripRoseCommandLineOptions(argcArgvList);

  // strip out frontend-specific options that would cause an error in the
  // backend linker (compiler).

  SgFile::stripTranslationCommandLineOptions(argcArgvList);

  // remove the original compiler/linker name
  if (argcArgvList.size() > 0)
    argcArgvList.erase(argcArgvList.begin());

  // remove all original file names
  Rose_STL_Container<string> sourceFilenames = get_sourceFileNameList();
  for (Rose_STL_Container<string>::iterator i = sourceFilenames.begin();
       i != sourceFilenames.end(); i++) {
#if USE_ABSOLUTE_PATHS_IN_SOURCE_FILE_LIST
#error "USE_ABSOLUTE_PATHS_IN_SOURCE_FILE_LIST is not supported yet"
    // DQ (9/1/2006): Check for use of absolute path and convert filename to
    // absolute path if required
    bool usesAbsolutePath = ((*i)[0] == '/');
    if (usesAbsolutePath == false) {
      string targetSourceFileToRemove =
          StringUtility::getAbsolutePathFromRelativePath(*i);
      printf("Converting source file to absolute path to search for it and "
             "remove it! targetSourceFileToRemove = %s \n",
             targetSourceFileToRemove.c_str());
      argcArgvList.remove(targetSourceFileToRemove);
    } else {
      argcArgvList.remove(*i);
    }
    if (find(argcArgvList.begin(), argcArgvList.end(), *i) !=
        argcArgvList.end()) {
      argcArgvList.erase(find(argcArgvList.begin(), argcArgvList.end(), *i));
    }
#else
    argcArgvList.erase(
        std::remove(argcArgvList.begin(), argcArgvList.end(), *i),
        argcArgvList.end());
#endif
  }

  // fix double quoted strings
  // DQ (4/14/2005): Fixup quoted strings in args fix "-DTEST_STRING_MACRO="Thu
  // Apr 14 08:18:33 PDT 2005" to be -DTEST_STRING_MACRO=\""Thu Apr 14 08:18:33
  // PDT 2005"\"  This is a problem in the compilation of a Kull file
  // (version.cc), when the backend is specified as
  // /usr/apps/kull/tools/mpig++-3.4.1.  The problem is that
  // /usr/apps/kull/tools/mpig++-3.4.1 is a wrapper for a shell script
  // /usr/local/bin/mpiCC which does not tend to observe quotes well.  The
  // solution is to add additional escaped quotes.
  for (Rose_STL_Container<string>::iterator i = argcArgvList.begin();
       i != argcArgvList.end(); i++) {
    if (fixQuotedArgumentForWrapperScript(*i)) {
      printf("Modified argument = %s \n", (*i).c_str());
    }
  }

  // Call the compile
  int errorCode = link(argcArgvList, linkerName);

  // return the error code from the compilation
  return errorCode;
}

// DQ (10/14/2010): Removing reference to macros defined in rose_config.h
// (defined in the header file as a default parameter). int SgProject::link (
// const std::vector<std::string>& argv )
int SgProject::link(const std::vector<std::string> &argv,
                    std::string linkerName) {
  // argv.size could be 0 after strip off compiler name, original source file,
  // etc ROSE_ASSERT(argv.size() > 0);

  // This link function will be moved into the SgProject IR node when complete
  const std::string whiteSpace = " ";
  // printf ("This link function is no longer called (I think!) \n");
  // ROSE_ABORT();

  // DQ (10/15/2005): Trap out case of C programs where we want to make sure
  // that we don't use the C++ compiler to do our linking!
  if (get_C_only() == true || get_C99_only() == true) {
    linkerName = BACKEND_C_COMPILER_NAME_WITH_PATH;
  } else {
    // The default is C++
    linkerName = BACKEND_CXX_COMPILER_NAME_WITH_PATH;

    if (get_Fortran_only() == true) {
      // linkerName = "f77 ";
      linkerName = BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH;
    } else {
      // Nothing to do here (case of C++)
    }
  }

  // This is a better implementation since it will include any additional
  // command line options that target the linker
  Rose_STL_Container<string> linkingCommand;

  linkingCommand.push_back(linkerName);
  // find all object files generated at file level compilation
  // The assumption is that -o objectFileName is made explicit and
  // is generated by SgFile::generateOutputFileName()
  for (int i = 0; i < numberOfFiles(); i++) {
    // DQ (2/25/2014): If this file was supressed in the compilation to build an
    // object file then it should be supressed in being used in the linking
    // stage. linkingCommand.push_back(get_file(i).generateOutputFileName());
    if (get_file(i).get_skipfinalCompileStep() == false) {
      linkingCommand.push_back(get_file(i).generateOutputFileName());
    }
  }

  // Add any options specified in the original command line (after
  // preprocessing)
  linkingCommand.insert(linkingCommand.end(), argv.begin(), argv.end());

  // Check if -o option exists, otherwise append -o a.out to the command line

  // Additional libraries to be linked with
  // Liao, 9/23/2009, optional linker flags to support OpenMP lowering targeting
  // GOMP if ((numberOfFiles() !=0) && (get_file(0).get_openmp_lowering()) Liao
  // 6/29/2012. sometimes rose translator is used as a wrapper for linking There
  // will be no SgFile at all in this case but we still want to append relevant
  // linking options for OpenMP
  if (get_openmp_linking()) {
    linkingCommand.push_back(resolveXompArchivePath());
#ifdef ROSE_LLVM_OPENMP_RUNTIME_LIBRARY
    string llvm_openmp_runtime_library(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY);
    if (llvm_openmp_runtime_library.empty()) {
      printf("Error: OpenMP lowering requires LLVM OpenMP runtime library.\n");
      ROSE_ABORT();
    }

    linkingCommand.push_back(llvm_openmp_runtime_library);
    string llvm_openmp_runtime_dir =
        std::filesystem::path(llvm_openmp_runtime_library)
            .parent_path()
            .string();
    if (!llvm_openmp_runtime_dir.empty()) {
      linkingCommand.push_back("-Wl,-rpath," + llvm_openmp_runtime_dir);
    }
    linkingCommand.push_back("-lpthread");
#else
    printf("Error: OpenMP lowering requires LLVM OpenMP runtime library.\n");
    ROSE_ABORT();
#endif
  }

  // TOO1 (2015/05/11): Causes configure-time tests to fail. Checking ld
  // linker, as example:
  //
  //     identityTranslator -print-prog-name=ld -rose:verbose 0
  //     In SgProject::link command line = g++ -print-prog-name=ld
  //     ld
  // if ( get_verbose() > 0 )
  //   {
  //     printf ("In SgProject::link command line = %s
  //     \n",CommandlineProcessing::generateStringFromArgList(linkingCommand,false,false).c_str());
  //   }

  int status = systemFromVector(linkingCommand);

  if (get_verbose() > 1) {
    printf("linker error status = %d \n", status);
  }

  // DQ (4/13/2015): Added testing and exiting on non-zero link status
  // (debugging use of redundant -o option). If the compile line has triggered
  // the link step then we don't want to do the linking here.  Note that we
  // can't disable the link in the compilation line because "-MMD -MF
  // .subdirs-install.d" options require the use of the non-absolute path (at
  // least that is my understanding of the problem).
  if (status != 0) {
  }

  return status;
}

// DQ (12/22/2005): Jochen's support for a constant (non-NULL) valued pointer
// to use to distinguish valid from invalid IR nodes within the memory pools.
namespace AST_FileIO {
SgNode *IS_VALID_POINTER() {
  // static SgNode* value = (SgNode*)(new char[1]);

  // DQ (1/17/2006): Set to the pointer value 0xffffffff (as used by
  // std::string::npos)
  static SgNode *value = (SgNode *)(std::string::npos);
  // printf ("In AST_FileIO::IS_VALID_POINTER(): value = %p \n",value);

  return value;
}

// similar vlue for reprentation of subsets of the AST
SgNode *TO_BE_COPIED_POINTER() {
  // static SgNode* value = (SgNode*)(new char[1]);
  static SgNode *value = (SgNode *)((std::string::npos)-1);
  // printf ("In AST_FileIO::TO_BE_COPIED_POINTER(): value = %p \n",value);
  return value;
}
} // namespace AST_FileIO

//! Prints pragma associated with a grammatical element.
/*!       (fill in more detail here!)
 */
void print_pragma(SgAttributePtrList &pattr, std::ostream &os) {
  SgAttributePtrList::const_iterator p = pattr.begin();
  if (p == pattr.end())
    return;
  else
    p++;

  while (p != pattr.end()) {
    if ((*p)->isPragma()) {
      SgPragma *pr = (SgPragma *)(*p);
      if (!pr->gotPrinted()) {
        os << std::endl << "#pragma " << pr->get_name() << std::endl;
        pr->setPrinted(1);
      }
    }
    p++;
  }
}

// Temporary function to be later put into Sg_FileInfo
StringUtility::FileNameLocation get_location(Sg_File_Info *X) {
  SgFile *file = SageInterface::getEnclosingFileNode(X->get_parent());
  ASSERT_not_null(file);
  string sourceFilename = file->getFileName();
  string sourceDirectory = StringUtility::getPathFromFileName(sourceFilename);

  StringUtility::FileNameClassification classification =
      StringUtility::classifyFileName(X->get_filenameString(), sourceDirectory,
                                      StringUtility::getOSType());

  // return StringUtility::FILENAME_LOCATION_UNKNOWN;
  return classification.getLocation();
}

StringUtility::FileNameLibrary get_library(Sg_File_Info *X) {
  SgFile *file = SageInterface::getEnclosingFileNode(X->get_parent());
  ASSERT_not_null(file);
  string sourceFilename = file->getFileName();
  string sourceDirectory = StringUtility::getPathFromFileName(sourceFilename);

  StringUtility::FileNameClassification classification =
      StringUtility::classifyFileName(X->get_filenameString(), sourceDirectory,
                                      StringUtility::getOSType());

  // return StringUtility::FILENAME_LIBRARY_UNKNOWN;
  return classification.getLibrary();
}

std::string get_libraryName(Sg_File_Info *X) {
  SgFile *file = SageInterface::getEnclosingFileNode(X->get_parent());
  ASSERT_not_null(file);
  string sourceFilename = file->getFileName();
  string sourceDirectory = StringUtility::getPathFromFileName(sourceFilename);

  StringUtility::FileNameClassification classification =
      StringUtility::classifyFileName(X->get_filenameString(), sourceDirectory,
                                      StringUtility::getOSType());

  // return "";
  return classification.getLibraryName();
}

StringUtility::OSType get_OS_type() {
  // return StringUtility::OS_TYPE_UNKNOWN;
  return StringUtility::getOSType();
}

int get_distanceFromSourceDirectory(Sg_File_Info *X) {
  SgFile *file = SageInterface::getEnclosingFileNode(X->get_parent());
  ASSERT_not_null(file);
  string sourceFilename = file->getFileName();
  string sourceDirectory = StringUtility::getPathFromFileName(sourceFilename);

  StringUtility::FileNameClassification classification =
      StringUtility::classifyFileName(X->get_filenameString(), sourceDirectory,
                                      StringUtility::getOSType());

  // return 0;
  return classification.getDistanceFromSourceDirectory();
}

int SgNode::numberOfNodesInSubtree() {
  int value = 0;

  class CountTraversal : public SgSimpleProcessing {
  public:
    int count;
    CountTraversal() : count(0) {}
    void visit(SgNode *n) { count++; }
  };

  CountTraversal counter;
  SgNode *thisNode = const_cast<SgNode *>(this);
  counter.traverse(thisNode, preorder);
  value = counter.count;

  return value;
}

namespace SgNode_depthOfSubtree {
// This class (AST traversal) could not be defined in the function
// SgNode::depthOfSubtree() So I have constructed a namespace for this class to
// be implemented outside of the function.

class DepthInheritedAttribute {
public:
  int treeDepth;
  DepthInheritedAttribute(int depth) : treeDepth(depth) {}
};

class MaxDepthTraversal : public AstTopDownProcessing<DepthInheritedAttribute> {
public:
  int maxDepth;
  MaxDepthTraversal() : maxDepth(0) {}

  DepthInheritedAttribute
  evaluateInheritedAttribute(SgNode *astNode,
                             DepthInheritedAttribute inheritedAttribute) {
    if (inheritedAttribute.treeDepth > maxDepth)
      maxDepth = inheritedAttribute.treeDepth;
    return DepthInheritedAttribute(inheritedAttribute.treeDepth + 1);
  }
};
} // namespace SgNode_depthOfSubtree

int SgNode::depthOfSubtree() {
  int value = 0;

  SgNode_depthOfSubtree::MaxDepthTraversal depthCounter;
  SgNode_depthOfSubtree::DepthInheritedAttribute inheritedAttribute(0);
  SgNode *thisNode = const_cast<SgNode *>(this);

  depthCounter.traverse(thisNode, inheritedAttribute);

  value = depthCounter.maxDepth;

  return value;
}

// DQ (10/3/2008): Added support for getting interfaces in a module
std::vector<SgInterfaceStatement *> SgModuleStatement::get_interfaces() const {
  std::vector<SgInterfaceStatement *> returnList;

  SgModuleStatement *definingModuleStatement =
      isSgModuleStatement(get_definingDeclaration());
  ASSERT_not_null(definingModuleStatement);

  SgClassDefinition *moduleDefinition =
      definingModuleStatement->get_definition();
  ASSERT_not_null(moduleDefinition);

  SgDeclarationStatementPtrList &declarationList =
      moduleDefinition->getDeclarationList();

  SgDeclarationStatementPtrList::iterator i = declarationList.begin();
  while (i != declarationList.end()) {
    SgInterfaceStatement *interfaceStatement = isSgInterfaceStatement(*i);
    if (interfaceStatement != nullptr) {
      returnList.push_back(interfaceStatement);
    }

    i++;
  }

  return returnList;
}

// DQ (11/23/2008): This is a static function
SgC_PreprocessorDirectiveStatement *
SgC_PreprocessorDirectiveStatement::createDirective(
    PreprocessingInfo *currentPreprocessingInfo) {
  // This is the new factory interface to build CPP directives as IR nodes.
  PreprocessingInfo::DirectiveType directive =
      currentPreprocessingInfo->getTypeOfDirective();

  // SgC_PreprocessorDirectiveStatement* cppDirective = new
  // SgEmptyDirectiveStatement();
  SgC_PreprocessorDirectiveStatement *cppDirective = nullptr;

  switch (directive) {
  case PreprocessingInfo::CpreprocessorUnknownDeclaration: {
    // I think this is an error...
    // locatedNode->addToAttachedPreprocessingInfo(currentPreprocessingInfoPtr);
    printf("Error: directive == "
           "PreprocessingInfo::CpreprocessorUnknownDeclaration \n");
    ROSE_ABORT();
  }

  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::CpreprocessorBlankLine:
  case PreprocessingInfo::ClinkageSpecificationStart:
  case PreprocessingInfo::ClinkageSpecificationEnd: {
    printf("Error: these cases could not generate a new IR node "
           "(directiveTypeName = %s) \n",
           PreprocessingInfo::directiveTypeName(directive).c_str());
    ROSE_ABORT();
  }

  case PreprocessingInfo::CpreprocessorIncludeDeclaration: {
    cppDirective = new SgIncludeDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorIncludeNextDeclaration: {
    cppDirective = new SgIncludeNextDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorDefineDeclaration: {
    cppDirective = new SgDefineDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorUndefDeclaration: {
    cppDirective = new SgUndefDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorIfdefDeclaration: {
    cppDirective = new SgIfdefDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorIfndefDeclaration: {
    cppDirective = new SgIfndefDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorIfDeclaration: {
    cppDirective = new SgIfDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorDeadIfDeclaration: {
    cppDirective = new SgDeadIfDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorElseDeclaration: {
    cppDirective = new SgElseDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorElifDeclaration: {
    cppDirective = new SgElseifDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorEndifDeclaration: {
    cppDirective = new SgEndifDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorLineDeclaration: {
    cppDirective = new SgLineDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorErrorDeclaration: {
    cppDirective = new SgErrorDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorWarningDeclaration: {
    cppDirective = new SgWarningDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorEmptyDeclaration: {
    cppDirective = new SgEmptyDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorIdentDeclaration: {
    cppDirective = new SgIdentDirectiveStatement();
    break;
  }
  case PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker: {
    cppDirective = new SgLinemarkerDirectiveStatement();
    break;
  }

  default: {
    printf("Error: directive not handled directiveTypeName = %s \n",
           PreprocessingInfo::directiveTypeName(directive).c_str());
    ROSE_ABORT();
  }
  }

  ASSERT_not_null(cppDirective);

  printf("In SgC_PreprocessorDirectiveStatement::createDirective(): "
         "currentPreprocessingInfo->getString() = %s \n",
         currentPreprocessingInfo->getString().c_str());

  cppDirective->set_directiveString(currentPreprocessingInfo->getString());

  printf("In SgC_PreprocessorDirectiveStatement::createDirective(): "
         "cppDirective->get_directiveString() = %s \n",
         cppDirective->get_directiveString().c_str());

  // Set the defining declaration to be a self reference...
  cppDirective->set_definingDeclaration(cppDirective);

  // Build source position information...
  cppDirective->set_startOfConstruct(
      new Sg_File_Info(*(currentPreprocessingInfo->get_file_info())));
  cppDirective->set_endOfConstruct(
      new Sg_File_Info(*(currentPreprocessingInfo->get_file_info())));

  return cppDirective;
}

bool StringUtility::popen_wrapper(const string &command,
                                  vector<string> &result) {
  // DQ (2/5/2009): Simple wrapper for Unix popen command.

  const int SIZE = 10000;
  bool returnValue = true;
  FILE *fp = nullptr;
  char buffer[SIZE];

  result = vector<string>();

  if ((fp = popen(command.c_str(), "r")) == nullptr) {
    cerr << "Files or processes cannot be created" << endl;
    returnValue = false;
    return returnValue;
  }

  string current_string;
  while (fgets(buffer, sizeof(buffer), fp)) {
    current_string = buffer;
    if (current_string[current_string.size() - 1] != '\n') {
      cerr << "SIZEBUF too small (" << SIZE << ")" << endl;
      returnValue = false;
      return returnValue;
    }
    ROSE_ASSERT(current_string[current_string.size() - 1] == '\n');
    result.push_back(current_string.substr(0, current_string.size() - 1));
  }

  if (pclose(fp) == -1) {
    cerr << ("Cannot execute pclose");
    returnValue = false;
  }

  return returnValue;
}

SgFunctionDeclaration *
SgFunctionCallExp::getAssociatedFunctionDeclaration() const {
  // This is helpful in chasing down the associated declaration to this function
  // reference.
  SgFunctionDeclaration *returnFunctionDeclaration = nullptr;

  SgFunctionSymbol *associatedFunctionSymbol = getAssociatedFunctionSymbol();
  // It can be NULL for a function pointer
  // ROSE_ASSERT(associatedFunctionSymbol != NULL);
  if (associatedFunctionSymbol != nullptr)
    returnFunctionDeclaration = associatedFunctionSymbol->get_declaration();

  // ROSE_ASSERT(returnFunctionDeclaration != NULL);

  return returnFunctionDeclaration;
}

SgFunctionSymbol *SgFunctionCallExp::getAssociatedFunctionSymbol() const {
  // This is helpful in chasing down the associated declaration to this function
  // reference. But this refactored function does the first step of getting the
  // symbol, so that it can also be used separately in the outlining support.
  SgFunctionSymbol *returnSymbol = nullptr;

  // Note that as I recall there are a number of different types of IR nodes
  // that the functionCallExp->get_function() can return (this is the complete
  // list, as tested in astConsistancyTests.C):
  //   - SgDotExp
  //   - SgDotStarOp
  //   - SgArrowExp
  //   - SgArrowStarOp
  //   - SgPointerDerefExp
  //   - SgAddressOfOp
  //   - SgFunctionRefExp
  //   - SgMemberFunctionRefExp
  //   - SgFunctionSymbol  // Liao 4/7/2017, discovered by a call to RAJA
  //   template functions using lambda expressions
  // schroder3 (2016-06-28): There are some more (see below).

  // Some virtual functions are resolved statically (e.g. for objects allocated
  // on the stack)
  bool isAlwaysResolvedStatically = false;

  SgExpression *functionExp = this->get_function();

  // schroder3 (2016-08-16): Moved the handling of SgPointerDerefExp and
  // SgAddressOfOp above the switch. Due to this
  //  all pointer dereferences and address-ofs are removed from the function
  //  expression before it is analyzed. Member functions that are an operand
  //  of a pointer dereference or address-of are supported due to this now.
  //
  // schroder3 (2016-06-28): Added SgAddressOp (for example "(&f)()",
  // "(*&***&**&*&f)()" or "(&***&**&*&f)()")
  //
  // Some frontends remove all SgPointerDerefExp nodes from an expression
  // like this:
  //    void f() { (***f)(); }
  // Others do not. Therefore, if the thing to which the pointers ultimately
  // point is a SgFunctionRefExp then we know the function, otherwise Liao's
  // comment below applies. [Robb Matzke 2012-12-28]
  //
  // Liao, 5/19/2009
  // A pointer to function can be associated to any functions with a matching
  // function type There is no single function declaration which is
  // associated with it. In this case return NULL should be allowed and the
  // caller has to handle it accordingly
  //
  while (isSgPointerDerefExp(functionExp) || isSgAddressOfOp(functionExp)) {
    functionExp = isSgUnaryOp(functionExp)->get_operand();
  }

  switch (functionExp->variantT()) {
  case V_SgPointerDerefExp:
  case V_SgAddressOfOp: {
    ROSE_ABORT();
  }
  case V_SgFunctionRefExp: {
    SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(functionExp);
    ASSERT_not_null(functionRefExp);
    returnSymbol = functionRefExp->get_symbol();

    // DQ (2/8/2009): Can we assert this! What about pointers to functions?
    ASSERT_not_null(returnSymbol);
    break;
  }

    // DQ (2/24/2013): Added case to support SgTemplateFunctionRefExp (now
    // generated as a result of work on 2/23/2013 specific to unknown function
    // handling in templates, now resolved by name).
  case V_SgTemplateFunctionRefExp: {
    SgTemplateFunctionRefExp *functionRefExp =
        isSgTemplateFunctionRefExp(functionExp);
    ASSERT_not_null(functionRefExp);
    returnSymbol = functionRefExp->get_symbol();

    // DQ (2/8/2009): Can we assert this! What about pointers to functions?
    ASSERT_not_null(returnSymbol);
    break;
  }

    // DQ (2/25/2013): Added case to support SgTemplateFunctionRefExp (now
    // generated as a result of work on 2/23/2013 specific to unknown function
    // handling in templates, now resolved by name).
  case V_SgTemplateMemberFunctionRefExp: {
    SgTemplateMemberFunctionRefExp *templateMemberFunctionRefExp =
        isSgTemplateMemberFunctionRefExp(functionExp);
    ASSERT_not_null(templateMemberFunctionRefExp);
    returnSymbol = templateMemberFunctionRefExp->get_symbol();

    // DQ (2/8/2009): Can we assert this! What about pointers to functions?
    ASSERT_not_null(returnSymbol);
    break;
  }

  case V_SgMemberFunctionRefExp: {
    SgMemberFunctionRefExp *memberFunctionRefExp =
        isSgMemberFunctionRefExp(functionExp);
    ASSERT_not_null(memberFunctionRefExp);
    returnSymbol = memberFunctionRefExp->get_symbol();

    // DQ (2/8/2009): Can we assert this! What about pointers to functions?
    ASSERT_not_null(returnSymbol);
    break;
  }

  case V_SgLambdaExp: {
    SgLambdaExp *lambdaExp = isSgLambdaExp(functionExp);
    ASSERT_not_null(lambdaExp);
    SgFunctionDeclaration *lambdaFunction = lambdaExp->get_lambda_function();
    if (lambdaFunction != nullptr) {
      if (SgFunctionDeclaration *definingDecl = isSgFunctionDeclaration(
              lambdaFunction->get_definingDeclaration())) {
        lambdaFunction = definingDecl;
      }

      returnSymbol =
          isSgFunctionSymbol(lambdaFunction->get_symbol_from_symbol_table());
      if (returnSymbol == nullptr && lambdaFunction->get_scope() != nullptr) {
        returnSymbol = lambdaFunction->get_scope()->lookup_function_symbol(
            lambdaFunction->get_name(), lambdaFunction->get_type());
      }
    }
    break;
  }

  case V_SgArrowExp: {
    // The lhs is the this pointer (SgThisExp) and the rhs is the member
    // function.
    SgArrowExp *arrayExp = isSgArrowExp(functionExp);
    ASSERT_not_null(arrayExp);

    SgMemberFunctionRefExp *memberFunctionRefExp =
        isSgMemberFunctionRefExp(arrayExp->get_rhs_operand());

    // DQ (2/21/2010): Relaxed this constraint because it failes in
    // fixupPrettyFunction test. ROSE_ASSERT(memberFunctionRefExp != NULL);
    if (memberFunctionRefExp != nullptr) {
      returnSymbol = memberFunctionRefExp->get_symbol();

      // DQ (2/8/2009): Can we assert this! What about pointers to functions?
      ASSERT_not_null(returnSymbol);
    }
    break;
  }

  case V_SgDotExp: {
    SgDotExp *dotExp = isSgDotExp(functionExp);
    ASSERT_not_null(dotExp);
    ASSERT_not_null(dotExp->get_rhs_operand());
    // There are four different types of function call reference expression in
    // ROSE.
    SgMemberFunctionRefExp *memberFunctionRefExp =
        isSgMemberFunctionRefExp(dotExp->get_rhs_operand());
    if (memberFunctionRefExp == nullptr) {
      // This could be a SgTemplateMemberFunctionRefExp (not derived from
      // SgMemberFunctionRefExp or SgFunctionRefExp). See test2013_70.C
      SgTemplateMemberFunctionRefExp *templateMemberFunctionRefExp =
          isSgTemplateMemberFunctionRefExp(dotExp->get_rhs_operand());
      if (templateMemberFunctionRefExp == nullptr) {
        SgTemplateFunctionRefExp *templateFunctionRefExp =
            isSgTemplateFunctionRefExp(dotExp->get_rhs_operand());
        if (templateFunctionRefExp == nullptr) {
          SgFunctionRefExp *functionRefExp =
              isSgFunctionRefExp(dotExp->get_rhs_operand());
          if (functionRefExp == nullptr) {
            SgVarRefExp *varRefExp = isSgVarRefExp(dotExp->get_rhs_operand());
            if (varRefExp == nullptr) {
              SgNonrealRefExp *nrRefExp =
                  isSgNonrealRefExp(dotExp->get_rhs_operand());
              if (nrRefExp == nullptr) {
                dotExp->get_rhs_operand()->get_file_info()->display(
                    "In SgFunctionCallExp::getAssociatedFunctionSymbol(): case "
                    "of SgDotExp: templateMemberFunctionRefExp == NULL: debug");
                printf(
                    "In SgFunctionCallExp::getAssociatedFunctionSymbol(): case "
                    "of SgDotExp: dotExp->get_rhs_operand() = %p = %s \n",
                    dotExp->get_rhs_operand(),
                    dotExp->get_rhs_operand()->class_name().c_str());
              } else {
                // FIXME should we return the non-real symbol?
              }
            } else {
              ASSERT_not_null(varRefExp);

              // DQ (8/20/2013): This is not a SgFunctionSymbol so we can't
              // return a valid symbol from this case of a function call from a
              // pointer to a function. returnSymbol = varRefExp->get_symbol();
            }
          } else {
            // I am unclear when this is possible, but STL code exercises it.
            ASSERT_not_null(functionRefExp);
            returnSymbol = functionRefExp->get_symbol();

            ASSERT_not_null(returnSymbol);
          }
        } else {
          // I am unclear when this is possible, but STL code exercises it.
          ASSERT_not_null(templateFunctionRefExp);
          returnSymbol = templateFunctionRefExp->get_symbol();

          ASSERT_not_null(returnSymbol);
        }
      } else {
        ASSERT_not_null(templateMemberFunctionRefExp);
        returnSymbol = templateMemberFunctionRefExp->get_symbol();

        ASSERT_not_null(returnSymbol);
      }
    } else {
      ASSERT_not_null(memberFunctionRefExp);
      returnSymbol = memberFunctionRefExp->get_symbol();

      ASSERT_not_null(returnSymbol);
    }

    // DQ (8/20/2013): Function pointers don't allow us to generate a proper
    // valid SgFunctionSymbol. DQ (2/8/2009): Can we assert this! What about
    // pointers to functions? ROSE_ASSERT(returnSymbol != NULL);

    // Virtual functions called through the dot operator are resolved statically
    // if they are not called on reference types.
    isAlwaysResolvedStatically = !isSgReferenceType(dotExp->get_lhs_operand());

    break;
  }

    // DotStar (Section 5.5 of C++ standard) is used to call a member function
    // pointer and implicitly specify the associated 'this' parameter. In this
    // case, we can't statically determine which function is getting called and
    // should return null.
  case V_SgDotStarOp: {
    break;
  }

    // ArrowStar (Section 5.5 of C++ standard) is used to call a member function
    // pointer and implicitly specify the associated 'this' parameter. In this
    // case, we can't statically determine which function is getting called and
    // should return null.
  case V_SgArrowStarOp: {
    break;
  }

    // DQ (2/25/2013): Added support for this case, but I would like to review
    // this (likely OK). It might be that this should resolve to a symbol.
  case V_SgConstructorInitializer: {
    ASSERT_not_null(functionExp->get_file_info());
    break;
  }

    // schroder3 (2016-06-28): Commented out the assignment of returnSymbol.
    // Reason:
    //  I think we should not return a symbol in this case because we can not
    //  say anything about the function that is actually called by this function
    //  call expression. E.g. in case of a call C of the return value of
    //  get_random_func_address ("get_random_func_address()()")
    //  get_random_func_address has nothing to do with the
    //                                                                                                     ^-C
    //  function called by C. Previously the returned symbol was therefore not
    //  the associated function symbol of this function call expression.
    //
    // DQ (2/25/2013): Added support for this case, but I would like to review
    // this (likely OK).
  case V_SgFunctionCallExp: {
    ASSERT_not_null(functionExp->get_file_info());

    //               SgFunctionCallExp* nestedFunctionCallExp =
    //               isSgFunctionCallExp(functionExp);
    //               ROSE_ASSERT(nestedFunctionCallExp != NULL);
    //
    //               returnSymbol =
    //               nestedFunctionCallExp->getAssociatedFunctionSymbol();
    break;
  }

    // DQ (2/25/2013): Added support for this case, verify as function call off
    // of an array of function pointers. This should not resolve to a symbol.
  case V_SgPntrArrRefExp: {
    break;
  }

    // DQ (2/25/2013): Added support for this case, from test2012_102.c.
    // Not clear if this should resolve to a symbol (I think likely yes).
  case V_SgCastExp: {
    break;
  }

    // YYH(2023/02/01): need to debug this how this could happen.
  case V_SgNonrealRefExp: {
    break;
    // ROSE_ABORT();
  }
    // DQ (2/25/2013): Added support for this case, from test2012_133.c.
    // This should not resolve to a symbol.
  case V_SgConditionalExp: {
    break;
  }

    // DQ (2/22/2013): added case to support something reported in
    // test2013_68.C, but not yet verified.
  case V_SgVarRefExp: {
    break;
  }

    // DQ (12/17/2016): added case to support reducing output spew
    // from C++11 tests and applications.
  case V_SgThisExp: {
    break;
  }
  case V_SgFunctionSymbol: {
    returnSymbol = isSgFunctionSymbol(functionExp);
    break;
  }
    // CLANG FRONTEND FIX: Handle SgIntVal and other value expressions that may
    // appear when Clang RecoveryExpr creates placeholders during parse errors
    // or template issues
  case V_SgIntVal:
  case V_SgFloatVal:
  case V_SgDoubleVal:
  case V_SgStringVal:
  case V_SgBoolValExp:
  case V_SgCharVal:
  case V_SgNullExpression: {
    // These are placeholder values from error recovery - cannot resolve to
    // function symbol Return NULL to indicate function symbol cannot be
    // determined
#if DEBUG_SAGE_SUPPORT_GETASSOCIATEDFUNCTION
    MLOG_WARN_C("sage_support",
                "Function call expression has value literal %s as callee "
                "(likely from parse error recovery), returning NULL\n",
                functionExp->class_name().c_str());
#endif
    break;
  }
  case V_SgPseudoDestructorRefExp: {
    // Pseudo-destructor calls don't correspond to a function symbol in the
    // usual sense.
    break;
  }
  default: {
    // Send out error message before the assertion, which may fail and stop
    // first otherwise.
    MLOG_ERROR_C("sage_support",
                 "There should be no other cases functionExp = %p = %s \n",
                 functionExp, functionExp->class_name().c_str());

    ASSERT_not_null(functionExp->get_file_info());

    // DQ (3/15/2017): Fixed to use mlog message logging.
    // if (Rose::ir_node_mlog[Rose::Diagnostics::DEBUG])
    {
      functionExp->get_file_info()->display(
          "In SgFunctionCallExp::getAssociatedFunctionSymbol(): case not "
          "supported: debug");
    }

    // schroder3 (2016-07-25): Changed "#if 1" to "#if 0" to remove ROSE_ASSERT.
    // If this member function is unable to determine the
    //  associated function then it should return 0 instead of raising an
    //  assertion.
    // DQ (2/23/2013): Allow this to be commented out so that I can generate the
    // DOT graphs to better understand the problem in test2013_69.C.
    // ROSE_ABORT();
  }
  }

  // If the function is virtual, the function call might actually be to a
  // different symbol. We should return NULL in this case to preserve
  // correctness
  if (returnSymbol != nullptr && !isAlwaysResolvedStatically) {
    SgFunctionModifier &functionModifier =
        returnSymbol->get_declaration()->get_functionModifier();
    if (functionModifier.isVirtual() || functionModifier.isPureVirtual()) {
      returnSymbol = nullptr;
    }
  }

  return returnSymbol;
}

// DQ (10/19/2010): This is moved from
// src/ROSETTA/Grammar/Cxx_GlobalDeclarations.macro to here since it is code in
// the header files that we would like to avoid.  My fear is that because of the
// way it works it is required to be inlined onto the stack of the calling
// function. #ifndef ROSE_PREVENT_CONSTRUCTION_ON_STACK #define
// ROSE_PREVENT_CONSTRUCTION_ON_STACK inline void
// preventConstructionOnStack(SgNode* n)
void preventConstructionOnStack(SgNode *n) {
#ifndef NDEBUG
  void *frameaddr = __builtin_frame_address(0);

  signed long dist = (char *)n - (char *)frameaddr;

  // DQ (12/6/2009): This fails for the 4.0.4 compiler, but only in 64-bit when
  // run with Hudson. I can't reporduce the problem using the 4.0.4 compiler,
  // but it is uniformally a problem since it fails on all tests (via hudson)
  // using older configurations (and also for the tests of the legacy frontend
  // binary). assert (dist < -10000 || dist > 10000);

#ifdef __GNUC__
  // Note that this is a test of the backend compiler, it seems that we don't
  // track the compiler used to compile ROSE, but this is what we would want.

#if (BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER == 4) &&                        \
    (BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER == 0)
  // For the GNU 4.0.x make this a warning, since it appears to fail due to a
  // poor implementaiton for just this compiler and no other version of GNU.
  // Just that we are pringing this warning is a problem for many tests (maybe
  // this should be enable within verbose mode).
  if (dist < -10000 || dist > 10000) {
  }
  // For all other versions of the GNU compiler make this an error.
  assert(dist < -10000 || dist > 10000);
#endif
  // For all other compilers make this an error.
  assert(dist < -10000 || dist > 10000);
#endif // __GNUC__

#endif // NDEBUG
}
// #endif // ROSE_PREVENT_CONSTRUCTION_ON_STACK
