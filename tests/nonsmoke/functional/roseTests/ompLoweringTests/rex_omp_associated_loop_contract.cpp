#include "omp_lowering.h"

#include "rose.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
void requireExactUnrollDiscovery(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgOmpUnrollStatement);
  ROSE_ASSERT(nodes.size() == 4);
  for (SgNode *node : nodes) {
    SgOmpUnrollStatement *unroll = isSgOmpUnrollStatement(node);
    ROSE_ASSERT(unroll != nullptr && unroll->get_parent() != nullptr);
    const std::vector<SgNode *> siblings =
        unroll->get_parent()->get_traversalSuccessorContainer();
    ROSE_ASSERT(std::count(siblings.begin(), siblings.end(), unroll) == 1);
    ROSE_ASSERT(unroll->get_body() != nullptr &&
                unroll->get_body()->get_parent() == unroll);
    SgSourceFile *source = SageInterface::getEnclosingSourceFile(unroll);
    ROSE_ASSERT(source != nullptr && source->get_project() == project);
    ROSE_ASSERT(SageInterface::getProject(unroll) == project);
    ROSE_ASSERT(SageInterface::getEnclosingFunctionDeclaration(unroll) !=
                nullptr);
  }
}

SgOmpUnrollStatement *findUnroll(SgProject *project,
                                 const std::string &function_name,
                                 bool outermost = false) {
  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(project, V_SgOmpUnrollStatement);
  for (SgNode *node : nodes) {
    SgOmpUnrollStatement *unroll = isSgOmpUnrollStatement(node);
    SgFunctionDeclaration *function =
        SageInterface::getEnclosingFunctionDeclaration(unroll);
    if (function == nullptr ||
        function->get_name().getString() != function_name)
      continue;
    bool has_unroll_ancestor = false;
    for (SgNode *ancestor = unroll->get_parent();
         ancestor != nullptr && ancestor != function;
         ancestor = ancestor->get_parent()) {
      if (isSgOmpUnrollStatement(ancestor) != nullptr) {
        has_unroll_ancestor = true;
        break;
      }
    }
    if (!outermost || !has_unroll_ancestor)
      return unroll;
  }
  return nullptr;
}

SgForStatement *exactLoop(SgOmpUnrollStatement *directive) {
  return OmpSupport::requireExactAssociatedForLoop(
      directive, directive->get_body(),
      OmpSupport::AssociatedLoopPathContract::LoopTransformation,
      "associated-loop-contract");
}

