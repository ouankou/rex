
#include "rose.h"

#include <cstdlib>
#include <filesystem>

namespace {
std::string resolveTestOutputPath(const std::string &filename) {
  const char *output_dir = std::getenv("ROSE_TEST_OUTPUT_DIR");
  if (output_dir == nullptr || output_dir[0] == '\0') {
    return filename;
  }

  std::filesystem::path output_dir_path(output_dir);
  std::error_code ec;
  std::filesystem::create_directories(output_dir_path, ec);
  if (ec) {
    return filename;
  }

  return (output_dir_path / filename).string();
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);

  // Run internal consistency tests on AST
  AstTests::runAllTests(project);
  std::string filename = resolveTestOutputPath(
      SageInterface::generateProjectName(project) + ".AST.txt");
  SageInterface::printAST2TextFile(project, filename.c_str());

  return backend(project);
}
