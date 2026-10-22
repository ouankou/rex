#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>

static void requireExceptionFlow(SgProject *project) {
  using namespace VirtualCFG;
  unsigned throws = 0, sequences = 0, handlers = 0;
  const auto requireSymmetry = [](const CFGEdge &edge) {
    const auto incoming = edge.target().inEdges();
    const auto outgoing = edge.source().outEdges();
    ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), edge) !=
                incoming.end());
    ROSE_ASSERT(std::find(outgoing.begin(), outgoing.end(), edge) !=
                outgoing.end());
  };
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgThrowOp)) {
    auto *operation = isSgThrowOp(node);
    auto *literal = isSgIntVal(operation->get_operand());
    if (literal != nullptr &&
        (literal->get_value() < 701 || literal->get_value() > 707))
      continue;
    if (operation->get_operand() != nullptr && literal == nullptr)
      continue;
    auto *function = SageInterface::getEnclosingFunctionDeclaration(operation);
    ASSERT_not_null(function);
    if (literal == nullptr && function->get_name() != "rex_cfg_nested_catches")
      continue;
    ++throws;
    const auto outgoing = operation->cfgForEnd().outEdges();
    if (literal != nullptr &&
        (literal->get_value() == 701 || literal->get_value() == 704)) {
      ROSE_ASSERT(outgoing.empty());
    } else {
      ROSE_ASSERT(outgoing.size() == 1);
      auto *sequence =
          isSgCatchStatementSeq(outgoing.front().target().getNode());
      ROSE_ASSERT(sequence != nullptr &&
                  outgoing.front().target().getIndex() == 0);
      if (literal == nullptr) {
        auto *outer = isSgTryStmt(
            function->get_definition()->get_body()->get_statements().front());
        ROSE_ASSERT(outer != nullptr &&
                    sequence == outer->get_catch_statement_seq_root());
        auto *handler =
            SageInterface::getEnclosingNode<SgCatchOptionStmt>(operation);
        ROSE_ASSERT(handler != nullptr && handler->get_condition() != nullptr);
        const auto &variables = handler->get_condition()->get_variables();
        ROSE_ASSERT(
            outgoing.front().scopesBeingExited() ==
            SgInitializedNamePtrList(variables.rbegin(), variables.rend()));
      }
      requireSymmetry(outgoing.front());
    }
    auto *parent = operation->get_parent();
    for (unsigned index = 0; index <= parent->cfgIndexForEnd(); ++index)
      for (const auto &edge : parent->cfgInEdges(index))
        ROSE_ASSERT(edge.source() != operation->cfgForEnd());
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgTryStmt)) {
    auto *statement = isSgTryStmt(node);
    auto *function = SageInterface::getEnclosingFunctionDeclaration(statement);
    if (function == nullptr || SageInterface::insideSystemHeader(function))
      continue;
    ++sequences;
    auto *sequence = statement->get_catch_statement_seq_root();
    const auto &catches = statement->get_catch_statement_seq();
    for (unsigned index = 0; index < catches.size(); ++index) {
      ++handlers;
      auto *handler = isSgCatchOptionStmt(catches[index]);
      ASSERT_not_null(handler);
      const auto outgoing = CFGNode(sequence, index).outEdges();
      ROSE_ASSERT(outgoing.size() ==
                  (handler->get_condition() == nullptr ? 1 : 2));
      ROSE_ASSERT(outgoing.front().target() == handler->cfgForBeginning());
      ROSE_ASSERT(outgoing.front().condition() == eckTrue);
      for (const auto &edge : outgoing)
        requireSymmetry(edge);
      if (outgoing.size() == 2) {
        ROSE_ASSERT(outgoing.back().target() == CFGNode(sequence, index + 1));
        ROSE_ASSERT(outgoing.back().condition() == eckFalse);
      }
      const auto completion = handler->cfgForEnd().outEdges();
      ROSE_ASSERT(completion.size() == 1);
      const auto &modifier = function->get_specialFunctionModifier();
      const bool rethrows =
          statement->get_is_function_try_block() &&
          (modifier.isConstructor() || modifier.isDestructor());
      ROSE_ASSERT(completion.front().target() ==
                  (rethrows ? sequence->cfgForEnd() : statement->cfgForEnd()));
      requireSymmetry(completion.front());
      SgInitializedNamePtrList leaving;
      if (auto *condition = handler->get_condition()) {
        const auto &variables = condition->get_variables();
        leaving.assign(variables.rbegin(), variables.rend());
      }
      ROSE_ASSERT(completion.front().scopesBeingExited() == leaving);
      ROSE_ASSERT(completion.front().scopesBeingEntered().empty());
    }
  }
  ROSE_ASSERT(throws == 8 && sequences == 7 && handlers == 9);
}

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  requireExceptionFlow(project);

  bool saw_full_expression = false;
  bool saw_cleanup = false;
  bool saw_materialized_temporary = false;
  bool saw_full_expression_materialization = false;
  bool saw_automatic_materialization = false;
  bool saw_static_materialization = false;
  bool saw_thread_materialization = false;
  bool saw_containing_object_materialization = false;
  bool saw_bound_temporary = false;
  bool saw_constant_evaluated = false;
  bool saw_emitted_arrow = false;
  bool saw_implicit_object_access = false;

  auto observe_expression = [&](SgExpression *expression) {
    ROSE_ASSERT(expression != nullptr);
    saw_full_expression |= expression->has_semantic_wrapper(
        SgExpression::e_full_expression_wrapper);
    saw_cleanup |=
        expression->has_semantic_wrapper(SgExpression::e_cleanup_wrapper);
    saw_materialized_temporary |= expression->has_semantic_wrapper(
        SgExpression::e_materialized_temporary);
    if (expression->has_semantic_wrapper(
            SgExpression::e_materialized_temporary)) {
      const auto storage = expression->get_materialized_storage_duration();
      auto *owner = expression->get_materialized_lifetime_owner();
      ROSE_ASSERT(
          SgExpression::is_valid_temporary_materialization(storage, owner));
      saw_full_expression_materialization |=
          storage == SgExpression::e_temporary_storage_full_expression;
      saw_automatic_materialization |=
          storage == SgExpression::e_temporary_storage_automatic;
      saw_static_materialization |=
          storage == SgExpression::e_temporary_storage_static;
      saw_thread_materialization |=
          storage == SgExpression::e_temporary_storage_thread;
      saw_containing_object_materialization |=
          storage == SgExpression::e_temporary_storage_containing_object;
      auto *copy = SageInterface::copyExpression(expression);
      ROSE_ASSERT(copy->get_materialized_storage_duration() == storage &&
                  copy->get_materialized_lifetime_owner() == owner);
      SageInterface::deleteAST(copy);
    }
    saw_bound_temporary |=
        expression->has_semantic_wrapper(SgExpression::e_bound_temporary);
    if (expression->has_semantic_wrapper(SgExpression::e_bound_temporary)) {
      ROSE_ASSERT(SgExpression::is_valid_bound_temporary_destructor(
          expression->get_type(),
          expression->get_bound_temporary_destructor()));
      auto *copy = SageInterface::copyExpression(expression);
      ROSE_ASSERT(copy->get_bound_temporary_destructor() ==
                  expression->get_bound_temporary_destructor());
      SageInterface::deleteAST(copy);
    }
    saw_constant_evaluated |= expression->has_semantic_wrapper(
        SgExpression::e_constant_evaluated_wrapper);
    if (SgArrowExp *arrow = isSgArrowExp(expression)) {
      switch (arrow->get_emission_role()) {
      case SgArrowExp::e_emit_arrow_operator:
        saw_emitted_arrow = true;
        break;
      case SgArrowExp::e_implicit_object_access:
        saw_implicit_object_access = true;
        break;
      default:
        ROSE_ABORT();
      }
    }
  };

  for (SgNode *node : NodeQuery::querySubTree(project, V_SgExpression)) {
    observe_expression(isSgExpression(node));
  }

  // ValueExp::originalExpressionTree is an intentionally non-traversed edge:
  // it owns exact source spelling beside a canonical evaluated value. Audit
  // that semantic subtree explicitly so wrapper metadata cannot be hidden by
  // the generic traversal boundary.
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgValueExp)) {
    SgValueExp *value = isSgValueExp(node);
    ROSE_ASSERT(value != nullptr);
    SgExpression *source = value->get_originalExpressionTree();
    if (source == nullptr) {
      continue;
    }
    ROSE_ASSERT(source->get_parent() == value);
    for (SgNode *source_node :
         NodeQuery::querySubTree(source, V_SgExpression)) {
      observe_expression(isSgExpression(source_node));
    }
  }

  ROSE_ASSERT(saw_full_expression);
  ROSE_ASSERT(saw_cleanup);
  ROSE_ASSERT(saw_materialized_temporary);
  ROSE_ASSERT(saw_full_expression_materialization);
  ROSE_ASSERT(saw_automatic_materialization);
  ROSE_ASSERT(saw_static_materialization);
  ROSE_ASSERT(saw_thread_materialization);
  ROSE_ASSERT(saw_containing_object_materialization);
  ROSE_ASSERT(saw_bound_temporary);
  ROSE_ASSERT(saw_constant_evaluated);
  ROSE_ASSERT(saw_emitted_arrow);
  ROSE_ASSERT(saw_implicit_object_access);
  bool copied_lifetime_owner = false;
  bool saw_static_binding = false, saw_thread_binding = false;
  unsigned holding_variables = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgVariableDeclaration)) {
    auto *declaration = isSgVariableDeclaration(node);
    for (auto *name : declaration->get_variables()) {
      auto *pattern = name->get_structured_binding_pattern();
      if (pattern == nullptr)
        continue;
      const auto &storage =
          declaration->get_declarationModifier().get_storageModifier();
      for (auto *element : pattern->get_expressions()) {
        auto *reference = isSgVarRefExp(element);
        ROSE_ASSERT(reference != nullptr);
        auto *alias = reference->get_symbol()->get_declaration();
        ROSE_ASSERT(alias->get_initializer() == nullptr);
        ROSE_ASSERT(!NodeQuery::querySubTree(alias, V_SgExpression).empty());
        alias->validate_structured_binding();
        ROSE_ASSERT(alias->get_structured_binding_kind() ==
                        SgInitializedName::e_resolved_structured_binding &&
                    alias->get_structured_binding_owner() == name);
        auto *value = alias->get_structured_binding_value();
        ROSE_ASSERT(value != nullptr && value->get_parent() == alias);
        auto *holding = alias->get_structured_binding_holding_variable();
        ROSE_ASSERT(name->cfgFindChildIndex(alias) < name->cfgIndexForEnd());
        ROSE_ASSERT(value->cfgForBeginning().inEdges().size() == 1 &&
                    value->cfgForEnd().outEdges().size() == 1);
        if (holding != nullptr) {
          ++holding_variables;
          holding->validate_structured_binding();
          ROSE_ASSERT(holding->get_structured_binding_owner() == name &&
                      holding->get_structured_binding_alias() == alias);
          ROSE_ASSERT(alias->cfgIndexForEnd() == 2 &&
                      alias->cfgFindChildIndex(holding) == 0 &&
                      alias->cfgFindChildIndex(value) == 1);
          ROSE_ASSERT(holding->cfgForEnd().outEdges().size() == 1);
          auto *holding_declaration =
              isSgVariableDeclaration(holding->get_parent());
          ROSE_ASSERT(holding_declaration != nullptr);
          ROSE_ASSERT(holding_declaration->get_declarationModifier()
                          .get_storageModifier()
                          .isStatic() == storage.isStatic());
          ROSE_ASSERT(holding_declaration->get_is_thread_local() ==
                      declaration->get_is_thread_local());
          for (SgNode *expression : NodeQuery::querySubTree(
                   holding->get_initializer(), V_SgExpression))
            if (auto *temporary = isSgExpression(expression);
                temporary->has_semantic_wrapper(
                    SgExpression::e_materialized_temporary))
              ROSE_ASSERT(temporary->get_materialized_lifetime_owner() ==
                          holding);
        }
        auto *binding = isSgVariableDeclaration(alias->get_parent());
        ROSE_ASSERT(binding != nullptr);
        const auto &binding_storage =
            binding->get_declarationModifier().get_storageModifier();
        ROSE_ASSERT(binding_storage.isStatic() == storage.isStatic());
        ROSE_ASSERT(binding->get_is_thread_local() ==
                    declaration->get_is_thread_local());
        ROSE_ASSERT(binding_storage.get_thread_local_storage() ==
                    storage.get_thread_local_storage());
      }
      saw_static_binding |= storage.isStatic();
      saw_thread_binding |= declaration->get_is_thread_local();
    }
    if (declaration->get_variables().size() != 1 ||
        declaration->get_variables().front()->get_name() != "value")
      continue;
    auto *copy = SageInterface::deepCopySemantic(declaration);
    ROSE_ASSERT(copy->get_variables().size() == 1);
    for (SgNode *child : NodeQuery::querySubTree(copy, V_SgExpression))
      if (auto *expression = isSgExpression(child);
          expression->has_semantic_wrapper(
              SgExpression::e_materialized_temporary)) {
        ROSE_ASSERT(expression->get_materialized_lifetime_owner() ==
                    copy->get_variables().front());
        copied_lifetime_owner = true;
      }
    SageInterface::deleteAST(copy);
  }
  ROSE_ASSERT(copied_lifetime_owner);
  ROSE_ASSERT(saw_static_binding && saw_thread_binding);
  ROSE_ASSERT(holding_variables == 5);
  return backend(project);
}
