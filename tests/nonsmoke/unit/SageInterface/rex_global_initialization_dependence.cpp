#include "rose.h"

#include "AstInterface_ROSE.h"
#include "AstUtilInterface.h"
#include "CompSliceLocality.h"
#include "DepGraphTransform.h"
#include "LoopTreeDepComp.h"
#include "LoopTreeDummyNode.h"
#include "LoopTreeLocality.h"
#include "LoopTreeShadow.h"
#include "LoopTreeTransform.h"
#include "ReuseAnalysis.h"
#include "RoseAst.h"
#include "StmtInfoCollect.h"
#include "dependence_analysis.h"
#include "dependence_table.h"
#include "processSupport.h"
#include "transformationTracking.h"

#include <algorithm>
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct DeleteCopy {
  void operator()(SgNode *node) const {
    SageInterface::deleteAST(node,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
};

void requireSpatialReuseBindingIdentity(SgProject *project) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_name() != "rex_locality_shadowing" ||
        function->get_definition() == nullptr)
      continue;
    auto *index = function->get_parameterList()->get_args().at(1);
    SymbolicVar induction(index->get_name(), index->get_scope(), index);
    ROSE_ASSERT(induction.GetVarDeclaration() == index);
    const auto accesses =
        NodeQuery::querySubTree(function->get_definition(), V_SgPntrArrRefExp);
    ROSE_ASSERT(accesses.size() == 3);
    AstInterfaceImpl implementation(function->get_definition());
    AstInterface interface(&implementation);
    AstInterface *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    ROSE_ASSERT(SelfSpatialReuse(accesses[0], induction, 64) > 0);
    ROSE_ASSERT(SelfSpatialReuse(accesses[1], induction, 64) > 0);
    ROSE_ASSERT(SelfSpatialReuse(accesses[2], induction, 64) == 0);
    LoopTransformInterface::set_astInterfacePtr(previous);
    return;
  }
  ROSE_ABORT();
}

void requireSpatialReuseTargetLayout(SgProject *project) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_name() != "rex_locality_target_layout" ||
        function->get_definition() == nullptr)
      continue;
    const auto &parameters = function->get_parameterList()->get_args();
    ROSE_ASSERT(parameters.size() == 6);
    auto *index = parameters.back();
    SymbolicVar induction(index->get_name(), index->get_scope(), index);
    const auto accesses =
        NodeQuery::querySubTree(function->get_definition(), V_SgPntrArrRefExp);
    const auto sizes =
        NodeQuery::querySubTree(function->get_definition(), V_SgSizeOfOp);
    ROSE_ASSERT(accesses.size() == 5 && sizes.size() == 4);
    AstInterfaceImpl implementation(function->get_definition());
    AstInterface interface(&implementation);
    AstInterface *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    for (size_t i = 0; i < sizes.size(); ++i) {
      const auto &size = isSgSizeOfOp(sizes[i])->get_evaluated_value();
      ROSE_ASSERT(size && size->words()[1] == 0 && size->words()[0] > 0 &&
                  size->words()[0] < 64);
      const float expected = 1.0f - float(size->words()[0]) / 64;
      const float actual = SelfSpatialReuse(accesses[i], induction, 64);
      if (actual != expected) {
        std::cerr << "REX_TEST_ERROR[locality-target-layout]: array="
                  << parameters[i]->get_name()
                  << " element-bytes=" << size->words()[0]
                  << " expected=" << expected << " actual=" << actual << '\n';
        ROSE_ABORT();
      }
    }
    auto *lanes = isSgPntrArrRefExp(accesses.back());
    ROSE_ASSERT(lanes->get_result_storage_size_bytes() == 1);
    ROSE_ASSERT(lanes->get_subscript_stride_bits() ==
                std::vector<std::optional<long long>>{1});
    ROSE_ASSERT(SelfSpatialReuse(lanes, induction, 64) == 1.0f - 1.0f / 512);
    LoopTransformInterface::set_astInterfacePtr(previous);
    return;
  }
  ROSE_ABORT();
}

void requireOffsetOfSubscriptLayout(SgProject *project) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_name() != "rex_locality_offsetof_layout" ||
        function->get_definition() == nullptr)
      continue;
    const auto accesses =
        NodeQuery::querySubTree(function->get_definition(), V_SgPntrArrRefExp);
    const auto sizes =
        NodeQuery::querySubTree(function->get_definition(), V_SgSizeOfOp);
    ROSE_ASSERT(accesses.size() == 2 && sizes.size() == 2);
    for (SgNode *node : accesses) {
      auto *access = isSgPntrArrRefExp(node);
      const auto &size =
          isSgSizeOfOp(sizes[isSgArrayType(access->get_type()) ? 1 : 0])
              ->get_evaluated_value();
      ROSE_ASSERT(size && size->words()[1] == 0 && size->words()[0] > 0 &&
                  size->words()[0] < 64);
      const long long bytes = size->words()[0];
      ROSE_ASSERT(access->get_result_storage_size_bytes() == bytes);
      ROSE_ASSERT(access->get_subscript_stride_bits() ==
                  std::vector<std::optional<long long>>{bytes * 8});
    }
    return;
  }
  ROSE_ABORT();
}

void requireSliceReuseBindingIdentity(SgProject *project) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *function = isSgFunctionDefinition(node);
    if (function->get_declaration()->get_name() !=
        "rex_locality_slice_identity")
      continue;
    AstInterfaceImpl implementation(function);
    AstInterface interface(&implementation);
    AstInterface *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    auto *previousAlias = LoopTransformInterface::getAliasInfo();
    StmtVarAliasCollect aliases;
    aliases.analyze(interface, function);
    LoopTransformInterface::set_aliasInfo(&aliases);
    {
      LoopTreeDepCompCreate computation(function->get_body(), true, true);
      LoopTreeLocalityAnal locality(computation);
      CompSliceLocalityAnal analysis(locality, 64, 0);
      CompSliceNest slices[6];
      size_t count = 0;
      for (LoopTreeTraverseSelectLoop loops(computation.GetLoopTreeRoot());
           !loops.ReachEnd(); ++loops) {
        ROSE_ASSERT(count < 6);
        LoopTreeDepCompSubtree subtree(computation, loops.Current());
        DependenceHoisting().Analyze(subtree, slices[count]);
        ROSE_ASSERT(slices[count].NumberOfEntries() == 1);
        ++count;
      }
      ROSE_ASSERT(count == 6);
      CompSliceLocalityAnal::AstNodeSet references;
      ROSE_ASSERT(analysis.SpatialReuses(slices[0].Entry(0), slices[1].Entry(0),
                                         references) == 1);
      references.clear();
      ROSE_ASSERT(analysis.SpatialReuses(slices[0].Entry(0), slices[2].Entry(0),
                                         references) == 0);
      references.clear();
      ROSE_ASSERT(analysis.SpatialReuses(slices[3].Entry(0), slices[4].Entry(0),
                                         references) == 0);
      references.clear();
      ROSE_ASSERT(analysis.SpatialReuses(slices[3].Entry(0), slices[5].Entry(0),
                                         references) == 1);
    }
    LoopTransformInterface::set_aliasInfo(previousAlias);
    LoopTransformInterface::set_astInterfacePtr(previous);
    return;
  }
  ROSE_ABORT();
}

