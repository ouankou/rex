#include "IntraProcAliasAnalysis.h"

#include "abiStuff.h"
#include "sage3basic.h"

#include <iterator>
#include <queue>

using std::set;
using std::vector;
using VirtualCFG::CFGNode;

SgMemberFunctionRefExp *
AliasAnalysisDetail::virtualFunctionReference(SgFunctionCallExp *call) {
  auto *callee =
      CallTargetSet::unwrapExactImplicitCalleeConversions(call->get_function());
  if (isSgDotExp(callee) || isSgArrowExp(callee))
    callee = isSgBinaryOp(callee)->get_rhs_operand();
  auto *reference = isSgMemberFunctionRefExp(callee);
  if (reference == nullptr)
    return nullptr;
  return reference->get_virtual_call() ? reference : nullptr;
}

namespace {
std::set<SgNode *> intersectWrites(const std::set<SgNode *> &left,
                                   const std::set<SgNode *> &right) {
  std::set<SgNode *> result;
  std::set_intersection(left.begin(), left.end(), right.begin(), right.end(),
                        std::inserter(result, result.end()), left.key_comp());
  return result;
}

void clearAliases(CompReprBase &state, SgNode *storage) {
  ASSERT_not_null(storage);
  std::vector<SgGraphNode *> targets;
  state.computeValues(storage, 0, targets);
  for (SgGraphNode *target : targets)
    for (SgDirectedGraphEdge *edge :
         state.getGraph()->computeEdgeSetOut(target)) {
      const bool removed = state.getGraph()->removeDirectedEdge(edge);
      ROSE_ASSERT(removed);
    }
  if (isSgInitializedName(storage)) {
    state.writes.may.insert(storage);
    state.writes.must.insert(storage);
  }
}

SgNode *pointerDereferenceOperand(SgNode *node, int &depth) {
  while (auto *dereference = isSgPointerDerefExp(node)) {
    node = dereference->get_operand();
    ++depth;
  }
  ASSERT_not_null(node);
  return node;
}

bool isNullPointerValue(SgNode *node) {
  if (isSgNullptrValExp(node) != nullptr)
    return true;
  if (auto *cast = isSgCastExp(node)) {
    cast->validate_semantic_conversion();
    return cast->get_semantic_conversion_kind() ==
           SgCastExp::e_semantic_conversion_NullToPointer;
  }
  return false;
}

bool isReferenceBinding(SgType *type) {
  type = type->stripTypedefsAndModifiers();
  return isSgReferenceType(type) || isSgRvalueReferenceType(type);
}

bool isAliasValueType(SgType *type) {
  type = type->stripTypedefsAndModifiers();
  return isReferenceBinding(type) || isSgPointerType(type) ||
         isSgIntegralType(type) || isSgEnumType(type);
}

unsigned addressBits(const SgCastExp *cast) {
  for (SgType *type : {cast->get_operand()->get_type(), cast->get_type()}) {
    auto *pointer = isSgPointerType(type->stripTypedefsAndModifiers());
    if (pointer == nullptr)
      continue;
    type = pointer->get_base_type();
    while (true) {
      if (auto *alias = isSgTypedefType(type)) {
        type = alias->get_base_type();
      } else if (auto *qualified = isSgModifierType(type)) {
        const auto &modifier = qualified->get_typeModifier();
        // The recorded ABI supplies only the default address-space layout.
        ROSE_ASSERT((!modifier.haveAddressSpace() ||
                     modifier.get_address_space_value() == 0) &&
                    !modifier.isOpenclGlobal() && !modifier.isOpenclLocal() &&
                    !modifier.isOpenclConstant());
        type = qualified->get_base_type();
      } else {
        break;
      }
    }
  }
  return SageInterface::requireTargetDefaultPointerSizeBits(cast);
}

bool retainsAddressBits(SgType *type, const SgCastExp *cast) {
  type = type->stripTypedefsAndModifiers();
  if (!isSgIntegralType(type) && !isSgEnumType(type))
    return false;
  bool isUnsigned;
  unsigned width;
  SageInterface::requireTargetIntegerDomain(type, cast, isUnsigned, width);
  return width >= addressBits(cast);
}

bool preservesAddressProvenance(const SgCastExp *cast) {
  switch (cast->get_semantic_conversion_kind()) {
  case SgCastExp::e_semantic_conversion_NoOp:
  case SgCastExp::e_semantic_conversion_LValueToRValue:
  case SgCastExp::e_semantic_conversion_BitCast:
  case SgCastExp::e_semantic_conversion_DerivedToBase:
  case SgCastExp::e_semantic_conversion_UncheckedDerivedToBase:
  case SgCastExp::e_semantic_conversion_BaseToDerived:
  case SgCastExp::e_semantic_conversion_Dynamic:
  case SgCastExp::e_semantic_conversion_UserDefinedConversion:
    return true;
  case SgCastExp::e_semantic_conversion_LValueToRValueBitCast: {
    auto *type = cast->get_operand()->get_type()->stripTypedefsAndModifiers();
    if (auto *reference = isSgReferenceType(type))
      type = reference->get_base_type()->stripTypedefsAndModifiers();
    return isSgIntegralType(type) || isSgEnumType(type) ||
           isSgFloatingPointType(type);
  }
  case SgCastExp::e_semantic_conversion_PointerToIntegral:
    return retainsAddressBits(cast->get_type(), cast);
  case SgCastExp::e_semantic_conversion_IntegralToPointer:
    return retainsAddressBits(cast->get_operand()->get_type(), cast);
  case SgCastExp::e_semantic_conversion_IntegralCast:
    return retainsAddressBits(cast->get_operand()->get_type(), cast) &&
           retainsAddressBits(cast->get_type(), cast);
  default:
    return false;
  }
}

bool isStorageProjection(const SgCastExp *cast) {
  if (cast->get_value_category() == SgCastExp::e_value_category_prvalue)
    return false;
  switch (cast->get_semantic_conversion_kind()) {
  case SgCastExp::e_semantic_conversion_NoOp:
  case SgCastExp::e_semantic_conversion_UserDefinedConversion:
  case SgCastExp::e_semantic_conversion_BaseToDerived:
  case SgCastExp::e_semantic_conversion_DerivedToBase:
  case SgCastExp::e_semantic_conversion_UncheckedDerivedToBase:
    return true;
  default:
    return false;
  }
}

void applyCast(SgCastExp *cast, CompReprBase &state,
               const ClassHierarchyWrapper &hierarchy) {
  cast->validate_semantic_conversion();
  if (isStorageProjection(cast))
    return;
  if (isNullPointerValue(cast) || !preservesAddressProvenance(cast)) {
    state.addMustAliasRelation({cast, 0}, {cast, -1});
    return;
  }
  AliasRelationNode operand;
  ProcessExpression::processRHS(cast->get_operand(), operand);
  const auto kind = cast->get_semantic_conversion_kind();
  const bool rebind =
      isReferenceBinding(cast->get_type()) ||
      (SageInterface::isPointerType(cast->get_type()) &&
       (kind == SgCastExp::e_semantic_conversion_IntegralToPointer ||
        kind == SgCastExp::e_semantic_conversion_BitCast));
  std::vector<SgNode *> origins;
  if (operand.derefLevel == -1) {
    origins.push_back(operand.var);
  } else {
    std::vector<SgGraphNode *> values;
    state.computeValues(operand.var, operand.derefLevel + 1, values);
    for (auto *value : values)
      origins.push_back(value->get_SgNode());
  }
  if (origins.empty()) {
    // A missing value in reachable flow is unknown. Null has an explicit
    // value identity; pending control flow has no state at all.
    state.addMustAliasRelation({cast, 0}, {cast, -1});
    return;
  }
  if (!rebind && kind != SgCastExp::e_semantic_conversion_Dynamic) {
    state.addMustAliasRelation({cast, 0}, operand);
    return;
  }
  clearAliases(state, cast);
  for (auto *origin : origins) {
    bool concrete = isSgNewExp(origin) ||
                    (isSgInitializedName(origin) &&
                     isSgClassType(isSgInitializedName(origin)
                                       ->get_type()
                                       ->stripTypedefsAndModifiers()));
    if (concrete && kind == SgCastExp::e_semantic_conversion_Dynamic) {
      SgType *actualType = isSgNewExp(origin)
                               ? isSgNewExp(origin)->get_type()->dereference()
                               : isSgInitializedName(origin)->get_type();
      auto *actual = isSgClassType(actualType->stripTypedefsAndModifiers());
      auto *targetType =
          cast->get_type()->dereference()->stripTypedefsAndModifiers();
      if (auto *target = isSgClassType(targetType)) {
        ASSERT_not_null(actual);
        auto *actualOwner = isSgClassDeclaration(
            actual->get_declaration()->get_definingDeclaration());
        auto *targetOwner = isSgClassDeclaration(
            target->get_declaration()->get_definingDeclaration());
        ASSERT_not_null(actualOwner);
        ASSERT_not_null(targetOwner);
        auto *actualDefinition = actualOwner->get_definition();
        auto *targetDefinition = targetOwner->get_definition();
        ASSERT_not_null(actualDefinition);
        ASSERT_not_null(targetDefinition);
        concrete = actualDefinition == targetDefinition ||
                   hierarchy.getAncestorClasses(actualDefinition)
                           .count(targetDefinition) != 0;
      } else {
        concrete = isSgTypeVoid(targetType) != nullptr;
      }
    }
    state.addMayAliasRelation(
        {cast, 0},
        {concrete || isNullPointerValue(origin) ? origin : cast, -1});
  }
}

void bindReference(SgType *type, AliasRelationNode &left,
                   AliasRelationNode &right) {
  if (isReferenceBinding(type)) {
    left.derefLevel = 0;
    --right.derefLevel;
  }
}

void bindArguments(
    const SgExpressionPtrList &actual, const SgInitializedNamePtrList &formal,
    std::vector<std::pair<AliasRelationNode, AliasRelationNode>> &relations) {
  for (size_t index = 0; index < actual.size() && index < formal.size();
       ++index) {
    AliasRelationNode left, right;
    ProcessExpression::processLHS(formal[index], left);
    ProcessExpression::processRHS(actual[index], right);
    bindReference(formal[index]->get_type(), left, right);
    if (left.var != nullptr && right.var != nullptr)
      relations.push_back({left, right});
  }
}

bool mergeCallEntry(
    IntraProcAliasAnalysis &callee, CompReprPtr &incoming,
    const std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
        &arguments) {
  CompReprPtr previous;
  if (callee.getFunctionEntry().get() != nullptr) {
    previous = CompReprPtr(new CompactRepresentation);
    previous += callee.getFunctionEntry();
  }
  callee.setFunctionEntry(incoming);
  auto entry = callee.getFunctionEntry();
  for (const auto &argument : arguments)
    entry.get()->addMayAliasRelation(argument.first, argument.second);
  return previous != entry;
}
} // namespace

