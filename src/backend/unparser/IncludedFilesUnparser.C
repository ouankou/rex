#include "sage3basic.h"

#include <iostream>

#include "CollectionHelper.h"

#include "FileHelper.h"

#include "IncludingPreprocessingInfosCollector.h"

#include "IncludeDirective.h"

#include "IncludedFilesUnparser.h"

#include "unparser.h"

using namespace std;

namespace {
void buildIncludeTreeParentMap(
    SgIncludeFile *includeTreeRoot,
    map<string, set<string>> &includedToIncludingFiles) {
  if (includeTreeRoot == nullptr)
    return;

  vector<SgIncludeFile *> worklist;
  set<SgIncludeFile *> visited;
  worklist.push_back(includeTreeRoot);

  while (!worklist.empty()) {
    SgIncludeFile *includingFile = worklist.back();
    worklist.pop_back();
    if (includingFile == nullptr)
      continue;

    if (!visited.insert(includingFile).second)
      continue;

    const string includingFileName =
        FileHelper::normalizePathIfPossible(includingFile->get_filename());

    const SgIncludeFilePtrList &includeFileList =
        includingFile->get_include_file_list();
    for (size_t i = 0; i < includeFileList.size(); ++i) {
      SgIncludeFile *includedFile = includeFileList[i];
      if (includedFile == nullptr)
        continue;

      worklist.push_back(includedFile);

      const string includedFileName =
          FileHelper::normalizePathIfPossible(includedFile->get_filename());
      if (!includedFileName.empty() && !includingFileName.empty())
        includedToIncludingFiles[includedFileName].insert(includingFileName);
    }
  }
}
} // namespace

const string IncludedFilesUnparser::defaultUnparseFolderName =
    "_rose_unparsed_headers_";

// DQ (2/22/2021): Make these static so that we can refer to them within tools
// to support diffs.
std::map<std::string, std::set<std::string>>
    IncludedFilesUnparser::includingPathsMap;
std::map<std::string, std::string> IncludedFilesUnparser::unparseMap;
std::list<std::pair<int, std::string>>
    IncludedFilesUnparser::includeCompilerPaths;
std::set<std::string> IncludedFilesUnparser::modifiedFiles;
std::set<std::string> IncludedFilesUnparser::filesWithMarkedTransformations;
std::set<std::string> IncludedFilesUnparser::allFiles;
std::set<std::string> IncludedFilesUnparser::filesToUnparse;
std::set<std::string> IncludedFilesUnparser::filesWithUpdatedIncludePaths;
std::set<std::string> IncludedFilesUnparser::filesToCopy;

// DQ (2/23/2021): Make these static so that we can refer to them within tools
// to support diffs.
std::map<std::string, SgSourceFile *>
    IncludedFilesUnparser::unparseSourceFileMap;

// It is needed because otherwise, the default destructor breaks something.

IncludedFilesUnparser::~IncludedFilesUnparser() {
  // do nothing
}

IncludedFilesUnparser::IncludedFilesUnparser(SgProject *projectNode) {
  this->projectNode = projectNode;
}

string IncludedFilesUnparser::getUnparseRootPath() { return unparseRootPath; }

map<string, string> IncludedFilesUnparser::getUnparseMap() {
  return unparseMap;
}

map<string, SgScopeStatement *> IncludedFilesUnparser::getUnparseScopesMap() {
  return unparseScopesMap;
}

map<string, SgSourceFile *> IncludedFilesUnparser::getUnparseSourceFileMap() {
  // DQ (9/7/2018): Added to support retrival of SgSourceFile built in the
  // frontend.
  return unparseSourceFileMap;
}

set<string> IncludedFilesUnparser::getFilesToCopy() {
  // DQ (11/19/2018): Added access function.
  return filesToCopy;
}

list<string> IncludedFilesUnparser::getIncludeCompilerOptions() {
  list<string> includeCompilerOptions;
  for (list<pair<int, string>>::const_iterator it =
           includeCompilerPaths.begin();
       it != includeCompilerPaths.end(); it++) {
    includeCompilerOptions.push_back("-I" + it->second);
  }
  return includeCompilerOptions;
}

// void IncludedFilesUnparser::unparse()
void IncludedFilesUnparser::figureOutWhichFilesToUnparse() {
  // This function does not unparse any files, but identified which included
  // files will require unparsing (in addition to the original input source
  // file).

  // DQ (4/6/2020): Added assertion.
  ROSE_ASSERT(projectNode != NULL);

  // The project's including-preprocessing-info map is derived data that can
  // become stale after transformations (e.g. include directive deletion during
  // outlining). Rebuild it from the current AST to avoid dangling
  // PreprocessingInfo* pointers.
  {
    map<string, set<string>> emptyIncludedFilesMap;
    IncludingPreprocessingInfosCollector collector(projectNode,
                                                   emptyIncludedFilesMap);
    projectNode->set_includingPreprocessingInfosMap(collector.collect());
  }

#define DEBUG_FIGURE_OUT 0

#if DEBUG_FIGURE_OUT
  printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
  printf("In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): \n");
  printf(" --- projectNode->usingDeferredTransformations = %s \n",
         projectNode->get_usingDeferredTransformations() ? "true" : "false");
  printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
#endif

  workingDirectory = FileHelper::normalizePath(
      (*projectNode->get_fileList().begin())->getWorkingDirectory());
  string userSpecifiedUnparseRootFolder =
      projectNode->get_unparseHeaderFilesRootFolder();
  if (userSpecifiedUnparseRootFolder.empty() == true) {
    // No folder specified, use the default location.
    unparseRootPath = FileHelper::concatenatePaths(workingDirectory,
                                                   defaultUnparseFolderName);
  } else {
    if (FileHelper::isAbsolutePath(userSpecifiedUnparseRootFolder)) {
      unparseRootPath = userSpecifiedUnparseRootFolder;
    } else {
      unparseRootPath = FileHelper::concatenatePaths(
          workingDirectory, userSpecifiedUnparseRootFolder);
    }

    // Check that the specified location does not exist or is empty. This is
    // necessary to avoid data loss since this folder will be erased.
    if (FileHelper::isNotEmptyFolder(unparseRootPath)) {
      // DQ (1/29/2018): This case happens when running ROSE from the command
      // line and maybe we should automate the removal of this directory.
      cout << "Please make sure that the root folder for header files "
              "unparsing does not exist or is empty:"
           << unparseRootPath << endl;
      ROSE_ABORT();
    }
  }

  // Should be erased completely at every run to avoid name collisions with
  // previous runs.
  FileHelper::eraseFolder(unparseRootPath);
  filesWithMarkedTransformations.clear();
  filesWithUpdatedIncludePaths.clear();

  // collect immediately affected files as well as all traversed files

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("unparseAllHeaderFiles == false: calling traversal to determine "
           "modified header files \n");
  }

  // DQ (11/28/2018): I think the order of the traversal should be postorder
  // instead of preorder, because we sometimes mark the enclosing statement tn
  // as modified.  Note: the travesal sets the allFiles list.
  traverse(projectNode, preorder);

  // DQ (4/6/2020): We need a way to know when we want to trigger unparsing of
  // all header files.
  bool unparseAllHeaderFiles =
      (projectNode->get_usingDeferredTransformations() == false);

  // DQ (4/8/2020): if we are not using the defered evaluation (default) then
  // the default behavior is to unparse all header files.
  if (unparseAllHeaderFiles == true) {
    // DQ (4/14/2020): Commented out (valid for test6 but not test0.
    // ROSE_ASSERT(modifiedFiles.empty() == true);

    // modifiedFiles = allFiles;
    set<string>::iterator i = allFiles.begin();
    while (i != allFiles.end()) {
      string filename = *i;
      if (unparseSourceFileMap.find(filename) == unparseSourceFileMap.end()) {
        // #if 1
        // DQ (4/13/2020): Added header file unparsing feature specific debug
        // level.
        if (SgProject::get_unparseHeaderFilesDebug() >= 2) {
          printf("Adding filename = %s to modifiedFiles (IS a header file) \n",
                 filename.c_str());
        }
        // #endif
        modifiedFiles.insert(filename);
      } else {
      }

      i++;
    }

    // DQ (4/6/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 2) {
      printf("unparseAllHeaderFiles == true: set modifiedFiles = allFiles: "
             "modifiedFiles.size() = %zu \n",
             modifiedFiles.size());
      printf(" --- allFiles.size() = %zu \n", allFiles.size());
    }
  }

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): "
           "unparseAllHeaderFiles = %s \n",
           unparseAllHeaderFiles ? "true" : "false");
  }

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("List allFiles list: processing parent include files chain: (size = "
           "%zu): \n",
           allFiles.size());
  }

  // DQ (11/30/2019): Process the header files to include possible header files
  // that only contained another header files (and so are not supported within
  // the traversal).  This addresses at least test11 in the UnparseHeadersTest
  // directory.
  map<string, set<string>> includedToIncludingFiles;
  const SgFilePtrList &fileList = projectNode->get_fileList();
  for (SgFilePtrList::const_iterator file = fileList.begin();
       file != fileList.end(); ++file) {
    SgSourceFile *sourceFile = isSgSourceFile(*file);
    if (sourceFile == nullptr)
      continue;

    buildIncludeTreeParentMap(sourceFile->get_associated_include_file(),
                              includedToIncludingFiles);
  }

  const map<string, set<PreprocessingInfo *>> &includingPreprocessingInfosMap =
      projectNode->get_includingPreprocessingInfosMap();

  set<string> workSet = allFiles;
  while (!workSet.empty()) {
    set<string> newParents;
    for (const string &filename : workSet) {
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        printf("   --- allFiles entry = %s \n", filename.c_str());
      }

      const string normalizedFilename =
          FileHelper::normalizePathIfPossible(filename);

      map<string, set<string>>::const_iterator includeTreeEntry =
          includedToIncludingFiles.find(normalizedFilename);
      if (includeTreeEntry == includedToIncludingFiles.end())
        includeTreeEntry = includedToIncludingFiles.find(filename);
      if (includeTreeEntry != includedToIncludingFiles.end()) {
        for (const string &includingFileName : includeTreeEntry->second) {
          if (!FileHelper::fileExists(includingFileName))
            continue;
          if (allFiles.insert(includingFileName).second)
            newParents.insert(includingFileName);
        }
      }

      map<string, set<PreprocessingInfo *>>::const_iterator mapEntry =
          includingPreprocessingInfosMap.find(normalizedFilename);
      if (mapEntry == includingPreprocessingInfosMap.end())
        mapEntry = includingPreprocessingInfosMap.find(filename);
      if (mapEntry != includingPreprocessingInfosMap.end()) {
        const set<PreprocessingInfo *> &includingPreprocessingInfos =
            mapEntry->second;
        for (set<PreprocessingInfo *>::const_iterator
                 includingPreprocessingInfoPtr =
                     includingPreprocessingInfos.begin();
             includingPreprocessingInfoPtr != includingPreprocessingInfos.end();
             includingPreprocessingInfoPtr++) {
          string normalizedIncludingFileName =
              FileHelper::getNormalizedContainingFileName(
                  *includingPreprocessingInfoPtr);
          if (allFiles.insert(normalizedIncludingFileName).second)
            newParents.insert(normalizedIncludingFileName);
        }
      }
    }
    workSet.swap(newParents);
  }

  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("List allFiles list (size = %zu): \n", allFiles.size());
    for (set<string>::iterator i = allFiles.begin(); i != allFiles.end(); i++) {
      printf("   --- allFiles = %s \n", (*i).c_str());
    }
  }

