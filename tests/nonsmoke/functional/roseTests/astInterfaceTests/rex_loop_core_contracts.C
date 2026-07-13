#include "RoseAst.h"
#include "rose.h"

#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using CanonicalPlan = SageInterface::CheckedCanonicalLoopPlan;
using CanonicalStride = SageInterface::CheckedCanonicalLoopStride;
using UnrollPlan = SageInterface::CheckedLoopUnrollPlan;
using TilingPlan = SageInterface::CheckedLoopTilingPlan;

static_assert(!std::is_default_constructible_v<CanonicalStride>);
static_assert(!std::is_default_constructible_v<CanonicalPlan>);
static_assert(!std::is_default_constructible_v<UnrollPlan>);
static_assert(!std::is_default_constructible_v<TilingPlan>);
static_assert(!std::is_aggregate_v<CanonicalStride>);
static_assert(!std::is_aggregate_v<CanonicalPlan>);
static_assert(!std::is_aggregate_v<UnrollPlan>);
static_assert(!std::is_aggregate_v<TilingPlan>);
static_assert(!std::is_copy_assignable_v<CanonicalStride>);
static_assert(!std::is_copy_assignable_v<CanonicalPlan>);
static_assert(!std::is_copy_assignable_v<UnrollPlan>);
static_assert(!std::is_copy_assignable_v<TilingPlan>);
static_assert(!std::is_move_assignable_v<CanonicalStride>);
static_assert(!std::is_move_assignable_v<CanonicalPlan>);
static_assert(!std::is_move_assignable_v<UnrollPlan>);
static_assert(!std::is_move_assignable_v<TilingPlan>);
static_assert(!std::is_move_constructible_v<CanonicalStride>);
static_assert(!std::is_move_constructible_v<CanonicalPlan>);
static_assert(!std::is_move_constructible_v<UnrollPlan>);
static_assert(!std::is_move_constructible_v<TilingPlan>);
static_assert(
    std::is_same_v<decltype(std::declval<const CanonicalPlan &>().loop()),
                   SgForStatement &>);
static_assert(
    std::is_same_v<decltype(std::declval<const CanonicalPlan &>()
                                .stride()
                                .explicitExpression()),
                   std::optional<std::reference_wrapper<SgExpression>>>);
static_assert(
    std::is_same_v<decltype(SageInterface::requireCheckedCanonicalLoopPlan(
                       nullptr, "compile-contract")),
                   CanonicalPlan>);
static_assert(
    std::is_same_v<decltype(SageInterface::requireCheckedLoopUnrollPlan(
                       nullptr, 1, "compile-contract")),
                   UnrollPlan>);
static_assert(
    std::is_same_v<decltype(SageInterface::requireCheckedLoopTilingPlan(
                       nullptr, std::declval<const std::vector<size_t> &>(),
                       "compile-contract")),
                   TilingPlan>);

SgFunctionDefinition *findDefinition(SgProject *project,
                                     const std::string &name) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == name &&
        declaration->get_definition() != nullptr)
      return declaration->get_definition();
  }
  fprintf(stderr,
          "REX_TEST_INVARIANT[loop-core-function]: requested function=%s was "
          "not found\n",
          name.c_str());
  ROSE_ABORT();
}

std::vector<SgForStatement *> loopsIn(SgProject *project,
                                      const std::string &name) {
  return SageInterface::querySubTree<SgForStatement>(
      findDefinition(project, name), V_SgForStatement);
}

SgForStatement *singleLoop(SgProject *project, const std::string &name) {
  std::vector<SgForStatement *> loops = loopsIn(project, name);
  ROSE_ASSERT(loops.size() == 1);
  return loops.front();
}

SgForStatement *outerLoop(SgProject *project, const std::string &name,
                          size_t depth) {
  std::vector<SgForStatement *> loops = loopsIn(project, name);
  ROSE_ASSERT(loops.size() == depth);
  return loops.front();
}

