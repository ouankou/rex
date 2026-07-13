#include "sage3basic.h"

#include "unparseFortran_modfile.h"

#include "unparser.h"

#include "unparser_opt.h"

#include "rose_test_output_path.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace Rose;

namespace {
void requireExactModuleOutputOwner(SgSourceFile *sourceFile,
                                   SgModuleStatement *module) {
  ASSERT_not_null(sourceFile);
  ASSERT_not_null(module);
  Sg_File_Info *sourceInfo = sourceFile->get_file_info();
  Sg_File_Info *moduleInfo = module->get_file_info();
  const std::string physicalPath =
      moduleInfo != nullptr ? moduleInfo->get_physical_filename() : "";
  const std::size_t frontendInputOwners =
      !physicalPath.empty()
          ? std::count(
                sourceFile->get_frontendIncludeOwnershipPathList().begin(),
                sourceFile->get_frontendIncludeOwnershipPathList().end(),
                physicalPath) +
                std::count(
                    sourceFile->get_frontendExternalOwnershipPathList().begin(),
                    sourceFile->get_frontendExternalOwnershipPathList().end(),
                    physicalPath)
          : 0;
  const bool exactFrontendInput =
      moduleInfo != nullptr && !moduleInfo->isOutputInCodeGeneration() &&
      !moduleInfo->isCompilerGenerated() && frontendInputOwners == 1;
  if (sourceInfo == nullptr || moduleInfo == nullptr ||
      sourceInfo->get_physical_file_id() < 0 ||
      moduleInfo->get_physical_file_id() < 0 || sourceInfo->isShared() ||
      moduleInfo->isShared() || moduleInfo->isCompilerGenerated() ||
      (!moduleInfo->isOutputInCodeGeneration() && !exactFrontendInput) ||
      SageInterface::getEnclosingSourceFile(module) != sourceFile) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-module-output-owner]: module '"
              << module->get_name()
              << "' does not have one exact physical source range and active "
                 "Fortran translation-unit owner"
              << std::endl;
    ROSE_ABORT();
  }
}

void writeModuleFileAtomically(const std::string &outputFilename,
                               const std::string &contents) {
  const std::filesystem::path target(outputFilename);
  const std::filesystem::path parent =
      target.has_parent_path() ? target.parent_path() : ".";
  std::error_code error;
  if (!std::filesystem::is_directory(parent, error) || error) {
    std::cerr << "Error: REX module output directory is unavailable: " << parent
              << std::endl;
    ROSE_ABORT();
  }

  std::string stagingTemplate =
      (parent / (target.filename().string() + ".rex-tmp-XXXXXX")).string();
  std::vector<char> stagingName(stagingTemplate.begin(), stagingTemplate.end());
  stagingName.push_back('\0');
  const int descriptor = ::mkstemp(stagingName.data());
  if (descriptor < 0) {
    std::cerr << "Error: cannot create atomic REX module staging file: "
              << std::strerror(errno) << std::endl;
    ROSE_ABORT();
  }

  if (::fchmod(descriptor, 0644) != 0) {
    std::cerr << "Error: cannot set REX module staging permissions: "
              << std::strerror(errno) << std::endl;
    ROSE_ABORT();
  }

  size_t written = 0;
  while (written < contents.size()) {
    const ssize_t count = ::write(descriptor, contents.data() + written,
                                  contents.size() - written);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      std::cerr << "Error: cannot write REX module staging file: "
                << std::strerror(errno) << std::endl;
      ROSE_ABORT();
    }
    written += static_cast<size_t>(count);
  }
  if (::fsync(descriptor) != 0 || ::close(descriptor) != 0) {
    std::cerr << "Error: cannot finalize REX module staging file: "
              << std::strerror(errno) << std::endl;
    ROSE_ABORT();
  }

  std::filesystem::rename(stagingName.data(), target, error);
  if (error) {
    std::filesystem::remove(stagingName.data());
    std::cerr << "Error: cannot atomically install REX module file: "
              << error.message() << std::endl;
    ROSE_ABORT();
  }
}
} // namespace

string get_rmod_dir(SgFile *sfile) {
  vector<string> args = sfile->get_originalCommandLineArgumentList();
  string rmodDir;

  if (CommandlineProcessing::isOptionWithParameter(args, "-outputdir", "",
                                                   rmodDir, true) == true)
    return rmodDir;
  else
    return "";
}