#if DEBUG_FIGURE_OUT
  // DQ (4/6/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("List modifiedFiles list (size = %zu): \n", modifiedFiles.size());
    set<string>::iterator j = modifiedFiles.begin();
    size_t modified_file_counter = 0;
    while (j != modifiedFiles.end()) {
      printf("   --- modifiedFiles[%zu] = %s \n", modified_file_counter,
             (*j).c_str());

      j++;
      modified_file_counter++;
    }
  }
#endif

  initializeFilesToUnparse();

  // A more efficient way would be to do it incrementally rather than repeating
  // the whole iteration. But the probability of more than one iteration is
  // extremely low, so an average overhead is very insignificant.
  do {
    prepareForNewIteration();

    // DQ (11/23/2019): Forse the false branch as part of testing.
    // DQ (11/19/2018): When using the token-based unpasing we don't modify
    // include paths in the source or header files and so we don't need to
    // unparse as large of a set of header files as when the unparse_tokens
    // option is NOT used. if (projectNode->get_unparse_tokens() == false) if
    // (projectNode->get_unparse_tokens() == false &&
    // projectNode->get_unparseHeaderFiles() == false)
    if (projectNode->get_unparse_tokens() == false && false) {
      collectAdditionalFilesToUnparse();
      printf("Exiting as a test! \n");
      ROSE_ABORT();
    } else {
      // We need to for a list of header files to copy, since we can't use the
      // original source directory location as an include path because then we
      // can't pick up the unparsed header files. DQ (4/13/2020): Added header
      // file unparsing feature specific debug level.
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        printf("$$$$$$$$$$$$ Calling collectAdditionalFilesToUnparse() "
               "$$$$$$$$$$$$ \n");
      }
      collectAdditionalListOfHeaderFilesToCopy();
    }

    // DQ (4/14/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf(
          "In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): calling "
          "applyFunctionToIncludingPreprocessingInfos(filesToUnparse) \n");
    }

    applyFunctionToIncludingPreprocessingInfos(
        filesToUnparse,
        &IncludedFilesUnparser::collectIncludingPathsFromUnaffectedFiles);

    populateUnparseMap();

    collectIncludeCompilerPaths();

    // DQ (4/14/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("In IncludedFilesUnparser::figureOutWhichFilesToUnparse(): "
             "calling applyFunctionToIncludingPreprocessingInfos(allFiles) \n");
    }

    // DQ (4/15/2020): I don't think this is the cause.
    // DQ (4/14/2020): This causes a problem for the test8 regression test.
    applyFunctionToIncludingPreprocessingInfos(
        allFiles, &IncludedFilesUnparser::collectNotUnparsedPreprocessingInfos);

    collectNotUnparsedFilesThatRequireUnparsingToAvoidFileNameCollisions();

    if (SgProject::get_verbose() > 0) {
      CollectionHelper::printSet(
          newFilesToUnparse,
          "\nAdditional files to unparse due to path conflicts:", "");
      cout << endl << endl;
    }
    // DQ (4/13/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("At bottom of DO WHILE loop: newFilesToUnparse.size() = %zu \n",
             newFilesToUnparse.size());
    }
  } while (!newFilesToUnparse.empty());

  // DQ (11/13/2018): If we are unparsing from the token stream, then we can't
  // be modifying the include directives. This is also an issue because the
  // #include directives are a part of the white space, and thus transformations
  // of then can cause them to be unparsed twice (e.g. test9 in
  // UnparseHeader_tests).  Also, modicication of the include directives can
  // trigger unparsing from the AST (which would not otherwise be required).
  if (projectNode->get_unparse_tokens() == false) {
    applyFunctionToIncludingPreprocessingInfos(
        allFiles, &IncludedFilesUnparser::updatePreprocessingInfoPaths);
  }

  for (list<pair<int, string>>::const_iterator it =
           includeCompilerPaths.begin();
       it != includeCompilerPaths.end(); it++) {
    FileHelper::ensureFolderExists(it->second);
  }

#if DEBUG_FIGURE_OUT
  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 0) {
    printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
    printf("Leaving IncludedFilesUnparser::figureOutWhichFilesToUnparse(): \n");
    printf("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \n");
  }
#endif

  // DQ (4/5/2020): Exit as part of debugging.
}

