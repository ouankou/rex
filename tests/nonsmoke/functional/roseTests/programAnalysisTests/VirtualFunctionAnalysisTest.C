#include "sage3basic.h"

#include "AstDOTGeneration.h"

#include "CallGraph.h"

#include "CommandOptions.h"

#include "VirtualFunctionAnalysis.h"

#include <algorithm>
#include <iostream>

#include <filesystem>

#include <memory>

#include <set>

#include <string>

#include "rose_path_resolver.h"

using namespace std;

namespace {
void checkAliasUpdates() {
  auto name = [](const char *identifier, SgType *type) {
    return std::unique_ptr<SgInitializedName>(
        SageBuilder::buildInitializedName_nfi(identifier, type, nullptr));
  };
  auto *integer = SageBuilder::buildIntType();
  auto *pointer = SageBuilder::buildPointerType(integer);
  auto a = name("a", integer);
  auto b = name("b", integer);
  auto c = name("c", integer);
  auto p = name("p", pointer);
  auto q = name("q", pointer);
  auto r = name("r", SageBuilder::buildPointerType(pointer));
  CompactRepresentation aliases;
  aliases.addMustAliasRelation({p.get(), 0}, {a.get(), -1});
  aliases.addMayAliasRelation({p.get(), 0}, {b.get(), -1});
  aliases.addMustAliasRelation({p.get(), 0}, {c.get(), -1});
  std::vector<SgGraphNode *> targets;
  aliases.computeAliases(p.get(), 1, targets);
  ROSE_ASSERT(targets.size() == 1 && targets.front()->get_SgNode() == c.get());

  aliases.addMustAliasRelation({p.get(), 0}, {a.get(), -1});
  aliases.addMustAliasRelation({q.get(), 0}, {b.get(), -1});
  aliases.addMustAliasRelation({r.get(), 0}, {p.get(), -1});
  aliases.addMayAliasRelation({r.get(), 0}, {q.get(), -1});
  aliases.addMustAliasRelation({r.get(), 1}, {c.get(), -1});
  for (auto binding :
       {std::make_pair(p.get(), a.get()), std::make_pair(q.get(), b.get())}) {
    targets.clear();
    aliases.computeAliases(binding.first, 1, targets);
    std::set<SgNode *> identities;
    for (auto *target : targets)
      identities.insert(target->get_SgNode());
    ROSE_ASSERT(identities == (std::set<SgNode *>{binding.second, c.get()}));
  }
  std::unique_ptr<SgNullptrValExp> nullValue(
      SageBuilder::buildNullptrValExp_nfi());
  AliasRelationNode empty;
  ProcessExpression::processRHS(nullValue.get(), empty);
  aliases.addMustAliasRelation({p.get(), 0}, empty);
  targets.clear();
  aliases.computeAliases(p.get(), 1, targets);
  ROSE_ASSERT(targets.empty());
}

class CallSiteAliasAnalysis : public IntraProcAliasAnalysis {
public:
  using IntraProcAliasAnalysis::addVirtualFunction;
  using IntraProcAliasAnalysis::IntraProcAliasAnalysis;

  void checkExitSummary() {
    CompReprPtr boundary(new CompactRepresentation);
    setFunctionEntry(boundary);
    auto *definition = isSgFunctionDefinition(cfg->getEntry()->get_SgNode());
    StaticCFG::CFG full(definition);
    auto *functionExit = cfg->getExit();
    ROSE_ASSERT((functionExit != nullptr) == (full.getExit() != nullptr));
    if (functionExit != nullptr)
      ROSE_ASSERT(cfg->toCFGNode(functionExit) == definition->cfgForEnd());
    std::reverse(cfgNodes.begin(), cfgNodes.end());
    runCheck();
    CompReprPtr expected =
        functionExit != nullptr ? getCFGOutData(functionExit) : CompReprPtr();
    ROSE_ASSERT(getFunctionExit() == expected);
  }
};

class ConversionContractAnalysis : public VirtualFunctionAnalysis {
public:
  using VirtualFunctionAnalysis::VirtualFunctionAnalysis;

