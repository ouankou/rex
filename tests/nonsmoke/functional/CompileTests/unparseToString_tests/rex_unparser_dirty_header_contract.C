#include "rose.h"

#include "FileHelper.h"

#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
class TemporaryTree {
public:
  explicit TemporaryTree(std::filesystem::path path) : path_(std::move(path)) {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    error.clear();
    std::filesystem::create_directories(path_, error);
    if (error) {
      path_.clear();
    }
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
                               const std::string &normalizedHeader) {
  std::vector<SgIncludeFile *> worklist;
  std::set<SgIncludeFile *> visited;
  worklist.push_back(root);
  while (!worklist.empty()) {
    SgIncludeFile *includeFile = worklist.back();
    worklist.pop_back();
    if (includeFile == nullptr || !visited.insert(includeFile).second) {
      continue;
    }
    if (FileHelper::normalizePathIfPossible(includeFile->get_filename()) ==
        normalizedHeader) {
      return includeFile;
    }
    for (SgIncludeFile *child : includeFile->get_include_file_list()) {
      worklist.push_back(child);
    }
  }
  return nullptr;
}

class HeaderDeclarationFinder : public AstSimpleProcessing {
public:
  explicit HeaderDeclarationFinder(std::string normalizedHeader)
      : normalizedHeader_(std::move(normalizedHeader)) {}

  void visit(SgNode *node) override {
    if (declaration != nullptr) {
      return;
    }
    SgDeclarationStatement *candidate = isSgDeclarationStatement(node);
    if (candidate == nullptr || candidate->get_file_info() == nullptr) {
      return;
    }
    const std::string candidateFile = FileHelper::normalizePathIfPossible(
        candidate->get_file_info()->get_physical_filename());
    if (candidateFile == normalizedHeader_) {
      declaration = candidate;
    }
  }

