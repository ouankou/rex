#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "FlangModuleInfo.h"

#include "Rose/StringUtility/Convert.h"
#include "Rose/StringUtility/Replace.h"
#include "sage-build.h"
#include "sage3basic.h"

using namespace std;

namespace {
const std::array<std::string, 3> kModuleSuffixes{".rcmp", ".rmod", ".mod"};
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

bool has_fortran_source_suffix(const std::filesystem::path &path) {
  std::string ext = path.extension().string();
  if (ext.empty()) {
    return false;
  }
  for (const auto &suffix : kModuleSourceSuffixes) {
    if (equals_case_insensitive(ext, suffix)) {
      return true;
    }
  }
  return false;
}

bool is_intrinsic_module_name_lower(const std::string &name_lower) {
  static const std::array<std::string, 10> intrinsic_modules = {
      "iso_fortran_env", "iso_fortran_env_impl", "iso_c_binding",
      "omp_lib",         "omp_lib_kinds",        "openacc",
      "openacc_kinds",   "ieee_arithmetic",      "ieee_exceptions",
      "ieee_features"};
  for (const auto &module_name : intrinsic_modules) {
    if (name_lower == module_name) {
      return true;
    }
  }
  if (name_lower.rfind("__fortran_", 0) == 0) {
    return true;
  }
  return false;
}

bool is_compiler_builtin_module_name_lower(const std::string &name_lower) {
  return name_lower == "iso_fortran_env" ||
         name_lower == "iso_fortran_env_impl" ||
         name_lower.rfind("__fortran_", 0) == 0;
}

bool should_prefer_flang_intrinsic_module_lower(const std::string &name_lower) {
  return name_lower == "iso_c_binding" || name_lower == "omp_lib" ||
         name_lower == "omp_lib_kinds";
}

bool is_fixed_form_source(const std::filesystem::path &path) {
  std::string ext = path.extension().string();
  return equals_case_insensitive(ext, ".f");
}

std::map<std::string, std::string> module_source_index;
bool module_source_index_built = false;

std::string find_flang_intrinsic_module_dir() {
  std::string include_dir;
#ifdef ROSE_FLANG_INTRINSIC_INCLUDEDIR
  include_dir = ROSE_FLANG_INTRINSIC_INCLUDEDIR;
#endif
  if (include_dir.empty()) {
    return "";
  }

  std::filesystem::path flang_dir = include_dir;
  std::error_code ec;
  if (std::filesystem::exists(flang_dir, ec) &&
      std::filesystem::is_directory(flang_dir, ec)) {
    return flang_dir.string();
  }

  return "";
}

void index_module_source_file(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return;
  }

  const bool fixed_form = is_fixed_form_source(path);
  std::string line;
  while (std::getline(in, line)) {
    if (fixed_form && !line.empty()) {
      char first = line[0];
      if (first == 'c' || first == 'C' || first == '*' || first == '!') {
        continue;
      }
    }

    size_t comment_pos = line.find('!');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }

    std::string trimmed = Rose::StringUtility::trim(line);
    if (trimmed.empty()) {
      continue;
    }

    std::string lowered = Rose::StringUtility::convertToLowerCase(trimmed);
    if (lowered.rfind("module", 0) != 0) {
      continue;
    }

    std::string rest = Rose::StringUtility::trim(lowered.substr(6));
    if (rest.empty()) {
      continue;
    }
    if (rest.rfind("procedure", 0) == 0 || rest.rfind("function", 0) == 0 ||
        rest.rfind("subroutine", 0) == 0) {
      continue;
    }

    size_t end = 0;
    while (end < rest.size() &&
           (std::isalnum(static_cast<unsigned char>(rest[end])) ||
            rest[end] == '_')) {
      ++end;
    }
    std::string name = rest.substr(0, end);
    if (name.empty()) {
      continue;
    }

    if (module_source_index.find(name) == module_source_index.end()) {
      module_source_index[name] = path.string();
    }
  }
}

void build_module_source_index(const std::vector<std::string> &dirs) {
  if (module_source_index_built) {
    return;
  }
  module_source_index_built = true;

  for (const auto &dir : dirs) {
    std::filesystem::path path(dir);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) ||
        !std::filesystem::is_directory(path, ec)) {
      continue;
    }

    for (const auto &entry : std::filesystem::directory_iterator(path, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      const std::filesystem::path &candidate = entry.path();
      if (!has_fortran_source_suffix(candidate)) {
        continue;
      }
      index_module_source_file(candidate);
    }
  }
}

std::string find_module_source_by_name(const std::string &module_name,
                                       const std::vector<std::string> &dirs) {
  build_module_source_index(dirs);
  std::map<std::string, std::string>::const_iterator it =
      module_source_index.find(module_name);
  if (it != module_source_index.end()) {
    return it->second;
  }
  return "";
}

