#include <list>

#include <map>

#include <set>

#include <string>

using namespace std;

class IncludingPreprocessingInfosCollector : public AstSimpleProcessing {
private:
  SgProject *projectNode;
  map<string, set<string>> includedFilesMap;

  map<string, set<PreprocessingInfo *>> includingPreprocessingInfosMap;
  set<SgSourceFile *> processedSourceFiles;

  void addIncludingPreprocessingInfoToMap(PreprocessingInfo *preprocessingInfo);
  void collectFromSourceFile(SgSourceFile *sourceFile);
  void matchIncludedAndIncludingFiles();

protected:
  void visit(SgNode *astNode);

public:
  ~IncludingPreprocessingInfosCollector();
  IncludingPreprocessingInfosCollector(
      SgProject *projectNode, const map<string, set<string>> &includedFilesMap);
  map<string, set<PreprocessingInfo *>> collect();
};
