#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "FlangModuleInfo.h"

#include "Rose/StringUtility/Convert.h"
#include "Rose/StringUtility/Replace.h"
#include "sage-build.h"
#include "sage3basic.h"

using namespace std;

namespace {
const std::array<std::string, 3> kModuleSuffixes{".rcmp", ".rmod", ".mod"};

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

SgModuleStatement *
require_defining_module_statement(SgModuleStatement *module,
                                  const std::string &lower_name,
                                  const char *context) {
  if (module == nullptr || context == nullptr) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-definition]: "
        << (context != nullptr ? context : "module lookup")
        << " received a null module declaration";
    ROSE_ABORT();
  }

  SgModuleStatement *defining =
      isSgModuleStatement(module->get_definingDeclaration());
  if (defining == nullptr && module->get_definition() != nullptr) {
    defining = module;
  }
  SgModuleStatement *canonical =
      defining != nullptr
          ? isSgModuleStatement(defining->get_firstNondefiningDeclaration())
          : nullptr;
  if (defining == nullptr || canonical == nullptr ||
      defining->get_definingDeclaration() != defining ||
      canonical->get_definingDeclaration() != defining ||
      defining->get_definition() == nullptr ||
      !equals_case_insensitive(defining->get_name().str(), lower_name)) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-definition]: " << context
        << " did not resolve one exact defining declaration for module '"
        << lower_name << "'";
    ROSE_ABORT();
  }
  return defining;
}

void publish_external_module_ownership(SgSourceFile *source_file,
                                       const std::string &module_file) {
  if (source_file == nullptr) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "cannot attach module ownership for " << module_file
        << " without an active source file";
    ROSE_ABORT();
  }

  std::error_code ec;
  const std::filesystem::path canonical_path =
      std::filesystem::canonical(module_file, ec);
  if (ec || canonical_path.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "cannot canonicalize module ownership path " << module_file;
    ROSE_ABORT();
  }

  const std::string path = canonical_path.string();
  SgStringList &external_paths =
      source_file->get_frontendExternalOwnershipPathList();
  if (std::find(external_paths.begin(), external_paths.end(), path) ==
      external_paths.end()) {
    external_paths.push_back(path);
  }
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
  return "";
}

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

std::string find_intrinsic_module_file(const std::string &module_name) {
  const std::array<std::string, 4> intrinsic_dirs{
      find_flang_intrinsic_module_dir(),
      findRoseSupportPathFromSource("src/frontend/Flang/intrinsics",
                                    "src/frontend/Flang/intrinsics"),
      findRoseSupportPathFromBuild("src/frontend/Flang/intrinsics",
                                   "share/rose"),
      findRoseSupportPathFromSource("src/frontend/Flang/intrinsics",
                                    "share/rose")};
  for (const std::string &dir : intrinsic_dirs) {
    if (dir.empty()) {
      continue;
    }
    const std::string candidate = find_existing_module_file(
        (std::filesystem::path(dir) / module_name).string());
    if (!candidate.empty()) {
      return candidate;
    }
  }
  return "";
}

bool same_module_interface_identity(const std::string &left,
                                    const std::string &right) {
  if (left.empty() || right.empty()) {
    return false;
  }
  std::error_code leftError;
  std::error_code rightError;
  const std::filesystem::path leftCanonical =
      std::filesystem::canonical(left, leftError);
  const std::filesystem::path rightCanonical =
      std::filesystem::canonical(right, rightError);
  if (leftError || rightError) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-interface-identity]: "
           "cannot canonicalize exact module interface files '"
        << left << "' and '" << right << "'";
    ROSE_ABORT();
  }
  if (leftCanonical == rightCanonical) {
    return true;
  }

  const std::uintmax_t leftSize =
      std::filesystem::file_size(leftCanonical, leftError);
  const std::uintmax_t rightSize =
      std::filesystem::file_size(rightCanonical, rightError);
  if (leftError || rightError) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-interface-identity]: "
           "cannot read exact module interface file sizes for '"
        << leftCanonical.string() << "' and '" << rightCanonical.string()
        << "'";
    ROSE_ABORT();
  }
  if (leftSize != rightSize) {
    return false;
  }

  std::ifstream leftStream(leftCanonical, std::ios::binary);
  std::ifstream rightStream(rightCanonical, std::ios::binary);
  if (!leftStream.is_open() || !rightStream.is_open()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-interface-identity]: "
           "cannot open exact module interface files '"
        << leftCanonical.string() << "' and '" << rightCanonical.string()
        << "'";
    ROSE_ABORT();
  }
  const std::string leftContents{std::istreambuf_iterator<char>(leftStream),
                                 std::istreambuf_iterator<char>()};
  const std::string rightContents{std::istreambuf_iterator<char>(rightStream),
                                  std::istreambuf_iterator<char>()};
  if (leftStream.bad() || rightStream.bad()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-interface-identity]: "
           "cannot read exact module interface files '"
        << leftCanonical.string() << "' and '" << rightCanonical.string()
        << "'";
    ROSE_ABORT();
  }
  return leftContents == rightContents;
}