void requireLoopTreeIfScopes(SgProject *project) {
  struct EditTree : LoopTreeTransform {
    using LoopTreeTransform::UnlinkNode;
    void Drop(LoopTreeNode *node) {
      while (node->FirstChild() != nullptr)
        Drop(node->FirstChild());
      RemoveNode(node);
    }
  };
  struct ShadowTree : LoopTreeShadowCreate {
    void Copy(LoopTreeNode *source, LoopTreeNode *parent) {
      auto *shadow = CreateShadowNode(source);
      shadow->Link(parent, LoopTreeNode::AsLastChild);
      for (auto *child = source->FirstChild(); child != nullptr;
           child = child->NextSibling())
        Copy(child, shadow);
    }
    explicit ShadowTree(LoopTreeNode *source) : LoopTreeShadowCreate(0) {
      Copy(source, GetTreeRoot());
    }
  };
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *function = isSgFunctionDefinition(node);
    if (function->get_declaration()->get_name() != "rex_loop_tree_if_scopes")
      continue;
    auto *body = function->get_body();
    auto branches = NodeQuery::querySubTree(body, V_SgIfStmt);
    ROSE_ASSERT(branches.size() == 1);
    auto *branch = isSgIfStmt(branches.front());
    AstInterfaceImpl implementation(body);
    AstInterface interface(&implementation);
    AstInterface *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    auto *previous_alias = LoopTransformInterface::getAliasInfo();
    StmtVarAliasCollect aliases;
    aliases.analyze(interface, function);
    LoopTransformInterface::set_aliasInfo(&aliases);
    {
      LoopTreeDepCompCreate computation(body);
      auto *root = computation.GetLoopTreeRoot();
      ROSE_ASSERT(root->ChildCount() == 1);
      auto *scope = root->FirstChild();
      ROSE_ASSERT(scope->GetBodyAst() == body && scope->ChildCount() == 3);
      auto *control = scope->FirstChild()->NextSibling();
      ROSE_ASSERT(control->GetSourceAst() == branch);
      ROSE_ASSERT(control->ChildCount() == 2);
      LoopTreeCreate copied;
      control->CloneTree()->Link(copied.GetTreeRoot(),
                                 LoopTreeNode::AsLastChild);
      ShadowTree shadow(control);
      for (auto *tree : {control, copied.GetTreeRoot()->FirstChild(),
                         shadow.GetTreeRoot()->FirstChild()}) {
        auto *true_arm = tree->FirstChild();
        auto *false_arm = tree->LastChild();
        ROSE_ASSERT(true_arm->GetBodyRole() == LoopTreeNode::BodyRole::IfTrue);
        ROSE_ASSERT(false_arm->GetBodyRole() ==
                    LoopTreeNode::BodyRole::IfFalse);
        ROSE_ASSERT(true_arm->GetBodyAst() == branch->get_true_body());
        ROSE_ASSERT(false_arm->GetBodyAst() == branch->get_false_body());
        ROSE_ASSERT(true_arm->ChildCount() == 3);
        ROSE_ASSERT(false_arm->ChildCount() == 2);
        auto *nested = true_arm->LastChild();
        ROSE_ASSERT(nested->GetBodyRole() == LoopTreeNode::BodyRole::Block);
        ROSE_ASSERT(nested->ChildCount() == 2);
      }
      unsigned declarations = 0, scopes = 0;
      for (LoopTreeTraverse nodes(root); !nodes.ReachEnd(); nodes.Advance()) {
        auto *entry = nodes.Current();
        if (entry->GetBodyRole() != LoopTreeNode::BodyRole::None) {
          ROSE_ASSERT(entry->GetSourceAst() == AST_NULL);
          ROSE_ASSERT(computation.GetDepNode(entry) == nullptr);
          ++scopes;
        }
        if (isSgVariableDeclaration(entry->GetSourceAst().get_ptr())) {
          ROSE_ASSERT(entry->IsAtomicStatement());
          ROSE_ASSERT(computation.GetDepNode(entry) != nullptr);
          ++declarations;
        }
      }
      ROSE_ASSERT(scopes == 4 && declarations == 4);
      const auto original = body->unparseToString();
      const auto blocks = SgBasicBlock::numberOfNodes();
      const auto declarations_count = SgVariableDeclaration::numberOfNodes();
      const auto names = SgInitializedName::numberOfNodes();
      const auto tables = SgSymbolTable::numberOfNodes();
      const auto symbols = SgVariableSymbol::numberOfNodes();
      const auto references = SgVarRefExp::numberOfNodes();
      const auto literals = SgIntVal::numberOfNodes();
      auto *cloned = copied.GetTreeRoot()->FirstChild();
      auto *true_arm = cloned->FirstChild();
      auto *nested = true_arm->LastChild();
      EditTree edit;
      edit.UnlinkNode(nested);
      nested->Link(true_arm->FirstChild()->NextSibling(),
                   LoopTreeNode::AsPrevSibling);
      for (auto *tree : {root, copied.GetTreeRoot(), shadow.GetTreeRoot()}) {
        std::unique_ptr<SgStatement, DeleteCopy> emitted(
            isSgStatement(tree->CodeGen().get_ptr()));
        ROSE_ASSERT(emitted != nullptr);
        const auto emitted_branches =
            NodeQuery::querySubTree(emitted.get(), V_SgIfStmt);
        ROSE_ASSERT(emitted_branches.size() == 1);
        auto *emitted_if = isSgIfStmt(emitted_branches.front());
        ROSE_ASSERT(emitted_if != branch);
        auto *condition =
            isSgVariableDeclaration(emitted_if->get_conditional());
        ROSE_ASSERT(condition != nullptr);
        auto *selected = condition->get_variables().front();
        ROSE_ASSERT(selected->get_scope() == emitted_if);
        auto *yes = isSgBasicBlock(emitted_if->get_true_body());
        auto *no = isSgBasicBlock(emitted_if->get_false_body());
        ROSE_ASSERT(yes != nullptr && no != nullptr);
        ROSE_ASSERT(yes->get_statements().size() == 3 &&
                    no->get_statements().size() == 2);
        ROSE_ASSERT(isSgBasicBlock(yes->get_statements().at(
                        tree == copied.GetTreeRoot() ? 1 : 2)) != nullptr);
        for (auto *reference_node :
             NodeQuery::querySubTree(emitted_if, V_SgVarRefExp)) {
          auto *reference = isSgVarRefExp(reference_node);
          auto *binding = reference->get_symbol()->get_declaration();
          ROSE_ASSERT(binding != nullptr && binding->get_scope() != nullptr);
          ROSE_ASSERT(binding->get_scope()->find_symbol_from_declaration(
                          binding) == reference->get_symbol());
          if (binding->get_name() == "selected")
            ROSE_ASSERT(binding == selected);
          if (binding->get_name() == "local")
            ROSE_ASSERT(binding->get_scope() != body &&
                        SageInterface::isAncestor(emitted_if, binding));
        }
      }
      cloned->CloneTree()->Link(copied.GetTreeRoot(),
                                LoopTreeNode::AsLastChild);
      {
        std::unique_ptr<SgStatement, DeleteCopy> emitted(
            isSgStatement(copied.CodeGen().get_ptr()));
        const auto branches =
            NodeQuery::querySubTree(emitted.get(), V_SgIfStmt);
        ROSE_ASSERT(branches.size() == 2);
        auto *first = isSgIfStmt(branches.front());
        auto *second = isSgIfStmt(branches.back());
        auto *first_name = isSgVariableDeclaration(first->get_conditional())
                               ->get_variables()
                               .front();
        auto *second_name = isSgVariableDeclaration(second->get_conditional())
                                ->get_variables()
                                .front();
        ROSE_ASSERT(first_name != second_name &&
                    first_name->get_scope() == first &&
                    second_name->get_scope() == second);
      }
      {
        LoopTreeCreate projected;
        auto *selected = control->CloneTree();
        selected->Link(projected.GetTreeRoot(), LoopTreeNode::AsLastChild);
        edit.Drop(selected->LastChild());
        std::unique_ptr<SgIfStmt, DeleteCopy> emitted(
            isSgIfStmt(projected.CodeGen().get_ptr()));
        auto *empty = isSgBasicBlock(emitted->get_false_body());
        ROSE_ASSERT(empty != nullptr && empty->get_statements().empty());
        ROSE_ASSERT(empty->symbol_table_size() == 0);
        ROSE_ASSERT(
            isSgBasicBlock(emitted->get_true_body())->get_statements().size() ==
            3);
      }
      {
        const auto source_literals =
            NodeQuery::querySubTree(branch, V_SgIntVal);
        auto *source_literal = isSgIntVal(source_literals.front());
        std::unique_ptr<SgIntVal, DeleteCopy> replacement(
            SageBuilder::buildIntVal(987));
        LoopTreeCreate transformed;
        auto *recipe = new LoopTreeReplAst(source_literal, replacement.get());
        recipe->Link(transformed.GetTreeRoot(), LoopTreeNode::AsLastChild);
        control->CloneTree()->Link(recipe, LoopTreeNode::AsLastChild);
        std::unique_ptr<SgIfStmt, DeleteCopy> emitted(
            isSgIfStmt(transformed.CodeGen().get_ptr()));
        const auto emitted_literals =
            NodeQuery::querySubTree(emitted.get(), V_SgIntVal);
        ROSE_ASSERT(std::count_if(emitted_literals.begin(),
                                  emitted_literals.end(), [](SgNode *node) {
                                    return isSgIntVal(node)->get_value() == 987;
                                  }) == 1);
        ROSE_ASSERT(replacement->get_parent() == nullptr);
      }
      ROSE_ASSERT(SgBasicBlock::numberOfNodes() == blocks);
      ROSE_ASSERT(SgVariableDeclaration::numberOfNodes() == declarations_count);
      ROSE_ASSERT(SgInitializedName::numberOfNodes() == names);
      ROSE_ASSERT(SgSymbolTable::numberOfNodes() == tables);
      ROSE_ASSERT(SgVariableSymbol::numberOfNodes() == symbols);
      ROSE_ASSERT(SgVarRefExp::numberOfNodes() == references);
      ROSE_ASSERT(SgIntVal::numberOfNodes() == literals);
      ROSE_ASSERT(body->unparseToString() == original);
    }
    LoopTransformInterface::set_aliasInfo(previous_alias);
    LoopTransformInterface::set_astInterfacePtr(previous);
    return;
  }
  ROSE_ABORT();
}

