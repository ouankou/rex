/**
 * \file    sage_support.C
 * \author  Justin Too <too1@llnl.gov>
 * \date    April 4, 2012
 */

/*-----------------------------------------------------------------------------
 *  Dependencies
 *---------------------------------------------------------------------------*/

#include "Rose/FileSystem.h"

#include <limits>

#include "cmdline.h"

#include "FileHelper.h"

#include "processSupport.h"
#include "rex-backend-link-options.h"

#include "rose_config.h"
#include "sage3basic.h"

#include "sage_support.h"

#include "unparser.h"

#include "astJson/sageAstJson.h"

#include "rose_path_resolver.h"

#include "rose_paths.h"
#include "rose_test_output_path.h"

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

namespace {
using OriginalHeaderSnapshots = std::map<std::string, std::string>;

OriginalHeaderSnapshots &originalHeaderSnapshots(SgProject *project) {
  ASSERT_not_null(project);
  return project->get_original_header_snapshots();
}

void snapshotOriginalHeader(SgProject *project, const std::string &headerPath) {
  OriginalHeaderSnapshots &snapshots = originalHeaderSnapshots(project);
  if (snapshots.find(headerPath) != snapshots.end()) {
    return;
  }

  std::ifstream input(headerPath, std::ios::in | std::ios::binary);
  if (!input) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-snapshot]: unable to open %s\n",
            headerPath.c_str());
    ROSE_ABORT();
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  if (input.bad()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-snapshot]: unable to read %s\n",
            headerPath.c_str());
    ROSE_ABORT();
  }
  snapshots.emplace(headerPath, contents.str());
}

bool insertFrontendSourceFile(std::map<std::string, SgSourceFile *> &files,
                              const std::string &path, SgSourceFile *sourceFile,
                              const char *inventory) {
  ASSERT_not_null(sourceFile);
  const std::string normalizedPath = FileHelper::normalizePathIfPossible(path);
  const std::string normalizedSourcePath =
      FileHelper::normalizePathIfPossible(sourceFile->getFileName());
  if (normalizedPath.empty() || normalizedSourcePath.empty() ||
      normalizedPath != normalizedSourcePath) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-source]: inventory=%s path=%s "
            "source=%s has inconsistent file identity\n",
            inventory, path.c_str(), sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  const auto inserted = files.emplace(normalizedPath, sourceFile);
  if (!inserted.second && inserted.first->second != sourceFile) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-context]: inventory=%s path=%s "
            "maps to distinct source nodes %p and %p\n",
            inventory, normalizedPath.c_str(),
            static_cast<void *>(inserted.first->second),
            static_cast<void *>(sourceFile));
    ROSE_ABORT();
  }
  return inserted.second;
}
} // namespace

#include "attachPreprocessingInfo.h"

#include "tokenStreamMapping.h"

using namespace std;
using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

const string FileHelper::pathDelimiter = "/";

void detectMacroOrIncludeFileExpansions(SgSourceFile *sourceFile);

static bool shouldBuildTokenMapping(const SgSourceFile *sourceFile) {
  if (sourceFile == NULL) {
    return false;
  }

  return sourceFile->get_unparse_tokens();
}

static std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
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
      CommandlineProcessing::generateSourceFilenames(argv);
  CommandlineProcessing::removeAllFileNamesExcept(argv, fileList, headerPath);

  const char *headerLanguage = NULL;
  if (referenceFile->get_Cuda_only()) {
    headerLanguage = "cuda";
  } else if (referenceFile->get_OpenCL_only()) {
    headerLanguage = "cl";
  } else if (referenceFile->get_Cxx_only()) {
    headerLanguage = "c++-header";
  } else if (referenceFile->get_C_only() || referenceFile->get_C99_only() ||
             referenceFile->get_C11_only()) {
    headerLanguage = "c-header";
  } else {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-materialization-language]: "
            "header=%s reference=%p/%s has no exact supported Clang "
            "language\n",
            headerPath.c_str(), static_cast<const void *>(referenceFile),
            referenceFile->get_sourceFileNameWithPath().c_str());
    ROSE_ABORT();
  }

  // A header path is not a suffix-classified driver source.  Publish it as one
  // exact source operand using the reference translation unit's Clang
  // language; otherwise SgFile construction receives no source identity.
  argv.push_back("-x");
  argv.push_back(headerLanguage);
  argv.push_back(headerPath);
  Rose_STL_Container<string> headerSources =
      CommandlineProcessing::generateSourceFilenames(argv);
  if (headerSources.size() != 1 ||
      FileHelper::normalizePathIfPossible(headerSources.front()) !=
          FileHelper::normalizePathIfPossible(headerPath)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-materialization-command]: "
            "header=%s language=%s produced %zu source operands\n",
            headerPath.c_str(), headerLanguage, headerSources.size());
    ROSE_ABORT();
  }

  SgSourceFile *headerFile = new SgSourceFile(argv, project);
  ROSE_ASSERT(headerFile != NULL);

  SgSourceFile *owner = const_cast<SgSourceFile *>(referenceFile);
  if (referenceFile->get_isHeaderFile()) {
    SgIncludeFile *include = referenceFile->get_associated_include_file();
    ROSE_ASSERT(include != nullptr &&
                include->get_source_file() == referenceFile);
    owner = include->get_source_file_of_translation_unit();
  }
  ROSE_ASSERT(owner != nullptr);
  SgIncludeFile *root = owner->get_associated_include_file();
  ROSE_ASSERT(root != nullptr && root->get_isRootSourceFile() &&
              root->get_source_file() == owner &&
              root->get_source_file_of_translation_unit() == owner);
  SgFileList *externalFiles = owner->get_frontendExternalFileList();
  if (externalFiles == nullptr) {
    externalFiles = new SgFileList();
    externalFiles->set_parent(owner);
    owner->set_frontendExternalFileList(externalFiles);
  }
  ROSE_ASSERT(externalFiles->get_parent() == owner);
  headerFile->set_parent(externalFiles);
  externalFiles->get_listOfFiles().push_back(headerFile);

  headerFile->set_isHeaderFile(true);
  headerFile->set_unparseHeaderFiles(referenceFile->get_unparseHeaderFiles());
  headerFile->set_unparse_tokens(referenceFile->get_unparse_tokens());
  copyLanguageSettings(headerFile, referenceFile);

  headerFile->set_requires_C_preprocessor(false);

  if (headerFile->get_preprocessorDirectivesAndCommentsList() == NULL) {
    headerFile->set_preprocessorDirectivesAndCommentsList(
        new ROSEAttributesListContainer());
  }

  return headerFile;
}

static SgIncludeFile *
findDirectIncludeFileByPath(SgIncludeFile *includingFile,
                            const std::string &headerPath) {
  if (includingFile == NULL) {
    return NULL;
  }

  for (SgIncludeFile *includeFile : includingFile->get_include_file_list()) {
    if (includeFile == NULL) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-tree]: including file=%s has a "
              "null direct include\n",
              includingFile->get_filename().str());
      ROSE_ABORT();
    }
    if (includeFile->get_filename() == headerPath) {
      return includeFile;
    }
  }

  return NULL;
}

static void inventoryMaterializedIncludeSources(
    SgProject *project, SgIncludeFile *includeRoot,
    std::map<std::string, SgSourceFile *> &sourceFilesByPath) {
  ASSERT_not_null(project);
  if (includeRoot == nullptr) {
    return;
  }

  std::vector<SgIncludeFile *> worklist{includeRoot};
  std::set<SgIncludeFile *> visited;
  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-tree]: null include-file node "
              "during source inventory\n");
      ROSE_ABORT();
    }
    if (!visited.insert(includeFile).second) {
      continue;
    }

    if (SgSourceFile *sourceFile = includeFile->get_source_file()) {
      if (sourceFile->get_project() != project) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[header-context]: header=%s source=%p "
                "belongs to a different project\n",
                includeFile->get_filename().str(),
                static_cast<void *>(sourceFile));
        ROSE_ABORT();
      }
      insertFrontendSourceFile(sourceFilesByPath,
                               includeFile->get_filename().str(), sourceFile,
                               "include-tree");
    }

    for (SgIncludeFile *child : includeFile->get_include_file_list()) {
      if (child == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[include-tree]: include=%s contains "
                "a null child\n",
                includeFile->get_filename().str());
        ROSE_ABORT();
      }
      worklist.push_back(child);
    }
  }
}

static SgGlobal *findHeaderEmissionGlobalScope(SgSourceFile *translationUnit,
                                               const std::string &headerPath) {
  ASSERT_not_null(translationUnit);
  const std::string normalizedHeaderPath =
      FileHelper::normalizePathIfPossible(headerPath);
  if (normalizedHeaderPath.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-scope]: header=%s has no "
            "normalized path\n",
            headerPath.c_str());
    ROSE_ABORT();
  }

  SgGlobal *emissionGlobal = nullptr;
  SgGlobal *translationUnitGlobal = translationUnit->get_globalScope();
  if (translationUnitGlobal == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[header-scope]: translation unit=%s has "
            "no global scope\n",
            translationUnit->getFileName().c_str());
    ROSE_ABORT();
  }

  if (AttachedPreprocessingInfoType *records =
          translationUnitGlobal->getAttachedPreprocessingInfo()) {
    for (PreprocessingInfo *record : *records) {
      if (record == nullptr || record->get_file_info() == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[header-preprocessing]: global=%p "
                "contains an incomplete preprocessing record\n",
                static_cast<void *>(translationUnitGlobal));
        ROSE_ABORT();
      }
      if (FileHelper::normalizePathIfPossible(
              record->get_file_info()->get_filenameString()) ==
          normalizedHeaderPath) {
        emissionGlobal = translationUnitGlobal;
        break;
      }
    }
  }

  for (SgNode *node : NodeQuery::querySubTree(translationUnit, V_SgStatement)) {
    SgStatement *statement = isSgStatement(node);
    if (statement == nullptr || statement->get_file_info() == nullptr) {
      continue;
    }
    const std::string statementPath =
        FileHelper::normalizePathIfPossible(Sg_File_Info::getFilenameFromID(
            statement->get_file_info()->get_physical_file_id()));
    if (statementPath != normalizedHeaderPath) {
      continue;
    }

    SgNode *ancestor = statement;
    std::set<SgNode *> visited;
    while (ancestor != nullptr && isSgGlobal(ancestor) == nullptr) {
      if (!visited.insert(ancestor).second) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[header-scope]: header=%s "
                "statement=%p has a parent cycle at %p\n",
                normalizedHeaderPath.c_str(), static_cast<void *>(statement),
                static_cast<void *>(ancestor));
        ROSE_ABORT();
      }
      ancestor = ancestor->get_parent();
    }
    SgGlobal *candidate = isSgGlobal(ancestor);
    if (candidate == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[header-scope]: header=%s statement=%p "
              "has no translation-unit global emission scope\n",
              normalizedHeaderPath.c_str(), static_cast<void *>(statement));
      ROSE_ABORT();
    }
    if (emissionGlobal != nullptr && emissionGlobal != candidate) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[header-context]: header=%s has "
              "distinct emission globals %p and %p\n",
              normalizedHeaderPath.c_str(), static_cast<void *>(emissionGlobal),
              static_cast<void *>(candidate));
      ROSE_ABORT();
    }
    emissionGlobal = candidate;
  }
  return emissionGlobal;
}

static PreprocessingInfo *findIncludingPreprocessingInfo(
    SgProject *project,
    const map<string, set<PreprocessingInfo *>> &includingPreprocessingInfosMap,
    SgSourceFile *includingSourceFile, const std::string &includedPath) {
  ROSE_ASSERT(project != NULL);
  if (includingSourceFile == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-owner]: included=%s has no "
            "including source file\n",
            includedPath.c_str());
    ROSE_ABORT();
  }

  const std::string &normalizedIncludedPath = includedPath;
  const std::string normalizedIncludingPath =
      includingSourceFile->getFileName();
  if (normalizedIncludedPath.empty() || normalizedIncludingPath.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-owner]: included=%s including=%s "
            "has incomplete path identity\n",
            includedPath.c_str(), includingSourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  std::set<PreprocessingInfo *> candidates;
  map<string, set<PreprocessingInfo *>>::const_iterator mapIt =
      includingPreprocessingInfosMap.find(normalizedIncludedPath);
  if (mapIt != includingPreprocessingInfosMap.end()) {
    candidates.insert(mapIt->second.begin(), mapIt->second.end());
  }

  PreprocessingInfo *selected = nullptr;
  std::string selectedSpelling;
  bool selectedQuoted = false;
  PreprocessingInfo::DirectiveType selectedType =
      PreprocessingInfo::CpreprocessorUnknownDeclaration;
  for (PreprocessingInfo *preprocessingInfo : candidates) {
    if (preprocessingInfo == nullptr ||
        preprocessingInfo->get_file_info() == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-owner]: target=%s contains "
              "an incomplete Clang callback directive owner\n",
              normalizedIncludedPath.c_str());
      ROSE_ABORT();
    }
    const PreprocessingInfo::DirectiveType directiveType =
        preprocessingInfo->getTypeOfDirective();
    if (directiveType != PreprocessingInfo::CpreprocessorIncludeDeclaration &&
        directiveType !=
            PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-owner]: target=%s contains "
              "non-include Clang callback directive type=%d\n",
              normalizedIncludedPath.c_str(), static_cast<int>(directiveType));
      ROSE_ABORT();
    }
    if (preprocessingInfo->get_file_info()->get_physical_filename() !=
        normalizedIncludingPath) {
      continue;
    }
    IncludeDirective directive(preprocessingInfo->getString());
    const std::string spelling = directive.getIncludedPath();
    if (spelling.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-owner]: directive=%s from=%s "
              "has no literal include spelling\n",
              preprocessingInfo->getString().c_str(),
              normalizedIncludingPath.c_str());
      ROSE_ABORT();
    }
    if (selected == nullptr) {
      selected = preprocessingInfo;
      selectedSpelling = spelling;
      selectedQuoted = directive.isQuotedInclude();
      selectedType = directiveType;
      continue;
    }
    if (selectedSpelling != spelling ||
        selectedQuoted != directive.isQuotedInclude() ||
        selectedType != directiveType) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-owner]: including=%s reaches=%s "
              "through distinct directive spellings: %s and %s\n",
              normalizedIncludingPath.c_str(), normalizedIncludedPath.c_str(),
              selected->getString().c_str(),
              preprocessingInfo->getString().c_str());
      ROSE_ABORT();
    }
    Sg_File_Info *selectedInfo = selected->get_file_info();
    Sg_File_Info *candidateInfo = preprocessingInfo->get_file_info();
    if (candidateInfo->get_line() < selectedInfo->get_line() ||
        (candidateInfo->get_line() == selectedInfo->get_line() &&
         candidateInfo->get_col() < selectedInfo->get_col())) {
      selected = preprocessingInfo;
    }
  }

  if (selected == nullptr) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-owner]: including=%s has no "
            "exact Clang callback directive resolving to %s\n",
            normalizedIncludingPath.c_str(), normalizedIncludedPath.c_str());
    ROSE_ABORT();
  }
  return selected;
}