std::string require_exact_module_source_file(const std::string &moduleFile,
                                             const char *context) {
  std::error_code error;
  const std::filesystem::path canonical =
      std::filesystem::canonical(moduleFile, error);
  if (context == nullptr || moduleFile.empty() || error || canonical.empty() ||
      !std::filesystem::is_regular_file(canonical, error) || error) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-source-file]: "
        << (context != nullptr ? context : "module lookup")
        << " requires one exact existing semantic module source file, got '"
        << moduleFile << "'";
    ROSE_ABORT();
  }
  return canonical.string();
}

} // namespace

FlangModuleInfo::ModuleMapType FlangModuleInfo::moduleNameAstMap;
FlangModuleInfo::ModuleMapType FlangModuleInfo::intrinsicModuleNameAstMap;
std::vector<bool> FlangModuleInfo::activeIntrinsicModuleLoads;
unsigned int FlangModuleInfo::nestedSgFile;
SgProject *FlangModuleInfo::currentProject;
vector<string> FlangModuleInfo::inputDirs;

bool FlangModuleInfo::isModuleFile() { return nestedSgFile != 0; }

SgProject *FlangModuleInfo::getCurrentProject() { return currentProject; }

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

  const std::string flang_intrinsic_dir = find_flang_intrinsic_module_dir();
  addInputDir(inputDirs, flang_intrinsic_dir);

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
  if (currentProject != project) {
    if (!activeIntrinsicModuleLoads.empty()) {
      MLOG_ERROR_CXX("FlangModuleInfo")
          << "REX_FRONTEND_INVARIANT[fortran-module-registry]: project "
             "changed during an active module-load transaction";
      ROSE_ABORT();
    }
    moduleNameAstMap.clear();
    intrinsicModuleNameAstMap.clear();
  }
  currentProject = project;
}

void FlangModuleInfo::registerModule(SgModuleStatement *module) {
  if (module == nullptr || module->get_name().is_null()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-registry]: cannot register "
           "an unnamed or null module";
    ROSE_ABORT();
  }
  const string lowerName =
      Rose::StringUtility::convertToLowerCase(module->get_name().str());
  module = require_defining_module_statement(module, lowerName,
                                             "module registration");
  const bool intrinsic =
      !activeIntrinsicModuleLoads.empty() && activeIntrinsicModuleLoads.back();
  ModuleMapType &modules =
      intrinsic ? intrinsicModuleNameAstMap : moduleNameAstMap;
  auto insertion = modules.emplace(lowerName, module);
  if (!insertion.second && insertion.first->second != module) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-registry]: "
        << (intrinsic ? "intrinsic" : "non-intrinsic") << " module '"
        << lowerName << "' has two distinct AST declarations";
    ROSE_ABORT();
  }
}