void CompactRepresentation::computeAliases(SgGraphNode *node, int derefLevel,
                                           vector<SgGraphNode *> &sol) {
  ASSERT_require(derefLevel >= 0);

  if (node == NULL)
    return;

  if (derefLevel == 0) {
    sol.push_back(node);
  } else {

    std::set<SgDirectedGraphEdge *> edges = graph->computeEdgeSetOut(node);
    for (SgDirectedGraphEdge *ed : edges) {
      computeAliases(ed->get_to(), derefLevel - 1, sol);
    }
  }
}

SgGraphNode *CompactRepresentation::getGraphNode(SgNode *node) {
  ASSERT_require(isSgInitializedName(node) != nullptr ||
                 isSgExpression(node) != nullptr);

  SgGraphNode *g_node;

  if (all_nodes.count(node) == 0) {
    g_node = new SgGraphNode;
    g_node->set_SgNode(node);
    graph->addNode(g_node);
    all_nodes[node] = g_node;
  } else
    g_node = all_nodes.at(node);
  return g_node;
}

void CompactRepresentation::merge(SgIncidenceDirectedGraph *thatGraph) {
  set<SgGraphNode *> nodes = thatGraph->computeNodeSet();

  for (SgGraphNode *node : nodes) {
    std::set<SgDirectedGraphEdge *> edges = thatGraph->computeEdgeSetOut(node);
    SgGraphNode *g_from = getGraphNode(node->get_SgNode());
    for (SgDirectedGraphEdge *ed : edges) {
      addEdge(g_from, getGraphNode(ed->get_to()->get_SgNode()));
    }
  }
}