static SgIncludeFile *ensureSourceFileIncludeRoot(SgProject *project,
                                                  SgSourceFile *sourceFile) {
  ROSE_ASSERT(project != NULL);
  if (sourceFile == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-root]: cannot build a root for a "
            "null source file\n");
    ROSE_ABORT();
  }

  SgIncludeFile *includeRoot = sourceFile->get_associated_include_file();
  if (includeRoot != NULL) {
    return includeRoot;
  }

  if (sourceFile->get_isHeaderFile() == true) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-root]: header=%s lost its parent "
            "include node\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  const std::string sourcePath =
      FileHelper::normalizePathIfPossible(sourceFile->getFileName());
  if (sourcePath.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-root]: source=%p has no normalized "
            "filename\n",
            static_cast<void *>(sourceFile));
    ROSE_ABORT();
  }
  includeRoot = new SgIncludeFile(sourcePath);
  includeRoot->set_parent(sourceFile);
  includeRoot->set_filename(sourcePath);
  includeRoot->set_name_used_in_include_directive(
      FileHelper::getFileName(sourcePath));

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

static std::filesystem::path requireCanonicalSourcePath(const std::string &path,
                                                        const char *contract) {
  ASSERT_not_null(contract);
  if (path.empty()) {
    fprintf(stderr, "REX_FRONTEND_INVARIANT[%s]: source path is empty\n",
            contract);
    ROSE_ABORT();
  }

  std::error_code ec;
  const std::filesystem::path canonical = std::filesystem::weakly_canonical(
      std::filesystem::absolute(path, ec), ec);
  if (ec || canonical.empty() || !canonical.is_absolute()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[%s]: source path=%s has no exact "
            "canonical absolute identity: %s\n",
            contract, path.c_str(), ec.message().c_str());
    ROSE_ABORT();
  }
  return canonical;
}

static std::filesystem::path
commonAbsoluteDirectory(const std::filesystem::path &lhs,
                        const std::filesystem::path &rhs,
                        const char *contract) {
  ASSERT_not_null(contract);
  if (lhs.empty() || rhs.empty() || !lhs.is_absolute() || !rhs.is_absolute()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[%s]: cannot intersect non-absolute "
            "source roots lhs=%s rhs=%s\n",
            contract, lhs.string().c_str(), rhs.string().c_str());
    ROSE_ABORT();
  }

  std::filesystem::path shared;
  auto left = lhs.begin();
  auto right = rhs.begin();
  while (left != lhs.end() && right != rhs.end() && *left == *right) {
    shared /= *left;
    ++left;
    ++right;
  }
  if (shared.empty() || !shared.is_absolute()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[%s]: source roots lhs=%s rhs=%s have no "
            "exact common absolute directory\n",
            contract, lhs.string().c_str(), rhs.string().c_str());
    ROSE_ABORT();
  }
  return shared.lexically_normal();
}

static bool isWithinApplicationRoot(const std::filesystem::path &path,
                                    const std::filesystem::path &root) {
  if (path.empty() || root.empty() || !path.is_absolute() ||
      !root.is_absolute()) {
    return false;
  }
  const std::filesystem::path relative =
      path.lexically_relative(root).lexically_normal();
  if (relative.empty() || relative.is_absolute()) {
    return false;
  }
  for (const std::filesystem::path &component : relative) {
    if (component == "..")
      return false;
  }
  return true;
}

static void publishHeaderApplicationRoot(
    SgProject *project,
    const std::map<std::string, SgSourceFile *> &sourceFilesByPath) {
  ASSERT_not_null(project);
  std::filesystem::path applicationRoot = requireCanonicalSourcePath(
      project->get_applicationRootDirectory(), "application-root");
  const bool explicitRoot = project->get_usingApplicationRootDirectory();

  for (const auto &entry : sourceFilesByPath) {
    SgSourceFile *sourceFile = entry.second;
    ASSERT_not_null(sourceFile);
    SgIncludeFile *includeFile = sourceFile->get_associated_include_file();
    if (includeFile == nullptr) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[application-root]: materialized "
              "source=%s has no exact include owner\n",
              entry.first.c_str());
      ROSE_ABORT();
    }
    if (!includeFile->get_isApplicationFile())
      continue;

    const std::filesystem::path sourcePath =
        requireCanonicalSourcePath(entry.first, "application-root");
    const std::filesystem::path sourceDirectory = sourcePath.parent_path();
    if (sourceDirectory.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[application-root]: application "
              "source=%s has no exact parent directory\n",
              entry.first.c_str());
      ROSE_ABORT();
    }
    if (explicitRoot && !isWithinApplicationRoot(sourcePath, applicationRoot)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[application-root]: application "
              "source=%s escapes explicit root=%s\n",
              entry.first.c_str(), applicationRoot.string().c_str());
      ROSE_ABORT();
    }
    if (!explicitRoot) {
      applicationRoot = commonAbsoluteDirectory(
          applicationRoot, sourceDirectory, "application-root");
    }
  }

  project->set_applicationRootDirectory(applicationRoot.string());

  std::set<SgIncludeFile *> visited;
  std::vector<SgIncludeFile *> pending;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile != nullptr) {
      SgIncludeFile *root = sourceFile->get_associated_include_file();
      if (root == nullptr) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: CLI source=%s "
                "has no exact include root\n",
                sourceFile->getFileName().c_str());
        ROSE_ABORT();
      }
      pending.push_back(root);
    }
  }

  while (!pending.empty()) {
    SgIncludeFile *includeFile = pending.back();
    pending.pop_back();
    ASSERT_not_null(includeFile);
    if (!visited.insert(includeFile).second) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[application-root]: include=%p path=%s "
              "appears more than once in the exact include graph\n",
              static_cast<void *>(includeFile),
              includeFile->get_filename().str());
      ROSE_ABORT();
    }
    includeFile->set_applicationRootDirectory(applicationRoot.string());
    if (includeFile->get_isApplicationFile()) {
      const std::filesystem::path sourcePath = requireCanonicalSourcePath(
          includeFile->get_filename().str(), "application-root");
      if (!isWithinApplicationRoot(sourcePath, applicationRoot)) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: application "
                "include=%s escapes published root=%s\n",
                sourcePath.string().c_str(), applicationRoot.string().c_str());
        ROSE_ABORT();
      }
    }

    for (SgIncludeFile *child : includeFile->get_include_file_list()) {
      if (child == nullptr || child->get_parent_include_file() != includeFile) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: include=%p path=%s "
                "has a child without one exact parent edge\n",
                static_cast<void *>(includeFile),
                includeFile->get_filename().str());
        ROSE_ABORT();
      }
      pending.push_back(child);
    }
  }
}

static SgIncludeFile *
synthesizeIncludeFile(SgProject *project, SgSourceFile *includingSourceFile,
                      SgSourceFile *includedSourceFile,
                      const std::string &includedPath,
                      PreprocessingInfo *preprocessingInfo) {
  ROSE_ASSERT(project != NULL);
  if (includingSourceFile == NULL || includedSourceFile == NULL ||
      preprocessingInfo == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-node]: including=%p included=%p "
            "preprocessing=%p cannot synthesize incomplete include ownership\n",
            static_cast<void *>(includingSourceFile),
            static_cast<void *>(includedSourceFile),
            static_cast<void *>(preprocessingInfo));
    ROSE_ABORT();
  }

  SgIncludeFile *parentIncludeFile =
      includingSourceFile->get_associated_include_file();
  if (parentIncludeFile == NULL) {
    parentIncludeFile =
        ensureSourceFileIncludeRoot(project, includingSourceFile);
  }
  if (parentIncludeFile == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-node]: including=%s has no include "
            "tree parent\n",
            includingSourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  SgIncludeFile *includeFile =
      findDirectIncludeFileByPath(parentIncludeFile, includedPath);
  if (includeFile != NULL) {
    return includeFile;
  }

  if (includedPath.empty() || includedPath.find('\0') != std::string::npos ||
      !std::filesystem::path(includedPath).is_absolute()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-node]: included source=%s has no "
            "exact physical path\n",
            includedPath.c_str());
    ROSE_ABORT();
  }
  SgSourceFile *translationUnit =
      parentIncludeFile->get_source_file_of_translation_unit();
  if (translationUnit == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-node]: parent=%s has no owning "
            "translation unit\n",
            parentIncludeFile->get_filename().str());
    ROSE_ABORT();
  }
  const auto &paths = translationUnit->get_frontendIncludeOwnershipPathList();
  const auto &systems =
      translationUnit->get_frontendSystemIncludeOwnershipPathList();
  const auto &externals =
      translationUnit->get_frontendExternalOwnershipPathList();
  const bool textual =
      std::find(paths.begin(), paths.end(), includedPath) != paths.end();
  const bool isSystemInclude =
      std::find(systems.begin(), systems.end(), includedPath) != systems.end();
  const bool external = std::find(externals.begin(), externals.end(),
                                  includedPath) != externals.end();
  ROSE_ASSERT(textual || external);
  ROSE_ASSERT(!isSystemInclude || (textual && external));
  ROSE_ASSERT(isSystemInclude || !(textual && external));

  ASSERT_not_null(preprocessingInfo->get_file_info());
  IncludeDirective includeDirective(preprocessingInfo->getString());
  const std::string &nameUsedInIncludeDirective =
      includeDirective.getIncludedPath();
  includeFile = new SgIncludeFile(includedPath);
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
  includeFile->set_isApplicationFile(textual && !isSystemInclude);
  includeFile->set_parent_include_file(parentIncludeFile);
  includeFile->set_parent(parentIncludeFile);
  includeFile->set_source_file(includedSourceFile);

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
  auto requireWholeFileMapping = [&](SgNode *node) {
    ASSERT_not_null(node);
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::iterator mapIt =
        tokenMap.find(node);
    if (mapIt == tokenMap.end() || mapIt->second == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[header-whole-file-mapping]: file=%s "
              "node=%p/%s has no exact producer-owned token mapping\n",
              headerFile->getFileName().c_str(), static_cast<void *>(node),
              node->class_name().c_str());
      ROSE_ABORT();
    }
    const TokenStreamHalfOpenInterval &core = mapIt->second->halfOpenInterval(
        TokenStreamIntervalKind::token_subsequence);
    if (core.begin != 0 || core.end != static_cast<int>(tokenVector.size())) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[header-whole-file-mapping]: file=%s "
              "node=%p/%s interval=[%d,%d) does not own [0,%zu)\n",
              headerFile->getFileName().c_str(), static_cast<void *>(node),
              node->class_name().c_str(), core.begin, core.end,
              tokenVector.size());
      ROSE_ABORT();
    }
  };

  requireWholeFileMapping(headerFile);
  requireWholeFileMapping(headerFile->get_globalScope());
}