SgExpression *semanticExpression(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(expression))
    return macro->get_expanded_expression_checked();
  return expression;
}

std::vector<SgNode *> snapshotNodes(SgProject *project) {
  std::vector<SgNode *> result;
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node)
    result.push_back(*node);
  return result;
}

void requireUnrollIdentity(SgProject *project, SgForStatement *loop) {
  const std::vector<SgNode *> nodes = snapshotNodes(project);
  SgNode *parent = loop->get_parent();
  SgForInitStatement *initialization = loop->get_for_init_stmt();
  SgStatement *test = loop->get_test();
  SgExpression *increment = loop->get_increment();
  SgStatement *body = loop->get_loop_body();
  SageInterface::loopUnrolling(loop, 1);
  ROSE_ASSERT(snapshotNodes(project) == nodes);
  ROSE_ASSERT(loop->get_parent() == parent);
  ROSE_ASSERT(loop->get_for_init_stmt() == initialization);
  ROSE_ASSERT(loop->get_test() == test);
  ROSE_ASSERT(loop->get_increment() == increment);
  ROSE_ASSERT(loop->get_loop_body() == body);
}

void requireGuardedUnrollShape(SgProject *project, const std::string &name,
                               size_t factor) {
  SgFunctionDefinition *definition = findDefinition(project, name);
  std::vector<SgForStatement *> loops =
      SageInterface::querySubTree<SgForStatement>(definition, V_SgForStatement);
  ROSE_ASSERT(loops.size() == 1);
  ROSE_ASSERT(isSgBasicBlock(loops.front()->get_loop_body()) != nullptr);
  std::vector<SgIfStmt *> guards =
      SageInterface::querySubTree<SgIfStmt>(definition, V_SgIfStmt);
  std::vector<SgBreakStmt *> breaks =
      SageInterface::querySubTree<SgBreakStmt>(definition, V_SgBreakStmt);
  ROSE_ASSERT(guards.size() == factor - 1);
  ROSE_ASSERT(breaks.size() == factor - 1);
  ROSE_ASSERT(
      SageInterface::querySubTree<SgMultiplyOp>(definition, V_SgMultiplyOp)
          .empty());
  for (SgNode *node :
       NodeQuery::querySubTree(definition, V_SgInitializedName)) {
    SgInitializedName *initialized = isSgInitializedName(node);
    ROSE_ASSERT(initialized == nullptr ||
                initialized->get_name().getString().find("_lu_fringe_") != 0);
  }
}

void requireCountBoundedTilingShape(SgProject *project,
                                    const std::string &name) {
  SgFunctionDefinition *definition = findDefinition(project, name);
  bool found_index = false;
  bool found_count = false;
  for (SgNode *node :
       NodeQuery::querySubTree(definition, V_SgInitializedName)) {
    SgInitializedName *initialized = isSgInitializedName(node);
    if (initialized == nullptr)
      continue;
    if (initialized->get_generated_variable_role() ==
        SgInitializedName::e_generated_loop_tiling_index)
      found_index = true;
    if (initialized->get_generated_variable_role() ==
        SgInitializedName::e_generated_loop_tiling_increment)
      found_count = true;
  }
  ROSE_ASSERT(found_index && found_count);
  bool found_absent_control_increment = false;
  for (SgForStatement *loop : SageInterface::querySubTree<SgForStatement>(
           definition, V_SgForStatement)) {
    if (SgNullExpression *increment =
            isSgNullExpression(loop->get_increment())) {
      ROSE_ASSERT(increment->get_role() ==
                  SgNullExpression::e_null_expression_syntactic_absence);
      found_absent_control_increment = true;
    }
  }
  ROSE_ASSERT(found_absent_control_increment);
  ROSE_ASSERT(SageInterface::querySubTree<SgConditionalExp>(definition,
                                                            V_SgConditionalExp)
                  .empty());
}

