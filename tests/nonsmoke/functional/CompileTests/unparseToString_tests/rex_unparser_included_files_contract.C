#include "rose.h"

#include "FileHelper.h"
#include "IncludedFilesUnparser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
class TemporaryTree {
public:
  explicit TemporaryTree(std::filesystem::path path) : path_(std::move(path)) {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    error.clear();
    std::filesystem::create_directories(path_, error);
    if (error)
      path_.clear();
  }

  ~TemporaryTree() {
    if (!path_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

bool writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream output(path,
                       std::ios::out | std::ios::binary | std::ios::trunc);
  output.write(contents.data(), contents.size());
  output.close();
  return !output.fail();
}

SgIncludeFile *findIncludeFile(SgIncludeFile *root,
                               const std::string &normalizedFileName) {
  std::vector<SgIncludeFile *> worklist = {root};
  std::set<SgIncludeFile *> visited;
  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == nullptr || !visited.insert(includeFile).second)
      continue;
    if (FileHelper::normalizePathIfPossible(includeFile->get_filename()) ==
        normalizedFileName) {
      return includeFile;
    }
    for (SgIncludeFile *child : includeFile->get_include_file_list())
      worklist.push_back(child);
  }
  return nullptr;
}

class HeaderDeclarationFinder : public AstSimpleProcessing {
public:
  explicit HeaderDeclarationFinder(std::string normalizedFileName)
      : normalizedFileName_(std::move(normalizedFileName)) {}

  void visit(SgNode *node) override {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate == nullptr || candidate->get_file_info() == nullptr) {
      return;
    }
    Sg_File_Info *primary = candidate->get_file_info();
    Sg_File_Info *start = candidate->get_startOfConstruct();
    Sg_File_Info *end = candidate->get_endOfConstruct();
    const bool exact_source_surface =
        start != nullptr && end != nullptr && !primary->isCompilerGenerated() &&
        !primary->isFrontendSpecific() && primary->isOutputInCodeGeneration() &&
        !start->isCompilerGenerated() && !start->isFrontendSpecific() &&
        start->isOutputInCodeGeneration() && !end->isCompilerGenerated() &&
        !end->isFrontendSpecific() && end->isOutputInCodeGeneration() &&
        isSgAuxiliaryDeclarationList(candidate->get_parent()) == nullptr;
    if (!exact_source_surface ||
        FileHelper::normalizePathIfPossible(primary->get_physical_filename()) !=
            normalizedFileName_ ||
        candidate->get_name().getString() != "rex_exact_leaf" ||
        candidate->get_definingDeclaration() != candidate ||
        candidate->get_definition() == nullptr ||
        candidate->get_definition()->get_declaration() != candidate) {
      return;
    }
    if (declaration != nullptr && declaration != candidate) {
      fprintf(stderr,
              "included-files contract found multiple exact source "
              "definitions of rex_exact_leaf in %s\n",
              normalizedFileName_.c_str());
      ROSE_ABORT();
    }
    declaration = candidate;
  }

  SgDeclarationStatement *declaration = nullptr;

private:
  std::string normalizedFileName_;
};

struct Fixture {
  TemporaryTree temporary;
  std::filesystem::path inputDirectory;
  std::filesystem::path outputDirectory;
  std::filesystem::path sourcePath;
  std::filesystem::path parentPath;
  std::filesystem::path leafPath;
  std::filesystem::path copyPath;
  std::filesystem::path mainOutputPath;
  SgProject *project = nullptr;
  SgSourceFile *sourceFile = nullptr;
  SgIncludeFile *rootInclude = nullptr;
  SgIncludeFile *parentInclude = nullptr;
  SgIncludeFile *leafInclude = nullptr;
  SgIncludeFile *copyInclude = nullptr;
  SgSourceFile *parentSource = nullptr;
  SgSourceFile *leafSource = nullptr;
  SgSourceFile *copySource = nullptr;
  SgDeclarationStatement *leafDeclaration = nullptr;