void IncludedFilesUnparser::printDiagnosticOutput() {
  if (SgProject::get_verbose() >= 0) {
    printf("In IncludedFilesUnparser::printDiagnosticOutput(): Output internal "
           "data \n");
    CollectionHelper::printSet(allFiles, "\nAll files:", "");
    CollectionHelper::printSet(modifiedFiles, "\nModified files:", "");

    // DQ (10/9/2019): Added debugging support, this is not one of the lists we
    // support. CollectionHelper::printSet(modifiedIncludeFiles, "\nModified
    // files:", "");

    CollectionHelper::printSet(filesToUnparse, "\nFiles to unparse:", "");

    CollectionHelper::printSet(filesToCopy, "\nCopy files:", "");

    CollectionHelper::printMapOfSets(
        includingPathsMap,
        "\nIncluding paths map:", "Included file:", "Including path:");

    for (map<string, string>::const_iterator it = unparseMap.begin();
         it != unparseMap.end(); it++) {
      cout << "Unparsed file:" << it->first << "\nDestination:" << it->second
           << endl
           << endl;
    }

    cout << "\nInclude compiler paths:" << endl;
    for (list<pair<int, string>>::const_iterator it =
             includeCompilerPaths.begin();
         it != includeCompilerPaths.end(); it++) {
      cout << it->first << ":" << it->second << endl;
    }

    cout << endl << endl;
  }
}

void IncludedFilesUnparser::prepareForNewIteration() {

  filesToUnparse.insert(newFilesToUnparse.begin(), newFilesToUnparse.end());
  newFilesToUnparse.clear();
  includingPathsMap.clear();
  notUnparsedPreprocessingInfos.clear();
  unparseMap.clear();
  unparsePaths.clear();
  includeCompilerPaths.clear();

  // The unparse root path is always included (though could be redundant if no
  // included files need unparsing).
  addIncludeCompilerPath(0, unparseRootPath);
}

bool IncludedFilesUnparser::isInputFile(const string &absoluteFileName) {
  const SgFilePtrList &fileList = projectNode->get_fileList();
  for (size_t i = 0; i < fileList.size(); ++i) {
    if (absoluteFileName.compare(fileList[i]->getFileName()) == 0) {
      return true;
    }
  }
  return false;
}

void IncludedFilesUnparser::
    collectNotUnparsedFilesThatRequireUnparsingToAvoidFileNameCollisions() {
  newFilesToUnparse.clear();
  for (set<PreprocessingInfo *>::const_iterator preprocessingInfoPtr =
           notUnparsedPreprocessingInfos.begin();
       preprocessingInfoPtr != notUnparsedPreprocessingInfos.end();
       preprocessingInfoPtr++) {
    IncludeDirective includeDirective((*preprocessingInfoPtr)->getString());
    const string &includePath = includeDirective.getIncludedPath();
    if (isConflictingIncludePath(includePath)) {
      newFilesToUnparse.insert(
          FileHelper::getNormalizedContainingFileName(*preprocessingInfoPtr));
    }
  }
}

bool IncludedFilesUnparser::isConflictingIncludePath(
    const string &includePath) {
  for (list<pair<int, string>>::const_iterator includeCompilerPathsIterator =
           includeCompilerPaths.begin();
       includeCompilerPathsIterator != includeCompilerPaths.end();
       includeCompilerPathsIterator++) {
    const string &potentialIncludedFilePath = FileHelper::concatenatePaths(
        includeCompilerPathsIterator->second, includePath);
    if (FileHelper::fileExists(potentialIncludedFilePath)) {
      // This is a conflict with an existing file.
      return true;
    }
    for (set<string>::const_iterator unparsePathPtr = unparsePaths.begin();
         unparsePathPtr != unparsePaths.end(); unparsePathPtr++) {
      const string &unparsedIncludedFilePath =
          FileHelper::concatenatePaths(unparseRootPath, *unparsePathPtr);
      if (FileHelper::areEquivalentPaths(potentialIncludedFilePath,
                                         unparsedIncludedFilePath)) {
        // This is a conflict with a file that will be unparsed.
        return true;
      }
    }
  }
  return false;
}

// TODO: Probably this would not handle correctly cases like #include
// <../subdir/../A.h> because the normalized representation would be
//  <../A.h> and thus, "subdir" would not be created and the file would not be
//  found by the preprocessor. Check and fix, if needed.
void IncludedFilesUnparser::collectIncludeCompilerPaths() {
  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In IncludedFilesUnparser::collectIncludeCompilerPaths(): "
           "includingPathsMap.size() = %zu \n",
           includingPathsMap.size());
  }

  for (map<string, set<string>>::const_iterator mapEntry =
           includingPathsMap.begin();
       mapEntry != includingPathsMap.end(); mapEntry++) {
    string fileToUnparse = mapEntry->first;
    // DQ (4/13/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf(" --- In loop over includingPathsMap: fileToUnparse = %s \n",
             fileToUnparse.c_str());
    }
    map<string, string>::const_iterator unparseMapEntry =
        unparseMap.find(fileToUnparse);
    ROSE_ASSERT(unparseMapEntry != unparseMap.end());
    string commonPath = unparseMapEntry->second;
    const set<string> &includingPaths = mapEntry->second;
    for (set<string>::const_iterator includingPathPtr = includingPaths.begin();
         includingPathPtr != includingPaths.end(); includingPathPtr++) {
      string textualPathPart = FileHelper::getTextualPart(*includingPathPtr);
      size_t startPos = commonPath.rfind(textualPathPart);
      ROSE_ASSERT(startPos != string::npos);
      if (startPos != 0) {
        startPos--; // If did not match the whole commonPath, consider that path
                    // delimiter should also be removed
      }
      string includeCompilerPath = commonPath.substr(0, startPos);
      int upFolderCount = FileHelper::countUpsToParentFolder(*includingPathPtr);
      for (int i = 0; i < upFolderCount; i++) {
        includeCompilerPath = FileHelper::concatenatePaths(
            includeCompilerPath, defaultUnparseFolderName);
      }
      addIncludeCompilerPath(
          upFolderCount,
          FileHelper::concatenatePaths(unparseRootPath, includeCompilerPath));
    }
  }

  // DQ (4/13/2020): Added header file unparsing feature specific debug level.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("Leaving IncludedFilesUnparser::collectIncludeCompilerPaths(): "
           "includingPathsMap.size() = %zu \n",
           includingPathsMap.size());
  }
}

void IncludedFilesUnparser::addIncludeCompilerPath(
    int upFolderCount, const string &includeCompilerPath) {
  list<pair<int, string>>::iterator includeCompilerPathsIterator;
  // First, check if this include path is already present
  for (includeCompilerPathsIterator = includeCompilerPaths.begin();
       includeCompilerPathsIterator != includeCompilerPaths.end();
       includeCompilerPathsIterator++) {
    if (includeCompilerPath.compare(includeCompilerPathsIterator->second) ==
        0) {
      if (includeCompilerPathsIterator->first >= upFolderCount) {
        return; // This path is present with an equal or greater priority,
                // nothing to do
      } else {
        // This path is present with a lower priority, so remove it and proceed
        // in a regular way.
        includeCompilerPaths.erase(includeCompilerPathsIterator);
        break;
      }
    }
  }

  // If the path is not already present with a sufficiently high priority,
  // insert it at a position corresponding to its priority.
  pair<int, string> newIncludeCompilerPathsEntry(upFolderCount,
                                                 includeCompilerPath);
  includeCompilerPathsIterator = includeCompilerPaths.begin();
  while (includeCompilerPathsIterator != includeCompilerPaths.end()) {
    if (includeCompilerPathsIterator->first <= upFolderCount) {
      includeCompilerPaths.insert(includeCompilerPathsIterator,
                                  newIncludeCompilerPathsEntry);
      break;
    }
    includeCompilerPathsIterator++;
  }

  if (includeCompilerPathsIterator == includeCompilerPaths.end()) {
    // Iterated till the end, which means that the right place to insert was not
    // found, therefore append to the end.
    includeCompilerPaths.push_back(newIncludeCompilerPathsEntry);
  }
}

