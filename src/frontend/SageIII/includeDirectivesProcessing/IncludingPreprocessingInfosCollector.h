#include <list>

#include <map>

#include <set>

#include <string>

using namespace std;

class IncludingPreprocessingInfosCollector : public AstSimpleProcessing {
private:
  SgProject *projectNode;
  map<string, set<string>> normalizedIncludedFilesMap;

  map<string, set<PreprocessingInfo *>> includingPreprocessingInfosMap;
  set<PreprocessingInfo *> observedIncludePreprocessingInfos;
  set<SgSourceFile *> processedSourceFiles;

  void observeIncludePreprocessingInfo(PreprocessingInfo *preprocessingInfo);
  void collectFromSourceFile(SgSourceFile *sourceFile);

protected:
  void visit(SgNode *astNode);

public:
  ~IncludingPreprocessingInfosCollector();
  explicit IncludingPreprocessingInfosCollector(SgProject *projectNode);
  map<string, set<PreprocessingInfo *>> collect();
  const map<string, set<string>> &getIncludedFilesMap() const;
};
