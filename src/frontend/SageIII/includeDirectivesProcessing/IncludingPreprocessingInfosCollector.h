#ifndef ROSE_INCLUDING_PREPROCESSING_INFOS_COLLECTOR_H
#define ROSE_INCLUDING_PREPROCESSING_INFOS_COLLECTOR_H

#include <map>
#include <set>
#include <string>

class IncludingPreprocessingInfosCollector : public AstSimpleProcessing {
private:
  SgProject *projectNode;
  std::map<std::string, std::set<std::string>> normalizedIncludedFilesMap;

  std::map<std::string, std::set<PreprocessingInfo *>>
      includingPreprocessingInfosMap;
  std::set<PreprocessingInfo *> observedIncludePreprocessingInfos;
  std::set<SgSourceFile *> processedSourceFiles;

  void observeIncludePreprocessingInfo(PreprocessingInfo *preprocessingInfo);
  void collectFromSourceFile(SgSourceFile *sourceFile);

protected:
  void visit(SgNode *astNode);

public:
  ~IncludingPreprocessingInfosCollector();
  explicit IncludingPreprocessingInfosCollector(SgProject *projectNode);
  std::map<std::string, std::set<PreprocessingInfo *>> collect();
  const std::map<std::string, std::set<std::string>> &
  getIncludedFilesMap() const;
};

#endif