void requireLoopTreeHeaderScopes(SgProject *project) {
  struct EditTree : LoopTreeTransform {
    using LoopTreeTransform::UnlinkNode;
  };
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *function = isSgFunctionDefinition(node);
    if (function->get_declaration()->get_name() !=
        "rex_loop_tree_header_scopes")
      continue;
    auto *body = function->get_body();
    const auto original = body->unparseToString();
    AstInterfaceImpl implementation(body);
    AstInterface interface(&implementation);
    auto *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    for (int upper : {5, 2}) {
      LoopTreeDepCompCreate computation(body, false);
      LoopTreeTraverseSelectLoop loops(computation.GetLoopTreeRoot());
      ROSE_ASSERT(!loops.ReachEnd());
      auto *loop = loops.Current();
      loop->GetLoopInfo()->GetBound().lb = SymbolicVal(2);
      loop->GetLoopInfo()->GetBound().ub = SymbolicVal(upper);
      loop->GetLoopInfo()->GetStep() = SymbolicVal(2);
      loop->CloneTree()->Link(loop, LoopTreeNode::AsNextSibling);
      std::unique_ptr<SgBasicBlock, DeleteCopy> emitted(
          isSgBasicBlock(computation.CodeGen().get_ptr()));
      const auto copies =
          NodeQuery::querySubTree(emitted.get(), V_SgForStatement);
      ROSE_ASSERT(copies.size() == 2);
      SgInitializedName *previous_index = nullptr;
      for (auto *copy_node : copies) {
        auto *copy = isSgForStatement(copy_node);
        auto *declaration =
            isSgVariableDeclaration(copy->get_init_stmt().front());
        ROSE_ASSERT(declaration != nullptr &&
                    declaration->get_variables().size() == 1);
        auto *index = declaration->get_variables().front();
        ROSE_ASSERT(index != previous_index && index->get_scope() == copy);
        previous_index = index;
        auto *initializer = isSgAssignInitializer(index->get_initializer());
        ROSE_ASSERT(isSgIntVal(initializer->get_operand())->get_value() == 2);
        auto *test = isSgLessOrEqualOp(copy->get_test_expr());
        ROSE_ASSERT(test != nullptr);
        ROSE_ASSERT(isSgIntVal(test->get_rhs_operand())->get_value() == upper);
        ROSE_ASSERT(
            isSgIntVal(
                isSgPlusAssignOp(copy->get_increment())->get_rhs_operand())
                ->get_value() == 2);
        for (auto *reference_node :
             NodeQuery::querySubTree(copy, V_SgVarRefExp)) {
          auto *reference = isSgVarRefExp(reference_node);
          auto *binding = reference->get_symbol()->get_declaration();
          if (binding->get_name() == "index")
            ROSE_ASSERT(binding == index);
        }
      }
    }
    {
      LoopTreeDepCompCreate computation(body, false);
      LoopTreeTraverseSelectLoop loops(computation.GetLoopTreeRoot());
      ROSE_ASSERT(!loops.ReachEnd());
      auto *loop = loops.Current();
      auto *scope = loop->FirstChild();
      ROSE_ASSERT(scope != nullptr && scope->NextSibling() == nullptr &&
                  scope->GetBodyRole() == LoopTreeNode::BodyRole::Block);
      auto *branch = scope->FirstChild();
      ROSE_ASSERT(branch != nullptr && branch->NextSibling() == nullptr);
      auto *arm = branch->FirstChild();
      ROSE_ASSERT(arm != nullptr &&
                  arm->GetBodyRole() == LoopTreeNode::BodyRole::IfTrue);
      auto *armGuard =
          new LoopTreeRestrLoopRange(loop, 0, SymbolicVal(2), SymbolicVal(3));
      EditTree().UnlinkNode(arm);
      armGuard->Link(branch, LoopTreeNode::AsLastChild);
      arm->Link(armGuard, LoopTreeNode::AsLastChild);
      auto *guard =
          new LoopTreeRestrLoopRange(loop, 0, SymbolicVal(2), SymbolicVal(4));
      EditTree().UnlinkNode(scope);
      guard->Link(loop, LoopTreeNode::AsLastChild);
      scope->Link(guard, LoopTreeNode::AsLastChild);
      std::unique_ptr<SgBasicBlock, DeleteCopy> emitted(
          isSgBasicBlock(computation.CodeGen().get_ptr()));
      const auto copies =
          NodeQuery::querySubTree(emitted.get(), V_SgForStatement);
      ROSE_ASSERT(copies.size() == 1);
      auto *copy = isSgForStatement(copies.front());
      auto *wrapper = isSgIfStmt(copy->get_loop_body());
      ROSE_ASSERT(wrapper != nullptr && wrapper->get_parent() == copy);
      auto *retained = isSgBasicBlock(wrapper->get_true_body());
      ROSE_ASSERT(retained != nullptr && retained->get_parent() == wrapper);
      ROSE_ASSERT(retained->get_statements().size() == 1);
      auto *conditional = isSgIfStmt(retained->get_statements().front());
      ROSE_ASSERT(conditional != nullptr &&
                  conditional->get_parent() == retained);
      auto *guardedArm = isSgIfStmt(conditional->get_true_body());
      ROSE_ASSERT(guardedArm != nullptr &&
                  guardedArm->get_parent() == conditional);
      auto *armBlock = isSgBasicBlock(guardedArm->get_true_body());
      ROSE_ASSERT(armBlock != nullptr && armBlock->get_parent() == guardedArm);
      auto *local = isSgVariableDeclaration(armBlock->get_statements().front());
      ROSE_ASSERT(local != nullptr &&
                  local->get_variables().front()->get_scope() == armBlock);
      auto *declaration =
          isSgVariableDeclaration(copy->get_init_stmt().front());
      ROSE_ASSERT(declaration != nullptr);
      auto *index = declaration->get_variables().front();
      ROSE_ASSERT(index->get_scope() == copy);
      for (auto *reference_node :
           NodeQuery::querySubTree(copy, V_SgVarRefExp)) {
        auto *binding =
            isSgVarRefExp(reference_node)->get_symbol()->get_declaration();
        if (binding->get_name() == "index")
          ROSE_ASSERT(binding == index);
      }
    }
    auto *parameter =
        function->get_declaration()->get_parameterList()->get_args().front();
    SymbolicVar induction(parameter->get_name(), parameter->get_scope(),
                          parameter);
    std::unique_ptr<SgStatement, DeleteCopy> prototype(
        SageBuilder::buildNullStatement());
    for (int step : {1, -1}) {
      LoopTreeCreate tree;
      auto *loop = tree.CreateLoopNode(induction, SymbolicVal(2),
                                       SymbolicVal(2), SymbolicVal(step));
      loop->Link(tree.GetTreeRoot(), LoopTreeNode::AsLastChild);
      tree.CreateStmtNode(prototype.get())
          ->Link(loop, LoopTreeNode::AsLastChild);
      std::unique_ptr<SgNode, DeleteCopy> emitted(tree.CodeGen().get_ptr());
      auto *counted = isSgForStatement(emitted.get());
      ROSE_ASSERT(counted != nullptr);
      auto *increment = isSgPlusAssignOp(counted->get_increment());
      ROSE_ASSERT(increment != nullptr);
      ROSE_ASSERT(isSgIntVal(increment->get_rhs_operand())->get_value() ==
                  step);
      ROSE_ASSERT(isSgVarRefExp(increment->get_lhs_operand())
                      ->get_symbol()
                      ->get_declaration() == parameter);
    }
    for (int origin : {0, -5}) {
      for (int step : {2, -2}) {
        std::unique_ptr<SgStatement, DeleteCopy> unrestricted(
            SageBuilder::buildNullStatement());
        LoopTreeCreate tree;
        auto *loop = tree.CreateLoopNode(
            induction, SymbolicVal(origin + (step > 0 ? 0 : 5)),
            SymbolicVal(origin + (step > 0 ? 5 : 0)), SymbolicVal(step));
        loop->Link(tree.GetTreeRoot(), LoopTreeNode::AsLastChild);
        auto *guard = new LoopTreeRestrLoopRange(loop, 0, SymbolicVal(),
                                                 SymbolicVal(origin + 2));
        guard->Link(loop, LoopTreeNode::AsLastChild);
        tree.CreateStmtNode(prototype.get())
            ->Link(guard, LoopTreeNode::AsLastChild);
        tree.CreateStmtNode(unrestricted.get())
            ->Link(loop, LoopTreeNode::AsLastChild);
        ApplyLoopSplitting(tree.GetTreeRoot());
        std::map<bool, std::vector<int>> visits;
        for (LoopTreeTraverseSelectStmt statements(tree.GetTreeRoot());
             !statements.ReachEnd(); ++statements) {
          auto *statement = statements.Current();
          auto *parent = statement->Parent();
          while (!parent->IsLoop())
            parent = parent->Parent();
          const auto *info = parent->GetLoopInfo();
          int begin, end;
          ROSE_ASSERT(info->GetLoopLB().isConstInt(begin) &&
                      info->GetLoopUB().isConstInt(end) &&
                      info->GetStep() == SymbolicVal(step));
          const bool restricted = statement->GetSourceAst() == prototype.get();
          ROSE_ASSERT(restricted ||
                      statement->GetSourceAst() == unrestricted.get());
          for (int value = begin; step > 0 ? value <= end : value >= end;
               value += step)
            visits[restricted].push_back(value);
        }
        const std::vector<int> all =
            step > 0 ? std::vector<int>{origin, origin + 2, origin + 4}
                     : std::vector<int>{origin + 5, origin + 3, origin + 1};
        const std::vector<int> restricted =
            step > 0 ? std::vector<int>{origin, origin + 2}
                     : std::vector<int>{origin + 1};
        ROSE_ASSERT(visits[false] == all && visits[true] == restricted);
        for (LoopTreeTraverse nodes(tree.GetTreeRoot()); !nodes.ReachEnd();
             nodes.Advance())
          ROSE_ASSERT(dynamic_cast<LoopTreeRestrLoopRange *>(nodes.Current()) ==
                      nullptr);
        std::unique_ptr<SgNode, DeleteCopy> emitted(tree.CodeGen().get_ptr());
        auto copies = NodeQuery::querySubTree(emitted.get(), V_SgForStatement);
        ROSE_ASSERT(copies.size() == 2);
        std::vector<int> generated;
        for (auto *copy : copies) {
          SymbolicVal lower, upper, increment;
          ROSE_ASSERT(SymbolicValGenerator::IsFortranLoop(
              interface, copy, nullptr, &lower, &upper, &increment));
          int begin, end;
          ROSE_ASSERT(lower.isConstInt(begin) && upper.isConstInt(end) &&
                      increment == SymbolicVal(step));
          for (int value = begin; step > 0 ? value <= end : value >= end;
               value += step)
            generated.push_back(value);
        }
        ROSE_ASSERT(generated == all);
      }
    }
    for (auto variant : {V_SgWhileStmt, V_SgDoWhileStmt}) {
      const auto loops = NodeQuery::querySubTree(body, variant);
      ROSE_ASSERT(loops.size() == 1);
      for (auto *loop : loops) {
        LoopInfo info(loop);
        ROSE_ASSERT(!info.ReverseEnum());
      }
    }
    ROSE_ASSERT(body->unparseToString() == original);
    LoopTransformInterface::set_astInterfacePtr(previous);
    return;
  }
  ROSE_ABORT();
}

