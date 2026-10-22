#include "rose.h"

#include <set>
#include <string>

namespace {
void requireValues(SgProject *project) {
  std::set<std::string> observed;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInteger128Val)) {
    auto *literal = isSgInteger128Val(node);
    const auto value = literal->get_value();
    SgType *type = literal->get_type()->stripType(SgType::STRIP_MODIFIER_TYPE |
                                                  SgType::STRIP_TYPEDEF_TYPE);
    ROSE_ASSERT(value.isUnsigned()
                    ? isSgTypeUnsigned128bitInteger(type) != nullptr
                    : isSgTypeSigned128bitInteger(type) != nullptr);
    observed.insert(value.decimal());
    if (value.decimal() == "1267650600228229401496703205383") {
      ROSE_ASSERT(value.words()[0] == 7);
      ROSE_ASSERT(value.words()[1] == (UINT64_C(1) << 36));
    }
  }
  ROSE_ASSERT(observed.count("1267650600228229401496703205383") != 0);
  ROSE_ASSERT(observed.count("-1267650600228229401496703205383") != 0);
  ROSE_ASSERT(observed.count("340282366920938463463374607431768211455") != 0);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  requireValues(project);
  auto *copy = isSgProject(SageInterface::deepCopy(project));
  ROSE_ASSERT(copy != nullptr && copy != project);
  requireValues(copy);
  SageInterface::deleteAST(copy);
  requireValues(project);
  project->unparse();
  SageInterface::deleteAST(project);
  return 0;
}