void CompactRepresentation::merge(const CompReprBase &that) {
  merge(that.getGraph());
  writes.may.insert(that.writes.may.begin(), that.writes.may.end());
  writes.must.insert(that.writes.must.begin(), that.writes.must.end());
}

void CompactRepresentation::addEdge(SgGraphNode *g_from, SgGraphNode *g_to) {

  if (graph->checkIfDirectedGraphEdgeExists(g_from, g_to) == false) {
    graph->addDirectedEdge(new SgDirectedGraphEdge(g_from, g_to));
  }
}

void CompactRepresentation::init() {
  graph = new SgIncidenceDirectedGraph;
  this->all_nodes.clear();
  writes = {};
}

CompactRepresentation::CompactRepresentation(
    const CompactRepresentation &copy) {
  init();
  this->merge(copy);
}

CompactRepresentation &
CompactRepresentation::operator=(const CompactRepresentation &p) {
  if (this != &p) {
    AliasAnalysisDetail::deleteOwnedGraph(graph);
    graph = NULL;
    init();
    this->merge(p);
  }
  return *this;
}
void CompactRepresentation::computeAliases(SgNode *var, int derefLevel,
                                           vector<SgGraphNode *> &nodes) {
  std::vector<SgGraphNode *> values;
  computeValues(var, derefLevel, values);
  for (auto *value : values)
    if (!isNullPointerValue(value->get_SgNode()))
      nodes.push_back(value);
}

void CompactRepresentation::computeValues(SgNode *var, int derefLevel,
                                          vector<SgGraphNode *> &nodes) {
  ASSERT_require(var == nullptr || isSgInitializedName(var) != nullptr ||
                 isSgExpression(var) != nullptr);
  ASSERT_require(derefLevel >= 0);
  const auto found = all_nodes.find(var);
  if (found != all_nodes.end())
    computeAliases(found->second, derefLevel, nodes);
}

void CompactRepresentation::addMustAliasRelation(
    const AliasRelationNode &left, const AliasRelationNode &right) {

  std::vector<SgGraphNode *> leftNodes;
  std::vector<SgGraphNode *> rightNodes;

  if (left.var == NULL)
    return; // function call as parameter not resolved yet

  if (left == right)
    return;

  SgGraphNode *g_from = getGraphNode(left.var);
  computeAliases(getGraphNode(right.var), right.derefLevel + 1, rightNodes);
  computeAliases(g_from, left.derefLevel, leftNodes);

  const std::set<SgGraphNode *> leftTargets(leftNodes.begin(), leftNodes.end());
  for (SgGraphNode *target : leftTargets)
    if (isSgInitializedName(target->get_SgNode()) ||
        isSgNewExp(target->get_SgNode()))
      writes.may.insert(target->get_SgNode());
  auto *storage =
      leftTargets.size() == 1
          ? isSgInitializedName((*leftTargets.begin())->get_SgNode())
          : nullptr;
  // Allocation sites and instance fields summarize multiple objects.
  if (leftTargets.size() == 1 &&
      isSgCastExp((*leftTargets.begin())->get_SgNode())) {
    clearAliases(*this, (*leftTargets.begin())->get_SgNode());
  } else if (storage != nullptr &&
             (isSgClassDefinition(storage->get_scope()) == nullptr ||
              storage->get_storageModifier().isStatic())) {
    clearAliases(*this, storage);
  }

  for (SgGraphNode *leftNode : leftTargets)
    for (SgGraphNode *rightNode : rightNodes)
      addEdge(leftNode, rightNode);
}

void CompactRepresentation::addMayAliasRelation(
    const AliasRelationNode &left, const AliasRelationNode &right) {

  std::vector<SgGraphNode *> leftNodes;
  std::vector<SgGraphNode *> rightNodes;
  if (left.var == NULL)
    return;

  if (left == right)
    return;

  SgGraphNode *g_from = getGraphNode(left.var);
  SgGraphNode *g_to = getGraphNode(right.var);

  computeAliases(g_to, right.derefLevel + 1, rightNodes);
  computeAliases(g_from, left.derefLevel, leftNodes);

  for (SgGraphNode *leftNode : leftNodes)
    for (SgGraphNode *rightNode : rightNodes)
      addEdge(leftNode, rightNode);
}

bool CompactRepresentation::operator==(
    const CompactRepresentation &that) const {
  if (writes.may != that.writes.may || writes.must != that.writes.must ||
      all_nodes.size() != that.all_nodes.size())
    return false;
  for (const auto &[node, graphNode] : all_nodes) {
    const auto counterpart = that.all_nodes.find(node);
    if (counterpart == that.all_nodes.end())
      return false;
    const auto outgoing = graph->computeEdgeSetOut(graphNode);
    if (outgoing.size() !=
        that.graph->computeEdgeSetOut(counterpart->second).size())
      return false;
    for (SgDirectedGraphEdge *edge : outgoing) {
      const auto target = that.all_nodes.find(edge->get_to()->get_SgNode());
      if (target == that.all_nodes.end() ||
          !that.graph->checkIfDirectedGraphEdgeExists(counterpart->second,
                                                      target->second))
        return false;
    }
  }
  return true;
}

bool CompReprPtr::operator==(const CompReprPtr &that) const {
  CompactRepresentation *a = (static_cast<CompactRepresentation *>(ptr.get()));
  CompactRepresentation *b = (static_cast<CompactRepresentation *>(that.get()));
  if (a == NULL && b == NULL)
    return true;
  if (a == NULL || b == NULL)
    return false;
  return *a == *b;
}