void requireGenericLoopHeaders(SgProject *project) {
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *function = isSgFunctionDefinition(node);
    if (function->get_declaration()->get_name() !=
        "rex_loop_tree_generic_headers")
      continue;
    auto *body = function->get_body();
    const auto original = body->unparseToString();
    AstInterfaceImpl implementation(body);
    AstInterface interface(&implementation);
    auto *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    {
      LoopTreeDepCompCreate computation(body, false, true);
      LoopTreeTraverseSelectLoop loops(computation.GetLoopTreeRoot());
      ROSE_ASSERT(!loops.ReachEnd());
      auto *first = loops.Current();
      ROSE_ASSERT(isSgForStatement(first->GetSourceAst().get_ptr()) != nullptr);
      first->CloneTree()->Link(first, LoopTreeNode::AsNextSibling);
      std::unique_ptr<SgBasicBlock, DeleteCopy> emitted(
          isSgBasicBlock(computation.CodeGen().get_ptr()));
      const auto copies =
          NodeQuery::querySubTree(emitted.get(), V_SgForStatement);
      ROSE_ASSERT(copies.size() == 2);
      SgInitializedName *previous_index = nullptr, *previous_selected = nullptr;
      for (auto *copy_node : copies) {
        auto *copy = isSgForStatement(copy_node);
        auto *init = isSgVariableDeclaration(copy->get_init_stmt().front());
        auto *test = isSgVariableDeclaration(copy->get_test());
        ROSE_ASSERT(init != nullptr && test != nullptr);
        auto *index = init->get_variables().front();
        auto *selected = test->get_variables().front();
        ROSE_ASSERT(index != previous_index && selected != previous_selected);
        ROSE_ASSERT(index->get_scope() == copy &&
                    selected->get_scope() == copy);
        previous_index = index;
        previous_selected = selected;
        ROSE_ASSERT(isSgPlusPlusOp(copy->get_increment()) != nullptr);
        for (auto *ref_node : NodeQuery::querySubTree(copy, V_SgVarRefExp)) {
          auto *basis =
              isSgVarRefExp(ref_node)->get_symbol()->get_declaration();
          if (basis->get_name() == "index")
            ROSE_ASSERT(basis == index);
          if (basis->get_name() == "selected")
            ROSE_ASSERT(basis == selected);
        }
      }
      const auto while_nodes =
          NodeQuery::querySubTree(emitted.get(), V_SgWhileStmt);
      ROSE_ASSERT(while_nodes.size() == 1);
      auto *while_loop = isSgWhileStmt(while_nodes.front());
      auto *condition = isSgVariableDeclaration(while_loop->get_condition());
      ROSE_ASSERT(condition != nullptr);
      auto *selected = condition->get_variables().front();
      ROSE_ASSERT(selected->get_scope() == while_loop);
      for (auto *ref_node :
           NodeQuery::querySubTree(while_loop->get_body(), V_SgVarRefExp)) {
        auto *basis = isSgVarRefExp(ref_node)->get_symbol()->get_declaration();
        if (basis->get_name() == "selected")
          ROSE_ASSERT(basis == selected);
      }
      const auto do_nodes =
          NodeQuery::querySubTree(emitted.get(), V_SgDoWhileStmt);
      ROSE_ASSERT(do_nodes.size() == 1);
      auto *post_test = isSgDoWhileStmt(do_nodes.front());
      auto *false_value = isSgBoolValExp(
          isSgExprStatement(post_test->get_condition())->get_expression());
      ROSE_ASSERT(false_value != nullptr && !false_value->get_value());
      ROSE_ASSERT(
          isSgBasicBlock(post_test->get_body())->get_statements().size() == 1);
      const auto source_do = NodeQuery::querySubTree(body, V_SgDoWhileStmt);
      auto *source_condition =
          isSgExprStatement(isSgDoWhileStmt(source_do.front())->get_condition())
              ->get_expression();
      std::unique_ptr<SgBoolValExp, DeleteCopy> replacement(
          SageBuilder::buildBoolValExp(true));
      LoopTreeCreate transformed;
      auto *recipe = new LoopTreeReplAst(source_condition, replacement.get());
      recipe->Link(transformed.GetTreeRoot(), LoopTreeNode::AsLastChild);
      for (LoopTreeTraverseSelectLoop loop(computation.GetLoopTreeRoot());
           !loop.ReachEnd(); loop.Advance()) {
        if (isSgDoWhileStmt(loop.Current()->GetSourceAst().get_ptr()) !=
            nullptr)
          loop.Current()->CloneTree()->Link(recipe, LoopTreeNode::AsLastChild);
      }
      std::unique_ptr<SgDoWhileStmt, DeleteCopy> changed(
          isSgDoWhileStmt(transformed.CodeGen().get_ptr()));
      ROSE_ASSERT(changed != nullptr);
      auto *true_value = isSgBoolValExp(
          isSgExprStatement(changed->get_condition())->get_expression());
      ROSE_ASSERT(true_value != nullptr && true_value->get_value());
      ROSE_ASSERT(replacement->get_parent() == nullptr);
    }
    ROSE_ASSERT(body->unparseToString() == original);
    LoopTransformInterface::set_astInterfacePtr(previous);
    return;
  }
  ROSE_ABORT();
}

void requirePerfectLoopNests(SgProject *project) {
  const std::map<std::string, bool> expected{
      {"rex_loop_tree_perfect_nest", true},
      {"rex_loop_tree_split_nest", false},
      {"rex_loop_tree_conditional_nest", false}};
  unsigned checked = 0;
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *function = isSgFunctionDefinition(node);
    auto expectation =
        expected.find(function->get_declaration()->get_name().getString());
    if (expectation == expected.end())
      continue;
    AstInterfaceImpl implementation(function->get_body());
    AstInterface interface(&implementation);
    auto *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    {
      LoopTreeDepCompCreate computation(function->get_body(), false);
      LoopTreeTraverseSelectLoop loops(computation.GetLoopTreeRoot());
      ROSE_ASSERT(!loops.ReachEnd());
      auto *outer = loops.Current();
      ROSE_ASSERT(outer->IsPerfectLoopNest() == expectation->second);
      LoopTreeCreate cloned;
      outer->CloneTree()->Link(cloned.GetTreeRoot(), LoopTreeNode::AsLastChild);
      ROSE_ASSERT(cloned.GetTreeRoot()->IsPerfectLoopNest() ==
                  expectation->second);
    }
    LoopTransformInterface::set_astInterfacePtr(previous);
    ++checked;
  }
  ROSE_ASSERT(checked == expected.size());
}

