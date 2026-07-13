#include "nodeQuery.h"
#include "rose.h"

#include <array>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  const Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgSourceLocationBuiltinExp);
  using Kind = SgSourceLocationBuiltinExp::source_location_builtin_kind_enum;
  const std::array<Kind, 5> expected{SgSourceLocationBuiltinExp::e_file,
                                     SgSourceLocationBuiltinExp::e_file_name,
                                     SgSourceLocationBuiltinExp::e_function,
                                     SgSourceLocationBuiltinExp::e_line,
                                     SgSourceLocationBuiltinExp::e_column};
  ROSE_ASSERT(nodes.size() == expected.size());

  for (size_t index = 0; index < expected.size(); ++index) {
    SgSourceLocationBuiltinExp *builtin =
        isSgSourceLocationBuiltinExp(nodes[index]);
    ROSE_ASSERT(builtin != nullptr && builtin->get_kind() == expected[index]);
    ROSE_ASSERT(builtin->get_type() != nullptr &&
                builtin->get_type() == builtin->get_expression_type());
    if (index < 3) {
      ROSE_ASSERT(isSgPointerType(builtin->get_type()) != nullptr);
    } else {
      ROSE_ASSERT(isSgTypeUnsignedInt(builtin->get_type()) != nullptr);
    }
  }

  return backend(project);
}
