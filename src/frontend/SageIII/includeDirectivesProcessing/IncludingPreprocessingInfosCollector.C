#include "sage3basic.h"

#include <filesystem>
#include <iostream>

#include "IncludingPreprocessingInfosCollector.h"

using namespace std;

IncludingPreprocessingInfosCollector::~IncludingPreprocessingInfosCollector() {}

IncludingPreprocessingInfosCollector::IncludingPreprocessingInfosCollector(
    SgProject *projectNode) {
  ASSERT_not_null(projectNode);
  this->projectNode = projectNode;
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

  for (SgFile *file : projectNode->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile == nullptr) {
      continue;
    }
    const auto &resolvedDirectives =
        sourceFile->get_frontendResolvedIncludeDirectivesMap();
    for (const auto &[includedPath, directives] : resolvedDirectives) {
      if (!std::filesystem::path(includedPath).is_absolute() ||
          std::filesystem::path(includedPath).lexically_normal().string() !=
              includedPath ||
          directives.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[include-owner-index]: frontend "
                "target=%s has no exact path or directive owners\n",
                includedPath.c_str());
        ROSE_ABORT();
      }
      for (PreprocessingInfo *directive : directives) {
        if (directive == nullptr || directive->get_file_info() == nullptr ||
            observedIncludePreprocessingInfos.count(directive) == 0) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[include-owner-index]: source=%s "
                  "target=%s has an unattached frontend directive=%p\n",
                  sourceFile->getFileName().c_str(), includedPath.c_str(),
                  static_cast<void *>(directive));
          ROSE_ABORT();
        }
        const std::string normalizedIncludingPath =
            directive->get_file_info()->get_physical_filename();
        if (normalizedIncludingPath.empty()) {
          fprintf(stderr,
                  "REX_FRONTEND_INVARIANT[include-owner-index]: target=%s "
                  "has an owner with no exact physical source path\n",
                  includedPath.c_str());
          ROSE_ABORT();
        }
        normalizedIncludedFilesMap[normalizedIncludingPath].insert(
            includedPath);
        includingPreprocessingInfosMap[includedPath].insert(directive);
      }
    }
  }

  for (const auto &[includingPath, includedPaths] :
       normalizedIncludedFilesMap) {
    for (const std::string &includedPath : includedPaths) {
      const auto owners = includingPreprocessingInfosMap.find(includedPath);
      bool foundExactOwner = false;
      if (owners != includingPreprocessingInfosMap.end()) {
        for (PreprocessingInfo *directive : owners->second) {
          if (directive != nullptr && directive->get_file_info() != nullptr &&
              directive->get_file_info()->get_physical_filename() ==
                  includingPath) {
            foundExactOwner = true;
            break;
          }
        }
      }
      if (!foundExactOwner) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[include-owner-index]: active edge "
                "from=%s to=%s has no exact Clang callback owner\n",
                includingPath.c_str(), includedPath.c_str());
        ROSE_ABORT();
      }
    }
  }

  return includingPreprocessingInfosMap;
}

const map<string, set<string>> &
IncludingPreprocessingInfosCollector::getIncludedFilesMap() const {
  return normalizedIncludedFilesMap;
}

void IncludingPreprocessingInfosCollector::observeIncludePreprocessingInfo(
    PreprocessingInfo *preprocessingInfo) {
  ASSERT_not_null(preprocessingInfo);
  ASSERT_not_null(preprocessingInfo->get_file_info());
  observedIncludePreprocessingInfos.insert(preprocessingInfo);
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

      const PreprocessingInfo::DirectiveType directiveType =
          preprocessingInfo->getTypeOfDirective();
      if (directiveType == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
          directiveType ==
              PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
        observeIncludePreprocessingInfo(preprocessingInfo);
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
      for (AttachedPreprocessingInfoType::const_iterator it =
               preprocessingInfoType->begin();
           it != preprocessingInfoType->end(); it++) {
        PreprocessingInfo *preprocessingInfo = *it;
        const PreprocessingInfo::DirectiveType directiveType =
            preprocessingInfo->getTypeOfDirective();
        if (directiveType ==
                PreprocessingInfo::CpreprocessorIncludeDeclaration ||
            directiveType ==
                PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
          if (SgProject::get_verbose() >= 1) {
            cout << "Found #include directive in file: "
                 << preprocessingInfo->get_file_info()->get_physical_filename()
                 << endl;
          }
          observeIncludePreprocessingInfo(preprocessingInfo);
        }
      }
    }
  }

  SgSourceFile *sourceFile = isSgSourceFile(node);
  if (sourceFile != NULL) {
    collectFromSourceFile(sourceFile);
  }
}