void CompReprPtr::operator+=(const CompReprPtr &that) const {
  if (that.get() != NULL) {
    this->get()->merge(*(that.get()));
  }
}

void AliasInfoGenerator::init(SgGraphNode *n) {
  std::vector<std::pair<AliasRelationNode, AliasRelationNode>> rel;
  ins[n] = CompReprPtr();
  outs[n] = CompReprPtr();
  aliasRelations[n] = rel;
}

bool AliasInfoGenerator::addNewAliasRelation(
    SgGraphNode *node,
    std::pair<AliasRelationNode, AliasRelationNode> a_relation) {
  auto &relations = aliasRelations.at(node);
  if (std::find(relations.begin(), relations.end(), a_relation) !=
      relations.end())
    return false;
  relations.push_back(a_relation);
  return true;
}
CompReprPtr AliasInfoGenerator::getExitData(SgGraphNode *node) {
  return outs.at(node);
}
CompReprPtr AliasInfoGenerator::getEntryData(SgGraphNode *node) {
  return ins.at(node);
}

std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
AliasInfoGenerator::getAliasRelations(SgGraphNode *node) {
  return aliasRelations.at(node);
}

void IntraProcAliasAnalysis::buildCFG() {
  SgFunctionDefinition *defn = isSgFunctionDefinition(
      isSgFunctionDeclaration(
          isSgFunctionDeclaration(head)->get_definingDeclaration())
          ->get_definition());

  ASSERT_not_null(defn);
  cfg = new StaticCFG::CustomFilteredCFG<AliasCfgFilter>(defn);
  // run a bfs and list the nodes
  cfgNodes.clear();
  std::queue<SgGraphNode *> workQ;
  vector<SgGraphNode *> visited;
  workQ.push(cfg->getEntry());
  visited.push_back(cfg->getEntry());
  while (!workQ.empty()) {
    SgGraphNode *node = workQ.front();
    workQ.pop();
    cfgNodes.push_back(node);

    std::vector<SgDirectedGraphEdge *> outs = cfg->getOutEdges(node);
    for (unsigned int i = 0; i < outs.size(); i++)
      if (find(visited.begin(), visited.end(), outs[i]->get_to()) ==
          visited.end()) {
        workQ.push(outs[i]->get_to());
        visited.push_back(outs[i]->get_to());
      }
  }
}

IntraProcAliasAnalysis::IntraProcAliasAnalysis(
    SgNode *head, ClassHierarchyWrapper *_classHierarchy,
    CallGraphBuilder *_cgBuilder,
    std::unordered_map<SgFunctionDeclaration *, IntraProcAliasAnalysis *>
        &_mapping,
    std::unordered_map<SgExpression *, std::vector<SgFunctionDeclaration *>>
        &_resolver)
    : IntraProcDataFlowAnalysis<SgGraphNode, CompReprPtr>(head),
      classHierarchy(_classHierarchy), cfg(0), cgBuilder(_cgBuilder),
      mapping(_mapping), resolver(_resolver) {

  ROSE_ASSERT(isSgFunctionDeclaration(head));

  buildCFG();
  gen = new AliasInfoGenerator;
  CollectAliasRelations car(cfg, gen);
  car.run();
};

void IntraProcAliasAnalysis::run() {
  ASSERT_not_null(head);
  IntraProcDataFlowAnalysis<SgGraphNode, CompReprPtr>::run();
}

bool IntraProcAliasAnalysis::runCheck() {

  run();
  auto *definition = isSgFunctionDefinition(cfg->getEntry()->get_SgNode());
  ASSERT_not_null(definition);
  auto *sourceFunction = definition->get_declaration();
  // Now the inter procedural part
  //  First add the Virtual Functions based on object resolved so far
  //  Compute the IN of each function called from this one based on
  //     EntryS = InC U EntryS
  //  Also add the parameter aliases
  vector<SgFunctionCallExp *> functionCalls =
      SageInterface::querySubTree<SgFunctionCallExp>(sourceFunction,
                                                     V_SgFunctionCallExp);
  CFGNode cfgn;
  SgGraphNode *graphNode;
  bool change = false;
  // For each Function Call Site
  for (unsigned index = 0; index < functionCalls.size(); index++) {
    SgFunctionCallExp *funcCall = functionCalls[index];
    if (SageInterface::getEnclosingFunctionDeclaration(funcCall) !=
            sourceFunction ||
        CallTargetSet::isNonExecutableCallExpression(funcCall))
      continue;
    cfgn = funcCall->cfgForEnd();
    graphNode = cfg->toGraphNode(cfgn);
    if (graphNode == NULL) {
      // example: function call inside catch block is not added on controlflow
      // graph
      continue;
    }
    CompReprPtr callSiteIN = getCFGInData(graphNode);
    if (callSiteIN.get() == nullptr)
      continue;
    ROSE_ASSERT(resolver.count(funcCall) != 0);
    if (AliasAnalysisDetail::virtualFunctionReference(funcCall) != nullptr) {
      change |= updateVirtualFunctionInCallGraph(funcCall, callSiteIN);
    }

    // For each referenced functions
    for (unsigned j = 0; j < resolver.at(funcCall).size(); j++) {
      SgFunctionDeclaration *declaration = resolver.at(funcCall)[j];
      ASSERT_not_null(declaration);
      if (mapping.count(declaration) == 0) {
        // Compiler Generated functions;
        continue;
      }

      std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
          arg_relations;

      getFunctionParametersAliasRelations(funcCall, resolver.at(funcCall)[j],
                                          arg_relations);
      change |=
          mergeCallEntry(*mapping.at(declaration), callSiteIN, arg_relations);
    }
  }

  // handle the constructors
  vector<SgConstructorInitializer *> constrs =
      SageInterface::querySubTree<SgConstructorInitializer>(
          sourceFunction, V_SgConstructorInitializer);
  for (unsigned index = 0; index < constrs.size(); index++) {
    SgConstructorInitializer *con = constrs[index];
    if (SageInterface::getEnclosingFunctionDeclaration(con) != sourceFunction ||
        CallTargetSet::isNonExecutableCallExpression(con))
      continue;
    cfgn = con->cfgForEnd();
    graphNode = cfg->toGraphNode(cfgn);
    ASSERT_not_null(graphNode);
    CompReprPtr callSiteIN = getCFGInData(graphNode);
    if (callSiteIN.get() == nullptr)
      continue;

    ROSE_ASSERT(resolver.count(con) != 0);

    // For each referenced functions
    for (unsigned j = 0; j < resolver.at(con).size(); j++) {
      SgFunctionDeclaration *declaration = resolver.at(con)[j];
      ASSERT_not_null(declaration);

      if (mapping.count(declaration) == 0) {
        // Compiler Generated functions;
        continue;
      }

      std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
          arg_relations;

      getConstructorParametersAliasRelations(con, resolver.at(con)[j],
                                             arg_relations);

      change |=
          mergeCallEntry(*mapping.at(declaration), callSiteIN, arg_relations);
    }
  }

  const CompReprPtr previousExit = getFunctionExit();
  auto *functionExit = cfg->getExit();
  CompReprPtr c_out =
      functionExit != nullptr ? getCFGOutData(functionExit) : CompReprPtr();
  setFunctionExit(c_out);
  change |= (previousExit != getFunctionExit());
  return change;
}

