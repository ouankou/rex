#include "rose.h"

#include "patternRewrite.h"

#include <memory>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<RewriteRuleCombiner>);
static_assert(!std::is_copy_assignable_v<RewriteRuleCombiner>);
static_assert(!std::is_copy_constructible_v<PatternActionRule>);
static_assert(!std::is_copy_assignable_v<PatternActionRule>);

int main(int argc, char *argv[]) {
  SgProject *sageProject = frontend(argc, argv);
  SgNode *tempProject = sageProject;
  std::unique_ptr<RewriteRule> algebraicRules(getAlgebraicRules());
  rewrite(algebraicRules.get(), tempProject);
  sageProject = isSgProject(tempProject);
  ROSE_ASSERT(sageProject);

  sageProject->unparse();

  return 0;
}
