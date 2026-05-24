
#include "rose.h"
#include "rose_test_output_path.h"

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);

  // Run internal consistency tests on AST
  AstTests::runAllTests(project);
  std::string filename = Rose::TestOutput::resolvePath(
      SageInterface::generateProjectName(project) + ".AST.txt");
  SageInterface::printAST2TextFile(project, filename.c_str());

  return backend(project);
}
