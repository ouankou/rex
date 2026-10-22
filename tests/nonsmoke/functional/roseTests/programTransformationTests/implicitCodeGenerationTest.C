#include "CallGraph.h"
#include "checkPhysicalSourcePosition.h"

#include "destructorCallAnnotator.h"

#include "rose.h"

#include "shortCircuitingTransformation.h"
#include "staticCFG.h"

#include <algorithm>
#include <iostream>
#include <map>
using namespace std;

static void validateIterationConditionScopes(SgProject *project) {
  const auto requireExit = [](const VirtualCFG::CFGEdge &edge,
                              SgVariableDeclaration *condition) {
    const auto leaving = edge.scopesBeingExited();
    for (auto *name : condition->get_variables())
      ROSE_ASSERT(std::count(leaving.begin(), leaving.end(), name) == 1);
  };
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgWhileStmt)) {
    auto *loop = isSgWhileStmt(node);
    auto *condition = isSgVariableDeclaration(loop->get_condition());
    if (condition == nullptr)
      continue;
    for (const auto &edge : loop->cfgForEnd().outEdges())
      requireExit(edge, condition);
    for (const auto &edge : loop->get_body()->cfgForEnd().outEdges())
      requireExit(edge, condition);
    for (SgNode *statement :
         NodeQuery::querySubTree(loop->get_body(), V_SgContinueStmt))
      for (const auto &edge : statement->cfgForBeginning().outEdges())
        if (edge.target() == loop->cfgForBeginning())
          requireExit(edge, condition);
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgForStatement)) {
    auto *loop = isSgForStatement(node);
    for (const auto &edge : loop->cfgForEnd().outEdges()) {
      if (auto *condition = isSgVariableDeclaration(loop->get_test()))
        requireExit(edge, condition);
      for (auto *statement : loop->get_init_stmt())
        if (auto *declaration = isSgVariableDeclaration(statement))
          requireExit(edge, declaration);
        else if (auto *group = isSgDeclarationGroupStatement(statement))
          for (auto *member : group->get_declarations())
            if (auto *declaration = isSgVariableDeclaration(member))
              requireExit(edge, declaration);
    }
    if (auto *condition = isSgVariableDeclaration(loop->get_test()))
      for (const auto &edge : loop->get_increment()->cfgForEnd().outEdges())
        requireExit(edge, condition);
  }
}

static void validateDestructorEpilogues(SgProject *project, bool copyNative) {
  ClassHierarchyWrapper hierarchy(project);
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgReturnStmt)) {
    auto *statement = isSgReturnStmt(node);
    auto *definition = SageInterface::getEnclosingFunctionDefinition(statement);
    ROSE_ASSERT(definition != nullptr);
    for (const auto &edge : statement->cfgForEnd().outEdges()) {
      const auto leaving = edge.scopesBeingExited();
      for (auto *name : leaving)
        if (isSgBasicBlock(name->get_scope()))
          ROSE_ASSERT(SageInterface::isAncestor(name->get_scope(), statement));
      if (edge.target() != definition->cfgForEnd())
        continue;
      for (auto *parameter : definition->get_declaration()->get_args())
        if (!isSgTypeEllipse(parameter->get_type()))
          ROSE_ASSERT(std::count(leaving.begin(), leaving.end(), parameter) ==
                      1);
    }
  }
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *definition = isSgFunctionDefinition(node);
    auto *owner = isSgMemberFunctionDeclaration(definition->get_declaration());
    if (owner == nullptr ||
        !owner->get_specialFunctionModifier().isDestructor())
      continue;
    auto *epilogue = definition->get_destructor_epilogue();
    if (definition->get_destructor_epilogue_dependent()) {
      ROSE_ASSERT(epilogue == nullptr);
      continue;
    }
    ROSE_ASSERT(epilogue != nullptr && epilogue->get_parent() == definition);
    for (bool filtered : {false, true}) {
      StaticCFG::CFG graph(definition, filtered);
      ASSERT_not_null(graph.getExit());
      ROSE_ASSERT(graph.toCFGNode(graph.getExit()) == definition->cfgForEnd());
    }
    const auto edges = definition->cfgOutEdges(3);
    ROSE_ASSERT(edges.size() == 1 &&
                edges.front().target() == epilogue->cfgForBeginning());
    for (auto *expression : epilogue->get_expressions()) {
      auto *step = isSgCxxSubobjectDestruction(expression);
      ROSE_ASSERT(step != nullptr && step->get_parent() == epilogue);
      step->validate();
      Rose_STL_Container<SgFunctionDeclaration *> targets;
      CallTargetSet::getPropertiesForExpression(step, &hierarchy, targets);
      ROSE_ASSERT(targets.size() == 1 &&
                  targets.front()->get_firstNondefiningDeclaration() ==
                      step->get_destructor_declaration()
                          ->get_firstNondefiningDeclaration());
      const auto branches = step->cfgOutEdges(0);
      if (step->get_target_kind() ==
          SgCxxSubobjectDestruction::e_virtual_base) {
        ROSE_ASSERT(branches.size() == 2);
        ROSE_ASSERT(branches[0].condition() ==
                    VirtualCFG::eckCompleteObjectDestruction);
        ROSE_ASSERT(branches[1].condition() ==
                    VirtualCFG::eckBaseObjectDestruction);
      } else {
        ROSE_ASSERT(branches.size() == 1);
      }
    }
    auto *scope = isSgClassDefinition(owner->get_scope());
    ROSE_ASSERT(scope != nullptr);
    if (scope->get_declaration()->get_name() == "RexDestructorOwner") {
      const auto &steps = epilogue->get_expressions();
      ROSE_ASSERT(steps.size() == 4);
      ROSE_ASSERT(isSgCxxSubobjectDestruction(steps[0])
                      ->get_member_target()
                      ->get_name() == "second");
      ROSE_ASSERT(isSgCxxSubobjectDestruction(steps[1])
                      ->get_member_target()
                      ->get_name() == "first");
      ROSE_ASSERT(isSgCxxSubobjectDestruction(steps[2])->get_target_kind() ==
                  SgCxxSubobjectDestruction::e_nonvirtual_base);
      ROSE_ASSERT(isSgCxxSubobjectDestruction(steps[3])->get_target_kind() ==
                  SgCxxSubobjectDestruction::e_virtual_base);
    }
    if (copyNative) {
      auto *copy = SageInterface::deepCopySemantic(epilogue);
      ROSE_ASSERT(copy != nullptr && copy != epilogue &&
                  copy->get_parent() == nullptr);
      ROSE_ASSERT(copy->get_expressions().size() ==
                  epilogue->get_expressions().size());
      for (size_t index = 0; index < copy->get_expressions().size(); ++index) {
        auto *before =
            isSgCxxSubobjectDestruction(epilogue->get_expressions()[index]);
        auto *after =
            isSgCxxSubobjectDestruction(copy->get_expressions()[index]);
        ROSE_ASSERT(after != nullptr && after != before &&
                    after->get_parent() == copy);
        ROSE_ASSERT(after->get_destructor_declaration() ==
                    before->get_destructor_declaration());
        ROSE_ASSERT(after->get_member_target() == before->get_member_target());
        ROSE_ASSERT(after->get_base_target() == before->get_base_target());
        after->validate();
      }
      SageInterface::deleteAST(copy);
    }
  }
}

