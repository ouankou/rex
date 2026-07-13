#include "nodeQuery.h"
#include "rose.h"

#include <array>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  const Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgSourceLocationBuiltinExp);
  using Kind = SgSourceLocationBuiltinExp::source_location_builtin_kind_enum;
  const std::array<Kind, 3> expected{SgSourceLocationBuiltinExp::e_file,
                                     SgSourceLocationBuiltinExp::e_function,
                                     SgSourceLocationBuiltinExp::e_line};
  ROSE_ASSERT(nodes.size() == expected.size());
  for (size_t index = 0; index < expected.size(); ++index) {
    SgSourceLocationBuiltinExp *builtin =
        isSgSourceLocationBuiltinExp(nodes[index]);
    ROSE_ASSERT(builtin != nullptr && builtin->get_kind() == expected[index]);
    ROSE_ASSERT(builtin->get_expression_type() != nullptr &&
                builtin->get_type() == builtin->get_expression_type());
  }

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
