// DQ (10/5/2014): This is more strict now that we include rose_config.h in the
// sage3basic.h. #include <rose.h>

#include "sage3basic.h"

#include <iostream>

#include "FileHelper.h"

#include "IncludingPreprocessingInfosCollector.h"

using namespace std;

#define INCLUDE_SUUPORT_DEBUG_MESSAGES 0

// It is needed because otherwise, the default destructor breaks something.

IncludingPreprocessingInfosCollector::~IncludingPreprocessingInfosCollector() {
  // do nothing
}

IncludingPreprocessingInfosCollector::IncludingPreprocessingInfosCollector(
    SgProject *projectNode, const map<string, set<string>> &includedFilesMap) {
  this->projectNode = projectNode;
  this->includedFilesMap = includedFilesMap;
}

map<string, set<PreprocessingInfo *>>
IncludingPreprocessingInfosCollector::collect() {
  traverse(projectNode, preorder);

  for (std::map<SgSourceFile *,
                std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                    *>::const_iterator mapIt =
           Rose::tokenSubsequenceMapOfMapsBySourceFile.begin();
       mapIt != Rose::tokenSubsequenceMapOfMapsBySourceFile.end(); ++mapIt) {
    collectFromSourceFile(mapIt->first);
  }

  matchIncludedAndIncludingFiles();

  return includingPreprocessingInfosMap;
}

// Every pair IncludingFile -> IncludedFile that is produced by -H compiler
// option should have a matching pair IncludedFile -> PreprocessingInfo of
// IncludingFile in the map includingPreprocessingInfosMap. This way, ROSE
// ensures that it found all included files that are actually included by the
// compiler (-H option), though this is not a guarantee of correctness.
void IncludingPreprocessingInfosCollector::matchIncludedAndIncludingFiles() {
  for (map<string, set<string>>::const_iterator mapIterator =
           includedFilesMap.begin();
       mapIterator != includedFilesMap.end(); mapIterator++) {
    string includingFileName = mapIterator->first;
    set<string> includedFilesSet = mapIterator->second;
    for (set<string>::iterator includedFileNamePtr = includedFilesSet.begin();
         includedFileNamePtr != includedFilesSet.end(); includedFileNamePtr++) {
      map<string, set<PreprocessingInfo *>>::const_iterator mapEntry =
          includingPreprocessingInfosMap.find(*includedFileNamePtr);

      // DQ (11/20/2018): This fails for processing large files (time app).
      // ROSE_ASSERT(mapEntry != includingPreprocessingInfosMap.end());
      if (mapEntry != includingPreprocessingInfosMap.end()) {
        set<PreprocessingInfo *> includingPreprocessingInfos = mapEntry->second;
        bool isIncludingFilePresent = false;
        for (set<PreprocessingInfo *>::const_iterator it =
                 includingPreprocessingInfos.begin();
             it != includingPreprocessingInfos.end(); it++) {
          if (includingFileName.compare(FileHelper::normalizePath(
                  (*it)->get_file_info()->get_filenameString())) == 0) {
            isIncludingFilePresent = true;
            break;
          }
        }

        // DQ (11/20/2018): Unclear why the include file is not present as a
        // #include directive. ROSE_ASSERT(isIncludingFilePresent);
        if (isIncludingFilePresent == false) {
#if INCLUDE_SUUPORT_DEBUG_MESSAGES
          printf("WARNING: isIncludingFilePresent == false: Unclear why the "
                 "include file is not present as a #include directive \n");
#endif
        }
      } else {
#if INCLUDE_SUUPORT_DEBUG_MESSAGES
        printf("WARNING: There is no #include identified for a specific "
               "include path \n");
#endif
      }
    }
  }

  if (SgProject::get_verbose() >= 1) {
    cout << "\nIncluded and including file maps matched successfully!\n";
  }
}

void IncludingPreprocessingInfosCollector::addIncludingPreprocessingInfoToMap(
    PreprocessingInfo *preprocessingInfo) {
  string normalizedIncludedFile = FileHelper::normalizePath(
      projectNode->findIncludedFile(preprocessingInfo));
  if (!normalizedIncludedFile.empty()) {
    // if the included file was found, add it to the map
    map<string, set<PreprocessingInfo *>>::iterator mapEntry =
        includingPreprocessingInfosMap.find(normalizedIncludedFile);
    if (mapEntry != includingPreprocessingInfosMap.end()) {
      (mapEntry->second).insert(preprocessingInfo);
    } else {
      set<PreprocessingInfo *> includingPreprocessingInfos;
      includingPreprocessingInfos.insert(preprocessingInfo);
      includingPreprocessingInfosMap.insert(
          pair<string, set<PreprocessingInfo *>>(normalizedIncludedFile,
                                                 includingPreprocessingInfos));
    }
  }
}

void IncludingPreprocessingInfosCollector::collectFromSourceFile(
    SgSourceFile *sourceFile) {
  if (sourceFile == NULL || !processedSourceFiles.insert(sourceFile).second) {
    return;
  }

  ROSEAttributesListContainerPtr filePreprocInfo =
      sourceFile->get_preprocessorDirectivesAndCommentsList();
  if (filePreprocInfo == NULL) {
    return;
  }

  std::map<std::string, ROSEAttributesList *> &attributeLists =
      filePreprocInfo->getList();
  for (std::map<std::string, ROSEAttributesList *>::iterator listIterator =
           attributeLists.begin();
       listIterator != attributeLists.end(); ++listIterator) {
    ROSEAttributesList *attributeList = listIterator->second;
    if (attributeList == NULL) {
      continue;
    }

    std::vector<PreprocessingInfo *> &preprocessingInfos =
        attributeList->getList();
    for (std::vector<PreprocessingInfo *>::iterator preprocessingInfoIterator =
             preprocessingInfos.begin();
         preprocessingInfoIterator != preprocessingInfos.end();
         ++preprocessingInfoIterator) {
      PreprocessingInfo *preprocessingInfo = *preprocessingInfoIterator;
      if (preprocessingInfo == NULL) {
        continue;
      }

      if (preprocessingInfo->getTypeOfDirective() ==
          PreprocessingInfo::CpreprocessorIncludeDeclaration) {
        addIncludingPreprocessingInfoToMap(preprocessingInfo);
      }
    }
  }
}

void IncludingPreprocessingInfosCollector::visit(SgNode *node) {
  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode != NULL) {
    AttachedPreprocessingInfoType *preprocessingInfoType =
        locatedNode->getAttachedPreprocessingInfo();
    if (preprocessingInfoType != NULL) {
      string containingFileName = FileHelper::normalizePath(
          locatedNode->get_file_info()->get_filenameString());
      for (AttachedPreprocessingInfoType::const_iterator it =
               preprocessingInfoType->begin();
           it != preprocessingInfoType->end(); it++) {
        PreprocessingInfo *preprocessingInfo = *it;
        if (preprocessingInfo->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorIncludeDeclaration) {
          if (SgProject::get_verbose() >= 1) {
            cout << "Found #include directive in file: " << containingFileName
                 << endl;
          }
          addIncludingPreprocessingInfoToMap(preprocessingInfo);
        }
      }
    }
  }

  SgSourceFile *sourceFile = isSgSourceFile(node);
  if (sourceFile != NULL) {
    collectFromSourceFile(sourceFile);
  }
}