void clear_module_source_index() {
  module_source_index.clear();
  module_source_index_built = false;
}
} // namespace

FlangModuleInfo::ModuleMapType FlangModuleInfo::moduleNameAstMap;
unsigned int FlangModuleInfo::nestedSgFile;
SgProject *FlangModuleInfo::currentProject;
vector<string> FlangModuleInfo::inputDirs;
vector<string> FlangModuleInfo::sourceDirs;

bool FlangModuleInfo::isModuleFile() { return nestedSgFile != 0; }

SgProject *FlangModuleInfo::getCurrentProject() { return currentProject; }

string FlangModuleInfo::find_file_from_inputDirs(const string &basename) {
  std::string module_name = Rose::StringUtility::convertToLowerCase(
      std::filesystem::path(basename).filename().string());

  // Prefer compiler-provided intrinsic modules when available. These modules
  // carry target-specific ABI and OpenMP runtime details from the Flang build.
  if (should_prefer_flang_intrinsic_module_lower(module_name)) {
    const std::string flang_intrinsic_dir = find_flang_intrinsic_module_dir();
    if (!flang_intrinsic_dir.empty()) {
      const std::string flang_candidate = find_existing_module_file(
          (std::filesystem::path(flang_intrinsic_dir) / module_name).string());
      if (!flang_candidate.empty()) {
        return flang_candidate;
      }
    }
  }

  std::string module_source =
      find_module_source_by_name(module_name, sourceDirs);
  if (!module_source.empty()) {
    return module_source;
  }

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
  sourceDirs.clear();
  clear_module_source_index();
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

  // Add bundled intrinsic modules that LLVM Flang does not provide, currently
  // OpenACC.
  const std::string intrinsic_src_path = findRoseSupportPathFromSource(
      "src/frontend/Flang/intrinsics", "src/frontend/Flang/intrinsics");
  const std::string intrinsic_build_path = findRoseSupportPathFromBuild(
      "src/frontend/Flang/intrinsics", "share/rose");
  const std::string intrinsic_install_path = findRoseSupportPathFromSource(
      "src/frontend/Flang/intrinsics", "share/rose");
  addInputDir(inputDirs, intrinsic_src_path);
  addInputDir(inputDirs, intrinsic_build_path);
  addInputDir(inputDirs, intrinsic_install_path);
  addInputDir(sourceDirs, intrinsic_src_path);
  addInputDir(sourceDirs, intrinsic_install_path);

  const std::string flang_intrinsic_dir = find_flang_intrinsic_module_dir();
  addInputDir(inputDirs, flang_intrinsic_dir);
  addInputDir(sourceDirs, flang_intrinsic_dir);

  // Add source file directories to find module sources in the same tree.
  for (const auto &source : project->get_sourceFileNameList()) {
    std::filesystem::path source_path(source);
    std::string dir = source_path.has_parent_path()
                          ? source_path.parent_path().string()
                          : std::string(".");
    addInputDir(inputDirs, dir);
    addInputDir(sourceDirs, dir);
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

  if (is_compiler_builtin_module_name_lower(lowerName)) {
    return nullptr;
  }

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

bool FlangModuleInfo::isIntrinsicModuleName(const string &modName) {
  return is_intrinsic_module_name_lower(
      Rose::StringUtility::convertToLowerCase(modName));
}

SgSourceFile *FlangModuleInfo::createSgSourceFile(const string &moduleName) {
  int errorCode = 0;
  vector<string> argv;
  SgScopeStatement *saved_scope = SageBuilder::topScopeStack();
  SgSourceFile *saved_source_file = Rose::builder::getSgSourceFile();
  const auto savedSourcePositionMode =
      SageBuilder::getSourcePositionClassificationMode();

  const string moduleBase = Rose::StringUtility::convertToLowerCase(moduleName);
  string moduleFileName = find_existing_module_file(moduleName);
  if (moduleFileName.empty()) {
    moduleFileName = find_existing_module_file(moduleBase);
  }

  if (moduleFileName.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "File moduleFileName = " << moduleBase
        << "[.rcmp|.rmod|.mod] NOT FOUND (expected to be present)";
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

  if (extension == ".rmod" || extension == ".rcmp" || extension == ".mod") {
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

  if (extension == ".rmod" || extension == ".rcmp" || extension == ".mod") {
    newFile->set_sourceFileUsesFortran90FileExtension(true);
    newFile->set_outputFormat(SgFile::e_free_form_output_format);
    newFile->set_backendCompileFormat(SgFile::e_free_form_output_format);
    newFile->set_F90_only();
  }

  newFile->runFrontend(errorCode);
  ROSE_ASSERT(errorCode == 0);

  Rose::builder::setSgSourceFile(saved_source_file);
  SageBuilder::setSourcePositionClassificationMode(savedSourcePositionMode);

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
