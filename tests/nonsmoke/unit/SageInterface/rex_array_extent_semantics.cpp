#include "rose.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>

namespace {
void requireExtents(SgProject *project) {
  const std::map<std::string, uint64_t> expected{
      {"rex_small", 4},
      {"rex_wide", UINT64_C(4294967300)},
      {"rex_signed_boundary", UINT64_C(2147483648)}};
  std::map<std::string, SgArrayType *> found;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    auto *declaration = isSgTypedefDeclaration(node);
    const std::string name = declaration->get_name().getString();
    auto entry = expected.find(name);
    if (entry == expected.end())
      continue;
    auto *array = isSgArrayType(declaration->get_base_type());
    ROSE_ASSERT(array != nullptr && array->get_index() != nullptr);
    uint64_t extent = 0;
    if (auto *value = isSgIntVal(array->get_index())) {
      ROSE_ASSERT(value->get_value() >= 0);
      extent = static_cast<uint64_t>(value->get_value());
    } else {
      auto *wide_value = isSgUnsignedLongLongIntVal(array->get_index());
      ROSE_ASSERT(wide_value != nullptr);
      extent = wide_value->get_value();
    }
    ROSE_ASSERT(extent == entry->second);
    if (extent <= std::numeric_limits<size_t>::max()) {
      ROSE_ASSERT(SageInterface::getArrayElementCount(array) == extent);
    }
    ROSE_ASSERT(found.emplace(name, array).second);
  }
  ROSE_ASSERT(found.size() == expected.size());
  ROSE_ASSERT(found.at("rex_small") != found.at("rex_wide"));
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  requireExtents(project);
  auto *copy = isSgProject(SageInterface::deepCopy(project));
  ROSE_ASSERT(copy != nullptr && copy != project);
  requireExtents(copy);
  SageInterface::deleteAST(copy);
  requireExtents(project);
  project->unparse();
  SageInterface::deleteAST(project);
  return 0;
}