int main(int argc, char **argv) {
  assert(argv);
  vector<string> argvList(argv, argv + argc);

  bool doShortCircuitingTransformation = CommandlineProcessing::isOption(
      argvList, "-icg:", "shortCircuitingTransformation", true);
  bool doDestructorCallAnnotator = CommandlineProcessing::isOption(
      argvList, "-icg:", "destructorCallAnnotator", true);

  cout << "doShortCircuitingTransformation = "
       << doShortCircuitingTransformation << endl;
  cout << "doDestructorCallAnnotator       = " << doDestructorCallAnnotator
       << endl;

  SgProject *prj = frontend(argvList);
  checkPhysicalSourcePosition(prj);
  validateIterationConditionScopes(prj);
  validateDestructorEpilogues(prj, true);

  for (SgNode *node : NodeQuery::querySubTree(prj, V_SgCtorInitializerList)) {
    const auto &initializers = isSgCtorInitializerList(node)->get_ctors();
    for (SgInitializedName *initializer : initializers) {
      if (initializer->get_preinitialization() ==
          SgInitializedName::e_delegation_constructor)
        ROSE_ASSERT(initializers.size() == 1);
    }
  }

  if (doShortCircuitingTransformation) {
    shortCircuitingTransformation(prj);
  }

  if (doDestructorCallAnnotator) {
    const auto staticReferences = [prj]() {
      std::map<SgInitializedName *, size_t> references;
      for (auto *node : NodeQuery::querySubTree(prj, V_SgVarRefExp)) {
        auto *name = isSgVarRefExp(node)->get_symbol()->get_declaration();
        auto *declaration = isSgVariableDeclaration(name->get_parent());
        if (declaration == nullptr)
          continue;
        const auto &storage =
            declaration->get_declarationModifier().get_storageModifier();
        if (storage.isStatic() || storage.isExtern() ||
            storage.get_thread_local_storage() ||
            declaration->get_is_thread_local())
          ++references[name];
      }
      return references;
    };
    const auto before = staticReferences();
    destructorCallAnnotator(prj);
    ROSE_ASSERT(staticReferences() == before);
    for (SgNode *node : NodeQuery::querySubTree(prj, V_SgInitializedName)) {
      auto *name = isSgInitializedName(node);
      if (name->get_structured_binding_scope() == nullptr)
        continue;
      name->validate_structured_binding();
      auto *scope = name->get_scope();
      ROSE_ASSERT(scope != nullptr && name->get_name().is_null() &&
                  scope->find_symbol_from_declaration(name) == nullptr);
      for (auto *element :
           name->get_structured_binding_pattern()->get_expressions()) {
        auto *reference = isSgVarRefExp(element);
        ROSE_ASSERT(reference != nullptr &&
                    scope->lookup_variable_symbol(
                        reference->get_symbol()->get_name()) ==
                        reference->get_symbol());
      }
    }
    bool hasNestedFixture = false;
    size_t nestedChecks = 0;
    for (SgNode *node : NodeQuery::querySubTree(prj, V_SgFunctionDefinition)) {
      auto *definition = isSgFunctionDefinition(node);
      hasNestedFixture |=
          definition->get_declaration()->get_name() == "rex_icg_local_class";
      if (definition->get_declaration()->get_name() != "rex_icg_nested_cleanup")
        continue;
      size_t cleanupCalls = 0;
      for (SgNode *callNode :
           NodeQuery::querySubTree(definition, V_SgFunctionCallExp)) {
        auto *target =
            isSgFunctionCallExp(callNode)->getAssociatedFunctionDeclaration();
        if (target != nullptr &&
            target->get_specialFunctionModifier().isDestructor())
          ++cleanupCalls;
      }
      if (cleanupCalls != 1)
        std::cerr << "nested cleanup calls: " << cleanupCalls << '\n';
      ROSE_ASSERT(cleanupCalls == 1);
      ++nestedChecks;
    }
    ROSE_ASSERT(!hasNestedFixture || nestedChecks == 1);
  }

  AstTests::runAllTests(prj);
  validateDestructorEpilogues(prj, false);
  prj->get_file(0).set_unparse_includes(true);

  return backend(prj);
}