void IncludedFilesUnparser::updatePreprocessingInfoPaths(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {
  ASSERT_not_null(includingPreprocessingInfo);

  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In updatePreprocessingInfoPaths(): includedFile = %s \n",
           includedFile.c_str());
    printf(" --- includingPreprocessingInfo->getString() = %s \n",
           includingPreprocessingInfo->getString().c_str());
  }

  string normalizedIncludingFileName =
      FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);

  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In updatePreprocessingInfoPaths(): normalizedIncludingFileName = "
           "%s \n",
           normalizedIncludingFileName.c_str());

    printf("In updatePreprocessingInfoPaths(): filesToUnparse: \n");
    set<string>::const_iterator fileToUnparsePtr = filesToUnparse.begin();
    while (fileToUnparsePtr != filesToUnparse.end()) {
      printf(" --- *fileToUnparsePtr = %s \n", fileToUnparsePtr->c_str());
      fileToUnparsePtr++;
    }
  }

  if (filesToUnparse.find(normalizedIncludingFileName) !=
      filesToUnparse.end()) {
    // update include paths only in the unparsed files

    if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
      printf("In updatePreprocessingInfoPaths(): unparseMap: \n");
      map<string, string>::const_iterator unparseMapPtr = unparseMap.begin();
      while (unparseMapPtr != unparseMap.end()) {
        printf(" --- *unparseMapPtr first = %s second = %s \n",
               unparseMapPtr->first.c_str(), unparseMapPtr->second.c_str());
        unparseMapPtr++;
      }
    }
    map<string, string>::const_iterator includedFileUnparseMapEntry =
        unparseMap.find(includedFile);
    string replacementIncludeString;

    if (includedFileUnparseMapEntry != unparseMap.end()) {
      // Included file is unparsed, make the include directive bracketed and
      // relative to the unparse root.
      replacementIncludeString =
          "<" + includedFileUnparseMapEntry->second + ">";
    } else {
      // Included file is not unparsed, make the include directive quoted and
      // relative to the unparsed including file's containing folder.
      string includingFileUnparseFolder;
      if (isInputFile(normalizedIncludingFileName)) {
        // TODO: Currently, all input files are unparsed into the working
        // directory regardless of where they come from. If this is changed
        // (e.g. input files are unparsed in the folders of the original files),
        // use the commented part.
        includingFileUnparseFolder = workingDirectory;

        //                //Unparsed and original input files are in the same
        //                folder, so reuse the initial path: the file name of
        //                the unparsed
        //                //input file would be different, but this does not
        //                matter since we get its parent folder, which would be
        //                the same. includingFileUnparsePath =
        //                FileHelper::getParentFolder(normalizedIncludingFileName);
      } else {
        map<string, string>::const_iterator includingFileUnparseMapEntry =
            unparseMap.find(normalizedIncludingFileName);
        ROSE_ASSERT(includingFileUnparseMapEntry != unparseMap.end());
        includingFileUnparseFolder =
            FileHelper::getParentFolder(FileHelper::concatenatePaths(
                unparseRootPath, includingFileUnparseMapEntry->second));
      }

      replacementIncludeString = "\"" +
                                 FileHelper::getRelativePath(
                                     includingFileUnparseFolder, includedFile) +
                                 "\"";
    }

    string includeString = includingPreprocessingInfo->getString();
    const string originalIncludeString = includeString;
    if (SgProject::get_verbose() >= 0) {
      cout << "Original include string:" << includeString << endl;
    }

    IncludeDirective includeDirective(includeString);
    // Replace the original include directive with the new one, using a relative
    // path and brackets.
    includeString.replace(includeDirective.getStartPos() - 1,
                          includeDirective.getIncludedPath().size() + 2,
                          replacementIncludeString);
    includingPreprocessingInfo->setString(includeString);
    if (includeString != originalIncludeString) {
      filesWithUpdatedIncludePaths.insert(normalizedIncludingFileName);
    }
    if (SgProject::get_verbose() >= 0) {
      cout << "Updated include string:"
           << includingPreprocessingInfo->getString() << endl;
    }
  }

  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("Leaving updatePreprocessingInfoPaths(): includedFile = %s \n",
           includedFile.c_str());
  }
}

void IncludedFilesUnparser::populateUnparseMap() {

  // First, process files that need to go to a specific location
  for (map<string, set<string>>::const_iterator mapEntry =
           includingPathsMap.begin();
       mapEntry != includingPathsMap.end(); mapEntry++) {
    string fileToUnparse = mapEntry->first;
    const set<string> &includingPaths = mapEntry->second;
    set<string>::const_iterator includingPathsIterator = includingPaths.begin();
    string commonPath = *includingPathsIterator;
    includingPathsIterator++;
    while (includingPathsIterator != includingPaths.end()) {
      commonPath =
          FileHelper::pickMoreGeneralPath(commonPath, *includingPathsIterator);
      includingPathsIterator++;
    }

    unparseMap.insert(pair<string, string>(fileToUnparse, commonPath));
    ROSE_ASSERT(
        unparsePaths.find(commonPath) ==
        unparsePaths
            .end()); // check that all paths are indeed unique as expected
    unparsePaths.insert(commonPath);
  }

  // Next, proceed with all other files that will be unparsed
  for (set<string>::const_iterator fileToUnparsePtr = filesToUnparse.begin();
       fileToUnparsePtr != filesToUnparse.end(); fileToUnparsePtr++) {
    if (unparseMap.find(*fileToUnparsePtr) == unparseMap.end()) {
      // consider only files for which the unparse path was not set yet
      string fileName = FileHelper::getFileName(*fileToUnparsePtr);
      // Ensure that the unparse path (in this case - the file name) is unique
      // among all other unparse paths.
      string unparseFileName = fileName;
      int i = 1;
      while (unparsePaths.find(unparseFileName) != unparsePaths.end()) {
        stringstream i_str;
        i_str << i;
        unparseFileName = "rose" + i_str.str() + "_" + fileName;
        i++;
      }
      unparseMap.insert(
          pair<string, string>(*fileToUnparsePtr, unparseFileName));
      unparsePaths.insert(unparseFileName);
    }
  }
}

