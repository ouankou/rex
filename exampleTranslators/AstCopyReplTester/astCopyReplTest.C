#include "Rose/FileSystem.h"
#include "rose.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct DeleteProject {
  void operator()(SgProject *project) const {
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
};
using Project = std::unique_ptr<SgProject, DeleteProject>;

std::vector<std::string> emit(SgProject *project,
                              const std::filesystem::path &directory,
                              const std::string &stage) {
  AstTests::runAllTests(project);
  std::vector<std::string> output;
  for (size_t index = 0; index < project->get_fileList().size(); ++index) {
    auto *file = isSgSourceFile(project->get_fileList()[index]);
    ROSE_ASSERT(file != nullptr);
    const auto path =
        directory /
        (stage + "-" + std::to_string(index) +
         std::filesystem::path(file->getFileName()).extension().string());
    file->set_unparse_output_filename(path.string());
    file->unparse();
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
      throw std::runtime_error("Cannot read unparsed output: " + path.string());
    output.emplace_back(std::istreambuf_iterator<char>(stream),
                        std::istreambuf_iterator<char>());
    if (stream.bad())
      throw std::runtime_error("Failed reading unparsed output: " +
                               path.string());
  }
  return output;
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    SgFile::usage();
    return 1;
  }
  try {
    Project project(frontend(argc, argv));
    ROSE_ASSERT(project != nullptr && project->numberOfFiles() > 0);
    ROSE_ASSERT(project->get_frontendErrorCode() == 0);
    const auto directory = Rose::FileSystem::createTemporaryDirectory();
    std::cerr << "AST copy/replacement outputs: " << directory << '\n';
    const auto expected = emit(project.get(), directory, "original");

    // File metadata, tokens, declaration families and cross-file semantic edges
    // belong to the complete project graph. Copy and replace that owner instead
    // of interleaving declarations between unrelated global symbol tables.
    Project copy(SageInterface::deepCopySemantic(project.get()));
    ROSE_ASSERT(copy != nullptr && copy.get() != project.get());
    ROSE_ASSERT(emit(copy.get(), directory, "copy") == expected);
    SgProject *original = project.get();
    project = std::move(copy);
    ROSE_ASSERT(!SgNode::isLiveNode(original));
    ROSE_ASSERT(emit(project.get(), directory, "replacement") == expected);
    SgProject *replacement = project.get();
    project.reset();
    ROSE_ASSERT(!SgNode::isLiveNode(replacement));
    std::filesystem::remove_all(directory);
    std::cerr << "successful test\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "AST copy/replacement failed: " << error.what() << '\n';
    return 1;
  }
}
