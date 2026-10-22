#include "CallGraph.h"
#include "rose.h"

#include <cstring>
#include <set>
#include <string>
#include <utility>

namespace {
std::string functionName(SgGraphNode *node) {
  ROSE_ASSERT(node != nullptr);
  SgFunctionDeclaration *declaration =
      isSgFunctionDeclaration(node->get_SgNode());
  ROSE_ASSERT(declaration != nullptr);
  if (auto *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    return instantiation->get_templateName().getString();
  }
  if (auto *instantiation =
          isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    return instantiation->get_templateName().getString();
  }
  return declaration->get_name().getString();
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "missing_member_symbol") == 0) {
    SgMemberFunctionRefExp reference(
        static_cast<SgMemberFunctionSymbol *>(nullptr), 0, 0);
    return 0;
  }
  if (argc == 2 && std::strcmp(argv[1], "missing_member_declaration") == 0) {
    SgMemberFunctionSymbol symbol;
    SgMemberFunctionRefExp reference(&symbol, 0, 0);
    return 0;
  }
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  const auto projectAttributes = project->numberOfAttributes();

  ClassHierarchyWrapper hierarchy(project);
  unsigned covariantCalls = 0;
  unsigned incompleteMemberCalls = 0;
  unsigned inheritedMemberCalls = 0;
  unsigned blockCalls = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *declaration = call->getAssociatedFunctionDeclaration();
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(call);
    if (caller != nullptr &&
        caller->get_name() == "rex_callgraph_incomplete_member_pointer") {
      Rose_STL_Container<SgFunctionDeclaration *> targets;
      CallTargetSet::getDeclarationsForExpression(call, &hierarchy, targets);
      ROSE_ASSERT(targets.empty());
      ++incompleteMemberCalls;
    }
    const bool covariantMemberPointer =
        caller != nullptr &&
        (caller->get_name() ==
             "rex_callgraph_covariant_member_pointer_caller" ||
         caller->get_name() == "rex_callgraph_converted_member_pointer_caller");
    if (caller != nullptr &&
        caller->get_name() == "rex_callgraph_inherited_member_pointer_caller") {
      Rose_STL_Container<SgFunctionDeclaration *> targets;
      CallTargetSet::getDeclarationsForExpression(call, &hierarchy, targets);
      ROSE_ASSERT(targets.size() == 1);
      auto *target = isSgMemberFunctionDeclaration(targets.front());
      ROSE_ASSERT(target != nullptr);
      ROSE_ASSERT(target->get_name() == "rex_callgraph_inherited_target");
      ROSE_ASSERT(target->get_parameterList()->get_args().empty());
      auto *owner = isSgClassDefinition(target->get_scope());
      ROSE_ASSERT(owner != nullptr);
      ROSE_ASSERT(owner->get_declaration()->get_name() ==
                  "RexCallGraphInheritedBase");
      ++inheritedMemberCalls;
    }
    if (covariantMemberPointer ||
        (declaration != nullptr &&
         (declaration->get_name() == "rex_callgraph_covariant_pointer" ||
          declaration->get_name() == "rex_callgraph_covariant_lvalue" ||
          declaration->get_name() == "rex_callgraph_covariant_rvalue"))) {
      Rose_STL_Container<SgFunctionDeclaration *> targets;
      CallTargetSet::getDeclarationsForExpression(call, &hierarchy, targets);
      ROSE_ASSERT(targets.size() == 1);
      auto *target = isSgMemberFunctionDeclaration(targets.front());
      ROSE_ASSERT(target != nullptr);
      ROSE_ASSERT(target->get_functionModifier().isVirtual());
      ROSE_ASSERT(!target->get_functionModifier().isPureVirtual());
      ROSE_ASSERT(target->get_parameterList()->get_args().empty());
      auto *owner = isSgClassDefinition(target->get_scope());
      ROSE_ASSERT(owner != nullptr);
      ROSE_ASSERT(owner->get_declaration()->get_name() ==
                  "RexCallGraphCovariantDerived");
      ++covariantCalls;
    }
    if (declaration == nullptr ||
        declaration->get_name() != "rex_callgraph_block_target")
      continue;
    auto *definition =
        isSgFunctionDeclaration(declaration->get_definingDeclaration());
    ROSE_ASSERT(definition != nullptr &&
                definition->get_definition() != nullptr);
    ROSE_ASSERT(callGraphCanonicalDeclarationChain(declaration) == definition);
    ++blockCalls;
  }
  ROSE_ASSERT(blockCalls == 1);
  ROSE_ASSERT(covariantCalls == 5);
  ROSE_ASSERT(incompleteMemberCalls == 1);
  ROSE_ASSERT(inheritedMemberCalls == 1);

  size_t functionToPointerDecays = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    ROSE_ASSERT(cast != nullptr);
    if (cast->get_semantic_conversion_kind() !=
        SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
      continue;
    }
    ROSE_ASSERT(cast->get_cast_type() == SgCastExp::e_implicit_cast);
    ROSE_ASSERT(cast->get_operand() != nullptr);
    ROSE_ASSERT(cast->get_operand()->get_parent() == cast);
    ROSE_ASSERT(cast->get_type() != nullptr);
    SgPointerType *targetPointer =
        isSgPointerType(cast->get_type()->stripTypedefsAndModifiers());
    ROSE_ASSERT(targetPointer != nullptr);
    ROSE_ASSERT(isSgFunctionType(targetPointer->get_base_type()) != nullptr);
    ROSE_ASSERT(isSgFunctionType(cast->get_operand()->get_type()) != nullptr);
    ++functionToPointerDecays;
  }
  ROSE_ASSERT(functionToPointerDecays >= 4);

  size_t concreteTemplateCalls = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *declaration = call->getAssociatedFunctionDeclaration();
    if (isSgTemplateInstantiationFunctionDecl(declaration) == nullptr &&
        isSgTemplateInstantiationMemberFunctionDecl(declaration) == nullptr)
      continue;
    ++concreteTemplateCalls;
    auto *defining =
        declaration->get_definition() != nullptr
            ? declaration
            : isSgFunctionDeclaration(declaration->get_definingDeclaration());
    ROSE_ASSERT(defining != nullptr && defining->get_definition() != nullptr);
    ROSE_ASSERT(defining->get_definition()->get_declaration() == defining);
  }
  ROSE_ASSERT(concreteTemplateCalls >= 4);

  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *variable = isSgInitializedName(node);
    if (variable->get_name() != "function")
      continue;
    auto *pointer = isSgPointerType(variable->get_type());
    ROSE_ASSERT(pointer != nullptr);
    ROSE_ASSERT(isSgFunctionType(pointer->get_base_type()) != nullptr);
  }

  CallGraphBuilder builder(project);
  builder.buildCallGraph();
  SgIncidenceDirectedGraph *graph = builder.getGraph();
  ROSE_ASSERT(graph != nullptr);

  std::set<std::pair<std::string, std::string>> edges;
  std::set<SgClassDeclaration *> shadowOwners;
  std::set<SgClassDeclaration *> qualifiedOwners;
  std::set<SgClassDeclaration *> referenceResultOwners;
  std::set<std::string> exactFunctorCallers;
  std::set<std::string> covariantCallers;
  const rose_graph_integer_node_hash_map &nodes =
      graph->get_node_index_to_node_map();
  const rose_graph_integer_edge_hash_multimap &outEdges =
      graph->get_node_index_to_edge_multimap_edgesOut();
  for (const auto &[sourceIndex, edgeNode] : outEdges) {
    auto source = nodes.find(sourceIndex);
    ROSE_ASSERT(source != nodes.end());
    SgDirectedGraphEdge *edge = isSgDirectedGraphEdge(edgeNode);
    ROSE_ASSERT(edge != nullptr);
    ROSE_ASSERT(edge->get_from() == source->second);
    if (auto *member =
            isSgMemberFunctionDeclaration(edge->get_to()->get_SgNode())) {
      ROSE_ASSERT(member->get_name() != "operator_named");
      const std::string caller = functionName(edge->get_from());
      if (caller == "rex_callgraph_covariant_pointer_caller" ||
          caller == "rex_callgraph_covariant_lvalue_caller" ||
          caller == "rex_callgraph_covariant_rvalue_caller" ||
          caller == "rex_callgraph_covariant_member_pointer_caller" ||
          caller == "rex_callgraph_converted_member_pointer_caller") {
        ROSE_ASSERT(member->get_functionModifier().isVirtual());
        ROSE_ASSERT(member->get_parameterList()->get_args().empty());
        auto *owner = isSgClassDefinition(member->get_scope());
        ROSE_ASSERT(owner != nullptr);
        if (!member->get_functionModifier().isPureVirtual()) {
          ROSE_ASSERT(owner->get_declaration()->get_name() ==
                      "RexCallGraphCovariantDerived");
          covariantCallers.insert(caller);
        }
      }
      if ((caller == "rex_callgraph_functor_caller" ||
           caller == "rex_callgraph_named_functor_caller" ||
           caller == "rex_callgraph_returned_functor_caller") &&
          member->get_name() == "operator()") {
        const auto &parameters = member->get_parameterList()->get_args();
        ROSE_ASSERT(parameters.size() == 1);
        ROSE_ASSERT(isSgTypeInt(parameters.front()->get_type()) != nullptr);
        exactFunctorCallers.insert(caller);
      }
      if (functionName(edge->get_from()) ==
              "rex_callgraph_shadowed_parameter" &&
          member->get_name() == "rex_callgraph_shadow_target") {
        auto *owner = isSgClassDefinition(member->get_scope());
        ROSE_ASSERT(owner != nullptr);
        shadowOwners.insert(isSgClassDeclaration(
            owner->get_declaration()->get_firstNondefiningDeclaration()));
      }
      if (functionName(edge->get_from()) == "rex_callgraph_qualified_member" &&
          member->get_name() == "rex_callgraph_shadow_target") {
        auto *owner = isSgClassDefinition(member->get_scope());
        ROSE_ASSERT(owner != nullptr);
        qualifiedOwners.insert(isSgClassDeclaration(
            owner->get_declaration()->get_firstNondefiningDeclaration()));
      }
      if (functionName(edge->get_from()) == "rex_callgraph_reference_result" &&
          member->get_name() == "rex_callgraph_shadow_target") {
        auto *owner = isSgClassDefinition(member->get_scope());
        ROSE_ASSERT(owner != nullptr);
        referenceResultOwners.insert(isSgClassDeclaration(
            owner->get_declaration()->get_firstNondefiningDeclaration()));
      }
    }
    edges.emplace(functionName(edge->get_from()), functionName(edge->get_to()));
  }

  ROSE_ASSERT(edges.count({"rex_callgraph_direct_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_block_caller",
                           "rex_callgraph_block_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_field_pointer_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_member_caller",
                           "rex_callgraph_member_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_member_caller",
                           "rex_callgraph_static_member_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_const_pointer_caller",
                           "rex_callgraph_const_pointer_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_const_pointer_caller",
                           "rex_callgraph_mutable_pointer_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_variadic_caller",
                           "rex_callgraph_variadic_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_variadic_caller",
                           "rex_callgraph_nonvariadic_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_named_caller",
                           "rex_callgraph_named_a_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_named_caller",
                           "rex_callgraph_named_b_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_array_caller",
                           "rex_callgraph_array_four_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_array_caller",
                           "rex_callgraph_array_five_target"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_indexed_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_explicit_cast_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_nested_caller",
                           "rex_callgraph_factory"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_nested_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_functional_cast_caller",
                           "rex_callgraph_direct_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_functor_caller", "operator()"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_empty_pack_caller",
                           "rex_callgraph_empty_pack_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_value_caller",
                           "rex_callgraph_value_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_dependent_operator", "operator+"}) ==
              1);
  ROSE_ASSERT(edges.count({"rex_callgraph_temporary_caller",
                           "rex_callgraph_temporary_factory"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_temporary_caller",
                           "rex_callgraph_temporary_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_copy_caller",
                           "rex_callgraph_temporary_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_copy_caller",
                           "rex_callgraph_copy_default"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_constructor_caller",
                           "RexCallGraphConstructorDerived"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_constructor_caller",
                           "RexCallGraphConstructorBase"}) == 0);
  ROSE_ASSERT(edges.count({"RexCallGraphConstructorDerived",
                           "RexCallGraphConstructorBase"}) == 0);
  ROSE_ASSERT(edges.count({"RexCallGraphConstructorDerived",
                           "RexCallGraphConstructorMiddle"}) == 1);
  ROSE_ASSERT(edges.count({"RexCallGraphConstructorMiddle",
                           "RexCallGraphConstructorBase"}) == 1);
  CallGraphBuilder sourceBuilder(project);
  sourceBuilder.buildCallGraph([](SgFunctionDeclaration *declaration) {
    return declaration->get_frontend_declaration_origin() !=
           SgFunctionDeclaration::e_frontend_declaration_implicit;
  });
  std::set<std::pair<std::string, std::string>> sourceEdges;
  for (const auto &[index, edgeNode] :
       sourceBuilder.getGraph()->get_node_index_to_edge_multimap_edgesOut()) {
    auto *edge = isSgDirectedGraphEdge(edgeNode);
    ROSE_ASSERT(edge != nullptr);
    sourceEdges.emplace(functionName(edge->get_from()),
                        functionName(edge->get_to()));
  }
  ROSE_ASSERT(sourceEdges.count({"RexCallGraphConstructorDerived",
                                 "RexCallGraphConstructorMiddle"}) == 1);
  ROSE_ASSERT(sourceEdges.count({"RexCallGraphConstructorMiddle",
                                 "RexCallGraphConstructorBase"}) == 1);
  ROSE_ASSERT(sourceEdges.count({"rex_callgraph_constructor_caller",
                                 "RexCallGraphConstructorBase"}) == 0);
  ROSE_ASSERT(sourceEdges.count({"RexCallGraphGlobalDerived",
                                 "RexCallGraphGlobalMiddle"}) == 1);
  ROSE_ASSERT(sourceEdges.count({"RexCallGraphGlobalMiddle",
                                 "RexCallGraphConstructorBase"}) == 1);
  ROSE_ASSERT(sourceEdges.count({"RexCallGraphGlobalDerived",
                                 "RexCallGraphConstructorBase"}) == 0);
  for (const auto &[index, node] :
       sourceBuilder.getGraph()->get_node_index_to_node_map()) {
    ROSE_ASSERT(functionName(node) != "RexCallGraphLexicalConstructor");
  }
  ROSE_ASSERT(edges.count({"rex_callgraph_lexical_default",
                           "rex_callgraph_copy_default"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_unevaluated_call",
                           "rex_callgraph_copy_default"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_unevaluated_alignof_call",
                           "rex_callgraph_copy_default"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_unevaluated_typeid_call",
                           "rex_callgraph_copy_default"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_unevaluated_sizeof_call",
                           "rex_callgraph_copy_default"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_unevaluated_sizeof_vla_pointer",
                           "rex_callgraph_copy_default"}) == 0);
  ROSE_ASSERT(edges.count({"rex_callgraph_evaluated_sizeof_vla",
                           "rex_callgraph_copy_default"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_evaluated_sizeof_vla_alias",
                           "rex_callgraph_copy_default"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_dynamic_typeid",
                           "rex_callgraph_reference_factory"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_actual_default",
                           "rex_callgraph_copy_default"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_lambda_owner",
                           "rex_callgraph_lambda_body_target"}) == 0);
  ROSE_ASSERT(edges.count({"operator()", "rex_callgraph_lambda_body_target"}) ==
              1);
  ROSE_ASSERT(edges.count({"rex_callgraph_lambda_owner",
                           "rex_callgraph_capture_target"}) == 1);
  ROSE_ASSERT(edges.count({"rex_callgraph_member_default_owner",
                           "rex_callgraph_member_default_target"}) == 0);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  auto *defaultTarget = file->get_globalScope()->lookup_function_symbol(
      "rex_callgraph_copy_default");
  ROSE_ASSERT(defaultTarget != nullptr);
  auto *defaultDeclaration =
      canonicalFunctionDeclForCallGraph(defaultTarget->get_declaration());
  ROSE_ASSERT(defaultDeclaration->get_definition() != nullptr);
  Rose_STL_Container<SgExpression *> defaultSites;
  CallTargetSet::getExpressionsForDefinition(
      defaultDeclaration->get_definition(), &hierarchy, defaultSites);
  ROSE_ASSERT(project->numberOfAttributes() == projectAttributes);
  ROSE_ASSERT(!defaultSites.empty());
  for (auto *site : defaultSites) {
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(site);
    ROSE_ASSERT(caller != nullptr);
    ROSE_ASSERT(caller->get_name() != "rex_callgraph_unevaluated_call");
    ROSE_ASSERT(caller->get_name() != "rex_callgraph_unevaluated_alignof_call");
    ROSE_ASSERT(caller->get_name() != "rex_callgraph_unevaluated_typeid_call");
    ROSE_ASSERT(caller->get_name() != "rex_callgraph_unevaluated_sizeof_call");
    ROSE_ASSERT(caller->get_name() !=
                "rex_callgraph_unevaluated_sizeof_vla_pointer");
  }
  size_t unevaluatedOperators = 0;
  for (VariantT kind : {V_SgNoexceptOp, V_SgAlignOfOp}) {
    for (SgNode *node : NodeQuery::querySubTree(project, kind)) {
      auto *operation = isSgExpression(node);
      auto *caller = SageInterface::getEnclosingFunctionDeclaration(operation);
      if (caller == nullptr ||
          (caller->get_name() != "rex_callgraph_unevaluated_call" &&
           caller->get_name() != "rex_callgraph_unevaluated_alignof_call" &&
           caller->get_name() != "rex_callgraph_unevaluated_alignof_type"))
        continue;
      const auto outgoing = operation->cfgOutEdges(0);
      ROSE_ASSERT(outgoing.size() == 1);
      ROSE_ASSERT(outgoing.front().target() == operation->cfgForEnd());
      const auto incoming = operation->cfgInEdges(operation->cfgIndexForEnd());
      ROSE_ASSERT(incoming.size() == 1);
      ROSE_ASSERT(incoming.front().source() == operation->cfgForBeginning());
      auto *operand = isSgNoexceptOp(operation) != nullptr
                          ? isSgNoexceptOp(operation)->get_operand_expr()
                          : isSgAlignOfOp(operation)->get_operand_expr();
      if (operand != nullptr) {
        ROSE_ASSERT(operand->cfgForBeginning().inEdges().empty());
        ROSE_ASSERT(operand->cfgForEnd().outEdges().empty());
      } else {
        ROSE_ASSERT(isSgAlignOfOp(operation) != nullptr);
        ROSE_ASSERT(isSgAlignOfOp(operation)->get_operand_type() != nullptr);
      }
      ++unevaluatedOperators;
    }
  }
  ROSE_ASSERT(unevaluatedOperators == 3);
  size_t sizeofOperators = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgSizeOfOp)) {
    auto *operation = isSgSizeOfOp(node);
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(operation);
    if (caller == nullptr ||
        (caller->get_name() != "rex_callgraph_unevaluated_sizeof_call" &&
         caller->get_name() != "rex_callgraph_unevaluated_sizeof_type" &&
         caller->get_name() != "rex_callgraph_evaluated_sizeof_vla" &&
         caller->get_name() != "rex_callgraph_evaluated_sizeof_vla_alias" &&
         caller->get_name() != "rex_callgraph_unevaluated_sizeof_vla_pointer"))
      continue;
    const bool evaluated =
        caller->get_name() == "rex_callgraph_evaluated_sizeof_vla" ||
        caller->get_name() == "rex_callgraph_evaluated_sizeof_vla_alias";
    ROSE_ASSERT(operation->expression_operand_is_evaluated() == evaluated);
    auto *operand = operation->get_operand_expr();
    const auto outgoing = operation->cfgForBeginning().outEdges();
    const auto incoming = operation->cfgForEnd().inEdges();
    ROSE_ASSERT(outgoing.size() == 1 && incoming.size() == 1);
    ROSE_ASSERT(
        outgoing.front().target() ==
        (evaluated ? operand->cfgForBeginning() : operation->cfgForEnd()));
    ROSE_ASSERT(
        incoming.front().source() ==
        (evaluated ? operand->cfgForEnd() : operation->cfgForBeginning()));
    if (caller->get_name() == "rex_callgraph_evaluated_sizeof_vla") {
      auto *array = isSgArrayType(operand->get_type());
      ROSE_ASSERT(array != nullptr && array->get_is_variable_length_array());
    } else if (!evaluated && operand != nullptr) {
      ROSE_ASSERT(operand->cfgForBeginning().inEdges().empty());
      ROSE_ASSERT(operand->cfgForEnd().outEdges().empty());
    } else if (operand == nullptr) {
      ROSE_ASSERT(isSgTypeInt(operation->get_operand_type()) != nullptr);
    }
    SgTreeCopy sizeofCopy;
    auto *copied = isSgSizeOfOp(operation->copy(sizeofCopy));
    ROSE_ASSERT(copied != nullptr);
    ROSE_ASSERT(copied->expression_operand_is_evaluated() == evaluated);
    ROSE_ASSERT(copied->get_type() == operation->get_type());
    if (operand != nullptr) {
      ROSE_ASSERT(copied->get_operand_expr() != operand);
      ROSE_ASSERT(copied->get_operand_expr()->get_parent() == copied);
    }
    SageInterface::deleteAST(copied);
    ++sizeofOperators;
  }
  ROSE_ASSERT(sizeofOperators == 5);
  size_t dataSizeOperators = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgSizeOfOp)) {
    auto *operation = isSgSizeOfOp(node);
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(operation);
    if (caller == nullptr || caller->get_name() != "rex_callgraph_data_size")
      continue;
    ROSE_ASSERT(operation->get_operator_kind() == SgExpression::e_datasizeof);
    ROSE_ASSERT(operation->unparseToString().find("__datasizeof(") == 0);
    ROSE_ASSERT(!operation->expression_operand_is_evaluated());
    SgTreeCopy copy;
    auto *copied = isSgSizeOfOp(operation->copy(copy));
    ROSE_ASSERT(copied != nullptr &&
                copied->get_operator_kind() == operation->get_operator_kind());
    SgUnparse_Info info;
    info.set_current_source_file(
        SageInterface::getEnclosingNode<SgSourceFile>(operation));
    info.set_template_argument_qualification_context(
        SageInterface::getEnclosingNode<SgStatement>(operation));
    ROSE_ASSERT(copied->unparseToString(&info) == operation->unparseToString());
    SageInterface::deleteAST(copied);
    ++dataSizeOperators;
  }
  ROSE_ASSERT(dataSizeOperators == 2);
  size_t typeidOperators = 0;
  size_t writtenTypeidOperands = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgTypeIdOp)) {
    auto *operation = isSgTypeIdOp(node);
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(operation);
    if (caller == nullptr ||
        (caller->get_name() != "rex_callgraph_dynamic_typeid" &&
         caller->get_name() != "rex_callgraph_unevaluated_typeid_call" &&
         caller->get_name() != "rex_callgraph_typeid_type"))
      continue;
    auto *operand = operation->get_operand_expr();
    if (caller->get_name() == "rex_callgraph_typeid_type") {
      ROSE_ASSERT(operand == nullptr);
      if (auto *reference = isSgReferenceType(operation->get_operand_type())) {
        auto *qualified = isSgModifierType(reference->get_base_type());
        ROSE_ASSERT(qualified != nullptr &&
                    SageInterface::isConstType(qualified));
        auto *alias = isSgTypedefType(qualified->get_base_type());
        auto *declaration =
            alias ? isSgTypedefDeclaration(alias->get_declaration()) : nullptr;
        ROSE_ASSERT(declaration != nullptr &&
                    declaration->get_name() == "RexCallGraphTypeIdValue");
        ++writtenTypeidOperands;
      } else {
        ROSE_ASSERT(isSgTypeInt(operation->get_operand_type()) != nullptr);
      }
    } else {
      ROSE_ASSERT(operand != nullptr && operand->get_parent() == operation);
      ROSE_ASSERT(operation->get_operand_type() == nullptr);
    }
    const auto outgoing = operation->cfgForBeginning().outEdges();
    ROSE_ASSERT(outgoing.size() == 1);
    const bool evaluated = caller->get_name() == "rex_callgraph_dynamic_typeid";
    ROSE_ASSERT(operation->get_operand_is_evaluated() == evaluated);
    ROSE_ASSERT(
        outgoing.front().target() ==
        (evaluated ? operand->cfgForBeginning() : operation->cfgForEnd()));
    const auto incoming = operation->cfgForEnd().inEdges();
    ROSE_ASSERT(incoming.size() == 1);
    ROSE_ASSERT(
        incoming.front().source() ==
        (evaluated ? operand->cfgForEnd() : operation->cfgForBeginning()));
    if (!evaluated && operand != nullptr) {
      ROSE_ASSERT(operand->cfgForBeginning().inEdges().empty());
      ROSE_ASSERT(operand->cfgForEnd().outEdges().empty());
    }
    SgTreeCopy typeidCopy;
    auto *copied = isSgTypeIdOp(operation->copy(typeidCopy));
    ROSE_ASSERT(copied != nullptr);
    ROSE_ASSERT(copied->get_operand_is_evaluated() == evaluated);
    if (operand != nullptr) {
      ROSE_ASSERT(copied->get_operand_expr() != operand);
      ROSE_ASSERT(copied->get_operand_expr()->get_parent() == copied);
    } else {
      ROSE_ASSERT(copied->get_operand_expr() == nullptr);
      ROSE_ASSERT(copied->get_operand_type() == operation->get_operand_type());
    }
    ROSE_ASSERT(copied->get_type() == operation->get_type());
    SageInterface::deleteAST(copied);
    ++typeidOperators;
  }
  ROSE_ASSERT(typeidOperators == 4 && writtenTypeidOperands == 1);
  auto *base =
      file->get_globalScope()->lookup_class_symbol("RexCallGraphShadowBase");
  auto *derived =
      file->get_globalScope()->lookup_class_symbol("RexCallGraphShadowDerived");
  ROSE_ASSERT(base != nullptr && derived != nullptr);
  ROSE_ASSERT(shadowOwners.size() == 2);
  ROSE_ASSERT(
      shadowOwners.count(isSgClassDeclaration(
          base->get_declaration()->get_firstNondefiningDeclaration())) == 1);
  ROSE_ASSERT(
      shadowOwners.count(isSgClassDeclaration(
          derived->get_declaration()->get_firstNondefiningDeclaration())) == 1);
  ROSE_ASSERT(qualifiedOwners == shadowOwners);
  ROSE_ASSERT(referenceResultOwners == shadowOwners);
  ROSE_ASSERT(exactFunctorCallers.size() == 3);
  ROSE_ASSERT(covariantCallers.size() == 5);
  SgProject *otherProject = frontend(argc, argv);
  ROSE_ASSERT(otherProject != nullptr && otherProject != project);
  ROSE_ASSERT(frontendExitStatus(otherProject) == 0);
  ClassHierarchyWrapper otherHierarchy(otherProject);
  size_t hierarchyEdges = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDefinition)) {
    auto *definition = isSgClassDefinition(node);
    hierarchyEdges += hierarchy.getDirectSubclasses(definition).size();
    ROSE_ASSERT(otherHierarchy.getDirectSubclasses(definition).empty());
    ROSE_ASSERT(otherHierarchy.getSubclasses(definition).empty());
    ROSE_ASSERT(otherHierarchy.getAncestorClasses(definition).empty());
  }
  ROSE_ASSERT(hierarchyEdges > 0);
  size_t trivialConstructors = 0;
  size_t exactConstructorCalls = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgConstructorInitializer)) {
    auto *initializer = isSgConstructorInitializer(node);
    auto *constructor = initializer->get_declaration();
    if (constructor == nullptr)
      continue;
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(initializer);
    if (caller != nullptr &&
        caller->get_name() == "rex_callgraph_constructor_caller") {
      const auto targets =
          CallTargetSet::solveConstructorInitializer(initializer);
      ROSE_ASSERT(targets.size() == 1);
      ROSE_ASSERT(targets.front() ==
                  canonicalFunctionDeclForCallGraph(constructor));
      ++exactConstructorCalls;
    }
    auto *owner = isSgClassDefinition(constructor->get_scope());
    ROSE_ASSERT(owner != nullptr);
    if (owner->get_declaration()->get_name() == "RexCallGraphTrivial") {
      ROSE_ASSERT(constructor->get_functionModifier().isDefaultConstructor());
      ROSE_ASSERT(constructor->get_functionModifier().isTrivialSpecialMember());
      ++trivialConstructors;
    }
  }
  ROSE_ASSERT(trivialConstructors == 1);
  ROSE_ASSERT(exactConstructorCalls == 1);
  auto *otherFile = isSgSourceFile(&otherProject->get_file(0));
  ROSE_ASSERT(otherFile != nullptr);
  auto *otherTarget = otherFile->get_globalScope()->lookup_function_symbol(
      "rex_callgraph_direct_target");
  ROSE_ASSERT(otherTarget != nullptr);
  ROSE_ASSERT(builder.getGraphNodeFor(otherTarget->get_declaration()) ==
              nullptr);
  CallGraphBuilder filteredBuilder(project);
  size_t dereferencedFunctionPointers = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgPointerDerefExp)) {
    auto *dereference = isSgPointerDerefExp(node);
    if (isSgFunctionType(dereference->get_type()) == nullptr)
      continue;
    const auto targets = CallTargetSet::solveFunctionPointerCall(dereference);
    ROSE_ASSERT(!targets.empty());
    for (auto *target : targets)
      ROSE_ASSERT(callGraphNodeBelongsToProject(target, project));
    ++dereferencedFunctionPointers;
  }
  ROSE_ASSERT(dereferencedFunctionPointers >= 1);
  filteredBuilder.buildCallGraph(builtinFilter());
  size_t defaultMemberTargets = 0;
  for (const auto &[index, edgeNode] :
       filteredBuilder.getGraph()->get_node_index_to_edge_multimap_edgesOut()) {
    auto *edge = isSgDirectedGraphEdge(edgeNode);
    ROSE_ASSERT(edge != nullptr);
    if (functionName(edge->get_from()) ==
            "rex_callgraph_default_member_caller" &&
        functionName(edge->get_to()) == "RexCallGraphDefaultMember") {
      auto *constructor =
          isSgMemberFunctionDeclaration(edge->get_to()->get_SgNode());
      ROSE_ASSERT(constructor != nullptr);
      ROSE_ASSERT(constructor->get_functionModifier().isDefaultConstructor());
      ROSE_ASSERT(
          !constructor->get_functionModifier().isTrivialSpecialMember());
      ++defaultMemberTargets;
    }
  }
  ROSE_ASSERT(defaultMemberTargets == 1);
  for (const auto &[index, node] :
       filteredBuilder.getGraph()->get_node_index_to_node_map()) {
    ROSE_ASSERT(callGraphNodeBelongsToProject(node->get_SgNode(), project));
  }
  size_t filteredOperatorEdges = 0;
  for (const auto &[sourceIndex, edgeNode] :
       filteredBuilder.getGraph()->get_node_index_to_edge_multimap_edgesOut()) {
    SgDirectedGraphEdge *edge = isSgDirectedGraphEdge(edgeNode);
    ROSE_ASSERT(edge != nullptr);
    if (functionName(edge->get_from()) == "rex_callgraph_dependent_operator" &&
        functionName(edge->get_to()) == "operator+") {
      ++filteredOperatorEdges;
    }
  }
  ROSE_ASSERT(filteredOperatorEdges == 1);
  return 0;
}
