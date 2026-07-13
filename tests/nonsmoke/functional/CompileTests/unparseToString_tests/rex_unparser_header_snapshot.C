#include "rose.h"

#include "FileHelper.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
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

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::in | std::ios::binary);
  std::ostringstream contents;
  contents << input.rdbuf();
  return input.bad() ? std::string() : contents.str();
}

class HeaderInitializedNameFinder : public AstSimpleProcessing {
public:
  explicit HeaderInitializedNameFinder(std::string header)
      : header_(std::move(header)) {}

  void visit(SgNode *node) override {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (result != nullptr || candidate == nullptr ||
        candidate->get_file_info() == nullptr ||
        candidate->get_name() != "rex_snapshot_header_value") {
      return;
    }
    if (FileHelper::normalizePathIfPossible(
            candidate->get_file_info()->get_physical_filename()) == header_) {
      result = candidate;
    }
  }

  SgInitializedName *result = nullptr;

private:
  std::string header_;
};
} // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::current_path() /
      ("rex_unparser_header_snapshot_" + std::to_string(getpid()));
  TemporaryTree temporary(root);
  if (temporary.path().empty()) {
    return 1;
  }

  const std::filesystem::path inputDirectory = root / "input";
  const std::filesystem::path outputDirectory = root / "headers";
  std::error_code error;
  std::filesystem::create_directories(inputDirectory, error);
  if (error) {
    return 2;
  }

  const std::filesystem::path header =
      inputDirectory / "rex_unparser_header_snapshot.h";
  const std::filesystem::path untouchedHeader =
      inputDirectory / "rex_unparser_header_snapshot_untouched.h";
  const std::filesystem::path source =
      inputDirectory / "rex_unparser_header_snapshot.c";
  const std::filesystem::path mainOutput =
      root / "rose_rex_unparser_header_snapshot.c";
  const std::string untouchedHeaderContents =
      "#define REX_UNTOUCHED_SNAPSHOT_VALUE 23";
  if (!writeFile(header, "#define REX_SNAPSHOT_VALUE 17\n"
                         "static int rex_snapshot_header_value = 3;\n") ||
      !writeFile(untouchedHeader, untouchedHeaderContents) ||
      !writeFile(source, "#include \"rex_unparser_header_snapshot.h\"\n"
                         "#include "
                         "\"rex_unparser_header_snapshot_untouched.h\"\n"
                         "int rex_snapshot_value(void) { return "
                         "REX_SNAPSHOT_VALUE + "
                         "REX_UNTOUCHED_SNAPSHOT_VALUE; }\n")) {
    return 3;
  }

  const std::vector<std::string> arguments = {
      "rex_unparser_header_snapshot",
      "-rose:verbose",
      "0",
      "-rose:skipfinalCompileStep",
      "-rose:unparseHeaderFiles",
      "-rose:unparseHeaderFilesRootFolder",
      outputDirectory.string(),
      "-rose:applicationRootDirectory",
      inputDirectory.string(),
      "-rose:output",
      mainOutput.string(),
      "-I" + inputDirectory.string(),
      "-c",
      source.string()};

  SgProject *project = frontend(arguments);
  if (project == nullptr || project->get_fileList().empty()) {
    return 4;
  }

  const std::string normalizedHeader =
      FileHelper::normalizePathIfPossible(header.string());
  const std::string normalizedUntouchedHeader =
      FileHelper::normalizePathIfPossible(untouchedHeader.string());
  const std::map<std::string, std::string> &snapshots =
      project->get_original_header_snapshots();
  const auto headerSnapshot = snapshots.find(normalizedHeader);
  const auto untouchedHeaderSnapshot =
      snapshots.find(normalizedUntouchedHeader);
  if (headerSnapshot == snapshots.end() ||
      headerSnapshot->second != readFile(header) ||
      untouchedHeaderSnapshot == snapshots.end() ||
      untouchedHeaderSnapshot->second != untouchedHeaderContents) {
    return 5;
  }

  const SgStringList projectArgumentsBefore =
      project->get_originalCommandLineArgumentList();
  std::vector<SgStringList> fileArgumentsBefore;
  for (SgFile *file : project->get_fileList()) {
    if (file == nullptr) {
      return 6;
    }
    fileArgumentsBefore.push_back(file->get_originalCommandLineArgumentList());
  }

  HeaderInitializedNameFinder headerNameFinder(normalizedHeader);
  headerNameFinder.traverse(project, preorder);
  if (headerNameFinder.result == nullptr) {
    return 7;
  }
  headerNameFinder.result->set_name("rex_snapshot_header_value_changed");
  headerNameFinder.result->set_isModified(true);

  // Any backend copy from the live filesystem would now emit this failure.
  // The frontend-owned snapshot and parsed AST must remain authoritative.
  if (!writeFile(header, "#error REX_LATE_HEADER_READ\n") ||
      !writeFile(untouchedHeader, "#error REX_LATE_UNTOUCHED_HEADER_READ\n")) {
    return 8;
  }

  project->unparse();

  if (project->get_originalCommandLineArgumentList() !=
      projectArgumentsBefore) {
    return 9;
  }
  for (size_t index = 0; index < project->get_fileList().size(); ++index) {
    if (project->get_fileList()[index]->get_originalCommandLineArgumentList() !=
        fileArgumentsBefore[index]) {
      return 10;
    }
  }

  bool foundHeader = false;
  bool foundUntouchedHeader = false;
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(outputDirectory)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().filename() == "rex_unparser_header_snapshot.h") {
      foundHeader = true;
      const std::string output = readFile(entry.path());
      if (output.find("REX_SNAPSHOT_VALUE") == std::string::npos ||
          output.find("rex_snapshot_header_value_changed") ==
              std::string::npos ||
          output.find("REX_LATE_HEADER_READ") != std::string::npos) {
        std::cerr << "unexpected header output in " << entry.path() << ":\n"
                  << output;
        return 11;
      }
    } else if (entry.path().filename() ==
               "rex_unparser_header_snapshot_untouched.h") {
      foundUntouchedHeader = true;
      const std::string output = readFile(entry.path());
      if (output != untouchedHeaderContents) {
        std::cerr << "untouched header was not copied from the exact frontend "
                     "snapshot in "
                  << entry.path() << ":\n"
                  << output;
        return 12;
      }
    }
  }

  return foundHeader && foundUntouchedHeader &&
                 std::filesystem::is_regular_file(mainOutput)
             ? 0
             : 13;
}
