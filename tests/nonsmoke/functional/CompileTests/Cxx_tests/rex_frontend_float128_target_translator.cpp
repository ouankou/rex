#include "rose.h"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 3)
    return 2;
  const bool float128 = std::strcmp(argv[1], "float128") == 0;
  if (!float128 && std::strcmp(argv[1], "long_double") != 0)
    return 2;
  std::vector<std::string> arguments{argv[0]};
  for (int index = 2; index != argc; ++index)
    arguments.emplace_back(argv[index]);
  std::unique_ptr<SgProject> project(frontend(arguments));
  ROSE_ASSERT(project != nullptr && frontendExitStatus(project.get()) == 0);
  ROSE_ASSERT(project->get_fileList().size() == 1);
  auto *source = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
  auto *symbol = source->get_globalScope()->lookup_variable_symbol("value");
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
  SgType *type = symbol->get_declaration()->get_type();
  ROSE_ASSERT(float128 ? isSgTypeFloat128(type) != nullptr
                       : isSgTypeLongDouble(type) != nullptr);
  AstTests::runAllTests(project.get());
  return backend(project.get());
}