  Fixture(const std::filesystem::path &root, bool tokenMode,
          bool duplicateOccurrence)
      : temporary(root), inputDirectory(root / "input"),
        outputDirectory(root / "headers"),
        sourcePath(inputDirectory / "rex_unparser_included_files_contract.cpp"),
        parentPath(inputDirectory / "api" / "rex_unparser_exact_parent.hpp"),
        leafPath(inputDirectory / "api" / "detail" /
                 "rex_unparser_exact_leaf.hpp"),
        copyPath(inputDirectory / "copy" / "rex_unparser_exact_copy.hpp"),
        mainOutputPath(root / "rose_rex_unparser_included_files_contract.cpp") {
    if (temporary.path().empty())
      return;

    std::error_code error;
    std::filesystem::create_directories(parentPath.parent_path(), error);
    if (error)
      return;
    std::filesystem::create_directories(leafPath.parent_path(), error);
    if (error)
      return;
    std::filesystem::create_directories(copyPath.parent_path(), error);
    if (error ||
        !writeFile(leafPath, "#pragma once\n"
                             "inline int rex_exact_leaf() { return 17; }\n") ||
        !writeFile(parentPath,
                   "#include \"detail/rex_unparser_exact_leaf.hpp\"\n"
                   "inline int rex_exact_parent() { return "
                   "rex_exact_leaf(); }\n") ||
        !writeFile(copyPath,
                   duplicateOccurrence
                       ? "#include \"../api/detail/"
                         "rex_unparser_exact_leaf.hpp\"\n"
                         "inline int rex_exact_copy() { return "
                         "rex_exact_leaf(); }\n"
                       : "inline int rex_exact_copy() { return 19; }\n") ||
        !writeFile(sourcePath,
                   "#include \"api/rex_unparser_exact_parent.hpp\"\n"
                   "#include \"copy/rex_unparser_exact_copy.hpp\"\n"
                   "int rex_exact_source() { return rex_exact_parent() + "
                   "rex_exact_copy(); }\n")) {
      return;
    }

    std::vector<std::string> arguments = {
        "rex_unparser_included_files_contract",
        "-rose:verbose",
        "0",
        "-rose:skipfinalCompileStep",
        "-rose:unparseHeaderFiles",
        "-rose:unparseHeaderFilesRootFolder",
        outputDirectory.string(),
        "-rose:applicationRootDirectory",
        inputDirectory.string(),
        "-rose:output",
        mainOutputPath.string(),
        "-I" + inputDirectory.string(),
        "-std=c++20",
        "-c",
        sourcePath.string()};
    if (tokenMode)
      arguments.insert(arguments.begin() + 4, "-rose:unparse_tokens");
    project = frontend(arguments);
    if (project == nullptr || project->get_fileList().size() != 1)
      return;

    sourceFile = isSgSourceFile(project->get_fileList().front());
    rootInclude = sourceFile != nullptr
                      ? sourceFile->get_associated_include_file()
                      : nullptr;
    parentInclude = findIncludeFile(
        rootInclude, FileHelper::normalizePathIfPossible(parentPath.string()));
    leafInclude = findIncludeFile(
        rootInclude, FileHelper::normalizePathIfPossible(leafPath.string()));
    copyInclude = findIncludeFile(
        rootInclude, FileHelper::normalizePathIfPossible(copyPath.string()));
    parentSource =
        parentInclude != nullptr ? parentInclude->get_source_file() : nullptr;
    leafSource =
        leafInclude != nullptr ? leafInclude->get_source_file() : nullptr;
    copySource =
        copyInclude != nullptr ? copyInclude->get_source_file() : nullptr;

    HeaderDeclarationFinder finder(
        FileHelper::normalizePathIfPossible(leafPath.string()));
    finder.traverse(project, preorder);
    leafDeclaration = finder.declaration;
  }

  bool complete() const {
    return project != nullptr && sourceFile != nullptr &&
           rootInclude != nullptr && parentInclude != nullptr &&
           leafInclude != nullptr && copyInclude != nullptr &&
           parentSource != nullptr && leafSource != nullptr &&
           copySource != nullptr && leafDeclaration != nullptr;
  }
};
} // namespace