void generateModFile(SgFile *sfile) {
  ASSERT_not_null(sfile);
  ASSERT_not_null(sfile->get_file_info());

  // file name, with full path.
  string originalModuleFilenameWithPath =
      sfile->get_file_info()->get_filenameString();
  if (originalModuleFilenameWithPath.empty()) {
    std::cerr << "Error: cannot generate REX module file without an input "
                 "filename"
              << std::endl;
    ROSE_ABORT();
  }

  if (SgProject::get_verbose() > 0) {
    printf("In generateModFile(): Generating a Fortran 90 specific module "
           "(*.rmod file) for file = %s \n",
           originalModuleFilenameWithPath.c_str());
  }

  SgSourceFile *sourceFile = isSgSourceFile(sfile);
  ASSERT_not_null(sourceFile);
  SgGlobal *globalScope = sourceFile->get_globalScope();
  ASSERT_not_null(globalScope);

  // A class-like module has a semantic canonical forward declaration as well
  // as its source-spelled defining declaration.  Only direct declarations in
  // the file's global lexical statement list own module-file output; a subtree
  // query also visits the semantic forward through the auxiliary-declaration
  // container and therefore invents a second output owner.
  std::vector<SgModuleStatement *> moduleDeclarationList;
  for (SgDeclarationStatement *declaration : globalScope->get_declarations()) {
    if (SgModuleStatement *module = isSgModuleStatement(declaration)) {
      requireExactModuleOutputOwner(sourceFile, module);
      if (module->get_parent() != globalScope ||
          module->get_scope() != globalScope ||
          module->get_definition() == nullptr ||
          module->get_definingDeclaration() != module) {
        std::cerr
            << "REX_UNPARSE_INVARIANT[fortran-module-output-declaration]: "
               "module '"
            << module->get_name()
            << "' is not one exact source-defining global declaration"
            << std::endl;
        ROSE_ABORT();
      }
      moduleDeclarationList.push_back(module);
    }
  }

  std::set<std::string> plannedOutputs;
  for (SgModuleStatement *module : moduleDeclarationList) {
    if (module->get_name().is_null()) {
      std::cerr << "Error: cannot generate a REX module file for an unnamed "
                   "module"
                << std::endl;
      ROSE_ABORT();
    }
    const std::string outputFilename = Rose::TestOutput::resolvePath(
        StringUtility::convertToLowerCase(module->get_name().getString()) +
            MOD_FILE_SUFFIX,
        get_rmod_dir(sfile));
    if (!plannedOutputs.insert(outputFilename).second) {
      std::cerr << "Error: multiple Fortran modules map to REX module output "
                << outputFilename << std::endl;
      ROSE_ABORT();
    }
  }

  for (SgModuleStatement *module_stmt : moduleDeclarationList) {
    // For a module named "xx" generate a file "xx.rose_mod" which contains
    // all the variable definitions and function declarations
    string outputDir = get_rmod_dir(sfile);
    string lowerModuleName =
        StringUtility::convertToLowerCase(module_stmt->get_name().getString());
    string outputFilename = Rose::TestOutput::resolvePath(
        lowerModuleName + MOD_FILE_SUFFIX, outputDir);

    // Cause the output of a message with verbose level is turned on.
    if (SgProject::get_verbose() > 0) {
      printf("In generateModFile() (loop over module declarations): Generating "
             "a Fortran 90 specific module file %s for module = %s \n",
             outputFilename.c_str(),
             module_stmt->get_name().getString().c_str());
    }

    std::ostringstream moduleOutput;
    moduleOutput << endl
                 << "! "
                    "========================================================"
                    "=========================== \n"
                 << "! <<Automatically generated for Rose Fortran Separate "
                    "Compilation, DO NOT MODIFY IT>> \n"
                 << "! "
                    "========================================================"
                    "=========================== \n"
                 << endl;
    SgUnparse_Info ninfo;

    ninfo.set_current_scope((SgScopeStatement *)module_stmt);
    ninfo.set_current_source_file(isSgSourceFile(sfile));
    ninfo.set_language(SgFile::e_Fortran_language);

    ninfo.set_SkipFormatting();

    // set the flag bit "outputFortranModFile"
    ninfo.set_outputFortranModFile();

    Unparser_Opt options(false, false, false, false, true, false, false, false,
                         false, false);
    {
      Unparser unp(&moduleOutput, originalModuleFilenameWithPath, options,
                   nullptr, nullptr);
      unp.currentFile = sfile;

      // The generated module is owned by the exact physical input file carried
      // in ninfo. The output suffix has no role in statement selection.
      FortranCodeGeneration_locatedNode myunp(&unp, outputFilename);
      myunp.unparseClassDeclStmt_module(module_stmt, ninfo);
    }
    if (!moduleOutput.good()) {
      std::cerr << "Error: failed while rendering REX module output "
                << outputFilename << std::endl;
      ROSE_ABORT();
    }
    writeModuleFileAtomically(outputFilename, moduleOutput.str());
  }
}
