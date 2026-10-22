#include "AstConsistencyTests.h"
#include "sage3basic.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {
enum class Defect { none, count, names, null_child, duplicate_child };

// Deliberately inconsistent public traversal overrides exercise the diagnostic
// independently of the generated accessors' own hard checks.
class TraversalFixture final : public SgBasicBlock {
public:
  TraversalFixture(SgNode *child, Defect defect)
      : child_(child), defect_(defect) {}

  SgNodePtrList get_traversalSuccessorContainer() const override {
    if (defect_ == Defect::null_child)
      return {nullptr};
    if (defect_ == Defect::duplicate_child)
      return {child_, child_};
    return {child_};
  }

  std::vector<std::string>
  get_traversalSuccessorNamesContainer() const override {
    if (defect_ == Defect::names)
      return {};
    if (defect_ == Defect::duplicate_child)
      return {"p_first", "p_second"};
    return {"p_first"};
  }

  size_t get_numberOfTraversalSuccessors() const override {
    return defect_ == Defect::count || defect_ == Defect::duplicate_child ? 2
                                                                          : 1;
  }

private:
  SgNode *child_;
  Defect defect_;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  Defect defect;
  if (std::strcmp(argv[1], "valid") == 0)
    defect = Defect::none;
  else if (std::strcmp(argv[1], "count") == 0)
    defect = Defect::count;
  else if (std::strcmp(argv[1], "names") == 0)
    defect = Defect::names;
  else if (std::strcmp(argv[1], "null") == 0)
    defect = Defect::null_child;
  else if (std::strcmp(argv[1], "duplicate") == 0)
    defect = Defect::duplicate_child;
  else
    return 2;

  auto child = std::make_unique<SgNullStatement>();
  auto fixture = std::make_unique<TraversalFixture>(child.get(), defect);
  AstTests::validateTraversalSuccessors(fixture.get());
  return 0;
}
