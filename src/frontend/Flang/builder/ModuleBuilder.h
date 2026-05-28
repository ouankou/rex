#ifndef ROSE_MODULE_BUILDER_H_
#define ROSE_MODULE_BUILDER_H_

#include <map>
#include <string>
#include <vector>

#define TEMPLATES 0

class SgProject;
class SgSourceFile;
class SgGlobal;
class SgSymbol;
class SgSymbolTable;
class SgEnumSymbol;
class SgClassSymbol;
class SgVariableSymbol;
class SgType;

namespace SageInterface {
SgProject *getProject();
}

// Need std=c++11
//
#if __cplusplus < 201103L
#ifndef nullptr
#define nullptr NULL
#endif
#endif

namespace Rose {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Load a Fortran module file.

/*
 *  Read a module file "*.{rmod,rcmp}"
 *  Create a SgFile for the module
 *  Create an entry for the module in the moduleNameMap
 */

#define SKIP_SYNTAX_CHECK "-rose:skip_syntax_check"

class ModuleBuilder {
  typedef std::map<std::string, SgSourceFile *> ModuleMapType;

public:
  ModuleBuilder() : nestedSgFile(0), currentProject(nullptr) {}

  bool areLoadingModuleFile() { return (nestedSgFile > 0 ? true : false); }

  void setCurrentProject(SgProject *project);
  SgProject *getCurrentProject() { return SageInterface::getProject(); }

  void loadModule(const std::string &module_name,
                  std::vector<std::string> &import_names, SgGlobal *file_scope);
  SgSourceFile *getModule(const std::string &module_name);
  void addMapping(const std::string &module_name, SgSourceFile *module_stmt);

  std::string getModuleFileSuffix() { return std::string(".rcmp"); }
  std::string findFileFromInputDirs(const std::string &name);
  std::string namespaceSymbolName(const std::string &name);

  void setInputDirs(SgProject *);

private:
  unsigned int nestedSgFile;
  SgProject *currentProject;

  std::vector<std::string> inputDirs;
  ModuleMapType moduleNameMap;

  SgSourceFile *createSgSourceFile(const std::string &module_name);

  void insertSymbol(SgSymbol *symbol, SgGlobal *file_scope);
  void loadSymbol(SgSymbol *symbol, SgSymbolTable *symbol_table,
                  SgGlobal *file_scope);
  void loadSymbol(SgEnumSymbol *symbol, SgSymbolTable *symbol_table,
                  SgGlobal *file_scope);
  void loadSymbol(SgClassSymbol *symbol, SgSymbolTable *symbol_table,
                  SgGlobal *file_scope);
  void loadSymbol(SgVariableSymbol *symbol, SgSymbolTable *symbol_table,
                  SgGlobal *file_scope);
  void loadTypeSymbol(SgType *type, SgSymbolTable *symbol_table,
                      SgGlobal *file_scope);

  void clearMap() { moduleNameMap.clear(); }
  void dumpMap();
};

class ModuleBuilderFactory {
public:
  static ModuleBuilder &get_compool_builder() {
    static ModuleBuilder compool_builder;
    return compool_builder;
  }
};

} // namespace Rose

#endif /* ROSE_MODULE_BUILDER_H_ */