void IntraProcAliasAnalysis::getFunctionParametersAliasRelations(
    SgFunctionCallExp *f_exp, SgFunctionDeclaration *funcDecl,
    std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
        &arg_relations) {

  ASSERT_not_null(funcDecl);

  auto *definition =
      isSgFunctionDeclaration(funcDecl->get_definingDeclaration());
  ASSERT_not_null(definition);
  bindArguments(f_exp->get_args()->get_expressions(),
                definition->get_parameterList()->get_args(), arg_relations);
}
void IntraProcAliasAnalysis::getConstructorParametersAliasRelations(
    SgConstructorInitializer *f_exp, SgFunctionDeclaration *funcDecl,
    std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
        &arg_relations) {

  ASSERT_not_null(funcDecl);
  f_exp->validate_argument_context();
  auto *definition =
      isSgFunctionDeclaration(funcDecl->get_definingDeclaration());
  ASSERT_not_null(definition);
  SgInitializedNamePtrList &formal =
      definition->get_parameterList()->get_args();
  if (f_exp->get_argument_source() !=
      SgConstructorInitializer::e_constructor_argument_expressions) {
    auto *owner = SageInterface::getEnclosingFunctionDeclaration(f_exp);
    ASSERT_not_null(owner);
    const auto &incoming = owner->get_parameterList()->get_args();
    ASSERT_require(incoming.size() == formal.size());
    for (size_t index = 0; index < formal.size(); ++index) {
      ASSERT_require(SageInterface::isEquivalentType(
          incoming[index]->get_type(), formal[index]->get_type()));
      if (isSgTypeEllipse(formal[index]->get_type()))
        continue;
      AliasRelationNode left, right;
      ProcessExpression::processLHS(formal[index], left);
      ProcessExpression::processRHS(incoming[index], right);
      bindReference(formal[index]->get_type(), left, right);
      if (left.var != nullptr && right.var != nullptr)
        arg_relations.push_back({left, right});
    }
    return;
  }

  bindArguments(f_exp->get_args()->get_expressions(), formal, arg_relations);
}

bool IntraProcAliasAnalysis::addVirtualFunction(SgType *type,
                                                SgFunctionCallExp *funcExp) {

  auto *func = AliasAnalysisDetail::virtualFunctionReference(funcExp);
  ASSERT_not_null(func);
  SgMemberFunctionDeclaration *memberFunctionDeclaration =
      isSgMemberFunctionDeclaration(func->get_symbol()->get_declaration());

  ASSERT_not_null(memberFunctionDeclaration);

  SgClassType *classType = isSgClassType(type);

  ASSERT_not_null(classType);

  SgFunctionDeclaration *functionDeclaration =
      CallTargetSet::getFirstVirtualFunctionDefinitionFromAncestors(
          classType, memberFunctionDeclaration, classHierarchy);

  if (functionDeclaration == NULL)
    return false;

  functionDeclaration = canonicalFunctionDeclForCallGraph(functionDeclaration);

  SgIncidenceDirectedGraph *callGraph = cgBuilder->getGraph();

  SgGraphNode *toNode = cgBuilder->hasGraphNodeFor(functionDeclaration);
  if (toNode == nullptr) {
    toNode =
        new SgGraphNode(functionDeclaration->get_qualified_name().getString());
    toNode->set_SgNode(functionDeclaration);
    callGraph->addNode(toNode);
    cgBuilder->addGraphNodeMapping(functionDeclaration, toNode);
  }

  SgFunctionDeclaration *fromDecl = isSgFunctionDeclaration(head);

  SgGraphNode *fromNode = cgBuilder->hasGraphNodeFor(fromDecl);

  ASSERT_not_null(fromNode);
  ASSERT_not_null(toNode);

  bool change = false;
  if (callGraph->checkIfDirectedGraphEdgeExists(fromNode, toNode) == false) {
    callGraph->addDirectedEdge(fromNode, toNode);
    change = true;
  }
  auto &targets = resolver.at(funcExp);
  if (std::find(targets.begin(), targets.end(), functionDeclaration) ==
      targets.end()) {
    targets.push_back(functionDeclaration);
    change = true;
  }
  return change;
}

bool IntraProcAliasAnalysis::updateVirtualFunctionInCallGraph(
    SgFunctionCallExp *funcCall, CompReprPtr &callSiteIN) {
  auto *reference = AliasAnalysisDetail::virtualFunctionReference(funcCall);
  ASSERT_not_null(reference);
  auto *access = isSgBinaryOp(reference->get_parent());
  ROSE_ASSERT(isSgArrowExp(access) || isSgDotExp(access));
  AliasRelationNode leftNode;
  ProcessExpression::processRHS(access->get_lhs_operand(), leftNode);
  if (isSgArrowExp(access))
    ++leftNode.derefLevel;
  std::vector<SgType *> aliases;
  getAliases(callSiteIN, leftNode, aliases);
  bool change = false;
  for (auto *type : aliases)
    change |= addVirtualFunction(type, funcCall);
  return change;
}