void IncludedFilesUnparser::collectNotUnparsedPreprocessingInfos(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {
  string normalizedIncludingFileName =
      FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
  if (filesToUnparse.find(includedFile) == filesToUnparse.end() &&
      filesToUnparse.find(normalizedIncludingFileName) ==
          filesToUnparse.end()) {
    // If both the included and the including files are NOT unparsed, collect
    // the including PreprocessingInfo.
    notUnparsedPreprocessingInfos.insert(includingPreprocessingInfo);
  }
}

void IncludedFilesUnparser::collectIncludingPathsFromUnaffectedFiles(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {
  string normalizedIncludingFileName =
      FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
  if (filesToUnparse.find(normalizedIncludingFileName) ==
      filesToUnparse.end()) {
    IncludeDirective includeDirective(includingPreprocessingInfo->getString());
    map<string, set<string>>::iterator mapEntry =
        includingPathsMap.find(includedFile);
    if (mapEntry != includingPathsMap.end()) {
      (mapEntry->second).insert(includeDirective.getIncludedPath());
    } else {
      set<string> includingPaths;
      includingPaths.insert(includeDirective.getIncludedPath());
      includingPathsMap.insert(
          pair<string, set<string>>(includedFile, includingPaths));
    }
  }
}

void IncludedFilesUnparser::initializeFilesToUnparse() {

#define DEBUG_INITIALIZER_FILES_TO_UNPARSE 0

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf("In initializeFilesToUnparse(): filesToUnparse.size() = %zu \n",
         filesToUnparse.size());
  printf(" --- modifiedFiles.size()                           = %zu \n",
         modifiedFiles.size());
#endif

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  if (modifiedFiles.size() > 0) {
    printf("List of modifiedFiles: \n");
    std::set<std::string>::iterator i = modifiedFiles.begin();
    while (i != modifiedFiles.end()) {
      string filename = *i;
      printf(" --- modifiedFiles: %s \n", filename.c_str());
      i++;
    }
  }
#endif

  // DQ (8/20/2019): Collect the comments and CPP directives of the modified
  // header files so that they can be unparsed. SgSourceFile* file = NULL;
  ASSERT_not_null(projectNode);
  SgSourceFile *file = isSgSourceFile(&(projectNode->get_file(0)));

  ASSERT_not_null(file);

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf(" --- file->get_header_file_unparsing_optimization() = %s \n",
         file->get_header_file_unparsing_optimization() ? "true" : "false");
#endif

  if (file->get_header_file_unparsing_optimization() == true) {
    // DQ (4/24/2021): Debugging header file optimization.
    // file->set_header_file_unparsing_optimization_header_file(true);

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("Perform collection of comments and CPP directives only on the "
           "header files \n");
    printf("####################################################### \n");
    printf("Processing comments and CPP directives for header files \n");
    printf("####################################################### \n");
#endif

    // Iterate over the modified files and collect comments and CPP directives
    // for any header files.
    std::set<SgIncludeFile *> modifiedIncludeFiles;
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("modifiedFiles.size() = %zu \n", modifiedFiles.size());
    printf("Initializing modifiedIncludeFiles.size() = %zu \n",
           modifiedIncludeFiles.size());
#endif
    // std::map<std::string, SgSourceFile*> unparseSourceFileMap;
    // std::set<std::string> modifiedFiles;
    std::set<std::string>::iterator i = modifiedFiles.begin();

    while (i != modifiedFiles.end()) {
      string filename = *i;
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
      printf("Iterating over modifiedFiles: Calling function to collect "
             "comments and CPP directives from filename = %s \n",
             filename.c_str());
#endif
      if (unparseScopesMap.find(filename) != unparseScopesMap.end()) {
        SgScopeStatement *scope = unparseScopesMap[filename];
        ASSERT_not_null(scope);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Found entry in unparseScopesMap: scope = %p = %s \n", scope,
               scope->class_name().c_str());
#endif
      } else {
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Entry not found in unparseScopesMap \n");
#endif
      }

      SgSourceFile *sourceFile = NULL;
      if (unparseSourceFileMap.find(filename) != unparseSourceFileMap.end()) {
        // SgSourceFile* sourceFile = unparseSourceFileMap[filename];
        sourceFile = unparseSourceFileMap[filename];
        ASSERT_not_null(sourceFile);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Found entry in unparseSourceFileMap: sourceFile = %p = %s \n",
               sourceFile, sourceFile->class_name().c_str());
#endif
      } else {
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Entry not found in unparseSourceFileMap \n");
#endif
      }

      // ASSERT_not_null(sourceFile);

      if (sourceFile != NULL) {
        ASSERT_not_null(sourceFile);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("sourceFile->getFileName()      = %s \n",
               sourceFile->getFileName().c_str());
        printf("sourceFile->get_isHeaderFile() = %s \n",
               sourceFile->get_isHeaderFile() ? "true" : "false");

        printf("Check if this is a header file: if so add it to the "
               "modifiedIncludeFiles list \n");
#endif
        if (sourceFile->get_isHeaderFile() == true) {
          SgNode *parent2 = sourceFile->get_parent();
          ASSERT_not_null(parent2);
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
          printf("parent2 = %p = %s \n", parent2,
                 parent2->class_name().c_str());
#endif
          SgIncludeFile *includeFile =
              isSgIncludeFile(sourceFile->get_parent());
          ASSERT_not_null(includeFile);

          modifiedIncludeFiles.insert(includeFile);
        }

      } else {
        // DQ (10/14/2019): This should be the case of a non-header file
        // (sometimes this is another generated file source file that was
        // modified).
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("sourceFile == NULL \n");
#endif
      }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
      printf("modifiedIncludeFiles.size() = %zu \n",
             modifiedIncludeFiles.size());
#endif
      i++;
    }

    // DQ (11/16/2019): We only want to unparse header files if they were
    // modified, and if they were modified then they should have had their CPP
    // directives and comments attached before being transformed. So this is too
    // late in the process.  In some cases (such as the transformations in the
    // regression tests) the transforamtions only change the name of a variable,
    // and so attaching teh comments this late is not an issue.  However, in
    // order to know when we will have to unparse a header file we either alwasy
    // unparse them (which is too slow) or we only unparse the header files that
    // will be transformed and then we have to attached the CPP directives and
    // comments before the transformation.  This is the concept of deferred
    // transformations (an option for the outliner).

    // DQ (10/9/2019): We only want to process the header files identified as
    // having been modified.
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf(
        "######################################################################"
        "############################################################### \n");
    printf("Iterate over the modified header files and process them to attach "
           "comments and CPP directives: modifiedIncludeFiles.size() = %zu \n",
           modifiedIncludeFiles.size());
    printf(
        "######################################################################"
        "############################################################### \n");
#endif
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("projectNode->get_usingDeferredTransformations() = %s \n",
           projectNode->get_usingDeferredTransformations() ? "true" : "false");
#endif
    // DQ (10/17/2020): This code processes the comments and CPP directives for
    // the files when get_usingDeferredTransformations() == false. But it might
    // be clearer to just always process the files independent of
    // get_usingDeferredTransformations() being true or false. if
    // (projectNode->get_usingDeferredTransformations() == false)
    {
      std::set<SgIncludeFile *>::iterator includeFileIterator =
          modifiedIncludeFiles.begin();

      while (includeFileIterator != modifiedIncludeFiles.end()) {
        SgIncludeFile *includeFile = *includeFileIterator;
        ASSERT_not_null(includeFile);

        string filename = includeFile->get_filename();
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Iterating over modifiedIncludeFiles: Calling function to "
               "collect comments and CPP directives from filename = %s \n",
               filename.c_str());
#endif
        SgSourceFile *sourceFile =
            isSgSourceFile(includeFile->get_source_file());
        ASSERT_not_null(sourceFile);

        // DQ (10/11/2019): This is required to be set when using the header
        // file optimization (tested in
        // AttachPreprocessingInfoTreeTrav::evaluateInheritedAttribute()).
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
        printf("Setting "
               "sourceFile->set_header_file_unparsing_optimization_header_file("
               "true), but it should have been set previously, I think! \n");
#endif
        // DQ (4/24/2021): Debugging header file optimization.
        sourceFile->set_header_file_unparsing_optimization_header_file(true);

        // DQ (4/1/2021): We need to set the unparse_tokens flag so that the
        // token mapping will be generated for the header files.
        if (file->get_unparse_tokens() == true) {
          sourceFile->set_unparse_tokens(true);
        }

        // DQ (10/11/2019): This is required to be set when using the header
        // file optimization (tested in
        // AttachPreprocessingInfoTreeTrav::evaluateInheritedAttribute()).
        ROSE_ASSERT(
            sourceFile->get_header_file_unparsing_optimization_header_file() ==
            true);

        // DQ (10/21/2019): This will be tested below, in
        // secondaryPassOverSourceFile(), if it is not in place then we need to
        // do it here. ROSEAttributesListContainerPtr filePreprocInfo =
        // sourceFile->get_preprocessorDirectivesAndCommentsList();

        // DQ (10/18/2020): Let this trigger when to process the CPP directives
        // and comments.
        if (sourceFile->get_processedToIncludeCppDirectivesAndComments() ==
            false) {
          // DQ (10/18/2020): This is enforced within
          // secondaryPassOverSourceFile() and attachPreprocessingInfo(), so
          // move the enforcement to be as early as possible.
          ROSE_ASSERT(
              sourceFile->get_processedToIncludeCppDirectivesAndComments() ==
              false);

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE || 0
          printf("In initializeFilesToUnparse(): sourceFile = %p name = %s "
                 "Calling file->secondaryPassOverSourceFile() \n",
                 sourceFile, sourceFile->getFileName().c_str());
#endif
          // DQ (4/22/2020): Location of call to insert redundant comments and
          // CPP directives.
          sourceFile->secondaryPassOverSourceFile();

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE || 0
          printf("DONE: In initializeFilesToUnparse(): sourceFile = %p name = "
                 "%s Calling file->secondaryPassOverSourceFile() \n",
                 sourceFile, sourceFile->getFileName().c_str());
#endif
        } else {
#if DEBUG_INITIALIZER_FILES_TO_UNPARSE || 0
          printf("In IncludedFilesUnparser::initializeFilesToUnparse(): "
                 "(already processed sourceFile = %p = %s) \n",
                 sourceFile, sourceFile->getFileName().c_str());
#endif
        }

        includeFileIterator++;
      }
    }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
    printf("In initializeFilesToUnparse(): file = %p = %s name = %s Calling "
           "file->set_header_file_unparsing_optimization_header_file(false) \n",
           file, file->class_name().c_str(), file->getFileName().c_str());
#endif
    // DQ (4/24/2021): Debugging header file optimization.
    // DQ (9/19/2019): Unclear to me why we want to set this to false, or if we
    // are doing so for the correct file.
    // file->set_header_file_unparsing_optimization_header_file(false);
  }

  // All modified files have to be unparsed.
  filesToUnparse = modifiedFiles;

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf("In initializeFilesToUnparse(): initialized with modifiedFiles: "
         "filesToUnparse.size() = %zu \n",
         filesToUnparse.size());
