#ifndef ROSE_FLANG_MODULE_INFO_H_
#define ROSE_FLANG_MODULE_INFO_H_

#include <map>
#include <string>
#include <vector>

#define SKIP_SYNTAX_CHECK "-rose:skip_syntax_check"

class SgModuleStatement;
class SgProject;
class SgSourceFile;

class FlangModuleInfo {
private:
  using ModuleMapType = std::map<std::string, SgModuleStatement *>;

  static SgProject *currentProject;
  static ModuleMapType moduleNameAstMap;
  static unsigned int nestedSgFile;
  static std::vector<std::string> inputDirs;
  static std::vector<std::string> sourceDirs;

public:
  static bool isModuleFile();
  static void setCurrentProject(SgProject *project);
  static SgProject *getCurrentProject();

  static SgModuleStatement *getModule(const std::string &modName);

  static void set_inputDirs(SgProject *project);

  FlangModuleInfo() = default;
  ~FlangModuleInfo() = default;

private:
  static std::string find_file_from_inputDirs(const std::string &basename);
  static SgSourceFile *createSgSourceFile(const std::string &moduleName);
  static void clearMap();
  static void dumpMap();
};

#endif /* ROSE_FLANG_MODULE_INFO_H_ */