void requireAssignmentNormalization(SgProject *project) {
  SgForStatement *loop = singleLoop(project, "assignment_increment");
  SageInterface::forLoopNormalization(loop, false);
  ROSE_ASSERT(isSgLessThanOp(semanticExpression(loop->get_test_expr())) !=
              nullptr);
  ROSE_ASSERT(isSgPlusAssignOp(semanticExpression(loop->get_increment())) !=
              nullptr);
  SgVariableDeclaration *declaration =
      isSgVariableDeclaration(SageInterface::getPreviousStatement(loop));
  ROSE_ASSERT(declaration != nullptr);
  ROSE_ASSERT(declaration->get_parent() == loop->get_parent());
  ROSE_ASSERT(declaration->get_scope() == loop->get_scope());
}

void requireTripCount(SgProject *project, const std::string &name,
                      std::optional<unsigned long long> expected) {
  SgForStatement *loop = singleLoop(project, name);
  SageInterface::CheckedCanonicalLoopPlan plan =
      SageInterface::requireCheckedCanonicalLoopPlan(loop, "trip-count-test");
  ROSE_ASSERT(SageInterface::exactCanonicalLoopTripCount(plan) == expected);
}

void requireCheckedPlanFactories(SgProject *project) {
  SgForStatement *implicit_loop = singleLoop(project, "trip_unsigned_safe");
  const CanonicalPlan implicit = SageInterface::requireCheckedCanonicalLoopPlan(
      implicit_loop, "checked-plan-api");
  ROSE_ASSERT(&implicit.loop() == implicit_loop);
  ROSE_ASSERT(&implicit.structuralOwner() == implicit_loop->get_parent());
  ROSE_ASSERT(&implicit.forInit() == implicit_loop->get_for_init_stmt());
  ROSE_ASSERT(&implicit.testStatement() == implicit_loop->get_test());
  ROSE_ASSERT(&implicit.testExpression() == implicit_loop->get_test_expr());
  ROSE_ASSERT(&implicit.incrementExpression() ==
              implicit_loop->get_increment());
  ROSE_ASSERT(&implicit.body() == implicit_loop->get_loop_body());
  ROSE_ASSERT(implicit.stride().kind() == CanonicalStride::Kind::implicit_unit);
  ROSE_ASSERT(implicit.stride().positiveValue() == 1);
  ROSE_ASSERT(!implicit.stride().explicitExpression());

  SgForStatement *explicit_loop = singleLoop(project, "unroll_descending");
  const CanonicalPlan explicit_plan =
      SageInterface::requireCheckedCanonicalLoopPlan(explicit_loop,
                                                     "checked-plan-api");
  const std::optional<std::reference_wrapper<SgExpression>>
      explicit_expression = explicit_plan.stride().explicitExpression();
  ROSE_ASSERT(explicit_plan.stride().kind() ==
              CanonicalStride::Kind::explicit_positive);
  ROSE_ASSERT(explicit_plan.stride().positiveValue() == 3);
  ROSE_ASSERT(explicit_expression.has_value());
  ROSE_ASSERT(explicit_expression->get().get_parent() != nullptr);

  const UnrollPlan unroll = SageInterface::requireCheckedLoopUnrollPlan(
      implicit_loop, 1, "checked-plan-api");
  ROSE_ASSERT(&unroll.loop().loop() == implicit_loop);
  ROSE_ASSERT(unroll.factor() == 1);

  SgForStatement *outer = outerLoop(project, "tiled_vector", 2);
  const TilingPlan tiling = SageInterface::requireCheckedLoopTilingPlan(
      outer, std::vector<size_t>{1, 1}, "checked-plan-api");
  ROSE_ASSERT(&tiling.outerLoop() == outer);
  ROSE_ASSERT(tiling.loops().size() == 2);
  ROSE_ASSERT(tiling.tileSizes() == std::vector<size_t>({1, 1}));
  ROSE_ASSERT(&tiling.loops().front().loop() == outer);
}