#endif

  // All input files are also unparsed by default.
  SgFilePtrList inputFilesList = projectNode->get_fileList();
  for (SgFilePtrList::const_iterator inputFilePtr = inputFilesList.begin();
       inputFilePtr != inputFilesList.end(); inputFilePtr++) {
    filesToUnparse.insert(FileHelper::normalizePath(
        (*inputFilePtr)->getFileName())); // normalize just in case it is not
                                          // normalized by default as expected
  }

#if DEBUG_INITIALIZER_FILES_TO_UNPARSE
  printf("Leaving initializeFilesToUnparse(): filesToUnparse.size() = %zu \n",
         filesToUnparse.size());
#endif
}

void IncludedFilesUnparser::collectAdditionalFilesToUnparse() {
  // Recursively add to filesToUnparse set any file that includes using quotes
  // (or an absolute path) at least one of the files that is already in
  // filesToUnparse set.
  set<string> workingSet = filesToUnparse;

  while (!workingSet.empty()) {
    newFilesToUnparse.clear();

    // DQ (4/14/2020): Added header file unparsing feature specific debug level.
    if (SgProject::get_unparseHeaderFilesDebug() >= 0) {
      printf("In IncludedFilesUnparser::collectAdditionalFilesToUnparse(): "
             "calling applyFunctionToIncludingPreprocessingInfos(workingSet) "
             "workingSet.size() = %zu \n",
             workingSet.size());
    }

    applyFunctionToIncludingPreprocessingInfos(
        workingSet, &IncludedFilesUnparser::collectNewFilesToUnparse);
    workingSet = newFilesToUnparse;
  }
}