void replaceDirectiveBody(SgOmpBodyStatement *directive,
                          SgStatement *replacement) {
  SgStatement *old = directive->get_body();
  ROSE_ASSERT(old != nullptr);
  directive->set_body(nullptr);
  old->set_parent(nullptr);
  ROSE_ASSERT(replacement != nullptr && replacement->get_parent() == nullptr);
  directive->set_body(replacement);
  replacement->set_parent(directive);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  std::vector<std::string> frontend_args{argv[0], "-rose:openmp:ast_only",
                                         "-rose:skipfinalCompileStep", "-c",
                                         argv[2]};
  SgProject *project = frontend(frontend_args);
  ROSE_ASSERT(project != nullptr);

  if (mode == "--reject-c-const-factor") {
    SgOmpUnrollStatement *c_const =
        findUnroll(project, "rex_omp_c_const_integral_constant");
    ROSE_ASSERT(c_const != nullptr);
    OmpSupport::transOmpUnroll(c_const);
    return 1;
  }

  requireExactUnrollDiscovery(project);

  SgOmpUnrollStatement *direct = findUnroll(project, "direct_loop");
  SgOmpUnrollStatement *nested =
      findUnroll(project, "nested_transform_loop", true);
  ROSE_ASSERT(direct != nullptr && nested != nullptr);

  if (mode == "--accept-direct") {
    ROSE_ASSERT(exactLoop(direct) != nullptr);
    return 0;
  }

  if (mode == "--accept-basic-block") {
    SgForStatement *loop = exactLoop(direct);
    SgBasicBlock *block = SageInterface::makeSingleStatementBodyToBlock(loop);
    ROSE_ASSERT(block != nullptr && direct->get_body() == block);
    ROSE_ASSERT(exactLoop(direct) == loop);
    return 0;
  }

  if (mode == "--accept-transform-wrapper") {
    ROSE_ASSERT(exactLoop(nested) != nullptr);
    return 0;
  }

  if (mode == "--accept-cxx-constexpr-factor") {
    OmpSupport::transOmpUnroll(direct);
    return 0;
  }

  if (mode == "--accept-cxx-const-factor") {
    SgOmpUnrollStatement *cxx_const = findUnroll(project, "const_direct_loop");
    ROSE_ASSERT(cxx_const != nullptr);
    OmpSupport::transOmpUnroll(cxx_const);
    return 0;
  }

  if (mode == "--reject-ambiguous-block") {
    SgForStatement *loop = exactLoop(direct);
    SgBasicBlock *block = SageInterface::makeSingleStatementBodyToBlock(loop);
    block->get_statements().push_back(loop);
    exactLoop(direct);
    return 1;
  }

  if (mode == "--reject-non-loop") {
    replaceDirectiveBody(direct, new SgNullStatement());
    exactLoop(direct);
    return 1;
  }

  if (mode == "--reject-wrong-root") {
    SgForStatement *other = exactLoop(nested);
    OmpSupport::requireExactAssociatedForLoop(
        direct, other,
        OmpSupport::AssociatedLoopPathContract::LoopTransformation,
        "associated-loop-contract");
    return 1;
  }

  if (mode == "--reject-parent") {
    SgForStatement *loop = exactLoop(direct);
    loop->set_parent(nested);
    exactLoop(direct);
    return 1;
  }

  if (mode == "--reject-parallel-wrapper" || mode == "--reject-task-wrapper" ||
      mode == "--reject-critical-wrapper") {
    SgForStatement *loop = exactLoop(direct);
    direct->set_body(nullptr);
    loop->set_parent(nullptr);
    SgOmpBodyStatement *wrapper = nullptr;
    if (mode == "--reject-parallel-wrapper")
      wrapper = new SgOmpParallelStatement(nullptr, loop);
    else if (mode == "--reject-task-wrapper")
      wrapper = new SgOmpTaskStatement(nullptr, loop);
    else
      wrapper = new SgOmpCriticalStatement(nullptr, loop, SgName());
    ROSE_ASSERT(wrapper != nullptr);
    if (loop->get_parent() == nullptr)
      loop->set_parent(wrapper);
    direct->set_body(wrapper);
    wrapper->set_parent(direct);
    exactLoop(direct);
    return 1;
  }

  if (mode == "--reject-attributed-wrapper") {
    SgForStatement *loop = exactLoop(direct);
    direct->set_body(nullptr);
    loop->set_parent(nullptr);
    SgAttributedStatement *attributed = new SgAttributedStatement(loop);
    if (loop->get_parent() == nullptr)
      loop->set_parent(attributed);
    direct->set_body(attributed);
    attributed->set_parent(direct);
    exactLoop(direct);
    return 1;
  }

  if (mode == "--reject-cycle") {
    SgForStatement *loop = exactLoop(direct);
    SgBasicBlock *block = SageInterface::makeSingleStatementBodyToBlock(loop);
    block->get_statements().clear();
    loop->set_parent(nullptr);
    block->get_statements().push_back(direct);
    direct->set_parent(block);
    exactLoop(direct);
    return 1;
  }

  if (mode == "--reject-continue") {
    SgOmpUnrollStatement *with_continue = findUnroll(project, "continue_loop");
    ROSE_ASSERT(with_continue != nullptr);
    OmpSupport::transOmpUnroll(with_continue);
    return 1;
  }

  if (mode == "--reject-stale-constant-symbol") {
    SgOmpClauseList *clauses = direct->get_clause_list();
    ROSE_ASSERT(clauses != nullptr && clauses->get_clauses().size() == 1);
    SgOmpPartialClause *partial =
        isSgOmpPartialClause(clauses->get_clauses().front());
    SgVarRefExp *reference =
        partial != nullptr ? isSgVarRefExp(partial->get_expression()) : nullptr;
    SgVariableSymbol *canonical =
        reference != nullptr ? isSgVariableSymbol(reference->get_symbol())
                             : nullptr;
    SgInitializedName *name =
        canonical != nullptr ? canonical->get_declaration() : nullptr;
    SgScopeStatement *scope = name != nullptr ? name->get_scope() : nullptr;
    ROSE_ASSERT(scope != nullptr && canonical->get_scope() == scope);
    ROSE_ASSERT(scope->find_symbol_from_declaration(name) == canonical);

    SgVariableSymbol *stale = new SgVariableSymbol(name);
    stale->set_parent(scope->get_symbol_table());
    ROSE_ASSERT(stale->get_scope() == scope);
    ROSE_ASSERT(scope->find_symbol_from_declaration(name) != stale);
    reference->set_symbol(stale);
    OmpSupport::transOmpUnroll(direct);
    return 1;
  }

  return 2;
}