std::string SgValueExp::get_constant_folded_value_as_string() const {
  // Use the folded value rather than unparsing the expression tree.
  string s;
  const int max_buffer_size = 500;
  char buffer[max_buffer_size];
  auto floatingValueString = [&](auto value) {
    const int length =
        snprintf(buffer, max_buffer_size, "%.*g",
                 std::numeric_limits<decltype(value)>::max_digits10,
                 static_cast<double>(value));
    ROSE_ASSERT(length >= 0 && length < max_buffer_size);
    return std::string(buffer, static_cast<size_t>(length));
  };
  switch (variantT()) {
  case V_SgInteger128Val:
    return isSgInteger128Val(this)->get_value().decimal();

  case V_SgFortranBOZVal: {
    const auto *value = isSgFortranBOZVal(this);
    ROSE_ASSERT(isSgTypeFortranBOZ(value->get_type()) != nullptr);
    const auto words = value->get_bits().words();
    std::string spelling = "z\"";
    for (std::size_t word = words.size(); word > 0; --word) {
      for (int shift = 60; shift >= 0; shift -= 4)
        spelling.push_back("0123456789abcdef"[(words[word - 1] >> shift) & 15]);
    }
    spelling += '"';
    return spelling;
  }

  case V_SgIntVal: {
    const SgIntVal *integerValueExpression = isSgIntVal(this);
    ASSERT_not_null(integerValueExpression);
    int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%d", numericValue);
    s = buffer;
    break;
  }

  case V_SgLongIntVal: {
    const SgLongIntVal *integerValueExpression = isSgLongIntVal(this);
    ASSERT_not_null(integerValueExpression);
    long int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%ld", numericValue);
    s = buffer;
    break;
  }

  case V_SgLongLongIntVal: {
    const SgLongLongIntVal *integerValueExpression = isSgLongLongIntVal(this);
    ASSERT_not_null(integerValueExpression);
    long long int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%lld", numericValue);
    s = buffer;
    break;
  }

  case V_SgShortVal: {
    const SgShortVal *integerValueExpression = isSgShortVal(this);
    ASSERT_not_null(integerValueExpression);
    short int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%d", numericValue);
    s = buffer;
    break;
  }

  case V_SgUnsignedShortVal: {
    const SgUnsignedShortVal *integerValueExpression =
        isSgUnsignedShortVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned short int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%u", numericValue);
    s = buffer;
    break;
  }

  case V_SgUnsignedLongLongIntVal: {
    const SgUnsignedLongLongIntVal *integerValueExpression =
        isSgUnsignedLongLongIntVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned long long int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%llu", numericValue);
    s = buffer;
    break;
  }

  case V_SgUnsignedLongVal: {
    const SgUnsignedLongVal *integerValueExpression = isSgUnsignedLongVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned long int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%lu", numericValue);
    s = buffer;
    break;
  }

  case V_SgUnsignedIntVal: {
    const SgUnsignedIntVal *integerValueExpression = isSgUnsignedIntVal(this);
    ASSERT_not_null(integerValueExpression);
    unsigned int numericValue = integerValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%u", numericValue);
    s = buffer;
    break;
  }

  case V_SgBoolValExp: {
    const SgBoolValExp *booleanValueExpression = isSgBoolValExp(this);
    ASSERT_not_null(booleanValueExpression);
    bool booleanValue = booleanValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%s",
             booleanValue == true ? "true" : "false");
    s = buffer;
    break;
  }

  case V_SgStringVal: {
    const SgStringVal *stringValueExpression = isSgStringVal(this);
    ASSERT_not_null(stringValueExpression);
    s = stringValueExpression->get_stringDelimiter() != 0
            ? stringValueExpression->get_byte_value()
            : stringValueExpression->get_cxx_literal_contents();
    break;
  }

  case V_SgCharVal:
  case V_SgWcharVal: {
    const int64_t value = variantT() == V_SgCharVal
                              ? isSgCharVal(this)->get_value()
                              : isSgWcharVal(this)->get_value();
    if (value >= 0 && value <= std::numeric_limits<unsigned char>::max() &&
        isalnum(static_cast<unsigned char>(value))) {
      s = std::string(1, static_cast<char>(value));
    } else {
      s = std::to_string(value);
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

  case V_SgChar16Val:
    return std::to_string(isSgChar16Val(this)->get_value());
  case V_SgChar32Val:
    return std::to_string(isSgChar32Val(this)->get_value());
  case V_SgFloat16Val:
    return floatingValueString(isSgFloat16Val(this)->get_value());
  case V_SgBFloat16Val:
    return floatingValueString(isSgBFloat16Val(this)->get_value());
  case V_SgFloat32Val:
    return floatingValueString(isSgFloat32Val(this)->get_value());
  case V_SgFloat64Val:
    return floatingValueString(isSgFloat64Val(this)->get_value());
  case V_SgFloat80Val:
    return isSgFloat80Val(this)->get_value().decimal();

  case V_SgFloatVal: {
    const SgFloatVal *floatValueExpression = isSgFloatVal(this);
    ASSERT_not_null(floatValueExpression);
    float numericValue = floatValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%f", numericValue);
    s = buffer;
    break;
  }

  case V_SgDoubleVal: {
    const SgDoubleVal *floatValueExpression = isSgDoubleVal(this);
    ASSERT_not_null(floatValueExpression);
    double numericValue = floatValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%lf", numericValue);
    s = buffer;
    break;
  }

  case V_SgLongDoubleVal: {
    const SgLongDoubleVal *floatValueExpression = isSgLongDoubleVal(this);
    ASSERT_not_null(floatValueExpression);
    s = floatValueExpression->get_value().decimal();
    break;
  }

  case V_SgFloat128Val: {
    const SgFloat128Val *floatValueExpression = isSgFloat128Val(this);
    ASSERT_not_null(floatValueExpression);
    s = floatValueExpression->get_value().decimal();
    break;
  }

  case V_SgEnumVal: {
    const SgEnumVal *enumValueExpression = isSgEnumVal(this);
    ASSERT_not_null(enumValueExpression);
    int numericValue = enumValueExpression->get_value();
    snprintf(buffer, max_buffer_size, "%d", numericValue);
    s = string("_enum_") + string(buffer);
    break;
  }

  case V_SgComplexVal: {
    const SgComplexVal *complexValueExpression = isSgComplexVal(this);
    ASSERT_not_null(complexValueExpression);

    auto requireFoldedComponent = [](SgExpression *component,
                                     const char *role) -> std::string {
      SgValueExp *value = isSgValueExp(component);
      if (value == nullptr) {
        fprintf(stderr,
                "REX_AST_INVARIANT[complex-folded-value]: %s component=%p/%s "
                "is a symbolic expression and has no folded numeric value\n",
                role, static_cast<void *>(component),
                component != nullptr ? component->class_name().c_str()
                                     : "<null>");
        ROSE_ABORT();
      }
      return value->get_constant_folded_value_as_string();
    };

    string real_string = "null";
    if (complexValueExpression->get_real_value() != nullptr)
      real_string = requireFoldedComponent(
          complexValueExpression->get_real_value(), "real");

    string imaginary_string = "null";
    if (complexValueExpression->get_imaginary_value() != nullptr)
      imaginary_string = requireFoldedComponent(
          complexValueExpression->get_imaginary_value(), "imaginary");

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

  default: {
    printf("Error SgValueExp::get_constant_folded_value_as_string(): case of "
           "value = %s not handled \n",
           this->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return s;
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

void SgFile::unparse(UnparseFormatHelp *formatHelp, UnparseDelegate *delegate) {
  unparseFile(this, formatHelp, delegate);
}

void SgFile::initializeSourcePosition(const std::string &sourceFilename) {
  Sg_File_Info *fileInfo = new Sg_File_Info(sourceFilename, 1, 1);
  ASSERT_not_null(fileInfo);

  set_startOfConstruct(fileInfo);
  fileInfo->set_parent(this);
  ASSERT_not_null(get_startOfConstruct());
  ASSERT_not_null(get_file_info());
}

void SgSourceFile::initializeGlobalScope() {
  ASSERT_not_null(get_startOfConstruct());

  Sg_File_Info *sourceInfo = get_file_info();
  if (sourceInfo == nullptr || sourceInfo->get_physical_file_id() < 0 ||
      sourceInfo->isShared()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[global-output-owner-producer]: source "
            "file=%p has no exact physical identity before global-scope "
            "construction\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }
  const string sourceFilename = sourceInfo->get_physical_filename();
  if (sourceFilename.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[global-output-owner-producer]: source "
            "file=%p has no registered physical filename\n",
            static_cast<void *>(this));
    ROSE_ABORT();
  }

  Sg_File_Info *globalScopeFileInfo = new Sg_File_Info(sourceFilename, 1, 1);
  ASSERT_not_null(globalScopeFileInfo);
  globalScopeFileInfo->setOutputInCodeGeneration();

  set_globalScope(new SgGlobal(globalScopeFileInfo));
  ASSERT_not_null(get_globalScope());

  if (SageInterface::is_language_case_insensitive()) {
    ROSE_ASSERT(SageBuilder::symbol_table_case_insensitive_semantics == true);
    get_globalScope()->setCaseInsensitive(true);
  }

  get_globalScope()->set_parent(this);

  ROSE_ASSERT(get_globalScope()->get_endOfConstruct() == nullptr);
  get_globalScope()->set_endOfConstruct(new Sg_File_Info(sourceFilename, 1, 1));
  ASSERT_not_null(get_globalScope()->get_endOfConstruct());
  get_globalScope()->get_endOfConstruct()->set_parent(get_globalScope());
  get_globalScope()->get_endOfConstruct()->setOutputInCodeGeneration();
  get_globalScope()->setOutputInCodeGeneration();

  ROSE_ASSERT(get_globalScope() != NULL);
  ROSE_ASSERT(get_globalScope()->get_startOfConstruct() != NULL);

  ROSE_ASSERT(get_globalScope()->get_endOfConstruct() != NULL);

  Sg_File_Info *globalStart = get_globalScope()->get_startOfConstruct();
  Sg_File_Info *globalEnd = get_globalScope()->get_endOfConstruct();
  if (globalStart == nullptr || globalEnd == nullptr ||
      globalStart->get_parent() != get_globalScope() ||
      globalEnd->get_parent() != get_globalScope() ||
      globalStart->get_physical_file_id() !=
          sourceInfo->get_physical_file_id() ||
      globalEnd->get_physical_file_id() != sourceInfo->get_physical_file_id() ||
      globalStart->get_line() != 1 || globalEnd->get_line() != 1 ||
      !globalStart->isOutputInCodeGeneration() ||
      !globalEnd->isOutputInCodeGeneration() ||
      !get_globalScope()->isOutputInCodeGeneration()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[global-output-owner-producer]: source "
            "file=%p global=%p failed one atomic exact physical owner "
            "publication\n",
            static_cast<void *>(this), static_cast<void *>(get_globalScope()));
    ROSE_ABORT();
  }
}

SgSourceFile *determineFileType(vector<string> argv, SgProject *project,
                                SgFileList *owner,
                                SgSourceFile::source_origin_enum origin) {
  ASSERT_not_null(owner);
  SgSourceFile *file = nullptr;

  ROSE_ASSERT(project != NULL);
  Rose_STL_Container<string> fileList =
      CommandlineProcessing::generateSourceFilenames(argv);

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

    const string sourceOperand = *(fileList.begin());
    ROSE_ASSERT(sourceOperand.find('\0') == string::npos);
    const CommandlineProcessing::ClangLanguageSelection clangLanguage =
        CommandlineProcessing::clangLanguageSelectionForSource(argv,
                                                               sourceOperand);
    const bool suffixSelected =
        clangLanguage.family ==
        CommandlineProcessing::ClangLanguageFamily::Suffix;
    const bool cSelected =
        clangLanguage.family == CommandlineProcessing::ClangLanguageFamily::C;
    const bool cxxSelected =
        clangLanguage.family == CommandlineProcessing::ClangLanguageFamily::Cxx;
    const bool cudaSelected = clangLanguage.family ==
                              CommandlineProcessing::ClangLanguageFamily::Cuda;
    const bool openclSelected =
        clangLanguage.family ==
        CommandlineProcessing::ClangLanguageFamily::OpenCL;

    if (!suffixSelected && commandLineSelectsFortran(argv)) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[input-language]: source '%s' has both "
              "an explicit Clang -x language and an explicit Fortran "
              "language selection\n",
              sourceOperand.c_str());
      ROSE_ABORT();
    }

    const string sourceFilename =
        origin == SgSourceFile::e_generated_source
            ? sourceOperand
            : std::filesystem::canonical(sourceOperand).string();
    const string filenameExtension =
        StringUtility::fileNameSuffix(sourceOperand);
    file = new SgSourceFile(argv, project, owner, sourceFilename, origin);

    if (suffixSelected &&
        CommandlineProcessing::isFortranFileNameSuffix(filenameExtension)) {
      file->set_sourceFileUsesFortranFileExtension(true);

      file->set_outputLanguage(SgFile::e_Fortran_language);

      file->set_inputLanguage(SgFile::e_Fortran_language);

      file->set_Fortran_only(true);

      Rose::is_Fortran_language = true;

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

      // The global scope uses the selected preprocessing source position.
      file->initializeGlobalScope();

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

        file->set_F2003_only();
        file->set_CoArrayFortran_only(true);
      }

      if (CommandlineProcessing::isFortran2008FileNameSuffix(
              filenameExtension) == true) {
        file->set_sourceFileUsesFortran2008FileExtension(true);

        // Use the filename suffix as a default means to set this value
        file->set_outputFormat(SgFile::e_free_form_output_format);
        file->set_backendCompileFormat(SgFile::e_free_form_output_format);

        file->set_F2008_only();
      }
    } else {
      SageBuilder::symbol_table_case_insensitive_semantics = false;

      {

        if (cxxSelected ||
            (suffixSelected &&
             CommandlineProcessing::isCppFileNameSuffix(filenameExtension))) {
          file->set_sourceFileUsesCppFileExtension(true);

          file->set_outputLanguage(SgFile::e_Cxx_language);
          file->set_inputLanguage(SgFile::e_Cxx_language);

          file->set_Cxx_only(true);

          Rose::is_Cxx_language = true;

          ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
          file->initializeGlobalScope();
        } else {
          if (cSelected ||
              (suffixSelected &&
               (CommandlineProcessing::isCFileNameSuffix(filenameExtension) ||
                CommandlineProcessing::isAssemblerFileNameSuffix(
                    filenameExtension)))) {
            file->set_sourceFileUsesCppFileExtension(false);

            file->set_outputLanguage(SgFile::e_C_language);
            file->set_inputLanguage(SgFile::e_C_language);

            file->set_C_only(true);

            Rose::is_C_language = true;

            // Default C standard is handled in cmdline.C; avoid forcing GNU
            // here.

            ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
            file->initializeGlobalScope();
          } else {
            if (cudaSelected ||
                (suffixSelected && CommandlineProcessing::isCudaFileNameSuffix(
                                       filenameExtension))) {
              file->set_outputLanguage(SgFile::e_Cxx_language);

              file->set_inputLanguage(SgFile::e_Cxx_language);

              file->set_Cuda_only(true);

              Rose::is_Cuda_language = true;

              ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
              file->initializeGlobalScope();
            } else if (openclSelected ||
                       (suffixSelected &&
                        CommandlineProcessing::isOpenCLFileNameSuffix(
                            filenameExtension))) {
              file->set_outputLanguage(SgFile::e_C_language);
              file->set_inputLanguage(SgFile::e_C_language);
              file->set_OpenCL_only(true);

              Rose::is_OpenCL_language = true;

              ROSE_ASSERT(file->get_requires_C_preprocessor() == false);
              file->initializeGlobalScope();
            } else if (suffixSelected && commandLineSelectsFortran(argv)) {
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

              file->initializeGlobalScope();
            } else {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[unsupported-input-language]: "
                      "file=%s has no supported input language\n",
                      sourceFilename.c_str());
              ROSE_ABORT();
            }
          }
        }
      }

      file->set_sourceFileUsesFortranFileExtension(false);
    }
  } else {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[missing-source-file]: command line has "
            "no source file\n");
    ROSE_ABORT();
  }

  ASSERT_not_null(file);

  ASSERT_not_null(file->get_parent());
  ASSERT_not_null(isSgFileList(file->get_parent()));
  ROSE_ASSERT(file->get_parent() == owner);

  if (file->get_preprocessorDirectivesAndCommentsList() == nullptr) {
    file->set_preprocessorDirectivesAndCommentsList(
        new ROSEAttributesListContainer());
  }
  ASSERT_not_null(file->get_preprocessorDirectivesAndCommentsList());

  return file;
}

