#ifndef ROSE_FLANG_MODULE_INFO_H_
#define ROSE_FLANG_MODULE_INFO_H_

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class SgModuleStatement;
class SgProject;
class SgSourceFile;
class SgNode;

class FlangModuleInfo {
private:
  using ModuleMapType = std::map<std::string, SgModuleStatement *>;

  static SgProject *currentProject;
  static ModuleMapType moduleNameAstMap;
  static ModuleMapType intrinsicModuleNameAstMap;
  static std::vector<bool> activeIntrinsicModuleLoads;
  static unsigned int nestedSgFile;
  static std::vector<std::string> inputDirs;

public:
  enum class ModuleNature { intrinsic, nonintrinsic };

  static bool isModuleFile();

  static void registerModule(SgModuleStatement *module);
  static void replaceAstIdentities(
      SgProject *project,
      const std::unordered_map<SgNode *, SgNode *> &replacements);
  static SgModuleStatement *getModule(const std::string &modName,
                                      const std::string &moduleFile,
                                      ModuleNature nature);
  static ModuleNature
  requireModuleNatureForSourceFile(const std::string &modName,
                                   const std::string &moduleFile);

  explicit FlangModuleInfo(SgProject *project);
  ~FlangModuleInfo();
  FlangModuleInfo(const FlangModuleInfo &) = delete;
  FlangModuleInfo &operator=(const FlangModuleInfo &) = delete;

private:
  SgProject *project_;
  static void set_inputDirs(SgProject *project);
  static SgSourceFile *createSgSourceFile(const std::string &moduleName);
  static void clearMap();
};

#endif /* ROSE_FLANG_MODULE_INFO_H_ */