void requireSiblingLoopSplitting(SgProject *project) {
  struct EditTree : LoopTreeTransform {
    using LoopTreeTransform::UnlinkNode;
  };
  unsigned checked = 0;
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *function = isSgFunctionDefinition(node);
    const auto name = function->get_declaration()->get_name();
    const bool scoped = name == "rex_loop_tree_split_declarations";
    if (name != "rex_loop_tree_split_siblings" && !scoped)
      continue;
    auto *body = function->get_body();
    const auto original = body->unparseToString();
    AstInterfaceImpl implementation(body);
    AstInterface interface(&implementation);
    auto *previous = LoopTransformInterface::get_astInterface();
    LoopTransformInterface::set_astInterface(interface);
    auto *previousAlias = LoopTransformInterface::getAliasInfo();
    StmtVarAliasCollect aliases;
    aliases.analyze(interface, function);
    LoopTransformInterface::set_aliasInfo(&aliases);
    {
      LoopTreeDepCompCreate computation(body);
      auto *root = computation.GetLoopTreeRoot();
      unsigned position = 0;
      for (LoopTreeTraverseSelectLoop loops(root); !loops.ReachEnd(); ++loops) {
        auto *loop = loops.Current();
        auto *scope = loop->FirstChild();
        ROSE_ASSERT(scope != nullptr && scope->NextSibling() == nullptr);
        auto *statement = scope->FirstChild();
        ROSE_ASSERT(statement != nullptr && scope->ChildCount() == 2);
        auto *guard = new LoopTreeRestrLoopRange(
            loop, 0, SymbolicVal(position == 0 ? 2 : 4),
            SymbolicVal(position == 0 ? 3 : 4));
        guard->Link(statement, LoopTreeNode::AsPrevSibling);
        EditTree().UnlinkNode(statement);
        statement->Link(guard, LoopTreeNode::AsLastChild);
        if (scoped) {
          ROSE_ASSERT(isSgIfStmt(statement->GetSourceAst().get_ptr()) !=
                      nullptr);
          LoopTreeSplitStmt()(statement, loop,
                              SymbolicVal(position == 0 ? 1 : 3));
        }
        ++position;
      }
      ROSE_ASSERT(position == 2);
      ApplyLoopSplitting(root);
      auto *graph = computation.GetDepGraph();
      unsigned statements = 0, graphNodes = 0;
      for (LoopTreeTraverseSelectStmt nodes(root); !nodes.ReachEnd(); ++nodes) {
        auto *dependency = computation.GetDepNode(nodes.Current());
        ROSE_ASSERT(dependency != nullptr && graph->ContainNode(dependency));
        ROSE_ASSERT(dependency->GetInfo() == nodes.Current());
        ROSE_ASSERT(dependency->NumOfLoops() == 1 &&
                    dependency->GetDomain().NumOfLoops() == 1);
        ++statements;
      }
      for (auto nodes = graph->GetNodeIterator(); !nodes.ReachEnd(); ++nodes) {
        ROSE_ASSERT(computation.GetDepNode(nodes.Current()->GetInfo()) ==
                    nodes.Current());
        ++graphNodes;
      }
      ROSE_ASSERT(statements == (scoped ? 12 : 8) && graphNodes == statements);
      for (LoopTreeTraverse nodes(root); !nodes.ReachEnd(); nodes.Advance())
        ROSE_ASSERT(dynamic_cast<LoopTreeRestrLoopRange *>(nodes.Current()) ==
                    nullptr);
      std::unique_ptr<SgBasicBlock, DeleteCopy> emitted(
          isSgBasicBlock(computation.CodeGen().get_ptr()));
      unsigned writes = 0;
      std::map<bool, std::vector<std::pair<int, int>>> intervals;
      const auto &parameters =
          function->get_declaration()->get_parameterList()->get_args();
      for (auto *copy_node :
           NodeQuery::querySubTree(emitted.get(), V_SgForStatement)) {
        auto *copy = isSgForStatement(copy_node);
        auto *declaration =
            isSgVariableDeclaration(copy->get_init_stmt().front());
        ROSE_ASSERT(declaration != nullptr);
        auto *index = declaration->get_variables().front();
        auto *initializer = isSgAssignInitializer(index->get_initializer());
        const int lower = isSgIntVal(initializer->get_operand())->get_value();
        auto *test = isSgLessOrEqualOp(copy->get_test_expr());
        ROSE_ASSERT(test != nullptr);
        const int upper = isSgIntVal(test->get_rhs_operand())->get_value();
        ROSE_ASSERT(index->get_scope() == copy);
        for (auto *assignment_node :
             NodeQuery::querySubTree(copy->get_loop_body(), V_SgPlusAssignOp)) {
          auto *assignment = isSgPlusAssignOp(assignment_node);
          auto *access = isSgPntrArrRefExp(assignment->get_lhs_operand());
          ROSE_ASSERT(access != nullptr);
          auto arrays =
              NodeQuery::querySubTree(access->get_lhs_operand(), V_SgVarRefExp);
          ROSE_ASSERT(arrays.size() == 1);
          auto *array =
              isSgVarRefExp(arrays.front())->get_symbol()->get_declaration();
          ROSE_ASSERT(array == parameters[0] || array == parameters[1]);
          const bool first = array == parameters[0];
          auto indices =
              NodeQuery::querySubTree(access->get_rhs_operand(), V_SgVarRefExp);
          ROSE_ASSERT(
              indices.size() == 1 &&
              isSgVarRefExp(indices.front())->get_symbol()->get_declaration() ==
                  index);
          auto values = NodeQuery::querySubTree(assignment->get_rhs_operand(),
                                                V_SgVarRefExp);
          if (values.empty()) {
            ROSE_ASSERT(
                isSgIntVal(assignment->get_rhs_operand())->get_value() == 1);
            intervals[first].emplace_back(lower, upper);
          } else {
            ROSE_ASSERT(values.size() == 1);
            auto *binding =
                isSgVarRefExp(values.front())->get_symbol()->get_declaration();
            if (scoped) {
              auto declarations = NodeQuery::querySubTree(
                  copy->get_loop_body(), V_SgVariableDeclaration);
              ROSE_ASSERT(declarations.size() == 1);
              auto *local = isSgVariableDeclaration(declarations.front());
              ROSE_ASSERT(local->get_variables().front() == binding);
              auto *scope = isSgBasicBlock(local->get_parent());
              ROSE_ASSERT(scope != nullptr && binding->get_scope() == scope);
              ROSE_ASSERT(scope->get_statements().size() == 2 &&
                          scope->get_statements().front() == local);
              auto references = NodeQuery::querySubTree(
                  binding->get_initializer(), V_SgVarRefExp);
              ROSE_ASSERT(references.size() == 1 &&
                          isSgVarRefExp(references.front())
                                  ->get_symbol()
                                  ->get_declaration() == index);
            } else {
              ROSE_ASSERT(binding == index);
            }
            ROSE_ASSERT(lower == (first ? 2 : 4) && upper == (first ? 3 : 4));
            ++writes;
          }
        }
      }
      ROSE_ASSERT(writes == 2);
      for (auto &entry : intervals)
        std::sort(entry.second.begin(), entry.second.end());
      const std::vector<std::pair<int, int>> first{{0, 1}, {2, 3}, {4, 5}};
      const std::vector<std::pair<int, int>> second{{0, 3}, {4, 4}, {5, 5}};
      ROSE_ASSERT(intervals[true] == first && intervals[false] == second);
    }
    ROSE_ASSERT(body->unparseToString() == original);
    LoopTransformInterface::set_aliasInfo(previousAlias);
    LoopTransformInterface::set_astInterfacePtr(previous);
    ++checked;
  }
  ROSE_ASSERT(checked == 2);
}