IntraProcAliasAnalysis::~IntraProcAliasAnalysis() {
  delete cfg;
  delete gen;
}

void IntraProcAliasAnalysis::getAliases(CompReprPtr &ptr,
                                        AliasRelationNode &node,
                                        std::vector<SgType *> &aliases) {
  if (isNullPointerValue(node.var))
    return;

  std::vector<SgGraphNode *> graph_aliases;
  ptr.get()->computeAliases(node.var, node.derefLevel, graph_aliases);

  for (unsigned int i = 0; i < graph_aliases.size(); i++) {
    SgNode *identity = graph_aliases[i]->get_SgNode();
    if (auto *variable = isSgInitializedName(identity)) {
      aliases.push_back(variable->get_type());
    } else if (auto *allocation = isSgNewExp(identity)) {
      aliases.push_back(allocation->get_type()->dereference());
    } else {
      auto *value = isSgExpression(identity);
      ASSERT_not_null(value);
      SgType *type = value->get_type();
      bool indirect = value->isLValue();
      if (auto *cast = isSgCastExp(value))
        indirect |=
            cast->get_value_category() != SgCastExp::e_value_category_prvalue;
      while (true) {
        type = type->stripTypedefsAndModifiers();
        if (auto *pointer = isSgPointerType(type))
          type = pointer->get_base_type();
        else if (auto *reference = isSgReferenceType(type))
          type = reference->get_base_type();
        else if (auto *reference = isSgRvalueReferenceType(type))
          type = reference->get_base_type();
        else
          break;
        indirect = true;
      }
      auto *classType = isSgClassType(type);
      ASSERT_not_null(classType);
      aliases.push_back(classType);
      if (indirect) {
        auto *definition = isSgClassDeclaration(
            classType->get_declaration()->get_definingDeclaration());
        ASSERT_not_null(definition);
        ASSERT_not_null(definition->get_definition());
        for (auto *subclass :
             classHierarchy->getSubclasses(definition->get_definition()))
          aliases.push_back(subclass->get_declaration()->get_type());
      }
    }
  }
}
void IntraProcAliasAnalysis::setFunctionEntry(CompReprPtr &n) {
  ASSERT_not_null(n.get());
  CompReprPtr entry = getFunctionEntry();
  if (entry.get() == nullptr) {
    entry = CompReprPtr(new CompactRepresentation);
    gen->setEntryData(cfg->getEntry(), entry);
  }
  entry += n;
  entry.get()->writes = {};
}

void IntraProcAliasAnalysis::setFunctionExit(CompReprPtr &n) { exit = n; }

CompReprPtr IntraProcAliasAnalysis::meet_data(const CompReprPtr &d1,
                                              const CompReprPtr &d2) {

  if (d1.get() == nullptr && d2.get() == nullptr)
    return CompReprPtr();
  CompReprPtr result = CompReprPtr(new CompactRepresentation);
  result += d1;
  result += d2;
  if (d1.get() != nullptr && d2.get() != nullptr)
    result.get()->writes.must =
        intersectWrites(d1.get()->writes.must, d2.get()->writes.must);

  return result;
};

std::vector<SgGraphNode *>
IntraProcAliasAnalysis::getPredecessors(SgGraphNode *n) {
  std::vector<SgGraphNode *> nodes;
  cfg->getGraph()->getPredecessors(n, nodes);
  return nodes;
}

void ProcessExpression::processLHS(SgNode *node,
                                   struct AliasRelationNode &arNode) {

  SgInitializedName *variable = nullptr;
  SgVarRefExp *var_exp;
  int derefLevel = 0;
  switch (node->variantT()) {

  case V_SgInitializedName: {
    SgInitializedName *init_exp = isSgInitializedName(node);

    ASSERT_not_null(init_exp);

    variable = init_exp;

  } break;

  case V_SgFunctionCallExp: {
    auto *call = isSgFunctionCallExp(node);
    if (isReferenceBinding(call->get_type())) {
      arNode.var = call;
      arNode.derefLevel = 1;
      return;
    }
  } break;

  case V_SgVarRefExp: {
    var_exp = isSgVarRefExp(node);
    ASSERT_not_null(var_exp);

    ASSERT_not_null(var_exp->get_symbol());
    variable = var_exp->get_symbol()->get_declaration();
    ASSERT_not_null(variable);
  } break;

  case V_SgPointerDerefExp: {
    SgPointerDerefExp *deref_exp = isSgPointerDerefExp(node);
    ASSERT_not_null(deref_exp);
    ProcessExpression::processLHS(
        pointerDereferenceOperand(deref_exp, derefLevel), arNode);
    arNode.derefLevel += derefLevel;
    return;
  } break;

  case V_SgDotExp:
  case V_SgArrowExp: {
    SgBinaryOp *bin_exp = isSgBinaryOp(node);
    if (bin_exp != NULL) {
      ProcessExpression::processLHS(bin_exp->get_rhs_operand(), arNode);
      return;
    }
  } break;

  case V_SgCastExp: {
    auto *cast = isSgCastExp(node);
    cast->validate_semantic_conversion();
    if (cast->get_semantic_conversion_kind() ==
            SgCastExp::e_semantic_conversion_LValueToRValue ||
        cast->get_semantic_conversion_kind() ==
            SgCastExp::e_semantic_conversion_NoOp) {
      ProcessExpression::processLHS(cast->get_operand(), arNode);
      return;
    }
  } break;

  default:
    variable = nullptr;
  };

  if (variable != nullptr) {
    if (isReferenceBinding(variable->get_type()))
      derefLevel = 1;
    else if (!isAliasValueType(variable->get_type()))
      variable = nullptr;
  }

  arNode.var = variable;
  arNode.derefLevel = derefLevel;
}