SgModuleStatement *FlangModuleInfo::getModule(const string &modName,
                                              const string &moduleFile,
                                              ModuleNature nature) {
  const string lowerName = Rose::StringUtility::convertToLowerCase(modName);
  if (lowerName.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-lookup]: module name is "
           "empty";
    ROSE_ABORT();
  }

  auto registered = [&](ModuleMapType &modules,
                        const char *registryKind) -> SgModuleStatement * {
    const auto found = modules.find(lowerName);
    if (found == modules.end()) {
      return nullptr;
    }
    return require_defining_module_statement(found->second, lowerName,
                                             registryKind);
  };
  const string intrinsicPath = find_intrinsic_module_file(lowerName);
  const string nameWithPath =
      require_exact_module_source_file(moduleFile, "module AST lookup");
  const bool actualIntrinsic =
      same_module_interface_identity(nameWithPath, intrinsicPath);
  if (nature == ModuleNature::intrinsic && !actualIntrinsic) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-nature]: explicit "
           "intrinsic module '"
        << lowerName
        << "' did not resolve to a compiler or bundled intrinsic "
           "module file";
    ROSE_ABORT();
  }
  if (nature == ModuleNature::nonintrinsic && actualIntrinsic) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-nature]: explicit "
           "non-intrinsic module '"
        << lowerName
        << "' resolved only to a compiler or bundled intrinsic module file";
    ROSE_ABORT();
  }

  ModuleMapType &modules =
      actualIntrinsic ? intrinsicModuleNameAstMap : moduleNameAstMap;
  if (SgModuleStatement *module = registered(
          modules, actualIntrinsic ? "resolved intrinsic module lookup"
                                   : "resolved non-intrinsic module lookup")) {
    return module;
  }

  const size_t numberOfModulesBefore =
      moduleNameAstMap.size() + intrinsicModuleNameAstMap.size();
  if (SgProject::get_verbose() > 1) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "In FlangModuleInfo::getModule(" << lowerName
        << "): nameWithPath = " << nameWithPath
        << ", intrinsic = " << actualIntrinsic;
  }

  activeIntrinsicModuleLoads.push_back(actualIntrinsic);
  SgSourceFile *newModuleFile = createSgSourceFile(nameWithPath);
  if (activeIntrinsicModuleLoads.empty() ||
      activeIntrinsicModuleLoads.back() != actualIntrinsic) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-load-transaction]: module '"
        << lowerName << "' did not preserve its exact nested load nature";
    ROSE_ABORT();
  }
  activeIntrinsicModuleLoads.pop_back();
  if (newModuleFile == nullptr) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-lookup]: cannot parse "
           "module file for '"
        << lowerName << "'";
    ROSE_ABORT();
  }

  Rose_STL_Container<SgNode *> moduleDeclarationList =
      NodeQuery::querySubTree(newModuleFile, V_SgModuleStatement);
  if (moduleDeclarationList.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-definition]: parsed file '"
        << nameWithPath << "' contains no module declarations";
    ROSE_ABORT();
  }

  SgModuleStatement *modStmt = nullptr;
  for (SgNode *node : moduleDeclarationList) {
    SgModuleStatement *candidate = isSgModuleStatement(node);
    if (candidate == nullptr ||
        !equals_case_insensitive(candidate->get_name().str(), lowerName)) {
      continue;
    }
    SgModuleStatement *defining = require_defining_module_statement(
        candidate, lowerName, "parsed module lookup");
    if (modStmt != nullptr && modStmt != defining) {
      MLOG_ERROR_CXX("FlangModuleInfo")
          << "REX_FRONTEND_INVARIANT[fortran-module-definition]: "
          << (actualIntrinsic ? "intrinsic" : "non-intrinsic") << " module '"
          << lowerName << "' has two distinct defining declarations";
      ROSE_ABORT();
    }
    modStmt = defining;
  }
  if (modStmt == nullptr) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-definition]: parsed file '"
        << nameWithPath << "' does not define requested module '" << lowerName
        << "'";
    ROSE_ABORT();
  }

  const auto insertion = modules.emplace(lowerName, modStmt);
  if (!insertion.second && insertion.first->second != modStmt) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-definition]: "
        << (actualIntrinsic ? "intrinsic" : "non-intrinsic") << " module '"
        << lowerName
        << "' was registered with a different defining declaration during "
           "nested parsing";
    ROSE_ABORT();
  }
  const size_t numberOfModulesAfter =
      moduleNameAstMap.size() + intrinsicModuleNameAstMap.size();
  if (numberOfModulesAfter < numberOfModulesBefore) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-registry]: module registry "
           "shrunk during nested parsing";
    ROSE_ABORT();
  }

  if (SgProject::get_verbose() > 2) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "Leaving FlangModuleInfo::getModule(" << lowerName
        << "): modStmt = " << modStmt << ", intrinsic = " << actualIntrinsic;
  }
  return modStmt;
}