void requireLoopDependenceSourceIdentity(SgInitializedName *variable) {
  struct ReorderTree : LoopTreeTransform {
    using LoopTreeTransform::RemoveNode;
    using LoopTreeTransform::UnlinkNode;
  };
  struct LeafShadowTree : LoopTreeShadowCreate {
    explicit LeafShadowTree(LoopTreeNode *node) : LoopTreeShadowCreate(0) {
      CreateShadowNode(node)->Link(GetTreeRoot(), LoopTreeNode::AsLastChild);
    }
  };
  auto *write = SageBuilder::buildExprStatement(SageBuilder::buildAssignOp(
      SageBuilder::buildVarRefExp(variable), SageBuilder::buildIntVal(1),
      variable->get_type()));
  auto *read =
      SageBuilder::buildExprStatement(SageBuilder::buildVarRefExp(variable));
  auto *body = SageBuilder::buildBasicBlock(write, read);
  AstInterfaceImpl implementation(body);
  AstInterface interface(&implementation);
  AstInterface *previous = LoopTransformInterface::get_astInterface();
  LoopTransformInterface::set_astInterface(interface);
  {
    LoopTreeDepCompCreate computation(body, false);
    auto *tree_root = computation.GetLoopTreeRoot();
    ROSE_ASSERT(tree_root->ChildCount() == 1);
    auto *root = tree_root->FirstChild();
    ROSE_ASSERT(root->GetBodyAst() == body);
    ROSE_ASSERT(root->GetBodyRole() == LoopTreeNode::BodyRole::Block);
    ROSE_ASSERT(root->GetSourceAst() == AST_NULL);
    ROSE_ASSERT(root->ChildCount() == 2);
    auto *write_node = root->FirstChild();
    auto *read_node = root->LastChild();
    ROSE_ASSERT(write_node->GetOrigStmt() == write);
    ROSE_ASSERT(read_node->GetOrigStmt() == read);
    {
      DepCompAstRefGraphCreate references;
      const AstNodePtr first(std::string("rex_first_reference"));
      const AstNodePtr second(std::string("rex_second_reference"));
      auto *first_reference = references.CreateNode(first, write_node);
      auto *second_reference = references.CreateNode(second, write_node);
      ROSE_ASSERT(first_reference != second_reference);
      ROSE_ASSERT(first_reference->GetInfo().orig == first);
      ROSE_ASSERT(second_reference->GetInfo().orig == second);
      ROSE_ASSERT(references.CreateNode(first, write_node) == first_reference);
      ROSE_ASSERT(references.CreateNode(first, read_node) != first_reference);
    }
    {
      LeafShadowTree shadow(write_node);
      ROSE_ASSERT(shadow.GetTreeRoot()->FirstChild()->GetSourceAst() == write);
      for (LoopTreeNode *node : {write_node, shadow.GetTreeRoot()}) {
        const size_t blocks = SgBasicBlock::numberOfNodes();
        const size_t symbols = SgSymbolTable::numberOfNodes();
        AstNodePtr emitted = node->CodeGen();
        ROSE_ASSERT(SgBasicBlock::numberOfNodes() == blocks);
        ROSE_ASSERT(SgSymbolTable::numberOfNodes() == symbols);
        auto *statement = isSgExprStatement(emitted.get_ptr());
        ROSE_ASSERT(statement != nullptr && statement != write);
        auto *assignment = isSgAssignOp(statement->get_expression());
        ROSE_ASSERT(assignment != nullptr);
        auto *reference = isSgVarRefExp(assignment->get_lhs_operand());
        ROSE_ASSERT(reference != nullptr &&
                    reference->get_symbol()->get_declaration() == variable);
        SageInterface::deleteAST(
            statement, SageInterface::DeleteAstMode::kRequireIsolated);
      }
    }
    {
      LoopTreeDepGraphCreate graph(computation.GetTreeNodeMap());
      auto *first = graph.CreateNode(write_node);
      auto *second = graph.CreateNode(read_node);
      const auto dependency =
          DepInfoGenerator::GetDepInfo(0, 0, DEPTYPE_TRUE, true, 0);
      graph.CreateEdge(first, first, dependency);
      CopySplitNodeEdge(&graph, first, second, DomainCond(0));
      for (auto *source : {first, second}) {
        for (auto *sink : {first, second}) {
          GraphCrossEdgeIterator<LoopTreeDepGraphCreate> edges(&graph, source,
                                                               sink);
          ROSE_ASSERT(!edges.ReachEnd());
          ROSE_ASSERT(edges.Current()->GetInfo() == dependency);
          ++edges;
          ROSE_ASSERT(edges.ReachEnd());
        }
      }
    }
    ROSE_ASSERT(computation.GetDepNode(write_node) == nullptr &&
                computation.GetDepNode(read_node) == nullptr);
    ReorderTree().UnlinkNode(write_node);
    write_node->Link(read_node, LoopTreeNode::AsNextSibling);
    computation.BuildDepGraph();
    auto *write_dependency = computation.GetDepNode(write_node);
    auto *read_dependency = computation.GetDepNode(read_node);
    ROSE_ASSERT(write_dependency != nullptr && read_dependency != nullptr);
    auto *graph = static_cast<LoopTreeDepGraphCreate *>(
        write_dependency->GetGraphCreate());
    ROSE_ASSERT(computation.GetDepGraph() == graph);
    bool reached = false;
    for (auto edges =
             graph->GetNodeEdgeIterator(write_dependency, GraphAccess::EdgeOut);
         !edges.ReachEnd(); ++edges) {
      auto *edge = edges.Current();
      ROSE_ASSERT(graph->GetEdgeEndPoint(edge, GraphAccess::EdgeIn) ==
                  read_dependency);
      ROSE_ASSERT(SageInterface::getEnclosingStatement(
                      edge->GetInfo().SrcRef().get_ptr()) == write);
      ROSE_ASSERT(SageInterface::getEnclosingStatement(
                      edge->GetInfo().SnkRef().get_ptr()) == read);
      reached = true;
    }
    ROSE_ASSERT(reached);
    LoopTreeLocalityAnal locality(computation);
    ROSE_ASSERT(locality.GetInputGraph()->ContainNode(write_dependency));
    ROSE_ASSERT(locality.GetInputGraph()->ContainNode(read_dependency));
    ReorderTree().RemoveNode(write_node);
    ROSE_ASSERT(computation.GetDepNode(write_node) == nullptr);
    ROSE_ASSERT(computation.GetDepNode(read_node) == read_dependency);
    ROSE_ASSERT(graph->ContainNode(read_dependency));
    ROSE_ASSERT(locality.GetInputGraph()->ContainNode(read_dependency));
    for (auto *remaining :
         {computation.GetDepGraph(), locality.GetInputGraph()}) {
      for (auto nodes = remaining->GetNodeIterator(); !nodes.ReachEnd();
           ++nodes)
        ROSE_ASSERT(nodes.Current()->GetInfo() == read_node);
    }
  }
  LoopTransformInterface::set_astInterfacePtr(previous);
  SageInterface::deleteAST(body,
                           SageInterface::DeleteAstMode::kRequireIsolated);
}

void requireLoopDependenceLoopBoundary() {
  auto *loop = SageBuilder::buildWhileStmt(
      SageBuilder::buildExprStatement(SageBuilder::buildBoolValExp(false)),
      SageBuilder::buildBasicBlock(
          SageBuilder::buildExprStatement(SageBuilder::buildIntVal(1))));
  auto *inside = SageBuilder::buildExprStatement(SageBuilder::buildIntVal(2));
  auto *branch = SageBuilder::buildIfStmt(
      SageBuilder::buildExprStatement(SageBuilder::buildBoolValExp(true)),
      SageBuilder::buildBasicBlock(loop, inside), nullptr);
  auto *outside = SageBuilder::buildExprStatement(SageBuilder::buildIntVal(3));
  auto *body = SageBuilder::buildBasicBlock(branch, outside);
  AstInterfaceImpl implementation(body);
  AstInterface interface(&implementation);
  AstInterface *previous = LoopTransformInterface::get_astInterface();
  LoopTransformInterface::set_astInterface(interface);
  {
    LoopTreeDepCompCreate computation(body);
    auto *tree_root = computation.GetLoopTreeRoot();
    ROSE_ASSERT(tree_root->ChildCount() == 1);
    auto *root = tree_root->FirstChild();
    ROSE_ASSERT(root->GetBodyAst() == body);
    ROSE_ASSERT(root->GetBodyRole() == LoopTreeNode::BodyRole::Block);
    ROSE_ASSERT(root->GetSourceAst() == AST_NULL);
    ROSE_ASSERT(root->ChildCount() == 2);
    auto *branch_node = root->FirstChild();
    ROSE_ASSERT(branch_node->GetSourceAst() == branch);
    ROSE_ASSERT(branch_node->ChildCount() == 1);
    auto *arm = branch_node->FirstChild();
    ROSE_ASSERT(arm->GetBodyRole() == LoopTreeNode::BodyRole::IfTrue);
    ROSE_ASSERT(arm->GetBodyAst() == branch->get_true_body());
    ROSE_ASSERT(arm->GetSourceAst() == AST_NULL);
    ROSE_ASSERT(arm->ChildCount() == 2);
    ROSE_ASSERT(arm->FirstChild()->GetOrigStmt() == loop);
    ROSE_ASSERT(arm->FirstChild()->ChildCount() == 0);
    ROSE_ASSERT(arm->LastChild()->GetOrigStmt() == inside);
    ROSE_ASSERT(root->LastChild()->GetOrigStmt() == outside);
    ROSE_ASSERT(computation.GetDepNode(arm) == nullptr);
    ROSE_ASSERT(computation.GetDepNode(arm->FirstChild()) != nullptr);
    ROSE_ASSERT(computation.GetDepNode(arm->LastChild()) != nullptr);
    ROSE_ASSERT(computation.GetDepNode(root->LastChild()) != nullptr);
    {
      LoopTreeLocalityAnal locality(computation);
      for (auto nodes = computation.GetDepGraph()->GetNodeIterator();
           !nodes.ReachEnd(); ++nodes)
        ROSE_ASSERT(locality.GetInputGraph()->ContainNode(nodes.Current()));
    }
    computation.DetachDepGraph();
    for (LoopTreeTraverseSelectStmt nodes(root); !nodes.ReachEnd(); ++nodes)
      ROSE_ASSERT(computation.GetDepNode(nodes.Current()) == nullptr);
    computation.BuildDepGraph();
    for (LoopTreeTraverseSelectStmt nodes(root); !nodes.ReachEnd(); ++nodes) {
      auto *dependency = computation.GetDepNode(nodes.Current());
      ROSE_ASSERT(dependency != nullptr);
      ROSE_ASSERT(computation.GetDepGraph()->ContainNode(dependency));
      ROSE_ASSERT(dependency->GetInfo() == nodes.Current());
    }
    {
      LoopTreeLocalityAnal locality(computation);
      computation.DetachDepGraph();
      ROSE_ASSERT(computation.GetDepGraph() == nullptr);
      computation.BuildDepGraph();
      for (auto nodes = computation.GetDepGraph()->GetNodeIterator();
           !nodes.ReachEnd(); ++nodes) {
        auto *dependency = nodes.Current();
        ROSE_ASSERT(computation.GetDepNode(dependency->GetInfo()) ==
                    dependency);
        ROSE_ASSERT(locality.GetInputGraph()->ContainNode(dependency));
      }
    }
  }
  LoopTransformInterface::set_astInterfacePtr(previous);
  SageInterface::deleteAST(body,
                           SageInterface::DeleteAstMode::kRequireIsolated);
}