  void checkLostProvenance() {
    unsigned checked = 0;
    for (const auto &[function, analysis] : intraAliases) {
      if (function->get_name() != "rex_vfa_narrow_integer_sink" &&
          function->get_name() != "rex_vfa_boolean_integer_sink")
        continue;
      auto *definition =
          isSgFunctionDeclaration(function->get_definingDeclaration());
      ROSE_ASSERT(definition != nullptr && definition->get_args().size() == 1);
      auto entry = analysis->getFunctionEntry();
      ASSERT_not_null(entry.get());
      std::vector<SgGraphNode *> values;
      entry.get()->computeValues(definition->get_args().front(), 1, values);
      ROSE_ASSERT(!values.empty());
      for (auto *value : values) {
        auto *expression = isSgExpression(value->get_SgNode());
        ROSE_ASSERT(expression != nullptr && !isSgNewExp(expression));
        ROSE_ASSERT(isSgIntegralType(
            expression->get_type()->stripTypedefsAndModifiers()));
      }
      ++checked;
    }
    ROSE_ASSERT(checked == 2);
  }
};

std::filesystem::path resolveDotOutputDir(const RosePathRoots &roots) {
  std::filesystem::path output_dir;
  std::error_code ec;
  if (!roots.build_root.empty()) {
    output_dir =
        std::filesystem::path(roots.build_root) / "tests" / "dot-output";
  } else {
    output_dir = std::filesystem::temp_directory_path(ec);
  }
  if (ec) {
    ec.clear();
    output_dir = std::filesystem::current_path(ec);
    if (ec) {
      return std::filesystem::path(".");
    }
  }

  ec.clear();
  std::filesystem::create_directories(output_dir, ec);
  if (ec) {
    ec.clear();
    output_dir = std::filesystem::current_path(ec);
    if (ec) {
      return std::filesystem::path(".");
    }
  }
  return output_dir;
}
} // namespace

void PrintUsage(char *name) {
  cerr << name << " <options> " << "<program name>" << "\n";
  cerr << "-dot :generate DOT output \n";
}

