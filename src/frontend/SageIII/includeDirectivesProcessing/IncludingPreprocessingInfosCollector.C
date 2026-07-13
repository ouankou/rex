// DQ (10/5/2014): This is more strict now that we include rose_config.h in the
// sage3basic.h. #include <rose.h>

#include "sage3basic.h"

#include <iostream>

#include "FileHelper.h"
#include "IncludeDirective.h"

#include "IncludingPreprocessingInfosCollector.h"

using namespace std;

#define INCLUDE_SUUPORT_DEBUG_MESSAGES 0

namespace {
std::string normalizeIncludeSpelling(const std::string &spelling) {
  std::string normalized = spelling;
  while (normalized.rfind("./", 0) == 0) {
    normalized.erase(0, 2);
  }
  return normalized;
}

bool normalizedPathEndsWithIncludeSpelling(const std::string &normalizedPath,
                                           const std::string &spelling) {
  const std::string normalizedSpelling = normalizeIncludeSpelling(spelling);
  if (normalizedPath.empty() || normalizedSpelling.empty()) {
    return false;
  }
  if (normalizedPath == normalizedSpelling) {
    return true;
  }
  const std::string suffix = "/" + normalizedSpelling;
  return normalizedPath.size() >= suffix.size() &&
         normalizedPath.compare(normalizedPath.size() - suffix.size(),
                                suffix.size(), suffix) == 0;
}
} // namespace

// It is needed because otherwise, the default destructor breaks something.

IncludingPreprocessingInfosCollector::~IncludingPreprocessingInfosCollector() {
  // do nothing
}

IncludingPreprocessingInfosCollector::IncludingPreprocessingInfosCollector(
    SgProject *projectNode, const map<string, set<string>> &includedFilesMap) {
  this->projectNode = projectNode;
  for (const auto &[includingFile, includedFiles] : includedFilesMap) {
    const std::string normalizedIncludingFile =
        FileHelper::normalizePathIfPossible(includingFile);
    if (normalizedIncludingFile.empty()) {
      fprintf(stderr,
              "REX_FRONTEND_INVARIANT[include-owner-index]: including file "
              "has no normalized path: %s\n",
              includingFile.c_str());
      ROSE_ABORT();
    }

    std::set<std::string> &normalizedIncludedFiles =
        normalizedIncludedFilesMap[normalizedIncludingFile];
    for (const std::string &includedFile : includedFiles) {
      const std::string normalizedIncludedFile =
          FileHelper::normalizePathIfPossible(includedFile);
      if (normalizedIncludedFile.empty()) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[include-owner-index]: active include "
                "from=%s has no normalized path: %s\n",
                normalizedIncludingFile.c_str(), includedFile.c_str());
        ROSE_ABORT();
      }
      normalizedIncludedFiles.insert(normalizedIncludedFile);
    }
  }
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

  return includingPreprocessingInfosMap;
}

void IncludingPreprocessingInfosCollector::addIncludingPreprocessingInfoToMap(
    PreprocessingInfo *preprocessingInfo) {
  ASSERT_not_null(preprocessingInfo);
  ASSERT_not_null(preprocessingInfo->get_file_info());
  const std::string containingFile = FileHelper::normalizePathIfPossible(
      preprocessingInfo->get_file_info()->get_filenameString());
  if (containingFile.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-owner]: preprocessing record=%p "
            "has no containing file\n",
            static_cast<void *>(preprocessingInfo));
    ROSE_ABORT();
  }

  const auto activeIncludes = normalizedIncludedFilesMap.find(containingFile);
  if (activeIncludes == normalizedIncludedFilesMap.end()) {
    return;
  }

  IncludeDirective includeDirective(preprocessingInfo->getString());
  const std::string includedSpelling = includeDirective.getIncludedPath();
  if (includedSpelling.empty()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-owner]: directive=%s from=%s has "
            "no literal include spelling\n",
            preprocessingInfo->getString().c_str(), containingFile.c_str());
    ROSE_ABORT();
  }

  std::set<std::string> candidates;
  const std::string relativeCandidate =
      FileHelper::normalizePathIfPossible(FileHelper::concatenatePaths(
          FileHelper::getParentFolder(containingFile), includedSpelling));
  const std::string absoluteCandidate =
      FileHelper::isAbsolutePath(includedSpelling)
          ? FileHelper::normalizePathIfPossible(includedSpelling)
          : std::string();
  for (const std::string &normalizedActiveInclude : activeIncludes->second) {
    const bool exactAbsoluteMatch =
        !absoluteCandidate.empty() &&
        absoluteCandidate == normalizedActiveInclude;
    const bool exactRelativeMatch =
        !FileHelper::isAbsolutePath(includedSpelling) &&
        relativeCandidate == normalizedActiveInclude;
    if (exactAbsoluteMatch || exactRelativeMatch ||
        normalizedPathEndsWithIncludeSpelling(normalizedActiveInclude,
                                              includedSpelling)) {
      candidates.insert(normalizedActiveInclude);
    }
  }

  // Raw preprocessing inventories contain directives in inactive conditional
  // branches. They are syntax, but not include edges for this translation.
  if (candidates.empty()) {
    return;
  }
  if (candidates.size() != 1) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[include-owner]: directive=%s from=%s "
            "matches %zu active physical includes\n",
            preprocessingInfo->getString().c_str(), containingFile.c_str(),
            candidates.size());
    ROSE_ABORT();
  }

  includingPreprocessingInfosMap[*candidates.begin()].insert(preprocessingInfo);
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
        const PreprocessingInfo::DirectiveType directiveType =
            preprocessingInfo->getTypeOfDirective();
        if (directiveType ==
                PreprocessingInfo::CpreprocessorIncludeDeclaration ||
            directiveType ==
                PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
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