void requireTrackingNodeLifetimes(SgProject *project) {
  const auto literals = NodeQuery::querySubTree(project, V_SgIntVal);
  ROSE_ASSERT(!literals.empty());
  auto *source = isSgIntVal(literals.front());
  SgTreeCopy input_copy;
  auto *input = isSgIntVal(source->copy(input_copy));
  ROSE_ASSERT(input != nullptr);
  auto *output = SageInterface::deepCopy(source);
  TransformationTracking::registerAstSubtreeIds(input);
  TransformationTracking::registerAstSubtreeIds(output);
  const AST_NODE_ID input_id = TransformationTracking::getId(input);
  const AST_NODE_ID output_id = TransformationTracking::getId(output);
  ROSE_ASSERT(input_id != 0 && output_id != input_id);
  TransformationTracking::addInputNode(output, input);
  const auto origin = TransformationTracking::getFileInfo(input_id);
  ROSE_ASSERT(origin.first != nullptr && origin.second != nullptr);
  ROSE_ASSERT(origin.first != input->get_startOfConstruct());
  ROSE_ASSERT(origin.first->get_filenameString() ==
              source->get_startOfConstruct()->get_filenameString());
  ROSE_ASSERT(origin.first->get_line() ==
              source->get_startOfConstruct()->get_line());

  SageInterface::deleteAST(input);
  ROSE_ASSERT(TransformationTracking::getNode(input_id) == nullptr);
  auto *replacement = SageInterface::deepCopy(source);
  ROSE_ASSERT(replacement == input);
  ROSE_ASSERT(TransformationTracking::getId(replacement) == 0);
  TransformationTracking::registerAstSubtreeIds(replacement);
  ROSE_ASSERT(TransformationTracking::getId(replacement) > output_id);
  ROSE_ASSERT(TransformationTracking::getNode(input_id) == nullptr);
  ROSE_ASSERT(TransformationTracking::getFileInfo(input_id) == origin);
  ROSE_ASSERT(SgNode::isLiveNode(origin.first) &&
              SgNode::isLiveNode(origin.second));
  ROSE_ASSERT(TransformationTracking::inputIDs.at(output_id).count(input_id) ==
              1);
  SageInterface::deleteAST(replacement);
  SageInterface::deleteAST(output);
  ROSE_ASSERT(TransformationTracking::getNode(output_id) == nullptr);
}

bool requireDependenceTokenBoundaries() {
  std::istringstream filteredRecords(
      "keep : [ read ] input ; blocked : [ read ] input ; "
      "keep : [ read ] blocked ; keep : [ ignored ] input ; "
      "other_blocked : [ read ] input ;");
  AstUtilInterface::SelectDependences filtered;
  filtered.add_selection("blocked", "blocked", "ignored");
  filtered.add_selection("other_blocked");
  filtered.CollectFromFile(filteredRecords);
  ROSE_ASSERT(filtered.get_result().size() == 1);
  ROSE_ASSERT(filtered.get_result().front().first_entry() == "keep");
  ROSE_ASSERT(filtered.get_result().front().second_entry() == "input");
  ROSE_ASSERT(filtered.get_result().front().type_entry() == "read");

  // Separate records can contribute independent attributes to one node.
  std::istringstream nodeRecords("node : [ first ] ; node : [ second ] ;");
  AstUtilInterface::DependenceTable nodeTable(false);
  nodeTable.CollectFromFile(nodeRecords);
  ROSE_ASSERT(nodeTable.get_nodeInfo("node") ==
              (std::vector<std::string>{"first", "second"}));

  const std::vector<std::string> operator_names = {
      "operator[]",        "operator()",      "operator new",
      "operator new[]",    "operator delete", "operator delete[]",
      "operator co_await", "operator+",       "operator-",
      "operator*",         "operator/",       "operator%",
      "operator^",         "operator&",       "operator|",
      "operator~",         "operator!",       "operator=",
      "operator<",         "operator>",       "operator+=",
      "operator-=",        "operator*=",      "operator/=",
      "operator%=",        "operator^=",      "operator&=",
      "operator|=",        "operator<<",      "operator>>",
      "operator>>=",       "operator<<=",     "operator==",
      "operator!=",        "operator<=",      "operator>=",
      "operator<=>",       "operator&&",      "operator||",
      "operator++",        "operator--",      "operator,",
      "operator->*",       "operator->",      "operator bool",
      "operator\"\"_tag",  "rex::operator[]", "rex::operator=="};

  std::ostringstream serialized;
  for (const std::string &name : operator_names) {
    serialized << AstUtilInterface::DependenceEntry(name, "source", "call",
                                                    "exact")
               << '\n';
  }
  serialized << AstUtilInterface::DependenceEntry("cooperator", "source",
                                                  "read", "exact")
             << '\n'
             << AstUtilInterface::DependenceEntry("target", "cooperator",
                                                  "read", "exact")
             << '\n';

  std::istringstream input(serialized.str());
  AstUtilInterface::SelectDependences dependences;
  dependences.CollectFromFile(input);

  const std::vector<AstUtilInterface::DependenceEntry> &entries =
      dependences.get_result();
  const auto contains = [&entries](const std::string &first,
                                   const std::string &second,
                                   const std::string &type) {
    for (const AstUtilInterface::DependenceEntry &entry : entries) {
      if (entry.first_entry() == first && entry.second_entry() == second &&
          entry.type_entry() == type && entry.attr_entry() == "exact")
        return true;
    }
    return false;
  };
  bool complete = entries.size() == operator_names.size() + 2 &&
                  contains("cooperator", "source", "read") &&
                  contains("target", "cooperator", "read");
  for (const std::string &name : operator_names)
    complete = complete && contains(name, "source", "call");
  if (complete)
    return true;

  std::cerr << "dependence-table operator and identifier boundaries did not "
               "round-trip exactly"
            << std::endl;
  return false;
}

void requireSignatureDictionaryMetadata(SgProject *project) {
  SgVarRefExp *reference = nullptr;
  RoseAst tree(project);
  for (auto node = tree.begin(); node != tree.end(); ++node)
    if (auto *candidate = isSgVarRefExp(*node))
      if (candidate->get_symbol()->get_name() == "source") {
        reference = candidate;
        break;
      }
  ROSE_ASSERT(reference != nullptr);
  AstInterfaceImpl implementation(reference);
  AstInterface ast(&implementation);
  std::string filename;
  int line = -1;
  ROSE_ASSERT(ast.get_fileInfo(reference, &filename, &line));
  AstUtilInterface::SetSaveVariableDictionary(true);
  const std::string signature =
      AstUtilInterface::GetVariableSignature(reference);
  AstUtilInterface::SetSaveVariableDictionary(false);
  std::ostringstream output;
  AstUtilInterface::OutputSignatureDictionary(output);
  std::istringstream input(output.str());
  AstUtilInterface::SelectDependences metadata;
  metadata.CollectFromFile(input);
  ROSE_ASSERT(metadata.get_result().size() == 2);
  bool sawLine = false, sawType = false;
  for (const auto &entry : metadata.get_result()) {
    ROSE_ASSERT(entry.first_entry() == signature);
    ROSE_ASSERT(entry.second_entry() == filename);
    sawLine |= entry.type_entry() == "line" &&
               entry.attr_entry() == std::to_string(line);
    sawType |= entry.type_entry() == "type" && entry.attr_entry() == "int";
  }
  ROSE_ASSERT(sawLine && sawType);
}

bool requireInitializationInput(const std::string &dependences,
                                const std::string &target,
                                const std::string &input) {
  const std::string expected = target + " : [ read ] " + input + " = init ;";
  if (dependences.find(expected) != std::string::npos)
    return true;
  std::cerr << "missing global initializer input dependence '" << expected
            << "':\n"
            << dependences;
  return false;
}

bool rejectInitializationTarget(const std::string &dependences,
                                const std::string &target) {
  const std::string unexpected = target + " : [ read ] ";
  if (dependences.find(unexpected) == std::string::npos)
    return true;
  std::cerr << "local initializer escaped into the whole-program dependence "
               "table as '"
            << unexpected << "':\n"
            << dependences;
  return false;
}

SgVariableDeclaration *findVariableDeclaration(SgProject *project,
                                               const std::string &name) {
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(*node);
    if (declaration != nullptr && declaration->get_variables().size() == 1 &&
        declaration->get_variables().front()->get_name().getString() == name)
      return declaration;
  }
  std::cerr << "declaration '" << name << "' was not found" << std::endl;
  return nullptr;
}

SgTemplateVariableDeclaration *
findTemplateVariableDeclaration(SgProject *project, const std::string &name) {
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgTemplateVariableDeclaration *declaration =
        isSgTemplateVariableDeclaration(*node);
    if (declaration != nullptr && declaration->get_variables().size() == 1 &&
        declaration->get_variables().front()->get_name().getString() == name)
      return declaration;
  }
  std::cerr << "template variable declaration '" << name << "' was not found"
            << std::endl;
  return nullptr;
}