void applyRuntimeTransforms(SgProject *project) {
  requireCheckedPlanFactories(project);
  requireTripCount(project, "unroll_signed_extrema", 3);
  requireTripCount(project, "unroll_unsigned_extrema", 3);
  requireTripCount(project, "trip_unsigned_safe", 4);
  requireTripCount(project, "trip_unsigned_wrap", std::nullopt);
  requireTripCount(project, "trip_signed_overflow", std::nullopt);

  requireUnrollIdentity(project, singleLoop(project, "loop_with_continue"));

  SageInterface::loopUnrolling(singleLoop(project, "unroll_descending"), 3);
  requireGuardedUnrollShape(project, "unroll_descending", 3);
  SageInterface::loopUnrolling(singleLoop(project, "unroll_signed_extrema"), 4);
  requireGuardedUnrollShape(project, "unroll_signed_extrema", 4);
  SageInterface::loopUnrolling(singleLoop(project, "unroll_unsigned_extrema"),
                               4);
  requireGuardedUnrollShape(project, "unroll_unsigned_extrema", 4);
  SageInterface::loopUnrolling(singleLoop(project, "unroll_indexed_store"), 3);
  requireGuardedUnrollShape(project, "unroll_indexed_store", 3);

  SageInterface::loopTiling(outerLoop(project, "tiled_descending", 2), 2, 4);
  requireCountBoundedTilingShape(project, "tiled_descending");
  SageInterface::loopTiling(singleLoop(project, "tiled_zero_trip"), 1, 3);
  requireCountBoundedTilingShape(project, "tiled_zero_trip");
  SageInterface::loopTiling(outerLoop(project, "tiled_level_two", 2), 2, 2);
  requireCountBoundedTilingShape(project, "tiled_level_two");
  SageInterface::loopTiling(outerLoop(project, "tiled_level_three", 3), 3, 2);
  requireCountBoundedTilingShape(project, "tiled_level_three");
  SageInterface::loopTiling(singleLoop(project, "tiled_direct_body"), 1, 3);
  requireCountBoundedTilingShape(project, "tiled_direct_body");
  SageInterface::loopTiling(singleLoop(project, "tiled_continue"), 1, 3);
  requireCountBoundedTilingShape(project, "tiled_continue");
  SageInterface::loopTiling(outerLoop(project, "tiled_vector", 2),
                            std::vector<size_t>{2, 3});
  requireCountBoundedTilingShape(project, "tiled_vector");
}