int main(int argc, char **argv) {
  if (argc > 2)
    return 2;
  const std::string mode = argc == 2 ? argv[1] : "positive";
  const bool tokenMode = mode == "null-token-map" ||
                         mode == "query-missing-file-info" ||
                         mode == "duplicate-occurrence-positive";
  const std::filesystem::path root =
      std::filesystem::current_path() /
      ("rex_unparser_included_files_contract_" + std::to_string(getpid()));
  Fixture fixture(root, tokenMode, mode == "duplicate-occurrence-positive");
  if (!fixture.complete())
    return 3;

  if (mode == "positive") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.project->unparse();
    const std::filesystem::path parentOutput =
        fixture.outputDirectory / "api" / "rex_unparser_exact_parent.hpp";
    const std::filesystem::path leafOutput = fixture.outputDirectory / "api" /
                                             "detail" /
                                             "rex_unparser_exact_leaf.hpp";
    const std::filesystem::path copyOutput =
        fixture.outputDirectory / "copy" / "rex_unparser_exact_copy.hpp";
    return std::filesystem::is_regular_file(fixture.mainOutputPath) &&
                   std::filesystem::is_regular_file(parentOutput) &&
                   std::filesystem::is_regular_file(leafOutput) &&
                   std::filesystem::is_regular_file(copyOutput)
               ? 0
               : 4;
  }

  if (mode == "duplicate-occurrence-positive") {
    fixture.project->unparse();
    return 0;
  }

  if (mode == "null-token-map") {
    Rose::tokenSubsequenceMapOfMapsBySourceFile.emplace(nullptr, nullptr);
  } else if (mode == "query-missing-file-info") {
    Sg_File_Info *headerScopePosition =
        new Sg_File_Info(fixture.leafPath.string(), 1, 1);
    SgGlobal *headerScope = new SgGlobal(headerScopePosition);
    fixture.leafSource->set_globalScope(headerScope);
    headerScope->set_parent(fixture.leafSource);
    SgEmptyDeclaration *missingPosition = SageBuilder::buildEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_source_semicolon);
    Sg_File_Info *missingStart =
        new Sg_File_Info(fixture.leafPath.string(), 1, 1);
    Sg_File_Info *missingEnd =
        new Sg_File_Info(fixture.leafPath.string(), 1, 1);
    missingPosition->set_startOfConstruct(missingStart);
    missingPosition->set_endOfConstruct(missingEnd);
    missingStart->set_parent(missingPosition);
    missingEnd->set_parent(missingPosition);
    SageInterface::appendStatement(missingPosition, headerScope);
    // Corrupt the already-published source node at the exact contract under
    // test.  Constructing it without a source range would correctly fail much
    // earlier in appendStatement and never exercise the header-query check.
    missingPosition->set_startOfConstruct(nullptr);
    Rose::tokenSubsequenceMapOfMapsBySourceFile.emplace(fixture.leafSource,
                                                        nullptr);
  } else if (mode == "missing-associated-include") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.leafSource->set_associated_include_file(nullptr);
  } else if (mode == "foreign-associated-include") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.leafSource->set_associated_include_file(fixture.copyInclude);
  } else if (mode == "missing-parent-include") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.leafInclude->set_parent_include_file(nullptr);
  } else if (mode == "missing-parent-edge") {
    fixture.leafDeclaration->set_isModified(true);
    SgIncludeFilePtrList &children =
        fixture.parentInclude->get_include_file_list();
    children.erase(
        std::remove(children.begin(), children.end(), fixture.leafInclude),
        children.end());
  } else if (mode == "missing-source-spelling") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.leafInclude->set_name_used_in_include_directive("");
  } else if (mode == "absolute-source-spelling") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.leafInclude->set_name_used_in_include_directive(
        fixture.leafPath.string());
  } else if (mode == "unresolved-parent-output") {
    fixture.leafDeclaration->set_isModified(true);
    fixture.leafInclude->get_include_file_list().push_back(
        fixture.parentInclude);
    fixture.parentInclude->set_parent_include_file(fixture.leafInclude);
  } else if (mode == "copied-missing-associated-include" ||
             mode == "copied-unresolved-parent-output") {
    fixture.leafDeclaration->set_isModified(true);
    IncludedFilesUnparser planner(fixture.project);
    planner.figureOutWhichFilesToUnparse();
    const std::string normalizedCopy =
        FileHelper::normalizePathIfPossible(fixture.copyPath.string());
    if (planner.getFilesToCopy().count(normalizedCopy) != 1)
      return 7;

    if (mode == "copied-missing-associated-include") {
      fixture.copySource->set_associated_include_file(nullptr);
      planner.getCopiedFileOutputPath(normalizedCopy);
      return 0;
    }

    SgIncludeFile *orphanParent = new SgIncludeFile(
        (fixture.inputDirectory / "copy_orphan.hpp").string());
    orphanParent->set_filename(
        (fixture.inputDirectory / "copy_orphan.hpp").string());
    orphanParent->get_include_file_list().push_back(fixture.copyInclude);
    fixture.copyInclude->set_parent_include_file(orphanParent);
    planner.getCopiedFileOutputPath(normalizedCopy);
    return 0;
  } else {
    return 6;
  }

  fixture.project->unparse();
  return 0;
}