void SgFile::runFrontend(int &nextErrorCode) {
  ROSE_ASSERT(get_parent() != NULL);

  // A frontend invocation is an AST mutation boundary.  Names cached by an
  // analysis of the enclosing project are valid, but they cannot survive a
  // nested producer that may add or replace declarations.
  SgNode::clearGlobalMangledNameMap();

  // The frontend runs after the constructor has applied the file options.
  nextErrorCode = this->callFrontEnd();
  this->set_frontendErrorCode(nextErrorCode);
  // Mangled names are derived from declaration structure. A frontend call may
  // build or replace that structure, so its cache lifetime ends at this
  // producer boundary regardless of whether diagnostics stopped translation.
  SgNode::clearGlobalMangledNameMap();
  // Frontends are allowed to return a real diagnostic count for hard errors.
  // SgProject::parse/frontendExitStatus interpret those nonzero statuses; do
  // not treat them as an internal invariant violation here.
}

int SgProject::parse(const vector<string> &argv) {
  SgNode::SourceStateConstruction construction;
  TimingPerformance timer("AST (SgProject::parse(argc,argv)):");

  // Builds the file list (or none if this is a link line). Signals and hard
  // failures propagate directly; there is no recovery boundary.
  processCommandLine(argv);

  int errorCode = 0;

  if (get_sourceFileNameList().size() > 0) {
    rosePhaseTrace("project.parse.files.begin");
    errorCode = parse();
    rosePhaseTrace("project.parse.files.end");
  }

  // These static interning tables outlive individual files and projects.
  // A file parent would falsely claim ownership of their shared types.
  ROSE_ASSERT(SgNode::get_globalFunctionTypeTable()->get_parent() == nullptr);
  ROSE_ASSERT(SgNode::get_globalTypeTable()->get_parent() == nullptr);
  return errorCode;
}

SgSourceFile::SgSourceFile(vector<string> &argv, SgProject *project)
    : SgSourceFile() {
  ASSERT_not_null(project);
  doSetupForConstructor(argv, project, project->get_fileList_ptr());
}

SgSourceFile::SgSourceFile(vector<string> &argv, SgProject *project,
                           SgFileList *owner)
    : SgSourceFile() {
  doSetupForConstructor(argv, project, owner);
}

SgSourceFile::SgSourceFile(vector<string> &argv, SgProject *project,
                           SgFileList *owner, const string &sourceFilename,
                           source_origin_enum origin)
    : SgSourceFile(origin == e_generated_source,
                   origin == e_command_line_source) {
  ROSE_ASSERT(origin == e_parsed_source || origin == e_command_line_source ||
              origin == e_generated_source);
  doSetupForConstructor(argv, project, owner, sourceFilename);
}

int SgSourceFile::callFrontEnd() {
  int frontendErrorLevel = SgFile::callFrontEnd();
  if (get_Fortran_only() && get_requires_C_preprocessor()) {
    ASSERT_not_null(get_globalScope());
    ASSERT_not_null(get_globalScope()->get_startOfConstruct());
    ASSERT_not_null(get_globalScope()->get_endOfConstruct());
    Sg_File_Info *sourceInfo = get_file_info();
    Sg_File_Info *globalStart = get_globalScope()->get_startOfConstruct();
    Sg_File_Info *globalEnd = get_globalScope()->get_endOfConstruct();
    if (sourceInfo == nullptr || sourceInfo->get_physical_file_id() < 0 ||
        globalStart->get_physical_file_id() !=
            sourceInfo->get_physical_file_id() ||
        globalEnd->get_physical_file_id() !=
            sourceInfo->get_physical_file_id() ||
        globalStart->get_filenameString() != sourceInfo->get_filenameString() ||
        globalEnd->get_filenameString() != sourceInfo->get_filenameString()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[global-output-owner-producer]: "
              "preprocessed Flang source file=%p reached frontend completion "
              "without its original exact physical global owner\n",
              static_cast<void *>(this));
      ROSE_ABORT();
    }
  }
  ASSERT_not_null(get_globalScope());
  ASSERT_not_null(get_globalScope()->get_file_info());
  ROSE_ASSERT(
      get_globalScope()->get_file_info()->get_filenameString().empty() ==
      false);
  ASSERT_not_null(get_globalScope()->get_startOfConstruct());
  ASSERT_not_null(get_globalScope()->get_endOfConstruct());

  return frontendErrorLevel;
}

int SgProject::RunFrontend() {
  TimingPerformance timer("AST (SgProject::RunFrontend()):");
  return Rose::Frontend::Run(this);
}

int SgProject::parse() {
  SgNode::SourceStateConstruction construction;
  int errorCode = 0;

  TimingPerformance timer("AST (SgProject::parse()):");

#if defined(ROSE_FLANG_FRONTEND)
  FlangModuleInfo moduleSession(this);
#endif

  // Construct every file before running any frontend so one input can
  // reference declarations from another.
  Rose_STL_Container<string>::iterator nameIterator =
      p_sourceFileNameList.begin();
  std::vector<SgFile *> vectorOfFiles;
  while (nameIterator != p_sourceFileNameList.end()) {
    string currentFileName = *nameIterator;
    vector<string> argv = CommandlineProcessing::sliceCommandLineForSource(
        get_originalCommandLineArgumentList(), p_sourceFileNameList,
        currentFileName);
    SgSourceFile *newFile = determineFileType(
        argv, this, get_fileList_ptr(), SgSourceFile::e_command_line_source);
    ASSERT_not_null(newFile);
    ASSERT_not_null(newFile->get_startOfConstruct());
    ASSERT_not_null(newFile->get_parent());

    ASSERT_not_null(isSgFileList(newFile->get_parent()));
    ROSE_ASSERT(newFile->get_parent() == get_fileList_ptr());

    // This just adds the new file to the list of files stored internally (note:
    // this sets the parent of the newFile).
    set_file(*newFile);

    ASSERT_not_null(newFile->get_parent());

    // This list of files will be iterated over to call the frontend in the next
    // loop.
    vectorOfFiles.push_back(newFile);
    nameIterator++;
  }

  // Header/source output paths require one project-level anchor before the
  // frontend constructs any include graph.  An explicit command-line root is
  // authoritative; otherwise derive the exact common parent of every CLI
  // input.  Publishing that typed project contract here keeps path selection
  // out of the backend and makes multi-input behavior deterministic.
  if (get_applicationRootDirectory().empty() && !vectorOfFiles.empty()) {
    std::filesystem::path commonParent;
    bool firstInput = true;
    for (SgFile *file : vectorOfFiles) {
      ASSERT_not_null(file);
      const std::string &inputName = file->getFileName();
      if (inputName.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: project input has "
                "no filename\n");
        ROSE_ABORT();
      }

      std::error_code ec;
      std::filesystem::path inputPath = std::filesystem::weakly_canonical(
          std::filesystem::absolute(inputName, ec), ec);
      if (ec || inputPath.empty() || !inputPath.is_absolute() ||
          inputPath.filename().empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: input=%s cannot "
                "provide one exact normalized parent: %s\n",
                inputName.c_str(), ec.message().c_str());
        ROSE_ABORT();
      }
      const std::filesystem::path parent = inputPath.parent_path();
      if (parent.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: input=%s has no "
                "normalized parent directory\n",
                inputName.c_str());
        ROSE_ABORT();
      }

      if (firstInput) {
        commonParent = parent;
        firstInput = false;
        continue;
      }

      std::filesystem::path shared;
      auto lhs = commonParent.begin();
      auto rhs = parent.begin();
      while (lhs != commonParent.end() && rhs != parent.end() && *lhs == *rhs) {
        shared /= *lhs;
        ++lhs;
        ++rhs;
      }
      if (shared.empty() || !shared.is_absolute()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[application-root]: inputs do not "
                "share one exact absolute parent\n");
        ROSE_ABORT();
      }
      commonParent = shared;
    }
    if (firstInput || commonParent.empty() || !commonParent.is_absolute()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[application-root]: project did not "
              "derive one exact input anchor\n");
      ROSE_ABORT();
    }
    set_applicationRootDirectory(commonParent.string());
    set_usingApplicationRootDirectory(false);
  }

  errorCode = this->RunFrontend();
  if (errorCode != 0) {
    return errorCode;
  }

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

  if (!get_fileList().empty() && !get_useBackendOnly()) {
    rosePhaseTrace("AstPostProcessing.begin");
    AstPostProcessing(this);
    rosePhaseTrace("AstPostProcessing.end");
  }

  bool foundSourceFileMode = false;
  bool unparse_using_tokens = false;
  bool unparse_header_files = false;
  for (SgFile *file : get_fileList()) {
    ASSERT_not_null(file);
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile == nullptr || sourceFile->get_skip_unparse()) {
      continue;
    }
    if (!foundSourceFileMode) {
      foundSourceFileMode = true;
      unparse_using_tokens = sourceFile->get_unparse_tokens();
      unparse_header_files = sourceFile->get_unparseHeaderFiles();
      continue;
    }
    if (sourceFile->get_unparse_tokens() != unparse_using_tokens ||
        sourceFile->get_unparseHeaderFiles() != unparse_header_files) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[project-unparse-mode]: source=%s token="
              "%d headers=%d disagrees with project token=%d headers=%d\n",
              sourceFile->getFileName().c_str(),
              sourceFile->get_unparse_tokens() ? 1 : 0,
              sourceFile->get_unparseHeaderFiles() ? 1 : 0,
              unparse_using_tokens ? 1 : 0, unparse_header_files ? 1 : 0);
      ROSE_ABORT();
    }
  }

  // GB (9/4/2009): Moved the secondary pass over source files (which
  // attaches the preprocessing information) to this point. This way, the
  // secondary pass over each file runs after all fixes have been done. This
  // is relevant where the AstPostProcessing mechanism must first mark nodes
  // to be output before preprocessing information is attached.
  SgFilePtrList &files = get_fileList();
  const size_t inputFileCount = files.size();
  for (size_t index = 0; index < inputFileCount; ++index) {
    ASSERT_not_null(files[index]);
    rosePhaseTrace("secondaryPassOverSourceFile.begin");
    files[index]->secondaryPassOverSourceFile();
    rosePhaseTrace("secondaryPassOverSourceFile.end");
    ASSERT_not_null(files[index]);
    ROSE_ASSERT(
        files[index]->get_header_file_unparsing_optimization_header_file() ==
        false);
  }

  // Header unparsing consumes the exact active include graph that the Clang
  // frontend published with its preprocessing records.  A second compiler
  // invocation cannot reconstruct this graph: forced includes, macro state,
  // modules, and resource headers are part of the original frontend session.
  if (unparse_header_files) {
    IncludingPreprocessingInfosCollector includingPreprocessingInfosCollector(
        this);
    const map<string, set<PreprocessingInfo *>>
        &includingPreprocessingInfosMap =
            includingPreprocessingInfosCollector.collect();
    const map<string, set<string>> &includedFilesMap =
        includingPreprocessingInfosCollector.getIncludedFilesMap();
    set_includingPreprocessingInfosMap(includingPreprocessingInfosMap);

    std::map<std::string, SgSourceFile *> sourceFilesByPath;
    for (SgFile *file : get_fileList()) {
      SgSourceFile *sourceFile = isSgSourceFile(file);
      if (sourceFile == NULL) {
        continue;
      }
      std::string normalizedPath =
          FileHelper::normalizePathIfPossible(sourceFile->getFileName());
      if (normalizedPath.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[source-path]: source=%p has no "
                "normalized filename\n",
                static_cast<void *>(sourceFile));
        ROSE_ABORT();
      }
      insertFrontendSourceFile(sourceFilesByPath, normalizedPath, sourceFile,
                               "project-files");
      // A translation unit owns its include-tree root independently of
      // whether Clang observed any active outgoing include edges.  Do not
      // make root identity conditional on membership in includedFilesMap:
      // zero-edge translation units are still exact project-owned sources.
      ensureSourceFileIncludeRoot(this, sourceFile);
    }
    for (SgFile *file : get_fileList()) {
      SgSourceFile *sourceFile = isSgSourceFile(file);
      if (sourceFile == nullptr) {
        continue;
      }
      inventoryMaterializedIncludeSources(
          this, sourceFile->get_associated_include_file(), sourceFilesByPath);
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
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[token-map]: token inventory contains "
                  "a null source-file key\n");
          ROSE_ABORT();
        }
        if (sourceFile->get_project() != this) {
          continue;
        }
        std::string normalizedPath =
            FileHelper::normalizePathIfPossible(sourceFile->getFileName());
        if (normalizedPath.empty()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[token-map]: source=%p has no "
                  "normalized filename\n",
                  static_cast<void *>(sourceFile));
          ROSE_ABORT();
        }
        insertFrontendSourceFile(tokenMapFilesByPath, normalizedPath,
                                 sourceFile, "token-map");
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
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[header-path]: resolved include "
                    "does not exist: %s\n",
                    includedPath.c_str());
            ROSE_ABORT();
          }
          snapshotOriginalHeader(this, includedPath);

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
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[header-materialization]: failed "
                    "to build SgSourceFile for %s\n",
                    includedPath.c_str());
            ROSE_ABORT();
          }

          headerFile->set_isHeaderFile(true);
          headerFile->set_unparseHeaderFiles(
              traversalRoot->get_unparseHeaderFiles());
          headerFile->set_unparse_tokens(traversalRoot->get_unparse_tokens());
          copyLanguageSettings(headerFile, traversalRoot);

          if (insertFrontendSourceFile(sourceFilesByPath, includedPath,
                                       headerFile, "included-files")) {
            discoveredNewSourceFile = true;
          }
          if (unparse_using_tokens == true) {
            insertFrontendSourceFile(tokenMapFilesByPath, includedPath,
                                     headerFile, "included-token-map");
          }

          SgIncludeFile *includeFile = findDirectIncludeFileByPath(
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
          if (includeFile == NULL) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[include-node]: failed to build "
                    "SgIncludeFile for %s\n",
                    includedPath.c_str());
            ROSE_ABORT();
          }

          if (includeFile->get_source_file() == NULL) {
            includeFile->set_source_file(headerFile);
          } else if (includeFile->get_source_file() != headerFile) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[header-context]: header=%s has "
                    "distinct materialized source nodes %p and %p\n",
                    includedPath.c_str(),
                    static_cast<void *>(includeFile->get_source_file()),
                    static_cast<void *>(headerFile));
            ROSE_ABORT();
          }
          SgIncludeFile *associatedIncludeFile =
              headerFile->get_associated_include_file();
          if (associatedIncludeFile == NULL) {
            headerFile->set_associated_include_file(includeFile);
          } else {
            const std::string associatedPath =
                FileHelper::normalizePathIfPossible(
                    associatedIncludeFile->get_filename());
            if (associatedPath != includedPath ||
                associatedIncludeFile->get_source_file() != headerFile) {
              fprintf(
                  stderr,
                  "REX_FRONTEND_INVARIANT[header-context]: header=%s "
                  "canonical include=%p has path=%s source=%p, but direct "
                  "include=%p has source=%p\n",
                  includedPath.c_str(),
                  static_cast<void *>(associatedIncludeFile),
                  associatedPath.c_str(),
                  static_cast<void *>(associatedIncludeFile->get_source_file()),
                  static_cast<void *>(includeFile),
                  static_cast<void *>(includeFile->get_source_file()));
              ROSE_ABORT();
            }
          }
          SgIncludeFile *canonicalIncludeFile =
              headerFile->get_associated_include_file();
          if (canonicalIncludeFile == nullptr ||
              canonicalIncludeFile->get_source_file_of_translation_unit() ==
                  nullptr ||
              includeFile->get_source_file_of_translation_unit() == nullptr) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[header-context]: header=%s has "
                    "no exact translation-unit owner before preprocessing\n",
                    includedPath.c_str());
            ROSE_ABORT();
          }

          SgGlobal *emissionGlobal =
              findHeaderEmissionGlobalScope(traversalRoot, includedPath);
          if (emissionGlobal != nullptr) {
            SgGlobal *materializedGlobal = headerFile->get_globalScope();
            if (materializedGlobal != nullptr &&
                materializedGlobal != emissionGlobal &&
                !materializedGlobal->get_declarations().empty()) {
              fprintf(stderr,
                      "REX_FRONTEND_INVARIANT[header-context]: header=%s has "
                      "both a materialized global %p and emission global %p\n",
                      includedPath.c_str(),
                      static_cast<void *>(materializedGlobal),
                      static_cast<void *>(emissionGlobal));
              ROSE_ABORT();
            }
            headerFile->set_globalScope(emissionGlobal);
          } else if (headerFile->get_globalScope() == nullptr) {
            headerFile->initializeGlobalScope();
          }

          // Header preprocessing/token ownership is part of frontend
          // completion.  The backend must never attach comments, change file
          // modes, or manufacture mappings after user transformations.
          headerFile->set_header_file_unparsing_optimization_header_file(true);
          if (!headerFile->get_processedToIncludeCppDirectivesAndComments()) {
            headerFile->secondaryPassOverSourceFile();
          }
          if (!headerFile->get_processedToIncludeCppDirectivesAndComments()) {
            fprintf(stderr,
                    "REX_FRONTEND_INVARIANT[header-preprocessing]: failed to "
                    "finalize %s\n",
                    includedPath.c_str());
            ROSE_ABORT();
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

    for (const auto &entry : includedFilesMap) {
      const std::string includingPath =
          FileHelper::normalizePathIfPossible(entry.first);
      if (includingPath.empty() ||
          sourceFilesByPath.find(includingPath) == sourceFilesByPath.end() ||
          processedIncludingFiles.find(includingPath) ==
              processedIncludingFiles.end()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[include-closure]: including file=%s "
                "was not materialized and finalized by the frontend\n",
                entry.first.c_str());
        ROSE_ABORT();
      }
    }

    // The command-line input parents are only a provisional application root.
    // Once the frontend has materialized the exact include graph, expand an
    // implicit root to cover every non-system source. This preserves valid
    // parent-relative include spellings in the output tree. An explicitly
    // requested root remains authoritative and any application include that
    // escapes it is rejected here, before backend output planning.
    publishHeaderApplicationRoot(this, sourceFilesByPath);

    if (SgProject::get_verbose() >= 1) {
      printf("\nOutput info for unparse headers support: \n");
      cout << "\nIncluded files map:" << endl;
      for (const auto &[file, includedFiles] : includedFilesMap) {
        cout << "\nFile:" << file << endl;
        for (const auto &included : includedFiles)
          cout << "Included file:" << included << endl;
      }
      cout << "\n\nIncluding files map:" << endl;
      for (const auto &[file, directives] :
           get_includingPreprocessingInfosMap()) {
        cout << "\nFile:" << file << endl;
        for (const auto *directive : directives)
          cout << "Including file:"
               << directive->get_file_info()->get_filenameString() << endl;
      }
      cout << endl;
    }
  }

  if (SgProject::get_verbose() >= 1) {
    cout << "Sources parsed. AST generated." << endl;
  }

  if (get_verbose() > 3) {
    printf("In SgProject::parse() (verbose mode ON): \n");
    display("In SgProject::parse()");
  }
  return errorCode;
}