void replaceLoopBodyForStalePlan(SgForStatement *loop) {
  SgStatement *old_body = loop->get_loop_body();
  SgBasicBlock *replacement = SageBuilder::buildBasicBlock();
  loop->set_loop_body(replacement);
  replacement->set_parent(loop);
  old_body->set_parent(nullptr);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  const char *mode_value = std::getenv("REX_LOOP_CORE_MODE");
  const std::string mode = mode_value != nullptr ? mode_value : "runtime";

  if (mode == "runtime") {
    applyRuntimeTransforms(project);
  } else if (mode == "checked_plan_api") {
    requireCheckedPlanFactories(project);
  } else if (mode == "unroll_identity") {
    requireUnrollIdentity(project, singleLoop(project, "loop_with_continue"));
  } else if (mode == "assignment") {
    requireAssignmentNormalization(project);
  } else if (mode == "unroll_zero") {
    SageInterface::loopUnrolling(singleLoop(project, "unroll_descending"), 0);
  } else if (mode == "unroll_overflow") {
    SageInterface::loopUnrolling(singleLoop(project, "unroll_descending"),
                                 std::numeric_limits<size_t>::max());
  } else if (mode == "unroll_continue") {
    SageInterface::loopUnrolling(singleLoop(project, "loop_with_continue"), 3);
  } else if (mode == "unroll_label") {
    SageInterface::loopUnrolling(singleLoop(project, "loop_with_label"), 2);
  } else if (mode == "tiling_zero") {
    SageInterface::loopTiling(singleLoop(project, "tiled_zero_trip"), 1, 0);
  } else if (mode == "tiling_imperfect") {
    SageInterface::loopTiling(outerLoop(project, "imperfect_nest", 2), 2, 4);
  } else if (mode == "tiling_break") {
    SageInterface::loopTiling(singleLoop(project, "loop_with_break"), 1, 3);
  } else if (mode == "zero_stride") {
    SageInterface::loopUnrolling(singleLoop(project, "zero_stride"), 2);
  } else if (mode == "negative_stride") {
    SageInterface::loopUnrolling(singleLoop(project, "negative_stride"), 2);
  } else if (mode == "dynamic_stride") {
    SageInterface::loopUnrolling(singleLoop(project, "dynamic_stride"), 2);
  } else if (mode == "induction_write") {
    SageInterface::loopUnrolling(singleLoop(project, "induction_write"), 2);
  } else if (mode == "induction_address") {
    SageInterface::loopUnrolling(singleLoop(project, "induction_address"), 2);
  } else if (mode == "induction_call") {
    SageInterface::loopUnrolling(singleLoop(project, "induction_call"), 2);
  } else if (mode == "bound_write") {
    SageInterface::loopUnrolling(singleLoop(project, "bound_write"), 2);
  } else if (mode == "direction_mismatch") {
    SageInterface::forLoopNormalization(
        singleLoop(project, "mismatched_direction"), false);
  } else if (mode == "assignment_malformed") {
    SageInterface::forLoopNormalization(
        singleLoop(project, "malformed_assignment_increment"), false);
  } else if (mode == "owner_duplicate") {
    SgForStatement *loop = singleLoop(project, "loop_with_continue");
    SgBasicBlock *owner = isSgBasicBlock(loop->get_parent());
    ROSE_ASSERT(owner != nullptr);
    owner->get_statements().push_back(loop);
    (void)SageInterface::requireCheckedCanonicalLoopPlan(loop,
                                                         "owner-death-test");
  } else if (mode == "stale_trip_count") {
    SgForStatement *loop = singleLoop(project, "tiled_zero_trip");
    SageInterface::CheckedCanonicalLoopPlan plan =
        SageInterface::requireCheckedCanonicalLoopPlan(loop,
                                                       "stale-death-test");
    replaceLoopBodyForStalePlan(loop);
    (void)SageInterface::exactCanonicalLoopTripCount(plan);
  } else if (mode == "canonical_null") {
    (void)SageInterface::requireCheckedCanonicalLoopPlan(
        nullptr, "null-loop-death-test");
  } else if (mode == "tiling_empty") {
    (void)SageInterface::requireCheckedLoopTilingPlan(
        singleLoop(project, "tiled_zero_trip"), {}, "empty-tiling-death-test");
  } else if (mode == "stale_unroll") {
    SgForStatement *loop = singleLoop(project, "tiled_zero_trip");
    const UnrollPlan plan = SageInterface::requireCheckedLoopUnrollPlan(
        loop, 2, "stale-unroll-death-test");
    replaceLoopBodyForStalePlan(loop);
    SageInterface::commitLoopUnrolling(plan);
  } else if (mode == "stale_tiling") {
    SgForStatement *loop = singleLoop(project, "tiled_zero_trip");
    const TilingPlan plan = SageInterface::requireCheckedLoopTilingPlan(
        loop, {2}, "stale-tiling-death-test");
    replaceLoopBodyForStalePlan(loop);
    SageInterface::commitLoopTiling(plan);
  } else {
    ROSE_ASSERT(!"unknown loop core contract mode");
  }

  AstTests::runAllTests(project);
  return backend(project);
}