int main(int argc, char *argv[]) {

  if (argc <= 1) {
    PrintUsage(argv[0]);
    return -1;
  }
  SgProject *project = frontend(argc, argv);
  checkAliasUpdates();
  const bool checkConversions =
      project->numberOfFiles() == 1 &&
      project->get_file(0).get_sourceFileNameWithoutPath() ==
          "rex_vfa_pointer_conversion.cpp";

  printf("Build the call graph \n");

  CallGraphBuilder builder(project);
  builder.buildCallGraph();

  printf("DONE: Build the call graph \n");

  RosePathRoots roots = resolveRosePaths(argv[0]);
  std::filesystem::path dot_output_dir = resolveDotOutputDir(roots);

  // Generate call graph in dot format
  AstDOTGeneration dotgen;
  dotgen.writeIncidenceGraphToDOTFile(
      builder.getGraph(),
      (dot_output_dir / "full_call_graph.dot").string().c_str());

  printf("Calling SageInterface::changeAllBodiesToBlocks() \n");

  SageInterface::changeAllBodiesToBlocks(project);

  printf("DONE: Calling SageInterface::changeAllBodiesToBlocks() \n");

  SgFunctionDeclaration *mainDecl = SageInterface::findMain(project);
  if (mainDecl == NULL) {
    std::cerr
        << "Can't execute Virtual Function Analysis without main function\n";
    return 0;
  }

  printf("Calling VirtualFunctionAnalysis() \n");

  const auto initialGraphs = SgIncidenceDirectedGraph::numberOfNodes();
  const auto initialGraphNodes = SgGraphNode::numberOfNodes();
  auto *anal = new ConversionContractAnalysis(project);

  printf("Calling VirtualFunctionAnalysis(): run \n");

  for (SgNode *node : NodeQuery::querySubTree(project, V_SgArrowExp)) {
    auto *receiver = isSgCastExp(isSgArrowExp(node)->get_lhs_operand());
    if (receiver == nullptr ||
        receiver->get_semantic_conversion_kind() !=
            SgCastExp::e_semantic_conversion_LValueToRValue)
      continue;
    auto *reference = isSgVarRefExp(receiver->get_operand());
    if (reference == nullptr)
      continue;
    AliasRelationNode identity;
    ProcessExpression::processLHS(receiver, identity);
    ROSE_ASSERT(identity.var == reference->get_symbol()->get_declaration() &&
                identity.derefLevel ==
                    (SageInterface::isReferenceType(
                         reference->get_symbol()->get_declaration()->get_type())
                         ? 1
                         : 0));
  }

  const auto variables = SgVariableDeclaration::numberOfNodes();
  const auto symbols = SgVariableSymbol::numberOfNodes();
  ClassHierarchyWrapper hierarchy(project);
  {
    std::unordered_map<SgFunctionDeclaration *, IntraProcAliasAnalysis *>
        mapping;
    std::unordered_map<SgExpression *, std::vector<SgFunctionDeclaration *>>
        resolved;
    for (VariantT kind : {V_SgFunctionCallExp, V_SgConstructorInitializer})
      for (SgNode *node : NodeQuery::querySubTree(mainDecl, kind)) {
        auto *expression = isSgExpression(node);
        if (!CallTargetSet::isNonExecutableCallExpression(expression))
          CallTargetSet::getPropertiesForExpression(expression, &hierarchy,
                                                    resolved[expression]);
      }
    CallSiteAliasAnalysis probe(canonicalFunctionDeclForCallGraph(mainDecl),
                                &hierarchy, &builder, mapping, resolved);
    probe.checkExitSummary();
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *access = isSgBinaryOp(call->get_function());
    auto *owner = SageInterface::getEnclosingFunctionDeclaration(call);
    if (access == nullptr ||
        isSgMemberFunctionRefExp(access->get_rhs_operand()) == nullptr ||
        owner == nullptr ||
        !callGraphDeclHasIndependentCallableIdentity(owner) ||
        CallTargetSet::isNonExecutableCallExpression(call))
      continue;
    if (!isSgMemberFunctionRefExp(access->get_rhs_operand())
             ->get_virtual_call())
      continue;
    std::vector<SgFunctionDeclaration *> targets;
    CallTargetSet::getPropertiesForExpression(call, &hierarchy, targets);
    if (targets.empty() || !targets.front()->get_functionModifier().isVirtual())
      continue;
    auto *target = isSgMemberFunctionDeclaration(targets.front());
    auto *runtimeClass =
        isSgClassDeclaration(target->get_associatedClassDeclaration());
    ROSE_ASSERT(runtimeClass != nullptr);
    auto *source = builder.hasGraphNodeFor(owner);
    auto *destination = builder.hasGraphNodeFor(target);
    ROSE_ASSERT(source != nullptr && destination != nullptr &&
                builder.getGraph()->checkIfDirectedGraphEdgeExists(
                    source, destination));
    std::unordered_map<SgFunctionDeclaration *, IntraProcAliasAnalysis *>
        mapping;
    std::unordered_map<SgExpression *, std::vector<SgFunctionDeclaration *>>
        resolved{{call, {}}};
    CallSiteAliasAnalysis probe(canonicalFunctionDeclForCallGraph(owner),
                                &hierarchy, &builder, mapping, resolved);
    CompReprPtr incoming(new CompactRepresentation);
    probe.setFunctionEntry(incoming);
    auto *entryNode = probe.getAllNodes().front();
    ROSE_ASSERT(probe.getCFGInData(entryNode) == incoming);
    probe.applyCFGTransferFunction(entryNode);
    ROSE_ASSERT(probe.getCFGOutData(entryNode) == incoming);
    ROSE_ASSERT(probe.addVirtualFunction(runtimeClass->get_type(), call));
    ROSE_ASSERT(resolved.at(call).size() == 1 &&
                resolved.at(call).front() == target);
    ROSE_ASSERT(!probe.addVirtualFunction(runtimeClass->get_type(), call));
    ROSE_ASSERT(resolved.at(call).size() == 1);
  }
  anal->run();
  const auto activeGraphs = SgIncidenceDirectedGraph::numberOfNodes();
  const auto activeGraphNodes = SgGraphNode::numberOfNodes();
  anal->reset();
  anal->run();
  if (checkConversions)
    anal->checkLostProvenance();
  ROSE_ASSERT(SgIncidenceDirectedGraph::numberOfNodes() == activeGraphs);
  ROSE_ASSERT(SgGraphNode::numberOfNodes() == activeGraphNodes);
  ROSE_ASSERT(SgVariableDeclaration::numberOfNodes() == variables);
  ROSE_ASSERT(SgVariableSymbol::numberOfNodes() == symbols);

  printf("Calling VirtualFunctionAnalysis(): pruneCallGraph \n");

  anal->pruneCallGraph(builder);
  size_t conversionChecks = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *member =
        isSgMemberFunctionDeclaration(call->getAssociatedFunctionDeclaration());
    auto *owner = SageInterface::getEnclosingFunctionDeclaration(call);
    if (member == nullptr || !member->get_functionModifier().isVirtual() ||
        owner == nullptr ||
        !callGraphDeclHasIndependentCallableIdentity(owner) ||
        CallTargetSet::isNonExecutableCallExpression(call))
      continue;
    std::vector<SgFunctionDeclaration *> targets;
    anal->resolveFunctionCall(call, targets);
    if (checkConversions && (member->get_name() == "rex_vfa_pointer_dispatch" ||
                             member->get_name() == "rex_vfa_single_dispatch")) {
      std::set<std::string> classes;
      for (auto *declaration : targets) {
        auto *target = isSgMemberFunctionDeclaration(declaration);
        ROSE_ASSERT(target != nullptr);
        auto *classOwner =
            isSgClassDeclaration(target->get_associatedClassDeclaration());
        ROSE_ASSERT(classOwner != nullptr);
        classes.insert(classOwner->get_name().getString());
      }
      std::set<std::string> expected{"RexVfaExpected"};
      auto *access = isSgBinaryOp(call->get_function());
      auto *reference =
          access != nullptr
              ? isSgMemberFunctionRefExp(access->get_rhs_operand())
              : nullptr;
      ASSERT_not_null(reference);
      if (member->get_name() == "rex_vfa_single_dispatch" ||
          !reference->get_virtual_call())
        expected = {"RexVfaBase"};
      if (owner->get_name() == "rex_vfa_null_dispatch" ||
          owner->get_name() == "rex_vfa_unreachable_dispatch" ||
          owner->get_name() == "rex_vfa_null_single_dispatch" ||
          owner->get_name() == "rex_vfa_unreachable_single_dispatch")
        expected.clear();
      if (owner->get_name() == "rex_vfa_reassigned_integer")
        expected = {"RexVfaOther"};
      if (owner->get_name() == "rex_vfa_unknown_integer") {
        expected.insert("RexVfaBase");
        expected.insert("RexVfaOther");
        expected.insert("RexVfaExpectedChild");
      }
      if (owner->get_name() == "rex_vfa_consume" ||
          owner->get_name() == "rex_vfa_selected_call" ||
          owner->get_name() == "rex_vfa_maybe_write" ||
          owner->get_name() == "rex_vfa_choice_write")
        expected.insert("RexVfaOther");
      if (classes != expected || targets.size() != classes.size()) {
        std::cerr << owner->get_qualified_name() << ':'
                  << call->get_file_info()->get_line() << " dispatch targets:";
        for (const auto &name : classes)
          std::cerr << ' ' << name;
        std::cerr << '\n';
      }
      ROSE_ASSERT(classes == expected && targets.size() == classes.size());
      ++conversionChecks;
    }
    auto *source = builder.hasGraphNodeFor(owner);
    ROSE_ASSERT(source != nullptr);
    for (auto *target : targets) {
      ROSE_ASSERT(target == canonicalFunctionDeclForCallGraph(target));
      auto *destination = builder.hasGraphNodeFor(target);
      ROSE_ASSERT(destination != nullptr &&
                  builder.getGraph()->checkIfDirectedGraphEdgeExists(
                      source, destination));
    }
  }
  ROSE_ASSERT(!checkConversions || conversionChecks == 33);

  printf("Calling VirtualFunctionAnalysis(): writeIncidenceGraphToDOTFile \n");

  AstDOTGeneration dotgen2;
  dotgen2.writeIncidenceGraphToDOTFile(
      builder.getGraph(), (dot_output_dir / "call_graph.dot").string().c_str());

  printf("Calling VirtualFunctionAnalysis(): delete \n");

  delete anal;
  ROSE_ASSERT(SgIncidenceDirectedGraph::numberOfNodes() == initialGraphs);
  ROSE_ASSERT(SgGraphNode::numberOfNodes() == initialGraphNodes);

  printf("DONE: Calling VirtualFunctionAnalysis(): delete \n");

  return 0;
}
