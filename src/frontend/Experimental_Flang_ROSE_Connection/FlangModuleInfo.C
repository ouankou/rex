#include <array>
#include <filesystem>

#include "FlangModuleInfo.h"

#include "Rose/StringUtility/Replace.h"

#include "sage3basic.h"

using namespace std;

namespace {
const std::array<std::string, 2> kModuleSuffixes{".rcmp", ".rmod"};

string find_existing_module_file(const string &baseName) {
  if (std::filesystem::exists(baseName)) {
    return baseName;
  }

  for (const auto &suffix : kModuleSuffixes) {
    string candidate = baseName + suffix;
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  return "";
}
} // namespace

FlangModuleInfo::ModuleMapType FlangModuleInfo::moduleNameAstMap;
unsigned int FlangModuleInfo::nestedSgFile;
SgProject *FlangModuleInfo::currentProject;
vector<string> FlangModuleInfo::inputDirs;

bool FlangModuleInfo::isModuleFile() { return nestedSgFile != 0; }

SgProject *FlangModuleInfo::getCurrentProject() { return currentProject; }

string FlangModuleInfo::find_file_from_inputDirs(const string &basename) {
  for (const auto &dir : inputDirs) {
    string name = (std::filesystem::path(dir) / basename).string();
    string candidate = find_existing_module_file(name);
    if (!candidate.empty()) {
      return name;
    }
  }

  return basename;
}

void FlangModuleInfo::set_inputDirs(SgProject *project) {
  inputDirs.clear();
  vector<string> args = project->get_originalCommandLineArgumentList();
  string rmodDir;

  // Add path to intrinsic modules (ISO_C_BINDING, OMP_LIB, etc.).
  string intrinsic_mod_path = findRoseSupportPathFromSource(
      "src/3rdPartyLibraries/fortran-parser", "share/rose");
  inputDirs.push_back(intrinsic_mod_path);

  int sizeArgs = args.size();
  for (int i = 0; i < sizeArgs; i++) {
    if (args[i].find("-I", 0) == 0) {
      rmodDir = args[i].substr(2);
      string rmodDir_no_quotes =
          Rose::StringUtility::replaceAllCopy(rmodDir, "\"", "");

      if (std::filesystem::exists(rmodDir_no_quotes.c_str())) {
        inputDirs.push_back(rmodDir_no_quotes);
      } else {
        MLOG_DEBUG_CXX("FlangModuleInfo")
            << "The input directory does not exist (rose): " << rmodDir << endl;
      }
    }
  }
}

void FlangModuleInfo::setCurrentProject(SgProject *project) {
  currentProject = project;
}

SgModuleStatement *FlangModuleInfo::getModule(const string &modName) {
  size_t numberOfModules_before = moduleNameAstMap.size();

  string lowerName = Rose::StringUtility::convertToLowerCase(modName);

  ModuleMapType::iterator mapIterator = moduleNameAstMap.find(lowerName);
  SgModuleStatement *modStmt =
      (mapIterator != moduleNameAstMap.end()) ? mapIterator->second : nullptr;

  size_t numberOfModules_after = moduleNameAstMap.size();
  ROSE_ASSERT(numberOfModules_after >= numberOfModules_before);

  if (modStmt != nullptr) {
    if (SgProject::get_verbose() > 1) {
      MLOG_DEBUG_CXX("FlangModuleInfo")
          << "This module has been previously processed in this compilation "
             "unit.";
    }
    return modStmt;
  }

  string nameWithPath = find_file_from_inputDirs(lowerName);
  if (SgProject::get_verbose() > 1) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "In FlangModuleInfo::getModule(" << lowerName
        << "): nameWithPath = " << nameWithPath;
  }

  SgSourceFile *newModuleFile = createSgSourceFile(nameWithPath);
  if (newModuleFile == nullptr) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "In FlangModuleInfo::getModule(" << lowerName
        << "): cannot locate module file";
    return nullptr;
  }

  Rose_STL_Container<SgNode *> moduleDeclarationList =
      NodeQuery::querySubTree(newModuleFile, V_SgModuleStatement);
  if (moduleDeclarationList.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "In FlangModuleInfo::getModule(" << lowerName
        << "): no module declarations found";
    return nullptr;
  }

  modStmt = isSgModuleStatement(moduleDeclarationList[0]);
  ROSE_ASSERT(modStmt != nullptr);

  moduleNameAstMap.insert(ModuleMapType::value_type(lowerName, modStmt));

  if (SgProject::get_verbose() > 2) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "Leaving FlangModuleInfo::getModule(" << lowerName
        << "): modStmt = " << modStmt;
  }

  return modStmt;
}

SgSourceFile *FlangModuleInfo::createSgSourceFile(const string &moduleName) {
  int errorCode = 0;
  vector<string> argv;

  string moduleBase = Rose::StringUtility::convertToLowerCase(moduleName);
  string moduleFileName = find_existing_module_file(moduleBase);

  if (moduleFileName.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "File moduleFileName = " << moduleBase
        << "[.rcmp|.rmod] NOT FOUND (expected to be present)";
    return nullptr;
  }

  argv.push_back(SKIP_SYNTAX_CHECK);
  argv.push_back(moduleFileName);

  nestedSgFile++;

  if (SgProject::get_verbose() > 1) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "START FlangModuleInfo::createSgSourceFile(" << moduleFileName
        << "): nestedSgFile = " << nestedSgFile;
  }

  SgProject *project = getCurrentProject();
  SgSourceFile *newFile =
      isSgSourceFile(determineFileType(argv, errorCode, project));
  ROSE_ASSERT(newFile != nullptr);

  newFile->runFrontend(errorCode);
  ROSE_ASSERT(errorCode == 0);

  ROSE_ASSERT(newFile->get_startOfConstruct() != nullptr);

  // Mark module file as non-output.
  newFile->set_skipfinalCompileStep(true);
  newFile->set_skip_unparse(true);

  project->set_file(*newFile);

  if (SgProject::get_verbose() > 1) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "END FlangModuleInfo::createSgSourceFile(" << moduleFileName
        << "): nestedSgFile = " << nestedSgFile;
  }

  nestedSgFile--;

  return newFile;
}

void FlangModuleInfo::clearMap() { moduleNameAstMap.clear(); }

void FlangModuleInfo::dumpMap() {
  std::map<std::string, SgModuleStatement *>::iterator iter;

  std::cout << "Module Statement* map::" << std::endl;
  for (iter = moduleNameAstMap.begin(); iter != moduleNameAstMap.end();
       iter++) {
    std::cout << "FIRST : " << (*iter).first << " SECOND : " << (*iter).second
              << std::endl;
  }
}