void ProcessExpression::processRHS(SgNode *node,
                                   struct AliasRelationNode &arNode) {
  SgNode *identity = nullptr;
  SgVarRefExp *var_exp;
  int derefLevel = 0;

  switch (node->variantT()) {

  case V_SgFunctionCallExp:
    identity = node;
    derefLevel = isReferenceBinding(isSgFunctionCallExp(node)->get_type());
    break;

  case V_SgConditionalExp:
    identity = node;
    break;

  case V_SgInitializedName:
    identity = node;
    derefLevel = isReferenceBinding(isSgInitializedName(node)->get_type());
    break;

  case V_SgVarRefExp: {
    var_exp = isSgVarRefExp(node);

    ASSERT_not_null(var_exp);

    ASSERT_not_null(var_exp->get_symbol());
    identity = var_exp->get_symbol()->get_declaration();
    ASSERT_not_null(identity);
    derefLevel = isReferenceBinding(isSgInitializedName(identity)->get_type());
  } break;

  case V_SgPointerDerefExp: {
    SgPointerDerefExp *deref_exp = isSgPointerDerefExp(node);
    ASSERT_not_null(deref_exp);
    ProcessExpression::processRHS(
        pointerDereferenceOperand(deref_exp, derefLevel), arNode);
    arNode.derefLevel += derefLevel;
    return;
  } break;

  case V_SgAddressOfOp: {
    SgAddressOfOp *add_exp = isSgAddressOfOp(node);

    ASSERT_not_null(add_exp);

    ProcessExpression::processRHS(add_exp->get_operand(), arNode);

    identity = arNode.var;
    derefLevel = arNode.derefLevel - 1;

  } break;
  case V_SgDotExp:
  case V_SgArrowExp: {
    SgBinaryOp *bin_exp = isSgBinaryOp(node);
    if (bin_exp != NULL) {
      ProcessExpression::processRHS(bin_exp->get_rhs_operand(), arNode);
      return;
    }
  } break;
  case V_SgAssignOp: {
    SgExpression *lhs = NULL;
    SgExpression *rhs = NULL;
    if (SageInterface::isAssignmentStatement(node, &lhs, &rhs)) {

      ASSERT_not_null(rhs);
      ProcessExpression::processRHS(rhs, arNode);
      return;
    }
  } break;

  case V_SgCommaOpExp:
    ProcessExpression::processRHS(isSgCommaOpExp(node)->get_rhs_operand(),
                                  arNode);
    return;

  case V_SgCastExp: {
    auto *cast = isSgCastExp(node);
    cast->validate_semantic_conversion();
    if (isStorageProjection(cast)) {
      ProcessExpression::processRHS(cast->get_operand(), arNode);
      return;
    }
    identity = node;
  } break;

  case V_SgNewExp: {
    identity = isSgNewExp(node);
    ASSERT_not_null(identity);
    derefLevel = derefLevel - 1;
  } break;
  default:
    identity = isSgExpression(node);
    ROSE_ASSERT(identity == nullptr ||
                isSgExpression(identity)->has_semantic_value_type());
  }

  arNode.derefLevel = derefLevel;
  arNode.var = identity;
}

void CollectAliasRelations::processNode(SgGraphNode *g_node) {

  SgNode *node = g_node->get_SgNode();

  AliasRelationNode leftARNode, rightARNode;
  SgNode *lhs = NULL;
  SgNode *rhs = NULL;

  switch (node->variantT()) {
  case V_SgConditionalExp: {
    auto *conditional = isSgConditionalExp(node);
    conditional->validate();
    SgExpression *trueValue =
        conditional->get_operator_kind() ==
                SgConditionalExp::e_conditional_operator_standard
            ? conditional->get_true_exp()
            : conditional->get_conditional_exp();
    for (auto *value : {trueValue, conditional->get_false_exp()}) {
      ProcessExpression::processRHS(value, rightARNode);
      if (rightARNode.var != nullptr)
        g->addNewAliasRelation(g_node, {{conditional, 0}, rightARNode});
    }
    return;
  }
  case V_SgAssignOp: {
    SgExpression *lhs_expr = NULL;
    SgExpression *rhs_expr = NULL;
    if (!SageInterface::isAssignmentStatement(node, &lhs_expr, &rhs_expr)) {
      lhs = rhs = NULL;
    } else {
      lhs = lhs_expr;
      rhs = rhs_expr;
    }
  } break;

  case V_SgAssignInitializer: {
    SgAssignInitializer *assgn_i = isSgAssignInitializer(node);
    ASSERT_not_null(assgn_i);

    if (SgInitializedName *init_name =
            isSgInitializedName(assgn_i->get_parent())) {
      lhs = init_name;
    } else {
      lhs = isSgExpression(assgn_i->get_parent());
    }
    rhs = assgn_i->get_operand();

  } break;

  case V_SgReturnStmt: {
    SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(
        isSgFunctionDefinition(cfg->getEntry()->get_SgNode())
            ->get_declaration());

    ASSERT_not_null(functionDeclaration);

    SgFunctionType *functionType = functionDeclaration->get_type();
    SgType *returnType = functionType->get_return_type();

    if (!isAliasValueType(returnType))
      return;

    SgReturnStmt *returnStmt = isSgReturnStmt(node);
    rhs = returnStmt->get_expression();

    ProcessExpression::processRHS(rhs, rightARNode);
    if (isReferenceBinding(returnType))
      --rightARNode.derefLevel;
    g->addReturnStmt(rightARNode);
    return;
  } break;

  default:
    lhs = rhs = NULL;
    leftARNode.var = rightARNode.var = NULL;
  }

  if (lhs != NULL)
    ProcessExpression::processLHS(lhs, leftARNode);

  if (rhs != NULL)
    ProcessExpression::processRHS(rhs, rightARNode);

  if (isSgAssignInitializer(node) && isSgInitializedName(lhs))
    bindReference(isSgInitializedName(lhs)->get_type(), leftARNode,
                  rightARNode);

  if (leftARNode.var != NULL && rightARNode.var != NULL) {
    g->addNewAliasRelation(g_node, {leftARNode, rightARNode});
  }
}