void SgFile::doSetupForConstructor(const vector<string> &argv,
                                   SgProject *project, SgFileList *owner) {
  ASSERT_not_null(project);
  const auto files = CommandlineProcessing::generateSourceFilenames(argv);
  ROSE_ASSERT(files.size() == 1);
  ROSE_ASSERT(files.front().find('\0') == string::npos);
  doSetupForConstructor(argv, project, owner,
                        std::filesystem::canonical(files.front()).string());
}

void SgFile::doSetupForConstructor(const vector<string> &argv,
                                   SgProject *project, SgFileList *owner,
                                   const string &sourceFilename) {
  ASSERT_not_null(project);
  ASSERT_not_null(owner);
  ROSE_ASSERT(argv.size() > 1);
  for (const auto &argument : argv)
    ROSE_ASSERT(argument.find('\0') == string::npos);
  ROSE_ASSERT(!sourceFilename.empty() &&
              sourceFilename.find('\0') == string::npos &&
              std::filesystem::path(sourceFilename).is_absolute());
  preventConstructionOnStack(this);

  ROSE_ASSERT(get_parent() == nullptr);
  set_parent(owner);

  initialization();

  if (project->get_compileOnly()) {
    set_compileOnly(true);
  }

  ASSERT_not_null(get_parent());

  set_sourceFileNameWithPath(sourceFilename);

  set_sourceFileNameWithoutPath(StringUtility::stripPathFromFileName(
      get_sourceFileNameWithPath().c_str()));

  initializeSourcePosition(sourceFilename);
  ASSERT_not_null(get_file_info());

  string timerLabel =
      "AST SgFile Constructor for " + p_sourceFileNameWithPath + ":";
  TimingPerformance timer(timerLabel);

  get_originalCommandLineArgumentList() = argv;

  ASSERT_not_null(get_file_info());
  ASSERT_not_null(get_startOfConstruct());

  ASSERT_not_null(get_parent());
  ROSE_ASSERT(get_parent() == owner);
}

string
SgFile::generate_C_preprocessor_intermediate_filename(string sourceFilename) {
  // DQ (9/24/2013): We need this assertion to make sure we are not causing what
  // might be strange errors in memory.
  ROSE_ASSERT(sourceFilename.empty() == false);

  // Note: for "foo.F90" the fileNameSuffix() returns "F90"
  string filenameExtension = StringUtility::fileNameSuffix(sourceFilename);

  ROSE_ASSERT(filenameExtension.empty() == false);

  string sourceFileNameWithoutExtension =
      StringUtility::stripFileSuffixFromFileName(sourceFilename);

  ROSE_ASSERT(sourceFileNameWithoutExtension.empty() == false);

  // We need to turn on the 5th bit to make the capital a lower case character
  // (assume ASCII)
  filenameExtension[0] = filenameExtension[0] | (1 << 5);

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

  int fileNameIndex = 0;

  TimingPerformance timer("AST Front End Processing (SgFile):");

  int frontendErrorLevel = 0;

  vector<string> localCopy_argv = get_originalCommandLineArgumentList();
  processRoseCommandLineOptions(localCopy_argv);
  processBackendSpecificCommandLineOptions(localCopy_argv);

  vector<string> inputCommandLine;

  if (get_C_only() || get_Cxx_only() || get_Cuda_only() || get_OpenCL_only()) {
    build_CLANG_CommandLine(inputCommandLine, localCopy_argv, fileNameIndex);
  }

  if (p_useBackendOnly == true) {
    return 0;
  }

  ROSE_ASSERT(p_useBackendOnly == false);

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

#if defined(ROSE_FLANG_FRONTEND)
  // FMZ: 05/30/2008.  Do not generate module files for nested module loads.
  if (get_Fortran_only() == true && frontendErrorLevel == 0) {
    if (!FlangModuleInfo::isModuleFile()) {
      if (get_verbose() > 1)
        printf("Generating a Fortran module file (*.rmod) \n");

      generateModFile(this);

      if (get_verbose() > 1)
        printf("DONE: Generating a Fortran module file (*.rmod) \n");
    }
  }
#endif

  return frontendErrorLevel;
}

void SgFile::secondaryPassOverSourceFile() {
  TimingPerformance timer("AST secondaryPassOverSourceFile:");

  {
    {
      if (p_preprocessorDirectivesAndCommentsList == nullptr) {
        p_preprocessorDirectivesAndCommentsList =
            new ROSEAttributesListContainer();
      }
      ASSERT_not_null(p_preprocessorDirectivesAndCommentsList);
    }

    if (get_skip_commentsAndDirectives() == false) {
      SgSourceFile *sourceFile = isSgSourceFile(this);
      ASSERT_not_null(sourceFile);

      const bool using_flang_frontend = sourceFile->get_Fortran_only();
      const bool using_clang_frontend =
          sourceFile->get_C_only() || sourceFile->get_Cxx_only() ||
          sourceFile->get_Cuda_only() || sourceFile->get_OpenCL_only();
      ROSE_ASSERT(using_flang_frontend != using_clang_frontend);
      ROSE_ASSERT(
          !sourceFile->get_processedToIncludeCppDirectivesAndComments());
      rosePhaseTrace("secondaryPassOverSourceFile.attachPreprocessing.begin");
      if (using_flang_frontend) {
        sourceFile->set_processedToIncludeCppDirectivesAndComments(true);
      } else {
        // Clang already attached the exact preprocessing stream. Collect raw
        // tokens without attaching it a second time.
        attachPreprocessingInfo(sourceFile, "", false);
      }
      rosePhaseTrace("secondaryPassOverSourceFile.attachPreprocessing.end");

      {
        TimingPerformance timer("Header file support for tokens:");
        rosePhaseTrace("secondaryPassOverSourceFile.tokenSupport.begin");

        if (using_clang_frontend && sourceFile->get_unparse_tokens()) {
          ASSERT_not_null(sourceFile->get_globalScope());
          detectMacroOrIncludeFileExpansions(sourceFile);
        }
        rosePhaseTrace("secondaryPassOverSourceFile.tokenSupport.end");
      }

      // Liao, 3/31/2009 Handle OpenMP here to see macro calls within directives
#ifdef ROSE_BUILD_CPP_LANGUAGE_SUPPORT
      rosePhaseTrace("secondaryPassOverSourceFile.openmp.begin");
      if (Rose::AstJson::checkpointEnabled(
              sourceFile, Rose::AstJson::Checkpoint::PreOmpConstruction))
        sourceFile = Rose::AstJson::roundTripSourceFile(
            sourceFile, Rose::AstJson::Checkpoint::PreOmpConstruction);
      processOpenMP(sourceFile);
      rosePhaseTrace("secondaryPassOverSourceFile.openmp.end");
#endif
      if (sourceFile->get_openacc())
        printf("OpenACC support is turned on\n");

      ASSERT_not_null(sourceFile);

    } // end if get_skip_commentsAndDirectives() is false
  }

  ROSE_ASSERT(SgNode::get_globalFunctionTypeTable()->get_parent() == nullptr);
  ROSE_ASSERT(SgNode::get_globalTypeTable()->get_parent() == nullptr);
}

int SgSourceFile::build_Fortran_AST(vector<string> argv) {
#if !defined(ROSE_FLANG_FRONTEND)
  std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] Flang was not enabled "
               "in this build.\n";
  ROSE_ABORT();
#else

  vector<string> flangCommandLine;
  flangCommandLine.push_back("rex-flang-frontend");
  vector<string> flangArgs = argv;
  SgFile::stripRoseCommandLineOptions(flangArgs);
  // Flang's parse tree and cooked-source provenance are the canonical
  // producer for Fortran directive boundaries and physical spelling.  REX
  // still owns semantic Sage OpenMP/OpenACC construction through ompparser,
  // but disabling the Flang language features discards directives as
  // comments before the AST builder can receive typed provenance.
  if (get_openmp() || get_openacc()) {
    flangArgs.erase(std::remove_if(flangArgs.begin(), flangArgs.end(),
                                   [&](const std::string &arg) {
                                     if (get_openmp() &&
                                         (arg == "-fopenmp" ||
                                          arg == "-fopenmp-simd" ||
                                          arg.rfind("-fopenmp=", 0) == 0)) {
                                       return true;
                                     }
                                     return get_openacc() && arg == "-fopenacc";
                                   }),
                    flangArgs.end());
    if (get_openmp()) {
      flangArgs.push_back("-fopenmp");
    }
    if (get_openacc()) {
      flangArgs.push_back("-fopenacc");
    }
  }
  // Flang parses the selected file directly, independently of its extension.
  // Preserve its physical path and bytes so cooked provenance owns the real
  // source, including preprocessor diagnostics and include boundaries.
  const std::string &source_path = get_sourceFileNameWithPath();
  ASSERT_require(!source_path.empty());
  const std::string source_dir =
      std::filesystem::path(source_path).parent_path().string();
  const std::filesystem::path temp_dir =
      Rose::FileSystem::createTemporaryDirectory();
  const std::string flang_include_temp_dir = temp_dir.string();

  addFlangIntrinsicIncludeDir(flangCommandLine);
  if (flangArgs.size() > 1) {
    flangCommandLine.insert(flangCommandLine.end(), flangArgs.begin() + 1,
                            flangArgs.end());
  }
  if (!source_dir.empty() && !hasIncludeDir(flangCommandLine, source_dir)) {
    flangCommandLine.push_back("-I");
    flangCommandLine.push_back(source_dir);
  }
  CommandlineProcessing::ArgvStorage flangArgvStorage(flangCommandLine);
  int flangArgc = flangArgvStorage.argc();
  char **flangArgv = flangArgvStorage.argv();

  // SG (7/9/2015) In case of a mixed language project, force case sensitivity
  // here.
  SageBuilder::symbol_table_case_insensitive_semantics = true;

  const int status = experimental_fortran_main(flangArgc, flangArgv, this,
                                               flang_include_temp_dir);
  std::filesystem::remove_all(temp_dir);
  return status;
