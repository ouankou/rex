#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>

#include "FlangModuleInfo.h"

#include "Rose/StringUtility/Replace.h"

#include "sage3basic.h"

using namespace std;

namespace {
const std::array<std::string, 2> kModuleSuffixes{".rcmp", ".rmod"};
const std::array<std::string, 12> kModuleSourceSuffixes{
    ".f",   ".F",   ".f90", ".F90", ".f95", ".F95",
    ".f03", ".F03", ".f08", ".F08", ".f18", ".F18"};

bool equals_case_insensitive(const std::string &left,
                             const std::string &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(left[i])) !=
        std::tolower(static_cast<unsigned char>(right[i]))) {
      return false;
    }
  }
  return true;
}

string find_case_insensitive_file(const std::filesystem::path &dir,
                                  const std::string &name) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec) ||
      !std::filesystem::is_directory(dir, ec)) {
    return "";
  }
  for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    std::string filename = entry.path().filename().string();
    if (equals_case_insensitive(filename, name)) {
      return entry.path().string();
    }
  }
  return "";
}

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

  for (const auto &suffix : kModuleSourceSuffixes) {
    string candidate = baseName + suffix;
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  std::filesystem::path basePath(baseName);
  std::filesystem::path dir = basePath.has_parent_path()
                                  ? basePath.parent_path()
                                  : std::filesystem::path(".");
  std::string stem = basePath.filename().string();

  string match = find_case_insensitive_file(dir, stem);
  if (!match.empty()) {
    return match;
  }
  for (const auto &suffix : kModuleSuffixes) {
    match = find_case_insensitive_file(dir, stem + suffix);
    if (!match.empty()) {
      return match;
    }
  }
  for (const auto &suffix : kModuleSourceSuffixes) {
    match = find_case_insensitive_file(dir, stem + suffix);
    if (!match.empty()) {
      return match;
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
      return candidate;
    }
  }

  return basename;
}

void FlangModuleInfo::set_inputDirs(SgProject *project) {
  inputDirs.clear();
  vector<string> args = project->get_originalCommandLineArgumentList();
  string rmodDir;

  auto addInputDir = [](std::vector<std::string> &dirs,
                        const std::string &dir) {
    if (dir.empty()) {
      return;
    }
    std::filesystem::path path(dir);
    if (!std::filesystem::exists(path)) {
      return;
    }
    std::string normalized = path.lexically_normal().string();
    if (std::find(dirs.begin(), dirs.end(), normalized) == dirs.end()) {
      dirs.push_back(normalized);
    }
  };

  // Add path to intrinsic modules (ISO_C_BINDING, OMP_LIB, etc.).
  const std::string intrinsic_src_path =
      findRoseSupportPathFromSource("src/3rdPartyLibraries/fortran-parser",
                                    "src/3rdPartyLibraries/fortran-parser");
  const std::string intrinsic_install_path = findRoseSupportPathFromSource(
      "src/3rdPartyLibraries/fortran-parser", "share/rose");
  addInputDir(inputDirs, intrinsic_src_path);
  addInputDir(inputDirs, intrinsic_install_path);

  // Add source file directories to find module sources in the same tree.
  for (const auto &source : project->get_sourceFileNameList()) {
    std::filesystem::path source_path(source);
    std::string dir = source_path.has_parent_path()
                          ? source_path.parent_path().string()
                          : std::string(".");
    addInputDir(inputDirs, dir);
  }

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
  SgScopeStatement *saved_scope = SageBuilder::topScopeStack();

  const string moduleBase = Rose::StringUtility::convertToLowerCase(moduleName);
  string moduleFileName = find_existing_module_file(moduleName);
  if (moduleFileName.empty()) {
    moduleFileName = find_existing_module_file(moduleBase);
  }

  if (moduleFileName.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "File moduleFileName = " << moduleBase
        << "[.rcmp|.rmod] NOT FOUND (expected to be present)";
    return nullptr;
  }

  std::string extension =
      std::filesystem::path(moduleFileName).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  argv.push_back(SKIP_SYNTAX_CHECK);

  for (const auto &dir : inputDirs) {
    if (!dir.empty()) {
      argv.push_back("-I" + dir);
    }
  }

  if (SgProject *project = getCurrentProject()) {
    const SgStringList &macroSpecifierList = project->get_macroSpecifierList();
    for (const auto &macro : macroSpecifierList) {
      argv.push_back("-D" + macro);
    }
  }

  if (extension == ".rmod" || extension == ".rcmp") {
    argv.push_back("-ffree-form");
  }

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

  if (extension == ".rmod" || extension == ".rcmp") {
    newFile->set_sourceFileUsesFortran90FileExtension(true);
    newFile->set_outputFormat(SgFile::e_free_form_output_format);
    newFile->set_backendCompileFormat(SgFile::e_free_form_output_format);
    newFile->set_F90_only();
  }

  newFile->runFrontend(errorCode);
  ROSE_ASSERT(errorCode == 0);

  if (saved_scope != nullptr) {
    while (SageBuilder::topScopeStack() != saved_scope) {
      ASSERT_not_null(SageBuilder::topScopeStack());
      SageBuilder::popScopeStack();
    }
  } else {
    SageBuilder::clearScopeStack();
  }

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