void CollectAliasRelations::recursiveCollect(
    SgGraphNode *node, std::unordered_map<SgGraphNode *, COLOR> &colors) {

  if (node == NULL)
    return;

  colors[node] = GREY;

  processNode(node);

  std::vector<SgDirectedGraphEdge *> outs = cfg->getOutEdges(node);

  for (unsigned int i = 0; i < outs.size(); i++) {

    SgGraphNode *out = outs[i]->get_to();
    if (colors.at(out) == WHITE) {

      recursiveCollect(outs[i]->get_to(), colors);
    }
  }
  colors[node] = BLACK;
}

void CollectAliasRelations::run() {
  ASSERT_not_null(cfg);
  SgGraphNode *graphNode = cfg->getEntry();
  ASSERT_not_null(graphNode);
  ASSERT_not_null(isSgFunctionDefinition(graphNode->get_SgNode()));

  SgIncidenceDirectedGraph *graph = cfg->getGraph();

  set<SgGraphNode *> allNodes = graph->computeNodeSet();

  std::unordered_map<SgGraphNode *, COLOR> colors;

  for (SgGraphNode *node : allNodes) {
    colors[node] = WHITE;
    g->init(node);
  }

  recursiveCollect(graphNode, colors);
}

void IntraProcAliasAnalysis::applyCFGTransferFunction(SgGraphNode *s) {

  /**
   * If no relation exists then out =in
   * else
   *  out = (in -kill) + gen
   */
  std::vector<std::pair<AliasRelationNode, AliasRelationNode>> relations =
      gen->getAliasRelations(s);
  CompReprPtr repr(new CompactRepresentation);

  CompReprPtr in = gen->getEntryData(s); // out =in

  if (in.get() == nullptr) {
    gen->setExitData(s, CompReprPtr());
    return;
  }
  ASSERT_not_null(in.get());

  repr += in;

  if (auto *cast = isSgCastExp(s->get_SgNode())) {
    applyCast(cast, *repr.get(), *classHierarchy);
  } else if (s->get_SgNode()->variantT() == V_SgFunctionCallExp ||
             s->get_SgNode()->variantT() == V_SgConstructorInitializer) {
    SgExpression *exp = isSgExpression(s->get_SgNode());

    ROSE_ASSERT(resolver.count(exp) != 0);
    bool hasReturningTarget = false;
    std::vector<std::pair<SgFunctionDeclaration *, IntraProcAliasAnalysis *>>
        returning;
    AliasWriteEffects effects;
    for (SgFunctionDeclaration *decl : resolver.at(exp)) {
      ASSERT_not_null(decl);
      if (mapping.count(decl) != 0) {
        IntraProcAliasAnalysis *ipa = mapping.at(decl);
        ASSERT_not_null(ipa);
        CompReprPtr thatExit = ipa->getFunctionExit();
        if (thatExit.get() != nullptr) {
          effects.must =
              hasReturningTarget
                  ? intersectWrites(effects.must, thatExit.get()->writes.must)
                  : thatExit.get()->writes.must;
          effects.may.insert(thatExit.get()->writes.may.begin(),
                             thatExit.get()->writes.may.end());
          hasReturningTarget = true;
          returning.push_back({decl, ipa});
        }
      } else {
        effects.must.clear();
        hasReturningTarget = true;
        if (isSgFunctionCallExp(exp) && isAliasValueType(exp->get_type()))
          repr.get()->addMayAliasRelation({exp, 0}, {exp, -1});
      }
    }

    if (!hasReturningTarget) {
      gen->setExitData(s, CompReprPtr());
      return;
    }
    if (isSgConstructorInitializer(exp) ||
        isSgClassType(exp->get_type()->stripTypedefsAndModifiers()))
      repr.get()->addMayAliasRelation({exp, 0}, {exp, -1});

    for (SgNode *storage : effects.must)
      clearAliases(*repr.get(), storage);
    repr.get()->writes.may.insert(effects.may.begin(), effects.may.end());
    for (const auto &[decl, ipa] : returning) {
      CompReprPtr thatExit = ipa->getFunctionExit();
      std::set<SgNode *> entryIdentities;
      for (SgGraphNode *entry :
           ipa->getFunctionEntry().get()->getGraph()->computeNodeSet())
        entryIdentities.insert(entry->get_SgNode());
      for (SgGraphNode *output : thatExit.get()->getGraph()->computeNodeSet()) {
        SgNode *identity = output->get_SgNode();
        auto *owner = SageInterface::getEnclosingFunctionDeclaration(identity);
        const bool calleeOwned =
            owner != nullptr &&
            canonicalFunctionDeclForCallGraph(owner) == decl;
        if (entryIdentities.count(identity) != 0 && !calleeOwned &&
            thatExit.get()->writes.may.count(identity) == 0)
          continue;
        for (SgDirectedGraphEdge *edge :
             thatExit.get()->getGraph()->computeEdgeSetOut(output))
          repr.get()->addMayAliasRelation({identity, 0},
                                          {edge->get_to()->get_SgNode(), -1});
      }
      if (isSgFunctionCallExp(exp))
        for (const auto &returned : ipa->getReturnStmts())
          repr.get()->addMayAliasRelation({exp, 0}, returned);
    }

  } else {
    for (const auto &relation : relations)
      repr.get()->addMustAliasRelation(relation.first, relation.second);
    if (relations.empty())
      if (auto *expression = isSgExpression(s->get_SgNode())) {
        AliasRelationNode value;
        ProcessExpression::processRHS(expression, value);
        if (value == AliasRelationNode{expression, 0})
          repr.get()->addMustAliasRelation(value, {expression, -1});
      }
  }

  gen->setExitData(s, repr);
}

#define TEMPLATE_ONLY
#include "IntraProcDataFlowAnalysis.C"
template class IntraProcDataFlowAnalysis<SgGraphNode, CompReprPtr>;

template class StaticCFG::CustomFilteredCFG<AliasCfgFilter>;