#endif
}

//-----------------------------------------------------------------------------
// Rose::Frontend
//-----------------------------------------------------------------------------

int Rose::Frontend::Run(SgProject *project) {
  return Rose::Frontend::RunSerial(project);
} // Rose::Frontend::Run

int Rose::Frontend::RunSerial(SgProject *project) {
  ASSERT_not_null(project);
  if (SgProject::get_verbose() > 0)
    std::cout << "[INFO] [Frontend] Running in serial mode" << std::endl;

  std::vector<SgFile *> all_files = project->get_fileList();
  int status = 0;
  for (SgFile *file : all_files) {
    ASSERT_not_null(file);
    file->runFrontend(status);
    if (status != 0) {
      break;
    }
  }

  project->set_frontendErrorCode(status);

  return status;
} // Rose::Frontend::RunSerial

int SgSourceFile::build_C_and_Cxx_AST(vector<string> argv,
                                      vector<string> inputCommandLine) {
  // Mixed-language projects still need case-sensitive C/C++ symbols.
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

  ROSE_ASSERT(!argv.empty());
  if (get_verbose() > 1) {
    const std::string frontEndCommandLineString =
        argv[0] + " " +
        CommandlineProcessing::generateStringFromArgList(inputCommandLine);
    printf("In build_C_and_Cxx_AST(): Before calling frontend entry point: "
           "frontEndCommandLineString = %s \n",
           frontEndCommandLineString.c_str());
  }

  CommandlineProcessing::ArgvStorage cCxxArgvStorage(inputCommandLine);
  int c_cxx_argc = cCxxArgvStorage.argc();
  char **c_cxx_argv = cCxxArgvStorage.argv();

#ifdef ROSE_BUILD_CXX_LANGUAGE_SUPPORT
  int clang_main(int, char *[], SgSourceFile &sageFile,
                 const char *driver_argv0);
  int frontendErrorLevel =
      clang_main(c_cxx_argc, c_cxx_argv, *this, argv[0].c_str());

#else
  int frontendErrorLevel = 99;
  ROSE_ASSERT(!"[FATAL] [ROSE] [frontend] [C/C++] "
               "error: ROSE was not configured to support the C/C++ frontend.");
#endif

  return frontendErrorLevel;
}

int SgSourceFile::buildAST(vector<string> argv,
                           vector<string> inputCommandLine) {
  if (get_skip_parser())
    return 0;

  int frontendErrorLevel = 0;
  if (get_Fortran_only() == true) {
#if defined(ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT) || defined(ROSE_FLANG_FRONTEND)
    frontendErrorLevel = build_Fortran_AST(argv);
#else
    std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] Flang was not enabled "
                 "in this build.\n";
    ROSE_ABORT();
#endif
  } else {
    frontendErrorLevel = build_C_and_Cxx_AST(argv, inputCommandLine);
  }

  if (get_verbose() > 1)
    printf("DONE: frontend called (frontendErrorLevel = %d) \n",
           frontendErrorLevel);

  if (frontendErrorLevel != 0) {
    if (get_verbose() > 1)
      printf("frontendErrorLevel = %d \n", frontendErrorLevel);

    if (get_negative_test() == true) {
      if (get_verbose() > 1) {
        printf("(evaluation of frontend results) This is a negative tests, so "
               "an error in compilation is a PASS but a successful \n");
        printf(
            "compilation is not a FAIL since the failure might happen in the "
            "compilation of the generated code by the vendor compiler. \n");
      }
    } else {
      printf("Errors in Processing Input File: throwing an instance of "
             "\"frontend_failed\" exception due to syntax errors detected in "
             "the input code \n");
    }
    return frontendErrorLevel;
  }

  return frontendErrorLevel;
}

int SgFile::compileOutput(vector<string> &argv, int fileNameIndex) {
  TimingPerformance timer("AST Object Code Generation (compile output):");

  ROSE_ASSERT(fileNameIndex == 0);

  bool showBackendCommandLine = SgProject::get_showBackendCommandLine();

  if (showBackendCommandLine) {
    printf("\n\n***************************************************** \n");
    printf("Inside of SgFile::compileOutput() \n");
    printf("   --- get_unparse_output_filename() = %s \n",
           get_unparse_output_filename().c_str());
    printf("***************************************************** \n\n\n");
  }

  string compilerNameOrig = BACKEND_CXX_COMPILER_NAME_WITH_PATH;

  if (get_Fortran_only() == true) {
    compilerNameOrig = BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH;
  }

  string compilerName = compilerNameOrig + " ";

  SgProject *project = SageInterface::getProject(this);
  ASSERT_not_null(project);
  if (get_frontendErrorCode() != 0 || project->get_midendErrorCode() != 0 ||
      get_unparserErrorCode() != 0 || get_backendCompilerErrorCode() != 0) {
    fprintf(stderr,
            "REX_BACKEND_INVARIANT[pipeline-error]: file=%s frontend=%d "
            "midend=%d unparser=%d backend=%d refusing to compile a fallback "
            "input\n",
            getFileName().c_str(), get_frontendErrorCode(),
            project->get_midendErrorCode(), get_unparserErrorCode(),
            get_backendCompilerErrorCode());
    ROSE_ABORT();
  }

  if (get_unparse_output_filename().empty()) {
    if (!get_skip_unparse() && !project->get_useBackendOnly()) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[missing-unparse-output]: file=%s has no "
              "generated source to compile\n",
              getFileName().c_str());
      ROSE_ABORT();
    }
    // Backend-only and explicitly skipped-unparse modes compile the input by
    // design. They are not recovery paths for a failed frontend or unparser.
    set_unparse_output_filename(get_sourceFileNameWithPath());
  }

  if (SgProject::get_verbose() >= 1) {
    printf("Now call the backend (vendor's) compiler compilerNameOrig = %s for "
           "file = %s \n",
           compilerNameOrig.c_str(), get_unparse_output_filename().c_str());
  }

  // Build the commandline to hand off to the C++/C compiler
  vector<string> compilerCmdLine =
      buildCompilerCommandLineOptions(argv, fileNameIndex, compilerName);

  string sourceFilename = this->getFileName();
  string filenameExtension = StringUtility::fileNameSuffix(sourceFilename);
  const bool isAssemblerSource =
      CommandlineProcessing::isAssemblerFileNameSuffix(filenameExtension);
  if (get_Fortran_only()) {
    addFlangIntrinsicIncludeDir(compilerCmdLine);
  }

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
      if (!has_flag_with_value("-isystem", clang_include_root)) {
        compilerCmdLine.push_back("-isystem");
        compilerCmdLine.push_back(clang_include_root);
      }
      if (!builtin_header.empty() &&
          !has_flag_with_value("-include", builtin_header)) {
        compilerCmdLine.push_back("-include");
        compilerCmdLine.push_back(builtin_header);
      }
    }
  }
  int returnValueForCompiler = 0;

  if (get_skipfinalCompileStep() == false) {
    if (get_verbose() >= 1) {
      printf("calling systemFromVector() \n");
      printf("Number of command line arguments: %" PRIuPTR "\n",
             compilerCmdLine.size());
      for (size_t i = 0; i < compilerCmdLine.size(); ++i) {
        printf("Backend compiler arg[%" PRIuPTR "]: = %s\n", i,
               compilerCmdLine[i].c_str());
      }
      printf("End of command line for backend compiler\n");

      printf("SgFile::compileOutput(): get_skipfinalCompileStep() == false: "
             "compilerCmdLine = \n%s\n",
             CommandlineProcessing::generateStringFromArgList(compilerCmdLine)
                 .c_str());
    }
    MLOG_TRACE_CXX("sage_support")
        << "SgFile::compileOutput(): compilerCmdLine =\n"
        << CommandlineProcessing::generateStringFromArgList(compilerCmdLine)
        << std::endl;

    if (get_compileOnly() == true) {
      if (std::find(compilerCmdLine.begin(), compilerCmdLine.end(), "-c") ==
          compilerCmdLine.end()) {
        compilerCmdLine.push_back("-c");
      }
    }

    if (get_C_only() || get_Cxx_only() || get_Fortran_only()) {
      CommandlineProcessing::validateBackendCompileOnlyCommandLine(
          compilerCmdLine, get_unparse_output_filename());
    }

    if (showBackendCommandLine) {
      printf("In SgFile::compileOutput(): Calling systemFromVector(): "
             "compilerCmdLine = \n%s\n",
             CommandlineProcessing::generateStringFromArgList(compilerCmdLine)
                 .c_str());
    }

    returnValueForCompiler = systemFromVector(compilerCmdLine);

    MLOG_TRACE_CXX("sage_support")
        << "SgFile::compileOutput(): systemFromVector() returnValueForCompiler "
           "= "
        << returnValueForCompiler << std::endl;

    if (returnValueForCompiler != 0) {
      this->set_backendCompilerErrorCode(-1);
    }
  } // if (get_skipfinalCompileStep() == false)
  else {
    if (get_verbose() > 1)
      printf("COMPILER NOT CALLED: compilerNameString = %s \n",
             "<unknown>" /* compilerNameString.c_str() */);
  }

  int finalCompiledExitStatus = returnValueForCompiler;

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

  return finalCompiledExitStatus;
}