bool requireExactDeclarationEffects(SgVariableDeclaration *declaration) {
  ASSERT_not_null(declaration);
  ASSERT_require(declaration->get_variables().size() == 1);
  SgInitializedName *target = declaration->get_variables().front();
  ASSERT_not_null(target);

  AstInterface::AstNodeList variables;
  AstInterface::AstNodeList initializers;
  if (!AstInterface::IsVariableDecl(declaration, &variables, &initializers) ||
      variables.size() != 1 || initializers.size() != 1 ||
      AstNodePtrImpl(variables.front()).get_ptr() != target) {
    std::cerr << "declaration list for '" << target->get_name().getString()
              << "' contains aggregate designators or lost its exact target"
              << std::endl;
    return false;
  }

  bool saw_target_declaration = false;
  bool saw_spurious_declaration = false;
  std::function<bool(const AstNodePtr &, const AstNodePtr &,
                     AstUtilInterface::OperatorSideEffect)>
      collect = [target, &saw_target_declaration, &saw_spurious_declaration](
                    const AstNodePtr &node, const AstNodePtr &,
                    AstUtilInterface::OperatorSideEffect relation) {
        if (relation != AstUtilInterface::OperatorSideEffect::Decl)
          return true;
        if (AstNodePtrImpl(node).get_ptr() == target) {
          saw_target_declaration = true;
        } else {
          saw_spurious_declaration = true;
        }
        return true;
      };
  AstUtilInterface::ComputeAstSideEffects(declaration, &collect, nullptr);
  if (!saw_target_declaration || saw_spurious_declaration) {
    std::cerr << "declaration effects for '" << target->get_name().getString()
              << "' did not contain exactly the initialized object"
              << std::endl;
    return false;
  }
  return true;
}

bool requireExactEffects(SgNode *root,
                         AstUtilInterface::OperatorSideEffect effect,
                         std::vector<SgNode *> expected) {
  bool unexpected = false;
  std::function<bool(const AstNodePtr &, const AstNodePtr &,
                     AstUtilInterface::OperatorSideEffect)>
      collect = [effect, &expected,
                 &unexpected](const AstNodePtr &node, const AstNodePtr &,
                              AstUtilInterface::OperatorSideEffect relation) {
        if ((effect == AstUtilInterface::OperatorSideEffect::Read &&
             relation == AstUtilInterface::OperatorSideEffect::ReadUnknown) ||
            (effect == AstUtilInterface::OperatorSideEffect::Modify &&
             relation == AstUtilInterface::OperatorSideEffect::ModifyUnknown)) {
          unexpected = true;
        } else if (relation == effect) {
          SgVarRefExp *reference = isSgVarRefExp(node.get_ptr());
          SgNode *identity = reference != nullptr
                                 ? reference->get_symbol()->get_declaration()
                                 : node.get_ptr();
          const auto found =
              std::find(expected.begin(), expected.end(), identity);
          if (found == expected.end()) {
            unexpected = true;
          } else {
            expected.erase(found);
          }
        }
        return true;
      };
  AstUtilInterface::ComputeAstSideEffects(root, &collect, nullptr);
  if (!unexpected && expected.empty())
    return true;
  std::cerr << AstUtilInterface::OperatorSideEffectName(effect)
            << " effects did not match exact variable identities and counts\n";
  return false;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "--parse-dependence") {
    std::istringstream input(argv[2]);
    AstUtilInterface::SelectDependences dependences;
    dependences.CollectFromFile(input);
    return 0;
  }

  if (systemFromVector(
          {argv[0], "--parse-dependence", R"("valid name" : source ;)"}) != 0) {
    std::cerr << "valid quoted dependence token was rejected\n";
    return 1;
  }
  for (const char *input : {
           R"("unterminated)",
           R"("bad\q" : source ;)",
           R"(prefix"quoted" : source ;)",
       }) {
    if (systemFromVector({argv[0], "--parse-dependence", input}) !=
        128 + SIGABRT) {
      std::cerr << "malformed dependence token did not abort: " << input
                << '\n';
      return 1;
    }
  }

  if (!requireDependenceTokenBoundaries())
    return 1;

  SgProject *project = frontend(argc, argv);
  requireSignatureDictionaryMetadata(project);
  requireSliceReuseBindingIdentity(project);
  requireSpatialReuseBindingIdentity(project);
  requireSpatialReuseTargetLayout(project);
  requireOffsetOfSubscriptLayout(project);
  requireTrackingNodeLifetimes(project);
  ASSERT_not_null(project);

  SgTemplateVariableDeclaration *template_variable =
      findTemplateVariableDeclaration(project, "rex_template_variable");
  if (template_variable == nullptr ||
      !requireExactDeclarationEffects(template_variable))
    return 1;

  SgVariableDeclaration *designated_pair =
      findVariableDeclaration(project, "designated_pair");
  if (designated_pair == nullptr ||
      !requireExactDeclarationEffects(designated_pair))
    return 1;

  SgVariableDeclaration *source_declaration =
      findVariableDeclaration(project, "source");
  SgVariableDeclaration *delta_declaration =
      findVariableDeclaration(project, "delta");
  if (source_declaration == nullptr || delta_declaration == nullptr)
    return 1;
  SgInitializedName *source = source_declaration->get_variables().front();
  SgInitializedName *delta = delta_declaration->get_variables().front();
  requireLoopDependenceSourceIdentity(source);
  requireLoopDependenceLoopBoundary();
  requireLoopTreeIfScopes(project);
  requireLoopTreeHeaderScopes(project);
  requireGenericLoopHeaders(project);
  requirePerfectLoopNests(project);
  requireSiblingLoopSplitting(project);
  for (const auto &check :
       std::vector<std::pair<std::string, std::vector<SgNode *>>>{
           {"target", {source, delta}},
           {"rex_UNKNOWN_target", {source}},
           {"positional_pair", {source, delta, delta}},
           {"designated_pair", {source, delta}},
           {"nested_target", {source, delta}}}) {
    SgVariableDeclaration *declaration =
        findVariableDeclaration(project, check.first);
    if (declaration == nullptr ||
        !requireExactEffects(
            declaration->get_variables().front()->get_initializer(),
            AstUtilInterface::OperatorSideEffect::Read, check.second))
      return 1;
  }

  bool checked_memory_reads = false;
  SgVariableDeclaration *observed_declaration =
      findVariableDeclaration(project, "rex_observed");
  if (observed_declaration == nullptr)
    return 1;
  SgInitializedName *observed = observed_declaration->get_variables().front();
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    SgFunctionDefinition *definition = isSgFunctionDefinition(node);
    SgFunctionDeclaration *function = definition->get_declaration();
    if (function->get_name() != "rex_memory_read_controls")
      continue;
    const auto members = NodeQuery::querySubTree(definition, V_SgDotExp);
    const auto elements =
        NodeQuery::querySubTree(definition, V_SgPntrArrRefExp);
    ROSE_ASSERT(members.size() == 1 && elements.size() == 1 &&
                function->get_args().size() == 1 && !checked_memory_reads);
    SgInitializedName *index = function->get_args().front();
    if (!requireExactEffects(
            definition, AstUtilInterface::OperatorSideEffect::Read,
            {source, source, source, source, source, delta, delta, delta, delta,
             observed, members.front(), elements.front(), index, index}) ||
        !requireExactEffects(definition,
                             AstUtilInterface::OperatorSideEffect::Modify,
                             {source, source, source, source, source, source,
                              source, source, delta, delta, delta, delta}))
      return 1;
    checked_memory_reads = true;
  }
  ROSE_ASSERT(checked_memory_reads);

  AstUtilInterface::WholeProgramDependenceAnalysis analysis;
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *source_file = isSgSourceFile(file);
    ASSERT_not_null(source_file);
    SgGlobal *global = source_file->get_globalScope();
    ASSERT_not_null(global);
    for (SgDeclarationStatement *declaration : global->get_declarations()) {
      ASSERT_not_null(declaration);
      analysis.ComputeDependences(declaration, global);
    }
  }

  std::ostringstream output;
  analysis.OutputDependences(output);
  const std::string dependences = output.str();
  if (!requireInitializationInput(dependences, "target", "source") ||
      !requireInitializationInput(dependences, "target", "delta") ||
      !requireInitializationInput(dependences, "rex_UNKNOWN_target",
                                  "source") ||
      !requireInitializationInput(dependences, "positional_pair", "source") ||
      !requireInitializationInput(dependences, "positional_pair", "delta") ||
      !requireInitializationInput(dependences, "designated_pair", "source") ||
      !requireInitializationInput(dependences, "designated_pair", "delta") ||
      !requireInitializationInput(dependences, "rex_nested::nested_target",
                                  "source") ||
      !requireInitializationInput(dependences, "rex_nested::nested_target",
                                  "delta") ||
      !rejectInitializationTarget(dependences,
                                  "read_local_initializer::local") ||
      !rejectInitializationTarget(dependences,
                                  "read_local_initializer::function_static")) {
    return 1;
  }

  return 0;
}
