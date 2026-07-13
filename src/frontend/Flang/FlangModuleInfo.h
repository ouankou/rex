#ifndef ROSE_FLANG_MODULE_INFO_H_
#define ROSE_FLANG_MODULE_INFO_H_

#include <map>
#include <string>
#include <vector>

class SgModuleStatement;
class SgProject;
class SgSourceFile;

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
  static void setCurrentProject(SgProject *project);
  static SgProject *getCurrentProject();

  static void registerModule(SgModuleStatement *module);
  static SgModuleStatement *getModule(const std::string &modName,
                                      const std::string &moduleFile,
                                      ModuleNature nature);
  static ModuleNature
  requireModuleNatureForSourceFile(const std::string &modName,
                                   const std::string &moduleFile);

  static void set_inputDirs(SgProject *project);

  FlangModuleInfo() = default;
  ~FlangModuleInfo() = default;

private:
  static SgSourceFile *createSgSourceFile(const std::string &moduleName);
  static void clearMap();
  static void dumpMap();
};

#endif /* ROSE_FLANG_MODULE_INFO_H_ */