int SgProject::compileOutput() {
  TimingPerformance timer("AST Backend Compilation (SgProject):");

  int errorCode = 0;
  int linkingReturnVal = 0;
  if (get_stop_after_compilation_do_not_assemble_file() == true) {
    vector<string> originalCommandLine = get_originalCommandLineArgumentList();

    // strip out any rose options before passing the command line.
    SgFile::stripRoseCommandLineOptions(originalCommandLine);

    ROSE_ASSERT(!originalCommandLine.empty());

    if (get_C_only() == true) {
      originalCommandLine.erase(originalCommandLine.begin());
      originalCommandLine.insert(originalCommandLine.begin(),
                                 RexBackend::cCompilerCommand.begin(),
                                 RexBackend::cCompilerCommand.end());
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

  if (get_C_PreprocessorOnly() == true) {
    vector<string> originalCommandLine = get_originalCommandLineArgumentList();
    SgFile::stripRoseCommandLineOptions(originalCommandLine);
    ROSE_ASSERT(!originalCommandLine.empty());

    auto compiler_command = RexBackend::cxxCompilerCommand;
    if (get_Fortran_only()) {
      compiler_command = RexBackend::fortranCompilerCommand;
    } else if (get_C_only()) {
      compiler_command = RexBackend::cCompilerCommand;
    }
    ROSE_ASSERT(compiler_command.size() != 0);
    originalCommandLine.erase(originalCommandLine.begin());
    originalCommandLine.insert(originalCommandLine.begin(),
                               compiler_command.begin(),
                               compiler_command.end());

    if (SgProject::get_verbose() > 0) {
      for (unsigned int i = 0; i < originalCommandLine.size(); ++i) {
        printf("originalCommandLine[%u] = %s \n", i,
               originalCommandLine[i].c_str());
      }
    }

    errorCode = systemFromVector(originalCommandLine);

    ASSERT_not_null(SgNode::get_globalFunctionTypeTable());
    ROSE_ASSERT(SgNode::get_globalFunctionTypeTable()->get_parent() == nullptr);

  } else // non-preprocessing-only case
  {

    // Separate library outputs and explicitly skipped files do not take part
    // in this compile/link step. Counting them would discard a single input's
    // explicit object output by selecting multi-file command construction.
    const SgFilePtrList &filesToCompile = get_fileList();
    const bool multifile_support_compile_only_flag =
        std::count_if(filesToCompile.begin(), filesToCompile.end(),
                      [](const SgFile *file) {
                        ROSE_ASSERT(file != nullptr);
                        return !file->get_skipfinalCompileStep();
                      }) > 1;

    for (int i = 0; i < numberOfFiles(); i++) {
      SgFile &file = get_file(i);

      if (multifile_support_compile_only_flag == true) {
        file.set_compileOnly(true);

        file.set_multifile_support(true);
      }

      int localErrorCode = file.compileOutput(0);

      if (localErrorCode > errorCode) {
        errorCode = localErrorCode;
      }

      if (multifile_support_compile_only_flag == true) {
        file.set_compileOnly(false);
      }
    }

    if (errorCode == 0) {
      if (get_compileOnly() == false) {
        linkingReturnVal = link(BACKEND_CXX_COMPILER_NAME_WITH_PATH);
      }
    }
  } // end if preprocessing-only is false

  return errorCode + linkingReturnVal;
}

int SgProject::link(std::string linkerName) {
  if (numberOfFiles() == 0) {
    if (numberOfDirectories() != 0) {
      fprintf(stderr,
              "REX_BACKEND_INVARIANT[project-link-directory]: project has "
              "%d directories but no top-level source files; project-level "
              "link inputs cannot be derived from directory-owned files\n",
              numberOfDirectories());
      ROSE_ABORT();
    }
    if (get_verbose() > 0)
      cout << "SgProject::link maybe encountering an object file ..." << endl;
  } else {
    const SgFilePtrList &files = get_fileList();
    const bool has_link_input =
        std::any_of(files.begin(), files.end(), [](const SgFile *file) {
          ROSE_ASSERT(file != nullptr);
          return !file->get_skipfinalCompileStep();
        });
    if (get_compileOnly() || !has_link_input) {
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

  // remove the original compiler/linker name
  if (argcArgvList.size() > 0)
    argcArgvList.erase(argcArgvList.begin());

  // remove all original file names
  Rose_STL_Container<string> sourceFilenames = get_sourceFileNameList();
  for (Rose_STL_Container<string>::iterator i = sourceFilenames.begin();
       i != sourceFilenames.end(); i++) {
    argcArgvList.erase(
        std::remove(argcArgvList.begin(), argcArgvList.end(), *i),
        argcArgvList.end());
  }

  // Call the compile
  return link(argcArgvList, linkerName);
}

int SgProject::link(const std::vector<std::string> &argv,
                    std::string linkerName) {
  // These project predicates mean that at least one input uses the language.
  // Select the driver with the required language runtime, then supplement a
  // mixed Fortran/C++ link with the detected C++ runtime options below.
  Rose_STL_Container<string> linkingCommand;
  if (get_Fortran_only()) {
    linkingCommand.assign(RexBackend::fortranCompilerCommand.begin(),
                          RexBackend::fortranCompilerCommand.end());
  } else if (get_Cxx_only()) {
    linkingCommand.assign(RexBackend::cxxCompilerCommand.begin(),
                          RexBackend::cxxCompilerCommand.end());
  } else if (get_C_only() || get_C99_only()) {
    linkingCommand.assign(RexBackend::cCompilerCommand.begin(),
                          RexBackend::cCompilerCommand.end());
  } else {
    linkingCommand.push_back(linkerName);
  }
  ROSE_ASSERT(!linkingCommand.empty() && !linkingCommand.front().empty());
  // find all object files generated at file level compilation
  // The assumption is that -o objectFileName is made explicit and
  // is generated by SgFile::generateOutputFileName()
  for (int i = 0; i < numberOfFiles(); i++) {
    // DQ (2/25/2014): If this file was supressed in the compilation to build an
    // object file then it should be supressed in being used in the linking
    // stage. linkingCommand.push_back(get_file(i).generateOutputFileName());
    if (get_file(i).get_skipfinalCompileStep() == false) {
      linkingCommand.push_back(
          Rose::TestOutput::resolvePath(get_file(i).generateOutputFileName()));
    }
  }

  // Add any options specified in the original command line (after
  // preprocessing)
  linkingCommand.insert(linkingCommand.end(), argv.begin(), argv.end());

  if (get_Fortran_only() && get_Cxx_only()) {
    linkingCommand.insert(linkingCommand.end(),
                          RexBackend::cxxRuntimeLinkOptions.begin(),
                          RexBackend::cxxRuntimeLinkOptions.end());
  }

  // Object-only link invocations also require the selected OpenMP runtime.
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

  int status = systemFromVector(linkingCommand);

  if (get_verbose() > 1) {
    printf("linker error status = %d \n", status);
  }

  return status;
}

namespace ASTMemoryPool {
SgNode *validNodeMarker() {
  alignas(SgNode) static unsigned char marker;
  return reinterpret_cast<SgNode *>(&marker);
}
} // namespace ASTMemoryPool

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
  case PreprocessingInfo::CpreprocessorPragmaDeclaration: {
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

static SgFunctionSymbol *functionSymbolForCallee(SgExpression *functionExp);

SgFunctionDeclaration *
SgFunctionCallExp::getAssociatedFunctionDeclaration() const {
  SgExpression *functionExpression = get_function();
  if (functionExpression == nullptr ||
      functionExpression->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-call-callee-owner]: function call=%p "
            "has no exact owned callee expression\n",
            static_cast<const void *>(this));
    ROSE_ABORT();
  }

  return getAssociatedFunctionDeclarationForCallee(functionExpression);
}

SgFunctionDeclaration *
SgFunctionCallExp::getAssociatedFunctionDeclarationForCallee(
    SgExpression *functionExpression) {
  ASSERT_not_null(functionExpression);
  SgExpression *semanticFunctionExpression = functionExpression;
  std::unordered_set<SgExpression *> visitedFunctionWrappers;
  while (visitedFunctionWrappers.insert(semanticFunctionExpression).second) {
    SgExpression *operand = nullptr;
    if (auto *macro = isSgMacroExpansionExp(semanticFunctionExpression)) {
      operand = macro->get_expanded_expression_checked();
    } else if (SgCastExp *castExpression =
                   isSgCastExp(semanticFunctionExpression)) {
      castExpression->validate_semantic_conversion();
      const auto conversion = castExpression->get_semantic_conversion_kind();
      if (castExpression->get_cast_type() != SgCastExp::e_implicit_cast ||
          (conversion !=
               SgCastExp::e_semantic_conversion_FunctionToPointerDecay &&
           conversion != SgCastExp::e_semantic_conversion_LValueToRValue &&
           conversion != SgCastExp::e_semantic_conversion_NoOp))
        break;
      operand = castExpression->get_operand_i();
    } else if (SgPointerDerefExp *pointerDeref =
                   isSgPointerDerefExp(semanticFunctionExpression)) {
      operand = pointerDeref->get_operand_i();
    } else if (SgAddressOfOp *addressOf =
                   isSgAddressOfOp(semanticFunctionExpression)) {
      operand = addressOf->get_operand_i();
    } else {
      break;
    }
    if (operand == nullptr ||
        operand->get_parent() != semanticFunctionExpression) {
      fprintf(stderr,
              "REX_AST_INVARIANT[function-call-callee-owner]: function "
              "wrapper=%p/%s has no exact owned operand\n",
              static_cast<void *>(semanticFunctionExpression),
              semanticFunctionExpression->class_name().c_str());
      ROSE_ABORT();
    }
    semanticFunctionExpression = operand;
  }

  // A template-function reference has two deliberately independent edges:
  // its symbol names the primary template exactly as written, while its typed
  // semantic declaration names the selected callable specialization.  The
  // call-level query must not collapse those identities by consulting the
  // spelling symbol.
  if (SgTemplateFunctionRefExp *templateReference =
          isSgTemplateFunctionRefExp(semanticFunctionExpression)) {
    return templateReference->getAssociatedFunctionDeclaration();
  }
  if (auto *member = isSgMemberFunctionRefExp(semanticFunctionExpression)) {
    return member->getAssociatedMemberFunctionDeclaration();
  }
  if (auto *member =
          isSgTemplateMemberFunctionRefExp(semanticFunctionExpression)) {
    return member->getAssociatedMemberFunctionDeclaration();
  }

  // Member references retain the selected declaration even when virtual
  // dispatch prevents identifying one runtime target symbol.
  SgExpression *memberReferenceExpression = nullptr;
  if (SgDotExp *dot = isSgDotExp(semanticFunctionExpression)) {
    memberReferenceExpression = dot->get_rhs_operand_i();
  } else if (SgArrowExp *arrow = isSgArrowExp(semanticFunctionExpression)) {
    memberReferenceExpression = arrow->get_rhs_operand_i();
  }
  if (memberReferenceExpression != nullptr) {
    if (memberReferenceExpression->get_parent() != semanticFunctionExpression) {
      fprintf(stderr,
              "REX_AST_INVARIANT[template-member-call-callee-owner]: "
              "member access=%p/%s has no exact owned callable reference\n",
              static_cast<void *>(semanticFunctionExpression),
              semanticFunctionExpression->class_name().c_str());
      ROSE_ABORT();
    }
    if (SgTemplateMemberFunctionRefExp *templateReference =
            isSgTemplateMemberFunctionRefExp(memberReferenceExpression)) {
      return templateReference->getAssociatedMemberFunctionDeclaration();
    }
    if (auto *member = isSgMemberFunctionRefExp(memberReferenceExpression)) {
      return member->getAssociatedMemberFunctionDeclaration();
    }
  }

  SgFunctionSymbol *associatedFunctionSymbol =
      functionSymbolForCallee(functionExpression);
  return associatedFunctionSymbol != nullptr
             ? associatedFunctionSymbol->get_declaration()
             : nullptr;
}

SgFunctionSymbol *SgFunctionCallExp::getAssociatedFunctionSymbol() const {
  SgExpression *functionExp = this->get_function();
  if (functionExp == nullptr) {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-call-callee]: function call has no "
            "owned callee expression\n");
    ROSE_ABORT();
  }
  if (functionExp->get_parent() != this) {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-call-callee-owner]: function-call "
            "callee is not owned by its containing call\n");
    ROSE_ABORT();
  }
  return functionSymbolForCallee(functionExp);
}