  SgDeclarationStatement *declaration = nullptr;

private:
  std::string normalizedHeader_;
};
} // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::current_path() /
      ("rex_unparser_dirty_header_contract_" + std::to_string(getpid()));
  TemporaryTree temporary(root);
  if (temporary.path().empty()) {
    return 1;
  }

  const std::filesystem::path inputDirectory = root / "input";
  const std::filesystem::path headerOutputDirectory = root / "headers";
  const std::filesystem::path header =
      inputDirectory / "rex_unparser_dirty_header.h";
  const std::filesystem::path source =
      inputDirectory / "rex_unparser_dirty_header.c";
  const std::filesystem::path mainOutput =
      root / "rose_rex_unparser_dirty_header.c";
  std::error_code error;
  std::filesystem::create_directories(inputDirectory, error);
  if (error ||
      !writeFile(header, "struct rex_dirty_header_type { int value; };\n") ||
      !writeFile(source,
                 "#include \"rex_unparser_dirty_header.h\"\n"
                 "int rex_dirty_header_value(struct rex_dirty_header_type "
                 "*value) { return value->value; }\n") ||
      !writeFile(mainOutput, "REX_DIRTY_HEADER_SENTINEL\n")) {
    return 2;
  }

  const std::vector<std::string> arguments = {
      "rex_unparser_dirty_header_contract",
      "-rose:verbose",
      "0",
      "-rose:skipfinalCompileStep",
      "-rose:unparseHeaderFiles",
      "-rose:unparseHeaderFilesRootFolder",
      headerOutputDirectory.string(),
      "-rose:applicationRootDirectory",
      inputDirectory.string(),
      "-rose:output",
      mainOutput.string(),
      "-I" + inputDirectory.string(),
      "-c",
      source.string()};
  SgProject *project = frontend(arguments);
  if (project == nullptr || project->get_fileList().empty()) {
    return 3;
  }

  SgSourceFile *mainFile = isSgSourceFile(project->get_fileList().front());
  const std::string normalizedHeader =
      FileHelper::normalizePathIfPossible(header.string());
  SgIncludeFile *includeFile =
      mainFile != nullptr
          ? findIncludeFile(mainFile->get_associated_include_file(),
                            normalizedHeader)
          : nullptr;
  SgSourceFile *headerFile =
      includeFile != nullptr ? includeFile->get_source_file() : nullptr;
  HeaderDeclarationFinder finder(normalizedHeader);
  finder.traverse(project, preorder);
  if (includeFile == nullptr || headerFile == nullptr ||
      finder.declaration == nullptr) {
    return 4;
  }

  // Snapshot copying still requires the exact frontend include graph. A
  // missing materialized source wrapper used to fall through to an
  // application-root/basename output guess.
  const std::filesystem::path missingOwnerDiagnosticPath =
      root / "missing-copy-owner.log";
  const std::string missingOwnerDiagnosticFile =
      missingOwnerDiagnosticPath.string();
  pid_t child = fork();
  if (child < 0) {
    return 5;
  }
  if (child == 0) {
    if (std::freopen(missingOwnerDiagnosticFile.c_str(), "w", stderr) ==
        nullptr) {
      _exit(90);
    }
    includeFile->set_source_file(nullptr);
    Rose::tokenSubsequenceMapOfMapsBySourceFile.erase(headerFile);
    project->unparse();
    _exit(0);
  }

  int status = 0;
  if (waitpid(child, &status, 0) != child) {
    return 6;
  }
  if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
    return 7;
  }

  std::ifstream missingOwnerDiagnostic(missingOwnerDiagnosticPath);
  std::string missingOwnerDiagnosticLine;
  std::getline(missingOwnerDiagnostic, missingOwnerDiagnosticLine);
  const std::string expectedMissingOwnerDiagnostic =
      "REX_UNPARSE_INVARIANT[header-include-graph]: operation=copy-output "
      "header=" +
      normalizedHeader + " has no exact materialized source-file owner";
  if (missingOwnerDiagnosticLine != expectedMissingOwnerDiagnostic) {
    std::cerr << "unexpected missing-owner diagnostic\nexpected: "
              << expectedMissingOwnerDiagnostic
              << "\nactual:   " << missingOwnerDiagnosticLine << '\n';
    return 8;
  }

  std::filesystem::remove_all(headerOutputDirectory, error);
  if (error || !writeFile(mainOutput, "REX_DIRTY_HEADER_SENTINEL\n")) {
    return 9;
  }

  const std::filesystem::path diagnosticPath = root / "dirty-header.log";
  const std::string diagnosticFile = diagnosticPath.string();
  child = fork();
  if (child < 0) {
    return 10;
  }
  if (child == 0) {
    if (std::freopen(diagnosticFile.c_str(), "w", stderr) == nullptr) {
      _exit(90);
    }
    finder.declaration->set_isModified(true);
    includeFile->set_source_file(nullptr);
    Rose::tokenSubsequenceMapOfMapsBySourceFile.erase(headerFile);
    project->unparse();
    _exit(0);
  }

  status = 0;
  if (waitpid(child, &status, 0) != child) {
    return 11;
  }
  if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
    return 12;
  }

  std::ifstream diagnostic(diagnosticPath);
  std::string diagnosticLine;
  std::getline(diagnostic, diagnosticLine);
  const std::string expectedDiagnostic =
      "REX_UNPARSE_INVARIANT[dirty-header-without-ast]: " + normalizedHeader +
      " was modified but the frontend did not materialize its SgSourceFile";
  if (diagnosticLine != expectedDiagnostic) {
    std::cerr << "unexpected dirty-header diagnostic\nexpected: "
              << expectedDiagnostic << "\nactual:   " << diagnosticLine << '\n';
    return 13;
  }

  std::ifstream output(mainOutput);
  std::string line;
  std::getline(output, line);
  return line == "REX_DIRTY_HEADER_SENTINEL" ? 0 : 14;
}