FlangModuleInfo::ModuleNature
FlangModuleInfo::requireModuleNatureForSourceFile(const string &modName,
                                                  const string &moduleFile) {
  const string lowerName = Rose::StringUtility::convertToLowerCase(modName);
  if (lowerName.empty()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-module-source-nature]: module "
           "name is missing";
    ROSE_ABORT();
  }
  const string exactFile = require_exact_module_source_file(
      moduleFile, "semantic module nature classification");

  const string intrinsicFile = find_intrinsic_module_file(lowerName);
  const ModuleNature nature =
      same_module_interface_identity(exactFile, intrinsicFile)
          ? ModuleNature::intrinsic
          : ModuleNature::nonintrinsic;
  if (SgProject::get_verbose() > 1) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "exact semantic module-file nature for '" << lowerName
        << "': source='" << exactFile << "', intrinsic='" << intrinsicFile
        << "', nature="
        << (nature == ModuleNature::intrinsic ? "intrinsic" : "nonintrinsic");
  }
  return nature;
}

SgSourceFile *FlangModuleInfo::createSgSourceFile(const string &moduleName) {
  int errorCode = 0;
  vector<string> argv;
  SgScopeStatement *saved_scope = SageBuilder::topScopeStack();
  SgSourceFile *saved_source_file = Rose::builder::getSgSourceFile();
  const auto savedSourcePositionMode =
      SageBuilder::getSourcePositionClassificationMode();

  const string moduleFileName =
      require_exact_module_source_file(moduleName, "nested module AST load");

  publish_external_module_ownership(saved_source_file, moduleFileName);

  std::string extension =
      std::filesystem::path(moduleFileName).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });

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

  SgFileList *externalFiles = saved_source_file->get_frontendExternalFileList();
  if (externalFiles == nullptr) {
    externalFiles = new SgFileList();
    ASSERT_not_null(externalFiles);
    externalFiles->set_parent(saved_source_file);
    saved_source_file->set_frontendExternalFileList(externalFiles);
  }
  if (externalFiles->get_parent() != saved_source_file ||
      newFile->get_parent() != project ||
      std::find(externalFiles->get_listOfFiles().begin(),
                externalFiles->get_listOfFiles().end(),
                newFile) != externalFiles->get_listOfFiles().end()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-external-module-owner]: module '"
        << moduleFileName
        << "' has no exact project-external AST ownership transaction";
    ROSE_ABORT();
  }
  externalFiles->get_listOfFiles().push_back(newFile);
  newFile->set_parent(externalFiles);
  if (newFile->get_parent() != externalFiles ||
      externalFiles->get_listOfFiles().empty() ||
      externalFiles->get_listOfFiles().back() != newFile ||
      std::find(project->get_fileList().begin(), project->get_fileList().end(),
                newFile) != project->get_fileList().end()) {
    MLOG_ERROR_CXX("FlangModuleInfo")
        << "REX_FRONTEND_INVARIANT[fortran-external-module-owner]: module '"
        << moduleFileName
        << "' did not acquire one exact non-lexical external project owner";
    ROSE_ABORT();
  }

  if (SgProject::get_verbose() > 1) {
    MLOG_DEBUG_CXX("FlangModuleInfo")
        << "END FlangModuleInfo::createSgSourceFile(" << moduleFileName
        << "): nestedSgFile = " << nestedSgFile;
  }

  nestedSgFile--;

  return newFile;
}

void FlangModuleInfo::clearMap() {
  moduleNameAstMap.clear();
  intrinsicModuleNameAstMap.clear();
}

void FlangModuleInfo::dumpMap() {
  std::map<std::string, SgModuleStatement *>::iterator iter;

  std::cout << "Module Statement* map::" << std::endl;
  for (iter = moduleNameAstMap.begin(); iter != moduleNameAstMap.end();
       iter++) {
    std::cout << "NON_INTRINSIC FIRST : " << (*iter).first
              << " SECOND : " << (*iter).second << std::endl;
  }
  for (iter = intrinsicModuleNameAstMap.begin();
       iter != intrinsicModuleNameAstMap.end(); iter++) {
    std::cout << "INTRINSIC FIRST : " << (*iter).first
              << " SECOND : " << (*iter).second << std::endl;
  }
}