static SgFunctionSymbol *functionSymbolForCallee(SgExpression *functionExp) {
  ASSERT_not_null(functionExp);
  SgFunctionSymbol *returnSymbol = nullptr;
  bool isAlwaysResolvedStatically = false;
  SgNode *functionExpOwner = functionExp->get_parent();
  auto isPseudoDestructorReference = [](SgExpression *reference) {
    if (auto *qualified = isSgScopeOp(reference)) {
      ASSERT_not_null(isSgTypeExpression(qualified->get_lhs_operand()));
      ASSERT_not_null(qualified->get_rhs_operand());
      reference = qualified->get_rhs_operand();
    }
    return isSgPseudoDestructorRefExp(reference) != nullptr;
  };
  auto resolvedNonrealFunctionSymbol =
      [](SgNonrealRefExp *reference) -> SgFunctionSymbol * {
    ASSERT_not_null(reference);
    if (reference->get_resolved_function_declaration() == nullptr) {
      if (reference->get_resolved_variable_declaration() != nullptr) {
        SageInterface::requireResolvedVariableTemplateReference(
            reference, "function-call callee");
      }
      return nullptr;
    }
    SgFunctionDeclaration *resolved =
        SageInterface::requireResolvedFunctionTemplateReference(
            reference, "function-call callee");
    SgFunctionDeclaration *canonical =
        isSgFunctionDeclaration(resolved->get_firstNondefiningDeclaration());
    SgFunctionSymbol *symbol =
        canonical != nullptr
            ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
            : nullptr;
    if (canonical == nullptr || symbol == nullptr ||
        symbol->get_declaration() != canonical) {
      fprintf(stderr,
              "REX_AST_INVARIANT[function-template-callee-symbol]: "
              "reference=%p resolved=%p has no exact canonical function "
              "symbol\n",
              static_cast<void *>(reference), static_cast<void *>(resolved));
      ROSE_ABORT();
    }
    return symbol;
  };
  auto requireIndirectCallableSymbol = [](SgVarRefExp *reference,
                                          const char *role) {
    SgVariableSymbol *symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    SgInitializedName *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    if (reference == nullptr || symbol == nullptr || declaration == nullptr ||
        symbol->get_symbol_basis() != declaration ||
        reference->get_type() == nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[indirect-callee-symbol]: %s reference=%p "
              "symbol=%p declaration=%p type=%p has no exact variable-symbol "
              "ownership\n",
              role != nullptr ? role : "<unknown>",
              static_cast<void *>(reference), static_cast<void *>(symbol),
              static_cast<void *>(declaration),
              reference != nullptr ? static_cast<void *>(reference->get_type())
                                   : nullptr);
      ROSE_ABORT();
    }
  };
  auto unwrapMacroExpansion = [&]() {
    while (SgMacroExpansionExp *macro = isSgMacroExpansionExp(functionExp)) {
      if (macro->get_parent() != functionExpOwner) {
        fprintf(stderr,
                "REX_AST_INVARIANT[macro-call-callee-owner]: macro spelling "
                "'%s' is not owned by its containing callee expression\n",
                macro->get_spelling().c_str());
        ROSE_ABORT();
      }
      functionExpOwner = macro;
      functionExp = macro->get_expanded_expression_checked();
    }
  };
  unwrapMacroExpansion();

  auto unwrapIdentityUnaryOperators = [&]() {
    while (isSgPointerDerefExp(functionExp) || isSgAddressOfOp(functionExp)) {
      SgUnaryOp *unary = isSgUnaryOp(functionExp);
      if (unary == nullptr || unary->get_parent() != functionExpOwner ||
          unary->get_operand() == nullptr ||
          unary->get_operand()->get_parent() != unary) {
        fprintf(stderr,
                "REX_AST_INVARIANT[function-call-callee-unary-owner]: "
                "identity-preserving callee unary=%p has no exact owned "
                "operand\n",
                static_cast<void *>(unary));
        ROSE_ABORT();
      }
      functionExpOwner = unary;
      functionExp = unary->get_operand();
      unwrapMacroExpansion();
    }
  };
  unwrapIdentityUnaryOperators();

  // The Clang frontend preserves implicit callee conversions as typed
  // SgCastExp nodes.  Resolve through only the exact conversion families that
  // cannot change callable identity; explicit casts and pointer-valued
  // expressions remain intentionally unresolved here.
  auto resolveFunctionDecayEndpoint = [](SgType *type,
                                         const char *role) -> SgType * {
    std::set<SgType *> seen;
    while (type != nullptr) {
      type = type->stripTypedefsAndModifiers();
      if (type == nullptr || !seen.insert(type).second) {
        fprintf(stderr,
                "REX_AST_INVARIANT[function-call-callee-conversion-type]: "
                "%s endpoint contains a null or cyclic semantic type chain\n",
                role != nullptr ? role : "<unknown>");
        ROSE_ABORT();
      }

      SgType *underlying = nullptr;
      if (SgTypeOfType *typeOf = isSgTypeOfType(type)) {
        underlying = typeOf->get_base_type();
      } else if (SgDeclType *declType = isSgDeclType(type)) {
        underlying = declType->get_base_type();
      }
      if (underlying == nullptr) {
        break;
      }
      type = underlying;
    }
    return type;
  };
  while (SgCastExp *cast = isSgCastExp(functionExp)) {
    cast->validate_semantic_conversion();
    if (cast->get_cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }
    if (cast->get_parent() != functionExpOwner ||
        cast->get_operand() == nullptr ||
        cast->get_operand()->get_parent() != cast) {
      fprintf(stderr,
              "REX_AST_INVARIANT[function-call-callee-conversion-owner]: "
              "implicit callee cast=%p has no exact owned operand\n",
              static_cast<void *>(cast));
      ROSE_ABORT();
    }

    switch (cast->get_semantic_conversion_kind()) {
    case SgCastExp::e_semantic_conversion_FunctionToPointerDecay: {
      SgType *result = cast->get_type();
      SgPointerType *target =
          result != nullptr
              ? isSgPointerType(result->stripTypedefsAndModifiers())
              : nullptr;
      SgType *source = cast->get_operand()->get_type();
      SgType *targetBase =
          target != nullptr ? target->get_base_type() : nullptr;
      SgType *resolvedSource = resolveFunctionDecayEndpoint(source, "source");
      SgType *resolvedTargetBase =
          resolveFunctionDecayEndpoint(targetBase, "result pointee");
      SgFunctionType *sourceFunction = isSgFunctionType(resolvedSource);
      SgFunctionType *targetFunction = isSgFunctionType(resolvedTargetBase);
      if (target == nullptr || sourceFunction == nullptr ||
          targetFunction == nullptr ||
          !SageInterface::isEquivalentFunctionType(sourceFunction,
                                                   targetFunction)) {
        if (SgFunctionType *sourceFunctionType =
                isSgFunctionType(resolvedSource)) {
          SgFunctionSymbol *sourceSymbol = nullptr;
          if (SgFunctionRefExp *reference =
                  isSgFunctionRefExp(cast->get_operand())) {
            sourceSymbol = reference->get_symbol();
          } else if (SgTemplateFunctionRefExp *reference =
                         isSgTemplateFunctionRefExp(cast->get_operand())) {
            sourceSymbol = reference->get_symbol();
          }
          SgFunctionType *targetFunctionType =
              isSgFunctionType(resolvedTargetBase);
          fprintf(
              stderr,
              "REX_AST_INVARIANT[function-call-callee-conversion-detail]: "
              "callee=%s source-return=%p/%s target-return=%p/%s "
              "source-args=%zu target-args=%zu\n",
              sourceSymbol != nullptr
                  ? sourceSymbol->get_name().getString().c_str()
                  : "<non-function-reference>",
              static_cast<void *>(sourceFunctionType->get_return_type()),
              sourceFunctionType->get_return_type() != nullptr
                  ? sourceFunctionType->get_return_type()->class_name().c_str()
                  : "<null>",
              targetFunctionType != nullptr
                  ? static_cast<void *>(targetFunctionType->get_return_type())
                  : nullptr,
              targetFunctionType != nullptr &&
                      targetFunctionType->get_return_type() != nullptr
                  ? targetFunctionType->get_return_type()->class_name().c_str()
                  : "<null>",
              sourceFunctionType->get_arguments().size(),
              targetFunctionType != nullptr
                  ? targetFunctionType->get_arguments().size()
                  : 0);
          if (targetFunctionType != nullptr) {
            const SgTypePtrList sourceArguments =
                sourceFunctionType->get_arguments();
            const SgTypePtrList targetArguments =
                targetFunctionType->get_arguments();
            const std::size_t common =
                std::min(sourceArguments.size(), targetArguments.size());
            for (std::size_t index = 0; index < common; ++index) {
              fprintf(
                  stderr,
                  "REX_AST_INVARIANT[function-call-callee-conversion-"
                  "detail]: argument=%zu source=%p/%s stripped=%p/%s "
                  "target=%p/%s stripped=%p/%s equivalent=%d\n",
                  index, static_cast<void *>(sourceArguments[index]),
                  sourceArguments[index] != nullptr
                      ? sourceArguments[index]->class_name().c_str()
                      : "<null>",
                  sourceArguments[index] != nullptr
                      ? static_cast<void *>(
                            sourceArguments[index]->stripTypedefsAndModifiers())
                      : nullptr,
                  sourceArguments[index] != nullptr
                      ? sourceArguments[index]
                            ->stripTypedefsAndModifiers()
                            ->class_name()
                            .c_str()
                      : "<null>",
                  static_cast<void *>(targetArguments[index]),
                  targetArguments[index] != nullptr
                      ? targetArguments[index]->class_name().c_str()
                      : "<null>",
                  targetArguments[index] != nullptr
                      ? static_cast<void *>(
                            targetArguments[index]->stripTypedefsAndModifiers())
                      : nullptr,
                  targetArguments[index] != nullptr
                      ? targetArguments[index]
                            ->stripTypedefsAndModifiers()
                            ->class_name()
                            .c_str()
                      : "<null>",
                  sourceArguments[index] != nullptr &&
                          targetArguments[index] != nullptr &&
                          SageInterface::isEquivalentType(
                              sourceArguments[index], targetArguments[index])
                      ? 1
                      : 0);
            }
          }
        }
        fprintf(stderr,
                "REX_AST_INVARIANT[function-call-callee-conversion]: "
                "callee=%p cast=%p function-to-pointer decay has incompatible "
                "source=%p/%s resolved-source=%p/%s result=%p/%s "
                "target-base=%p/%s resolved-target-base=%p/%s types\n",
                static_cast<void *>(functionExp), static_cast<void *>(cast),
                static_cast<void *>(source),
                source != nullptr ? source->class_name().c_str() : "<null>",
                static_cast<void *>(resolvedSource),
                resolvedSource != nullptr ? resolvedSource->class_name().c_str()
                                          : "<null>",
                static_cast<void *>(result),
                result != nullptr ? result->class_name().c_str() : "<null>",
                static_cast<void *>(targetBase),
                targetBase != nullptr ? targetBase->class_name().c_str()
                                      : "<null>",
                static_cast<void *>(resolvedTargetBase),
                resolvedTargetBase != nullptr
                    ? resolvedTargetBase->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }
      break;
    }
    case SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr: {
      SgType *result = cast->get_type();
      SgType *strippedResult =
          result != nullptr ? result->stripTypedefsAndModifiers() : nullptr;
      SgType *source = cast->get_operand()->get_type();
      SgType *strippedSource =
          source != nullptr ? source->stripTypedefsAndModifiers() : nullptr;
      SgPointerType *pointerResult = isSgPointerType(strippedResult);
      const bool functionResult = isSgFunctionType(strippedResult) != nullptr;
      const bool functionPointerResult =
          pointerResult != nullptr &&
          pointerResult->get_base_type() != nullptr &&
          isSgFunctionType(
              pointerResult->get_base_type()->stripTypedefsAndModifiers()) !=
              nullptr;
      if (isSgFunctionType(strippedSource) == nullptr ||
          functionResult == functionPointerResult) {
        fprintf(stderr,
                "REX_AST_INVARIANT[function-call-callee-conversion]: builtin "
                "function decay cast=%p operand=%p/%s source=%p/%s "
                "stripped-source=%p/%s result=%p/%s stripped-result=%p/%s "
                "function-result=%d function-pointer-result=%d has no exact "
                "LLVM builtin callee type shape\n",
                static_cast<void *>(cast),
                static_cast<void *>(cast->get_operand()),
                cast->get_operand()->class_name().c_str(),
                static_cast<void *>(cast->get_operand()->get_type()),
                cast->get_operand()->get_type() != nullptr
                    ? cast->get_operand()->get_type()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(strippedSource),
                strippedSource != nullptr ? strippedSource->class_name().c_str()
                                          : "<null>",
                static_cast<void *>(result),
                result != nullptr ? result->class_name().c_str() : "<null>",
                static_cast<void *>(strippedResult),
                strippedResult != nullptr ? strippedResult->class_name().c_str()
                                          : "<null>",
                functionResult ? 1 : 0, functionPointerResult ? 1 : 0);
        ROSE_ABORT();
      }
      break;
    }
    case SgCastExp::e_semantic_conversion_LValueToRValue:
    case SgCastExp::e_semantic_conversion_NoOp:
      if (cast->get_operand()->get_type() == nullptr ||
          SageInterface::containsUnknownType(cast->get_operand()->get_type()) ||
          SageInterface::containsUnknownType(cast->get_type())) {
        fprintf(stderr,
                "REX_AST_INVARIANT[function-call-callee-conversion]: "
                "transparent callee conversion has no exact source/result "
                "type\n");
        ROSE_ABORT();
      }
      break;
    default:
      fprintf(stderr,
              "REX_AST_INVARIANT[function-call-callee-conversion]: implicit "
              "callee cast has unsupported semantic conversion=%d\n",
              static_cast<int>(cast->get_semantic_conversion_kind()));
      ROSE_ABORT();
    }
    functionExpOwner = cast;
    functionExp = cast->get_operand();
    unwrapMacroExpansion();
    unwrapIdentityUnaryOperators();
  }

  switch (functionExp->variantT()) {
  case V_SgPointerDerefExp:
  case V_SgAddressOfOp: {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-call-callee-unary]: callable unary "
            "surface remained after exact identity normalization\n");
    ROSE_ABORT();
  }
  case V_SgFunctionRefExp: {
    SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(functionExp);
    ASSERT_not_null(functionRefExp);
    returnSymbol = functionRefExp->get_symbol();

    ASSERT_not_null(returnSymbol);
    break;
  }

  case V_SgTemplateFunctionRefExp: {
    SgTemplateFunctionRefExp *functionRefExp =
        isSgTemplateFunctionRefExp(functionExp);
    ASSERT_not_null(functionRefExp);
    returnSymbol = functionRefExp->get_symbol();

    ASSERT_not_null(returnSymbol);
    break;
  }

  case V_SgTemplateMemberFunctionRefExp: {
    SgTemplateMemberFunctionRefExp *templateMemberFunctionRefExp =
        isSgTemplateMemberFunctionRefExp(functionExp);
    ASSERT_not_null(templateMemberFunctionRefExp);
    returnSymbol = templateMemberFunctionRefExp->get_symbol();
    isAlwaysResolvedStatically =
        !templateMemberFunctionRefExp->get_virtual_call();

    ASSERT_not_null(returnSymbol);
    break;
  }

  case V_SgMemberFunctionRefExp: {
    SgMemberFunctionRefExp *memberFunctionRefExp =
        isSgMemberFunctionRefExp(functionExp);
    ASSERT_not_null(memberFunctionRefExp);
    returnSymbol = memberFunctionRefExp->get_symbol();
    isAlwaysResolvedStatically = !memberFunctionRefExp->get_virtual_call();

    ASSERT_not_null(returnSymbol);
    break;
  }

  case V_SgLambdaExp: {
    SgLambdaExp *lambdaExp = isSgLambdaExp(functionExp);
    ASSERT_not_null(lambdaExp);
    SgFunctionDeclaration *lambdaFunction = lambdaExp->get_lambda_function();
    ASSERT_not_null(lambdaFunction);
    auto *canonical = isSgFunctionDeclaration(
        lambdaFunction->get_firstNondefiningDeclaration());
    ASSERT_not_null(canonical);
    ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);
    ASSERT_not_null(canonical->get_scope());
    returnSymbol = isSgFunctionSymbol(
        canonical->get_scope()->find_symbol_from_declaration(canonical));
    ASSERT_not_null(returnSymbol);
    ROSE_ASSERT(returnSymbol->get_declaration() == canonical);
    break;
  }

  case V_SgArrowExp:
  case V_SgDotExp: {
    auto *access = isSgBinaryOp(functionExp);
    ASSERT_not_null(access);
    SgExpression *member = access->get_rhs_operand();
    ASSERT_not_null(member);
    if (auto *reference = isSgMemberFunctionRefExp(member)) {
      returnSymbol = reference->get_symbol();
      ASSERT_not_null(returnSymbol);
      isAlwaysResolvedStatically = !reference->get_virtual_call();
    } else if (auto *reference = isSgTemplateMemberFunctionRefExp(member)) {
      returnSymbol = reference->get_symbol();
      ASSERT_not_null(returnSymbol);
      isAlwaysResolvedStatically = !reference->get_virtual_call();
    } else if (auto *reference = isSgFunctionRefExp(member)) {
      returnSymbol = reference->get_symbol();
      ASSERT_not_null(returnSymbol);
    } else if (auto *reference = isSgTemplateFunctionRefExp(member)) {
      returnSymbol = reference->get_symbol();
      ASSERT_not_null(returnSymbol);
    } else if (auto *reference = isSgVarRefExp(member)) {
      requireIndirectCallableSymbol(reference, "member indirect callable");
    } else if (auto *reference = isSgNonrealRefExp(member)) {
      returnSymbol = resolvedNonrealFunctionSymbol(reference);
    } else if (!isPseudoDestructorReference(member)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[associated-function-symbol]: member call "
              "rhs=%p/%s has no typed callable-symbol role\n",
              static_cast<void *>(member), member->class_name().c_str());
      ROSE_ABORT();
    }
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

  case V_SgConstructorInitializer: {
    ASSERT_not_null(functionExp->get_file_info());
    break;
  }

  // A nested call can return a function pointer. Its own callee is not the
  // callee of the outer call.
  case V_SgFunctionCallExp: {
    ASSERT_not_null(functionExp->get_file_info());
    break;
  }

  case V_SgPntrArrRefExp: {
    break;
  }

  case V_SgCastExp: {
    break;
  }

  case V_SgNonrealRefExp: {
    returnSymbol =
        resolvedNonrealFunctionSymbol(isSgNonrealRefExp(functionExp));
    break;
  }
  case V_SgConditionalExp: {
    break;
  }

  case V_SgVarRefExp: {
    requireIndirectCallableSymbol(isSgVarRefExp(functionExp),
                                  "indirect callable");
    break;
  }

  // A dependent non-type template parameter can be the callee of a valid
  // dependent call.  It deliberately has no concrete function declaration or
  // symbol until instantiation, just like a function-pointer variable.
  case V_SgTemplateParameterVal: {
    break;
  }

  case V_SgThisExp: {
    break;
  }
  case V_SgIntVal:
  case V_SgFloatVal:
  case V_SgDoubleVal:
  case V_SgStringVal:
  case V_SgBoolValExp:
  case V_SgCharVal:
  case V_SgNullExpression: {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-call-callee]: non-callable literal "
            "%s cannot be a function-call callee\n",
            functionExp->class_name().c_str());
    ROSE_ABORT();
  }
  case V_SgPseudoDestructorRefExp: {
    // Pseudo-destructor calls don't correspond to a function symbol in the
    // usual sense.
    break;
  }
  default: {
    fprintf(stderr,
            "REX_AST_INVARIANT[function-call-callee]: unsupported callee "
            "node kind %s\n",
            functionExp->class_name().c_str());
    ROSE_ABORT();
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

void preventConstructionOnStack(SgNode *n) {
#if defined(__GNUC__) && !defined(NDEBUG)
  void *frameaddr = __builtin_frame_address(0);
  signed long dist = (char *)n - (char *)frameaddr;
  assert(dist < -10000 || dist > 10000);
#endif
}