void IncludedFilesUnparser::collectNewFilesToUnparse(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {

  IncludeDirective includeDirective(includingPreprocessingInfo->getString());
  if (includeDirective.isQuotedInclude() ||
      FileHelper::isAbsolutePath(includeDirective.getIncludedPath())) {
    string normalizedIncludingFileName =
        FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
    if (filesToUnparse.find(normalizedIncludingFileName) ==
        filesToUnparse.end()) {
      filesToUnparse.insert(normalizedIncludingFileName);
      newFilesToUnparse.insert(normalizedIncludingFileName);
    }
  }
}

void IncludedFilesUnparser::collectAdditionalListOfHeaderFilesToCopy() {
  // Recursively add to filesToUnparse set any file that includes using quotes
  // (or an absolute path) at least one of the files that is already in
  // filesToUnparse set.
  set<string> workingSet = filesToUnparse;

  // Using these input lists
  // std::set<std::string> allFiles;
  // std::set<std::string> filesToUnparse;

  // Compute this set.
  // std::set<std::string> filesToCopy;

  set<string> setOfPathsForFilesToUnparse;

  set<string>::iterator k = filesToUnparse.begin();
  while (k != filesToUnparse.end()) {
    string path = Rose::getPathFromFileName(*k);

    if (setOfPathsForFilesToUnparse.find(path) ==
        setOfPathsForFilesToUnparse.end()) {
      setOfPathsForFilesToUnparse.insert(path);
    }

    k++;
  }

  // We don't need set intersection because the filesToUnparse are a subset of
  // allFiles. Perform set intersection of setOfPathsToAllFiles with
  // setOfPathsForFilesToUnparse.

  ASSERT_not_null(projectNode);
  string applicationRootDirectory = projectNode->get_applicationRootDirectory();

  ROSE_ASSERT(filesToCopy.empty() == true);

  set<string>::iterator n = setOfPathsForFilesToUnparse.begin();
  while (n != setOfPathsForFilesToUnparse.end()) {
    string path_n = *n;

    // Find the subset of allFiles that are in each path and copy them to the
    // unparseRootPath.
    set<string>::iterator o = allFiles.begin();
    while (o != allFiles.end()) {
      string path_o = Rose::getPathFromFileName(*o);

      bool isUnparsed = (filesToUnparse.find(*o) != filesToUnparse.end());

      if (isUnparsed == false && path_o == path_n) {
        filesToCopy.insert(*o);
      }

      o++;
    }

    // Copy all header files from this path to the associated directory where
    // file are being unparsed.

    n++;
  }
}

void IncludedFilesUnparser::collectNewFilesToCopy(
    const string &includedFile, PreprocessingInfo *includingPreprocessingInfo) {

  IncludeDirective includeDirective(includingPreprocessingInfo->getString());
  if (includeDirective.isQuotedInclude() ||
      FileHelper::isAbsolutePath(includeDirective.getIncludedPath())) {
    string normalizedIncludingFileName =
        FileHelper::getNormalizedContainingFileName(includingPreprocessingInfo);
    if (filesToCopy.find(normalizedIncludingFileName) == filesToCopy.end()) {
      filesToCopy.insert(normalizedIncludingFileName);
      // newFilesToUnparse.insert(normalizedIncludingFileName);
    }
  }

  // Go through the list of allFiles, and identify any that are not in the list
  // of files to unparse, and add them to the list of files to copy.
  set<string>::iterator i = allFiles.begin();
  while (i != allFiles.end()) {
    if (modifiedFiles.find(*i) == modifiedFiles.end() &&
        filesToCopy.find(*i) == filesToCopy.end()) {

      // We need to exclude the ROSE preinclude file from being added to the
      // list of files to copy. filesToCopy.insert(*i); string
      // filenameWithOutPath = FileHelper::normalizePath(*i);
      string filenameWithOutPath = FileHelper::getFileName(*i);
      if (filenameWithOutPath != "rose_required_macros_and_functions.h") {
        filesToCopy.insert(*i);
      } else {
      }
    }

    i++;
  }
}

void IncludedFilesUnparser::applyFunctionToIncludingPreprocessingInfos(
    const set<string> &includedFiles,
    void (IncludedFilesUnparser::*funPtr)(
        const string &includedFile,
        PreprocessingInfo *includingPreprocessingInfo)) {

  for (set<string>::const_iterator includedFile = includedFiles.begin();
       includedFile != includedFiles.end(); includedFile++) {
    const map<string, set<PreprocessingInfo *>>
        &includingPreprocessingInfosMap =
            projectNode->get_includingPreprocessingInfosMap();
    map<string, set<PreprocessingInfo *>>::const_iterator mapEntry =
        includingPreprocessingInfosMap.find(*includedFile);
    if (mapEntry != includingPreprocessingInfosMap.end()) {
      // includedFile is really included, so look for all its including
      // preprocessing infos.
      const set<PreprocessingInfo *> &includingPreprocessingInfos =
          mapEntry->second;
      for (set<PreprocessingInfo *>::const_iterator
               includingPreprocessingInfoPtr =
                   includingPreprocessingInfos.begin();
           includingPreprocessingInfoPtr != includingPreprocessingInfos.end();
           includingPreprocessingInfoPtr++) {
        (this->*funPtr)(*includedFile, *includingPreprocessingInfoPtr);
      }
    }
  }
}

void IncludedFilesUnparser::addToUnparseScopesMap(const string &fileName,
                                                  SgNode *startNode) {
  // We need to find the innermost enclosing scope that is from the including
  // file (i.e. from a file that is different from this node's file) such that
  // we unparse the whole included file, not just the scope containing the
  // modified stuff.
  SgNode *enclosingScope =
      SageInterface::getEnclosingNode<SgScopeStatement>(startNode, false);
  while (enclosingScope != NULL &&
         fileName.compare(FileHelper::normalizePath(
             enclosingScope->get_file_info()->get_filenameString())) == 0) {
    enclosingScope = SageInterface::getEnclosingNode<SgScopeStatement>(
        enclosingScope, false);
  }

  if (enclosingScope != NULL) {
    // Found the innermost enclosing scope from the including file.
    unparseScopesMap.insert(pair<string, SgScopeStatement *>(
        fileName, isSgScopeStatement(enclosingScope)));

    if (SgProject::get_verbose() >= 1) {
      cout << "Enclosing node:" << enclosingScope->class_name() << endl;
      cout << "Enclosing node's file:"
           << enclosingScope->get_file_info()->get_filenameString() << endl;
    }
  }
}

void IncludedFilesUnparser::visit(SgNode *node) {

#define DEBUG_INCLUDE_FILE_UNPARSER_VISIT 0

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  printf("In IncludedFilesUnparser::visit(): node = %p = %s = %s isModified = "
         "%s \n",
         node, node->class_name().c_str(),
         SageInterface::get_name(node).c_str(),
         node->get_isModified() ? "true" : "false");
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
  if (functionDeclaration != NULL) {
    printf(" --- functionDeclaration name = %s \n",
           functionDeclaration->get_name().str());
    printf(" --- functionDeclaration->get_definition() = %p \n",
           functionDeclaration->get_definition());
  }
#endif

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  if (isSgGlobal(node) != NULL) {
    printf("In IncludedFilesUnparser::visit(): (SgGlobal): node = %p = %s = %s "
           "isModified = %s \n",
           node, node->class_name().c_str(),
           SageInterface::get_name(node).c_str(),
           node->get_isModified() ? "true" : "false");
  }
#endif

  // DQ (6/5/2019): Use this as a predicate to control output spew.
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  bool isStatement = (isSgStatement(node) != NULL);
#endif

  SgSourceFile *sourceFile = isSgSourceFile(node);
  if (sourceFile != NULL) {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    printf("Building unparseSgSourceFileMap: sourceFile = %p "
           "sourceFile->getFileName() = %s \n",
           sourceFile, sourceFile->getFileName().c_str());
#endif
    unparseSourceFileMap.insert(
        pair<string, SgSourceFile *>(sourceFile->getFileName(), sourceFile));

    // Save the header file report.
    SgHeaderFileReport *reportData = sourceFile->get_headerFileReport();

    if (reportData != NULL) {
      printf("####################################################### \n");
      printf("####################################################### \n");
      reportData->display("headerFileReport in IncludedFilesUnparser::visit()");
      printf("####################################################### \n");
      printf("####################################################### \n");
    } else {
    }
  }

  SgIncludeFile *includeFile = isSgIncludeFile(node);
  if (includeFile != NULL) {
    SgSourceFile *headerFile = isSgSourceFile(includeFile->get_source_file());
    if (headerFile != NULL) {
      unparseSourceFileMap.insert(
          pair<string, SgSourceFile *>(headerFile->getFileName(), headerFile));
    }
  }

  // DQ (9/7/2018): Looking for connections to the SgSourceFile in the
  // SgIncludeDirectiveStatement. We could just build up a map of filenames to
  // SgSourceFile IR nodes and then use it with the modifiedFiles set.
  SgIncludeDirectiveStatement *includeDirectiveStatement =
      isSgIncludeDirectiveStatement(node);
  if (includeDirectiveStatement != NULL) {
    SgHeaderFileBody *headerFileBody =
        includeDirectiveStatement->get_headerFileBody();

    // DQ (5/19/2020): In the new design, allow the headerFileBody to be NULL.
    // Include directives are now added uniformally to the AST, as part of
    // supporting unparsing of arbitrary subsets of header files.
    // ROSE_ASSERT(headerFileBody != NULL);
    if (headerFileBody != NULL) {
      SgSourceFile *headerFile = headerFileBody->get_include_file();

      // DQ (11/22/2018): We only build associated SgSourceFile for application
      // header files. ROSE_ASSERT(headerFile != NULL);
      if (headerFile != NULL) {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        printf("Building unparseSgSourceFileMap: headerFile = %p "
               "headerFile->getFileName() = %s \n",
               headerFile, headerFile->getFileName().c_str());
#endif
        unparseSourceFileMap.insert(pair<string, SgSourceFile *>(
            headerFile->getFileName(), headerFile));
      } else {
        if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
          printf("NOTE: In IncludedFilesUnparser::visit(): "
                 "includeDirectiveStatement = %p headerFile == "
                 "NULL \n",
                 includeDirectiveStatement);
        }
      }
    } else {
      if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
        // Header files may not have a materialized AST body (e.g.,
        // when the frontend doesn't build SgHeaderFileBody instances
        // for a directive).
        printf("NOTE: In IncludedFilesUnparser::visit(): headerFileBody "
               "== NULL: "
               "includeDirectiveStatement->get_directiveString() = %s \n",
               includeDirectiveStatement->get_directiveString().c_str());
      }
    }
  }

  Sg_File_Info *fileInfo = node->get_file_info();

  if (fileInfo != NULL) {
    // DQ (11/28/2018): Need to use the full filename (perhaps resolved of
    // symbolic links) because filename that match can represent files in
    // different directories.  Though this is more of an issue for system header
    // file. string normalizedFileName = FileHelper::normalizePath(fileInfo ->
    // get_filenameString()); string normalizedFileName =
    // FileHelper::normalizePath(fileInfo->get_physical_filename());
    int physical_file_id = fileInfo->get_physical_file_id();

    // string normalizedFileName =
    // FileHelper::normalizePath(fileInfo->getFilenameFromID(physical_file_id));
    string normalizedFileName = fileInfo->getFilenameFromID(physical_file_id);

    // DQ (10/14/2019): Trap cases where the normalizedFileName is not a valid
    // filename.
    if (normalizedFileName == "transformation") {
      SgSourceFile *sourceFile = SageInterface::getEnclosingSourceFile(node);
      ASSERT_not_null(sourceFile);
      normalizedFileName = sourceFile->getFileName();
    }

    bool isTransformation = fileInfo->isTransformation();
    bool isCompilerGenerated = fileInfo->isCompilerGenerated();
    bool isModified = node->get_isModified();

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    bool isShared = fileInfo->isShared();
    if (isStatement == true) {
      printf("In IncludedFilesUnparser::visit(): physical_file_id             "
             "= %d \n",
             physical_file_id);
      printf("In IncludedFilesUnparser::visit(): physical fileName (computed) "
             "= %s \n",
             fileInfo->get_physical_filename().c_str());
      printf("In IncludedFilesUnparser::visit(): physical fileName (raw)      "
             "= %s \n",
             fileInfo->getFilenameFromID(physical_file_id).c_str());
      printf("In IncludedFilesUnparser::visit(): normalizedFileName           "
             "= %s \n",
             normalizedFileName.c_str());
      printf("In IncludedFilesUnparser::visit(): isTransformation             "
             "= %s \n",
             isTransformation ? "true" : "false");
      printf("In IncludedFilesUnparser::visit(): isCompilerGenerated          "
             "= %s \n",
             isCompilerGenerated ? "true" : "false");
      printf("In IncludedFilesUnparser::visit(): isModified                   "
             "= %s \n",
             isModified ? "true" : "false");
      printf("In IncludedFilesUnparser::visit(): isShared                     "
             "= %s \n",
             isShared ? "true" : "false");
    }
#endif

    // if (!isTransformation && !isCompilerGenerated)
    if (((isTransformation == false) && (isCompilerGenerated == false)) ||
        ((isTransformation == true) &&
         (normalizedFileName != "transformation"))) {
      // avoid infos that do not have real file names

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
      if (isStatement == true) {
        printf("fileInfo->get_file_id() = %d \n", fileInfo->get_file_id());
        printf("fileInfo->get_physical_file_id() = %d \n",
               fileInfo->get_physical_file_id());
      }
#endif
      // if (fileInfo->get_file_id() >= 0)
      if (fileInfo->get_physical_file_id() >= 0) {
        // TODO: Investigate why it can be less than 0 (e.g. -2 with file name
        // being NULL_FILE). Note that any Sg_File_Info that is marked as
        // transformed, will output a filename "transformed" and a file_id that
        // is less than zero. What we want to use is the information about the
        // physical file which in general would have to map to whatever filename
        // a transformations considers itself to be associated with.  Need to
        // implement mechanisms to automatically set this (e.g. using the
        // physiscal file Id of surreountings statements) and check that is is
        // consistant as well.
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        if (isStatement == true) {
          printf(
              "In IncludedFilesUnparser::visit(): !isTransformation && "
              "!isCompilerGenerated: node = %p = %s normalizedFileName = %s \n",
              node, node->class_name().c_str(), normalizedFileName.c_str());
        }
#endif
        set<string>::const_iterator setEntry =
            allFiles.find(normalizedFileName);
        // TODO: This is assuming that if a header file is included in multiple
        // places, it is sufficient to unparse just one instance, i.e.
        // modifications to all places are identical. This needs to be
        // generalized.
        if (setEntry == allFiles.end()) {
          // This is a new file, process it.
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
          printf("In IncludedFilesUnparser::visit(): !isTransformation && "
                 "!isCompilerGenerated: This is a new file, process it: file = "
                 "%s \n",
                 normalizedFileName.c_str());
#endif
          allFiles.insert(normalizedFileName);

          // DQ (9/5/2018): Comment added.
          // We can't just set or use this on the SgSourceFile, since there is
          // only one file object for a translation unit. We need to add a file
          // name to use in the selection of statements to be unparsed, and then
          // the unparsing needs to be directed to that file (and back again,
          // changing whatever is defined to be the current file). This might
          // cause a large number of files to be open at the same time, in the
          // worst case, but that will be fine for now. get_unparse_tokens()

          addToUnparseScopesMap(normalizedFileName, node);
        } else {
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
          printf("This is NOT a new file: normalizedFileName = %s \n",
                 normalizedFileName.c_str());
#endif
        }
      } else {
        // DQ (10/14/2019): We might want to have forced the physical_file_id to
        // have been set to the associated source file. At the moment I will
        // ignore this issue and output a message so that we can review it
        // later.
      }
    }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    if (isStatement == true) {
      printf("List allFiles list (size = %zu): \n", allFiles.size());
      set<string>::iterator i = allFiles.begin();
      size_t counter = 0;
      while (i != allFiles.end()) {
        printf("   --- allFiles[%zu] = %s \n", counter, (*i).c_str());

        i++;
        counter++;
      }
    }
#endif

    // DQ (6/8/2019): Ignore SgSourceFile and SgProject IR nodes.
    SgSupport *supportNode = isSgSupport(node);
    // DQ (6/6/2019): I think that we want to handle statements that are either
    // marked isModified or isTransformation. NOTE: to support the header file
    // unparsing the associated physical file where the target transformation is
    // considered to live must be specified. if (node -> get_isModified()) if
    // (isModified == true) if (isModified == true || ( (isTransformation ==
    // true) && (normalizedFileName != "transformation") ) )
    if (supportNode == NULL &&
        (isModified == true || ((isTransformation == true) &&
                                (normalizedFileName != "transformation")))) {
      // DQ (6/6/2019): Modified statements are important for the token-based
      // unparsing, since they trigger the switch from using the token stream
      // for unparsing to using the AST for the unparsing.  However, the
      // modified flag is not important if we are not using the token based
      // unparsing.  When we are not using the token-based unparsing (or when we
      // are unparsing directly from the AST) the status of the isTransformation
      // flag is all that is important.

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
      printf("In IncludedFilesUnparser::visit(): node -> get_isModified(): "
             "node = %p = %s  \n",
             node, node->class_name().c_str());
#endif
      if (SgProject::get_verbose() > 2) {
        cout << "Found a modified node: " << node->class_name() << endl;
        cout << "   In file: " << normalizedFileName << endl;
        cout << "   Is transformation: " << isTransformation << endl;
        cout << "   Is compiler generated: " << isCompilerGenerated << endl;
      }
      // In a preorder traversal, this is not meaningful, since I understand
      // that the parent statement has already been visited. DQ (9/24/2018): If
      // this is not a statement, then mark the enclosing statement as modified.
      // DQ (11/28/2018): This should be an issue for a preorder traversal, so
      // this should be reconsidered.
      if (isSgStatement(node) == NULL) {
        // DQ (6/8/2019): Added error checking.
        ASSERT_not_null(node);
        SgStatement *enclosingStatement =
            SageInterface::getEnclosingStatement(node);
        if (enclosingStatement == NULL) {
          printf("Error: enclosingStatement == NULL: computed from node = %p = "
                 "%s \n",
                 node, node->class_name().c_str());
          ASSERT_not_null(node->get_file_info());
          node->get_file_info()->display(
              "Error: enclosingStatement == NULL: debug");
        }
        ASSERT_not_null(enclosingStatement);
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        printf("Found non-statement = %p = %s as modified, marking enclosing "
               "statement = %p = %s \n",
               node, node->class_name().c_str(), enclosingStatement,
               enclosingStatement->class_name().c_str());
#endif
        // DQ (9/24/2018): Mark this statement as a transformation, note that it
        // IS a transformantion instead of contains a transformation because the
        // statement is the current lowest level of grainularity in the token
        // based unparsing. enclosingStatement->set_modified(true);
        // enclosingStatement->set_containsTransformation(true);
        enclosingStatement->setTransformation();
      }

      // DQ (11/28/2018): I think this is a bug fix for the recognition of
      // transformed statements in either header files or source files within
      // the AST. if (!isTransformation && !isCompilerGenerated)
      if (isTransformation == true && isCompilerGenerated == false) {
        // avoid infos that do not have real file names
#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
        // printf ("In IncludedFilesUnparser::visit(): node -> get_isModified():
        // !isTransformation && !isCompilerGenerated: normalizedFileName = %s
        // \n",normalizedFileName.c_str());
        printf("In IncludedFilesUnparser::visit(): node -> get_isModified(): "
               "(isTransformation == true && isCompilerGenerated == false): "
               "normalizedFileName = %s \n",
               normalizedFileName.c_str());
#endif
        modifiedFiles.insert(normalizedFileName);
        filesWithMarkedTransformations.insert(normalizedFileName);

        // DQ (10/14/2019): Trap cases where the normalizedFileName is not a
        // valid filename.
        if (normalizedFileName == "transformation") {
          printf("ERROR: normalizedFileName = %s \n",
                 normalizedFileName.c_str());

          printf("Exiting as a test! \n");
          ROSE_ABORT();
        }
      }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
      printf("In IncludedFilesUnparser::visit(): node -> get_isModified(): "
             "output endl \n");
#endif
      // cout << endl << endl;
    }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
    if (isStatement == true) {
      printf("List modifiedFiles list (size = %zu): \n", modifiedFiles.size());
      set<string>::iterator j = modifiedFiles.begin();
      size_t modified_file_counter = 0;
      while (j != modifiedFiles.end()) {
        printf("   --- modifiedFiles[%zu] = %s \n", modified_file_counter,
               (*j).c_str());

        j++;
        modified_file_counter++;
      }
    }
#endif
  }

#if DEBUG_INCLUDE_FILE_UNPARSER_VISIT
  if (isStatement == true) {
    // printf ("Leaving IncludedFilesUnparser::visit(): node = %p = %s
    // modifiedFiles.size() = %zu
    // \n",node,SageInterface::get_name(node).c_str(),modifiedFiles.size());
    printf("Leaving IncludedFilesUnparser::visit(): node = %p = %s "
           "modifiedFiles.size() = %zu \n\n",
           node, node->class_name().c_str(), modifiedFiles.size());
  }
#endif
}
