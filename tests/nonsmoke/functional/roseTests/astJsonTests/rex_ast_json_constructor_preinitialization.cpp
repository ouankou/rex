#include "AstInterface_ROSE.h"
#include "CallGraph.h"
#include "IntraProcAliasAnalysis.h"
#include "StmtInfoCollect.h"
#include "astJson/sageAstJsonPrivate.h"
#include "liveDeadVarAnalysis.h"
#include "nodeState.h"
#include "rose.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace {

void requireLambdaRecipePreflight(SgProject *project) {
  using namespace Rose::AstJson;
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  const auto checkpoint = Checkpoint::PreOmpConstruction;
  auto ast = parseAstFileJson(
      buildJson(file, checkpoint, file, isSgFileList(file->get_parent())),
      checkpointName(checkpoint));
  auto lambda = std::find_if(
      ast.nodes.begin(), ast.nodes.end(),
      [](const NodeRecord &record) { return record.kind == "SgLambdaExp"; });
  ROSE_ASSERT(lambda != ast.nodes.end());
  auto &closure = ast.nodes.at(ast.index_by_id.at(
      requiredSingleEdgeTarget(*lambda, "lambda_closure_class")));
  auto recipe = std::find_if(
      closure.edges.begin(), closure.edges.end(),
      [](const EdgeRecord &edge) { return edge.field == "lambda_expression"; });
  ROSE_ASSERT(recipe != closure.edges.end() && recipe->target == lambda->id);
  recipe->target = 0;
  const auto lambdas = SgLambdaExp::numberOfNodes();
  const auto classes = SgClassDeclaration::numberOfNodes();
  const auto scopes = SgDeclarationScope::numberOfNodes();
  const auto positions = Sg_File_Info::numberOfNodes();
  const auto revision = SgNode::get_globalAstModificationSequence();
  for (bool constructor_reader : {false, true}) {
    NodeMap nodes;
    bool rejected = false;
    try {
      if (constructor_reader)
        rebuildConstructorOnlyNodes(ast, nodes);
      else
        (void)reconstructSourceFile(ast, file);
    } catch (const std::runtime_error &error) {
      if (std::string(error.what()) !=
          "AST JSON lambda has no exact declaration-scope owner")
        throw;
      rejected = true;
    }
    ROSE_ASSERT(rejected && nodes.empty());
    ROSE_ASSERT(SgLambdaExp::numberOfNodes() == lambdas &&
                SgClassDeclaration::numberOfNodes() == classes &&
                SgDeclarationScope::numberOfNodes() == scopes &&
                Sg_File_Info::numberOfNodes() == positions &&
                SgNode::get_globalAstModificationSequence() == revision);
  }
}

class ConstructorAliasAnalysis : public IntraProcAliasAnalysis {
public:
  using IntraProcAliasAnalysis::getConstructorParametersAliasRelations;
  using IntraProcAliasAnalysis::getFunctionParametersAliasRelations;
  using IntraProcAliasAnalysis::IntraProcAliasAnalysis;
};

class ConstructorArgumentReads : public StmtInfoCollect {
public:
  using StmtInfoCollect::AppendFuncCallArguments;
  using StmtInfoCollect::AppendFuncCallWrite;
  std::vector<std::pair<SgNode *, SgNode *>> reads;

private:
  void AppendVariableDecl(AstInterface &, const AstNodePtr &,
                          const AstNodePtr &) override {
    ROSE_ABORT();
  }
  void AppendAliasDecl(AstInterface &, const AstNodePtr &,
                       const AstNodePtr &) override {
    ROSE_ABORT();
  }
  void AppendModLoc(AstInterface &, const AstNodePtr &,
                    const AstNodePtr &) override {
    ROSE_ABORT();
  }
  void AppendReadLoc(AstInterface &, const AstNodePtr &read,
                     const AstNodePtr &formal) override {
    reads.emplace_back(read.get_ptr(), formal.get_ptr());
  }
  void AppendFuncCall(AstInterface &, const AstNodePtr &) override {
    ROSE_ABORT();
  }
};

void validateConstructorPreinitializers(SgProject *project,
                                        bool elisionPermitted) {
  for (SgFile *file : project->get_fileList())
    ROSE_ASSERT(file->is_gnu_standard());
  unsigned virtualBases = 0;
  unsigned nonvirtualBases = 0;
  unsigned dataMembers = 0;
  unsigned delegatingConstructors = 0;
  unsigned instantiatedInitializers = 0;
  unsigned instantiatedCalls = 0;
  unsigned semanticEntries = 0;
  unsigned inheritedArguments = 0;
  unsigned inheritedVirtualArguments = 0;
  unsigned inheritedVariadicArguments = 0;
  unsigned inheritedAliasParameters = 0;
  unsigned inheritedVirtualAliasParameters = 0;
  unsigned inheritedReferenceArguments = 0;

  unsigned namedReturns = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgReturnStmt)) {
    auto *statement = isSgReturnStmt(node);
    auto *function = SageInterface::getEnclosingFunctionDeclaration(statement);
    ROSE_ASSERT(function != nullptr);
    if (function->get_name() != "rex_return_copy" &&
        function->get_name() != "rex_return_move")
      continue;
    auto *candidate = statement->get_nrvo_candidate();
    ROSE_ASSERT(candidate != nullptr &&
                candidate->get_scope() ==
                    function->get_definition()->get_body());
    unsigned references = 0;
    for (SgNode *operand :
         NodeQuery::querySubTree(statement->get_expression(), V_SgVarRefExp))
      references +=
          isSgVarRefExp(operand)->get_symbol()->get_declaration() == candidate;
    ROSE_ASSERT(references == 1);
    ++namedReturns;
  }
  ROSE_ASSERT(namedReturns == 2);

  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgCtorInitializerList)) {
    SgCtorInitializerList *list = isSgCtorInitializerList(node);
    ROSE_ASSERT(list != nullptr);
    auto *constructor = isSgMemberFunctionDeclaration(list->get_parent());
    ROSE_ASSERT(constructor != nullptr);
    if (constructor->get_specialFunctionModifier().isConstructor())
      ROSE_ASSERT(
          constructor->get_functionModifier().hasSpecialMemberProperties());
    auto *owner = isSgClassDefinition(constructor->get_scope());
    ROSE_ASSERT(owner != nullptr);
    auto *instance = isSgTemplateInstantiationDecl(owner->get_declaration());
    if (instance != nullptr &&
        instance->get_templateName() == "RexConstructorTemplate" &&
        !list->get_ctors().empty()) {
      ROSE_ASSERT(constructor->get_definingDeclaration() == constructor);
      ROSE_ASSERT(constructor->get_definition() != nullptr);
      ROSE_ASSERT(list->get_file_info()->isCompilerGenerated());
      instantiatedInitializers += list->get_ctors().size();
      instantiatedCalls +=
          NodeQuery::querySubTree(list, V_SgFunctionCallExp).size();
      for (auto *entry : list->get_ctors()) {
        ROSE_ASSERT(entry->get_file_info()->isCompilerGenerated());
        ROSE_ASSERT(
            entry->get_initializer()->get_file_info()->isCompilerGenerated());
      }
    }
    for (size_t index = 0; index != list->get_ctors().size(); ++index) {
      auto *name = list->get_ctors()[index];
      if (name == nullptr || name->get_parent() != list ||
          name->get_initializer() == nullptr ||
          name->get_initializer()->get_parent() != name) {
        throw std::runtime_error(
            "constructor preinitializer has no exact structural owner");
      }
      const bool virtualBase = name->get_preinitialization() ==
                               SgInitializedName::e_virtual_base_class;
      const VirtualCFG::CFGNode entry(list, index), after(list, index + 1);
      const auto branches = entry.outEdges();
      ROSE_ASSERT(branches.size() == (virtualBase ? 2 : 1));
      ROSE_ASSERT(branches.front().target() == name->cfgForBeginning());
      if (virtualBase) {
        ROSE_ASSERT(entry.isInteresting());
        ROSE_ASSERT(branches.back().target() == after);
        ROSE_ASSERT(branches.front().condition() ==
                    VirtualCFG::eckCompleteObjectConstruction);
        ROSE_ASSERT(branches.back().condition() ==
                    VirtualCFG::eckBaseObjectConstruction);
        const auto filtered = VirtualCFG::InterestingNode(entry).outEdges();
        ROSE_ASSERT(filtered.size() == 2);
        ROSE_ASSERT(filtered.front().condition() ==
                    VirtualCFG::eckCompleteObjectConstruction);
        ROSE_ASSERT(filtered.back().condition() ==
                    VirtualCFG::eckBaseObjectConstruction);
      } else {
        ROSE_ASSERT(branches.front().condition() ==
                    VirtualCFG::eckUnconditional);
      }
      for (const auto &edge : branches) {
        const auto incoming = edge.target().inEdges();
        ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), edge) !=
                    incoming.end());
      }
      if (auto *initializer =
              isSgConstructorInitializer(name->get_initializer())) {
        initializer->validate_elision();
        initializer->validate_argument_context();
        const auto source = initializer->get_argument_source();
        if (source !=
            SgConstructorInitializer::e_constructor_argument_expressions) {
          ROSE_ASSERT(initializer->get_args()->get_expressions().empty());
          ROSE_ASSERT(!name->get_ctor_initializer_source_order().has_value());
          AstInterfaceImpl implementation(constructor);
          AstInterface interface(&implementation);
          AstNodePtr callee;
          AstInterface::AstNodeList arguments, expectedArguments;
          for (auto *parameter : constructor->get_args())
            if (!isSgTypeEllipse(parameter->get_type()))
              expectedArguments.emplace_back(parameter);
          ROSE_ASSERT(
              interface.IsFunctionCall(initializer, &callee, &arguments));
          ROSE_ASSERT(callee.get_ptr() == initializer &&
                      arguments == expectedArguments);
          ConstructorArgumentReads reads;
          reads.AppendFuncCallArguments(interface, initializer, nullptr);
          ROSE_ASSERT(reads.reads.size() == arguments.size());
          auto *selected = isSgFunctionDeclaration(
              initializer->get_declaration()->get_definingDeclaration());
          ROSE_ASSERT(selected != nullptr);
          ROSE_ASSERT(selected->get_firstNondefiningDeclaration() ==
                      initializer->get_declaration()
                          ->get_firstNondefiningDeclaration());
          const auto &formals = selected->get_args();
          for (size_t index = 0; index != arguments.size(); ++index) {
            ROSE_ASSERT(reads.reads[index].first == arguments[index].get_ptr());
            ROSE_ASSERT(reads.reads[index].second == formals[index]);
            ROSE_ASSERT(formals[index]->get_parent() ==
                        selected->get_parameterList());
          }
          if (owner->get_declaration()->get_name() ==
              "RexInheritedReferences") {
            AstInterface::AstNodeList writes;
            ROSE_ASSERT(interface.IsFunctionCall(initializer, nullptr, nullptr,
                                                 &writes));
            ROSE_ASSERT(arguments.size() == 3 && writes.size() == 2);
            ROSE_ASSERT(writes[0] == arguments[0] && writes[1] == arguments[1]);
            ++inheritedReferenceArguments;
          }
          LiveVarsLattice live(SgExpr2Var(initializer));
          NodeState state;
          const VirtualCFG::DataflowNode cfg(initializer->cfgForBeginning(),
                                             VirtualCFG::defaultFilter);
          const std::vector<Lattice *> lattices{&live};
          const Function function(constructor);
          LiveDeadVarsTransfer transfer(function, cfg, state, lattices);
          transfer.visit(static_cast<SgExpression *>(initializer));
          transfer.finish();
          std::set<varID> expected;
          for (auto *parameter : constructor->get_parameterList()->get_args())
            if (!isSgTypeEllipse(parameter->get_type()))
              expected.insert(varID(parameter));
          ROSE_ASSERT(!expected.empty() && live.liveVars == expected);
          for (auto *parameter : constructor->get_parameterList()->get_args()) {
            if (!isSgPointerType(parameter->get_type()))
              continue;
            AliasRelationNode incoming;
            ProcessExpression::processLHS(parameter, incoming);
            ROSE_ASSERT(static_cast<SgNode *>(incoming.var) == parameter &&
                        incoming.derefLevel == 0);
            std::unordered_map<SgFunctionDeclaration *,
                               IntraProcAliasAnalysis *>
                analyses;
            std::unordered_map<SgExpression *,
                               std::vector<SgFunctionDeclaration *>>
                resolver;
            ConstructorAliasAnalysis alias(constructor, nullptr, nullptr,
                                           analyses, resolver);
            std::vector<std::pair<AliasRelationNode, AliasRelationNode>>
                relations;
            alias.getConstructorParametersAliasRelations(
                initializer, initializer->get_declaration(), relations);
            auto *selected = isSgFunctionDeclaration(
                initializer->get_declaration()->get_definingDeclaration());
            ROSE_ASSERT(selected != nullptr && relations.size() == 1);
            ROSE_ASSERT(static_cast<SgNode *>(relations.front().first.var) ==
                        selected->get_parameterList()->get_args().front());
            ROSE_ASSERT(static_cast<SgNode *>(relations.front().second.var) ==
                        parameter);
            ROSE_ASSERT(relations.front().first.derefLevel == 0 &&
                        relations.front().second.derefLevel == 0);
            ++inheritedAliasParameters;
            if (source == SgConstructorInitializer::
                              e_constructor_inherited_virtual_base_arguments)
              ++inheritedVirtualAliasParameters;
          }
          if (source ==
              SgConstructorInitializer::e_constructor_inherited_arguments)
            ++inheritedArguments;
          else
            ++inheritedVirtualArguments;
          if (owner->get_declaration()->get_name() == "RexInheritedArguments")
            ++inheritedVariadicArguments;
        }
      }
      switch (name->get_preinitialization()) {
      case SgInitializedName::e_virtual_base_class:
        ++virtualBases;
        break;
      case SgInitializedName::e_nonvirtual_base_class:
        ++nonvirtualBases;
        break;
      case SgInitializedName::e_data_member:
        ROSE_ASSERT(name->get_ctor_initializer_field_target() != nullptr);
        ROSE_ASSERT(name->get_ctor_initializer_field_target()->get_scope() ==
                    owner);
        ROSE_ASSERT(SageInterface::isEquivalentType(
            name->get_type(),
            name->get_ctor_initializer_field_target()->get_type()));
        ++dataMembers;
        break;
      case SgInitializedName::e_delegation_constructor:
        ++delegatingConstructors;
        break;
      case SgInitializedName::e_unknown_preinitialization:
      case SgInitializedName::e_last_preinitialization:
      default:
        throw std::runtime_error(
            "constructor preinitializer lost its exact typed role");
      }
    }
    if (owner->get_declaration()->get_name() ==
            "RexConstructorSemanticEntries" &&
        constructor->get_definition() != nullptr) {
      const auto &entries = list->get_ctors();
      ROSE_ASSERT(entries.size() == 3);
      ROSE_ASSERT(entries[0]->get_preinitialization() ==
                  SgInitializedName::e_nonvirtual_base_class);
      ROSE_ASSERT(!entries[0]->get_ctor_initializer_source_order().has_value());
      ROSE_ASSERT(entries[1]->get_ctor_initializer_field_target()->get_name() ==
                  "first");
      ROSE_ASSERT(entries[2]->get_ctor_initializer_field_target()->get_name() ==
                  "second");
      ROSE_ASSERT(entries[1]->get_ctor_initializer_source_order() == 1);
      ROSE_ASSERT(entries[2]->get_ctor_initializer_source_order() == 0);
      const auto source = constructor->unparseToString();
      ROSE_ASSERT(source.find("second") < source.find("first"));
      ROSE_ASSERT(source.find("NonvirtualBase") == std::string::npos);
      ++semanticEntries;
    }
  }

  if (virtualBases == 0 || nonvirtualBases == 0 || dataMembers == 0 ||
      delegatingConstructors == 0) {
    throw std::runtime_error(
        "constructor preinitializer role coverage is incomplete");
  }
  ROSE_ASSERT(instantiatedInitializers == 6);
  ROSE_ASSERT(instantiatedCalls == 2);
  ROSE_ASSERT(semanticEntries == 1);
  ROSE_ASSERT(inheritedArguments != 0 && inheritedVirtualArguments != 0);
  ROSE_ASSERT(inheritedVariadicArguments == 1);
  ROSE_ASSERT(inheritedReferenceArguments == 1);
  ROSE_ASSERT(inheritedAliasParameters > 1 &&
              inheritedVirtualAliasParameters != 0);

  unsigned pointerInitializers = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *selected = call->getAssociatedFunctionDeclaration();
    if (selected == nullptr ||
        selected->get_name() != "rex_alias_pointer_return")
      continue;
    selected = isSgFunctionDeclaration(selected->get_definingDeclaration());
    ROSE_ASSERT(selected != nullptr);
    auto *initializer = isSgAssignInitializer(call->get_parent());
    ROSE_ASSERT(initializer != nullptr);
    auto *destination = isSgInitializedName(initializer->get_parent());
    ROSE_ASSERT(destination != nullptr);
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(call);
    std::unordered_map<SgFunctionDeclaration *, IntraProcAliasAnalysis *>
        analyses;
    std::unordered_map<SgExpression *, std::vector<SgFunctionDeclaration *>>
        resolver;
    ClassHierarchyWrapper hierarchy(project);
    ConstructorAliasAnalysis callee(selected, &hierarchy, nullptr, analyses,
                                    resolver);
    ConstructorAliasAnalysis callerAnalysis(caller, &hierarchy, nullptr,
                                            analyses, resolver);
    analyses.emplace(selected, &callee);
    resolver.emplace(call, std::vector<SgFunctionDeclaration *>{selected});
    std::vector<std::pair<AliasRelationNode, AliasRelationNode>> arguments;
    callerAnalysis.getFunctionParametersAliasRelations(call, selected,
                                                       arguments);
    const auto returns = callee.getReturnStmts();
    ROSE_ASSERT(arguments.size() == 1 && returns.size() == 1);
    auto *conversion = isSgCastExp(returns.front().var);
    ROSE_ASSERT(conversion != nullptr &&
                conversion->get_semantic_conversion_kind() ==
                    SgCastExp::e_semantic_conversion_LValueToRValue &&
                returns.front().derefLevel == 0);
    auto *returned = isSgVarRefExp(conversion->get_operand());
    ROSE_ASSERT(returned != nullptr &&
                returned->get_symbol()->get_declaration() ==
                    selected->get_args().front());
    CompReprPtr incoming(new CompactRepresentation);
    callerAnalysis.setFunctionEntry(incoming);
    bool changed;
    do {
      changed = callerAnalysis.runCheck();
      changed |= callee.runCheck();
    } while (changed);
    auto output = callerAnalysis.getFunctionExit();
    ASSERT_not_null(output.get());
    std::vector<SgGraphNode *> values;
    output.get()->computeValues(destination, 1, values);
    auto *address =
        isSgAddressOfOp(call->get_args()->get_expressions().front());
    ROSE_ASSERT(address != nullptr);
    auto *actual = isSgVarRefExp(address->get_operand());
    ROSE_ASSERT(actual != nullptr && values.size() == 1 &&
                values.front()->get_SgNode() ==
                    actual->get_symbol()->get_declaration());
    AstInterfaceImpl implementation(caller);
    AstInterface interface(&implementation);
    ConstructorArgumentReads reads;
    reads.AppendFuncCallArguments(interface, call, nullptr);
    const auto expectedRead = std::make_pair(
        static_cast<SgNode *>(call->get_args()->get_expressions().front()),
        static_cast<SgNode *>(selected->get_args().front()));
    ROSE_ASSERT(
        std::count(reads.reads.begin(), reads.reads.end(), expectedRead) == 1);
    ++pointerInitializers;
  }
  ROSE_ASSERT(pointerInitializers == 1);

  unsigned memberReads = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *selected = call->getAssociatedFunctionDeclaration();
    auto *scope = selected != nullptr
                      ? isSgClassDefinition(selected->get_scope())
                      : nullptr;
    if (scope == nullptr ||
        scope->get_declaration()->get_name() != "RexArgumentCallable")
      continue;
    selected = isSgFunctionDeclaration(selected->get_definingDeclaration());
    ROSE_ASSERT(selected != nullptr && selected->get_args().size() == 1);
    AstInterfaceImpl implementation(project);
    AstInterface interface(&implementation);
    ConstructorArgumentReads reads;
    reads.AppendFuncCallArguments(interface, call, nullptr);
    const auto expected = std::make_pair(
        static_cast<SgNode *>(call->get_args()->get_expressions().front()),
        static_cast<SgNode *>(selected->get_args().front()));
    ASSERT_require2(
        std::count(reads.reads.begin(), reads.reads.end(), expected) == 1,
        "callee: %s", call->get_function()->unparseToString().c_str());
    ROSE_ASSERT(std::count_if(reads.reads.begin(), reads.reads.end(),
                              [](const auto &read) {
                                return read.second != nullptr;
                              }) == 1);
    reads.AppendFuncCallWrite(interface, call);
    ++memberReads;
  }
  ROSE_ASSERT(memberReads == 3);

  unsigned explicitCallees = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    auto *call = isSgFunctionCallExp(node);
    auto *owner = SageInterface::getEnclosingFunctionDeclaration(call);
    if (owner == nullptr ||
        owner->get_name() != "rex_argument_explicit_identity")
      continue;
    AstInterfaceImpl implementation(owner);
    AstInterface interface(&implementation);
    AstNodePtr callee;
    ROSE_ASSERT(interface.IsFunctionCall(call, &callee));
    auto *cast = isSgCastExp(callee.get_ptr());
    ROSE_ASSERT(cast != nullptr &&
                cast->get_cast_type() != SgCastExp::e_implicit_cast);
    SgType *source_type = cast->get_source_type();
    ROSE_ASSERT(source_type != nullptr && source_type->has_source_prototype() &&
                source_type->get_parent() == cast);
    auto *copied_cast = isSgCastExp(SageInterface::deepCopy(cast));
    ROSE_ASSERT(copied_cast != nullptr && copied_cast != cast &&
                copied_cast->get_source_type() != source_type &&
                copied_cast->get_source_type()->get_parent() == copied_cast &&
                copied_cast->get_type() == cast->get_type());
    copied_cast->validate_semantic_conversion();
    SageInterface::deepDelete(copied_cast);
    ROSE_ASSERT(source_type->get_parent() == cast);
    ROSE_ASSERT(call->getAssociatedFunctionDeclaration() == nullptr);
    ++explicitCallees;
  }
  ROSE_ASSERT(explicitCallees == 1);

  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  auto *derivedSymbol = file->get_globalScope()->lookup_class_symbol("Derived");
  ROSE_ASSERT(derivedSymbol != nullptr);
  SgClassType *derivedType = derivedSymbol->get_declaration()->get_type();
  const auto lambdas = NodeQuery::querySubTree(project, V_SgLambdaExp);
  ROSE_ASSERT(lambdas.size() == 1);
  auto *closure = isSgLambdaExp(lambdas.front())->get_lambda_closure_class();
  ROSE_ASSERT(closure != nullptr && closure->get_type() != derivedType);
  unsigned derivedElidable = 0, closureElidable = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgConstructorInitializer)) {
    auto *initializer = isSgConstructorInitializer(node);
    if (!initializer->get_is_elidable())
      continue;
    initializer->validate_elision();
    auto *selected = initializer->get_declaration();
    auto *selectedOwner = isSgClassDefinition(selected->get_scope());
    ROSE_ASSERT(selectedOwner != nullptr);
    const bool externalClass =
        selectedOwner->get_declaration()->get_type() == derivedType;
    if (externalClass)
      ++derivedElidable;
    else {
      ROSE_ASSERT(selectedOwner->get_declaration()->get_type() ==
                  closure->get_type());
      ++closureElidable;
    }
    ROSE_ASSERT(initializer->get_elision_permitted() == elisionPermitted);
    SgExpression *operand = initializer->get_args()->get_expressions().front();
    std::string source = operand->unparseToString();
    if (initializer->get_is_braced_initialized())
      source = "{" + source + "}";
    else if (initializer->get_need_parenthesis_after_name())
      source = "(" + source + ")";
    ROSE_ASSERT(initializer->unparseToString() == source);
    const auto outgoing =
        VirtualCFG::CFGNode(initializer->get_args(), 1).outEdges();
    ROSE_ASSERT(outgoing.size() == (elisionPermitted ? 2 : 1));
    bool call = false, bypass = false;
    for (const auto &edge : outgoing) {
      call |= edge.target() ==
              (initializer->get_args()->get_expressions().size() == 1
                   ? VirtualCFG::CFGNode(initializer, 1)
                   : initializer->get_args()
                         ->get_expressions()[1]
                         ->cfgForBeginning());
      bypass |= edge.target() == initializer->cfgForEnd();
      const auto incoming = edge.target().inEdges();
      ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), edge) !=
                  incoming.end());
    }
    ROSE_ASSERT(call && bypass == elisionPermitted);
    SgCopyHelp::copiedNodeMapType mapping;
    auto *copy = isSgConstructorInitializer(
        SageInterface::deepCopyNodeWithIdentityMap(initializer, mapping));
    ROSE_ASSERT(copy != nullptr && copy != initializer);
    ROSE_ASSERT(copy->get_is_elidable());
    ROSE_ASSERT(copy->get_elision_permitted() == elisionPermitted);
    copy->validate_elision();
    if (externalClass) {
      ROSE_ASSERT(mapping.count(selected) == 0);
      ROSE_ASSERT(copy->get_declaration() == selected);
      ROSE_ASSERT(copy->get_expression_type() ==
                  initializer->get_expression_type());
    } else {
      ROSE_ASSERT(copy->get_declaration() == mapping.at(selected));
      ROSE_ASSERT(copy->get_declaration() != selected);
      ROSE_ASSERT(copy->get_declaration()->get_scope() ==
                  mapping.at(selectedOwner));
      ROSE_ASSERT(copy->get_declaration()->get_firstNondefiningDeclaration() ==
                  mapping.at(selected->get_firstNondefiningDeclaration()));
      auto *copiedClosure = isSgClassDeclaration(mapping.at(closure));
      ROSE_ASSERT(copiedClosure != nullptr && copiedClosure != closure);
      ROSE_ASSERT(copy->get_declaration()->get_scope() ==
                  copiedClosure->get_definition());
      ROSE_ASSERT(copy->get_expression_type() == copiedClosure->get_type());
      ROSE_ASSERT(copiedClosure->get_type()->get_declaration() ==
                  copiedClosure->get_firstNondefiningDeclaration());
    }
    ROSE_ASSERT(copy->get_args() != initializer->get_args());
    ROSE_ASSERT(copy->get_args()->get_expressions().front() != operand);
    ROSE_ASSERT(copy->get_is_braced_initialized() ==
                initializer->get_is_braced_initialized());
    ROSE_ASSERT(copy->get_need_parenthesis_after_name() ==
                initializer->get_need_parenthesis_after_name());
    const auto &originalArguments = initializer->get_args()->get_expressions();
    const auto &copiedArguments = copy->get_args()->get_expressions();
    ROSE_ASSERT(originalArguments.size() == copiedArguments.size());
    for (size_t index = 1; index < originalArguments.size(); ++index) {
      auto *use = isSgCxxDefaultInitExp(originalArguments[index]);
      auto *copiedUse = isSgCxxDefaultInitExp(copiedArguments[index]);
      ROSE_ASSERT(use != nullptr && copiedUse != nullptr && use != copiedUse);
      use->validate();
      copiedUse->validate();
      ROSE_ASSERT(copiedUse->get_expression() != use->get_expression());
      auto *parameter = use->get_source_declaration();
      ROSE_ASSERT(
          copiedUse->get_source_declaration() ==
          (mapping.count(parameter) != 0 ? mapping.at(parameter) : parameter));
    }
    SageInterface::deepDelete(copy);
    initializer->validate_elision();
  }
  if (project->get_Cxx14_only())
    ROSE_ASSERT(derivedElidable == 5 && closureElidable == 1);
  else {
    ROSE_ASSERT(project->get_Cxx17_only() || project->get_Cxx20_only());
    ROSE_ASSERT(derivedElidable == 0 && closureElidable == 0);
  }

  const auto defaults = NodeQuery::querySubTree(project, V_SgCxxDefaultInitExp);
  ROSE_ASSERT(!defaults.empty());
  unsigned memberUses = 0;
  SgInitializedName *memberField = nullptr;
  std::set<SgExpression *> memberExpansions;
  for (auto *node : defaults) {
    auto *use = isSgCxxDefaultInitExp(node);
    use->validate();
    ROSE_ASSERT(use->get_type() == use->get_expression()->get_type());
    const auto outgoing = use->cfgForBeginning().outEdges();
    ROSE_ASSERT(outgoing.size() == 1 &&
                outgoing.front().target() ==
                    use->get_expression()->cfgForBeginning());
    if (use->get_source_kind() == SgCxxDefaultInitExp::e_member) {
      ++memberUses;
      if (memberField == nullptr)
        memberField = use->get_source_declaration();
      ROSE_ASSERT(use->get_source_declaration() == memberField);
      ROSE_ASSERT(memberExpansions.insert(use->get_expression()).second);
      ROSE_ASSERT(use->get_file_info()->isCompilerGenerated());
      auto *arguments = isSgExprListExp(use->get_parent());
      ROSE_ASSERT(arguments != nullptr);
      auto *initializer = isSgConstructorInitializer(arguments->get_parent());
      ROSE_ASSERT(initializer != nullptr);
      ROSE_ASSERT(arguments->get_expressions().size() == 2 &&
                  arguments->get_expressions()[1] == use);
      ROSE_ASSERT(initializer->get_declaration() == nullptr);
      SgCopyHelp::copiedNodeMapType mapping;
      auto *copy = isSgConstructorInitializer(
          SageInterface::deepCopyNodeWithIdentityMap(initializer, mapping));
      auto *copiedUse =
          isSgCxxDefaultInitExp(copy->get_args()->get_expressions()[1]);
      ROSE_ASSERT(copiedUse != use &&
                  copiedUse->get_source_kind() ==
                      SgCxxDefaultInitExp::e_member &&
                  copiedUse->get_source_declaration() == memberField &&
                  copiedUse->get_expression() != use->get_expression());
      copiedUse->validate();
      SageInterface::deepDelete(copy);
    }
  }
  ROSE_ASSERT(memberUses == (project->get_Cxx20_only() ? 2 : 0));
  if (memberUses != 0) {
    auto *call = isSgFunctionCallExp(*memberExpansions.begin());
    ROSE_ASSERT(call != nullptr);
    ClassHierarchyWrapper hierarchy(project);
    Rose_STL_Container<SgFunctionDefinition *> definitions;
    CallTargetSet::getDefinitionsForExpression(call, &hierarchy, definitions);
    ROSE_ASSERT(definitions.size() == 1);
    Rose_STL_Container<SgExpression *> expressions;
    CallTargetSet::getExpressionsForDefinition(definitions.front(), &hierarchy,
                                               expressions);
    ROSE_ASSERT(expressions.size() == memberUses);
    for (SgExpression *expression : expressions)
      ROSE_ASSERT(memberExpansions.count(expression) == 1);
  }

  const auto allocations = NodeQuery::querySubTree(project, V_SgNewExp);
  ROSE_ASSERT(allocations.size() == 3);
  for (auto *node : allocations) {
    auto *allocation = isSgNewExp(node);
    const auto declarations = SgVariableDeclaration::numberOfNodes();
    const auto symbols = SgVariableSymbol::numberOfNodes();
    AliasRelationNode identity;
    ProcessExpression::processRHS(allocation, identity);
    ROSE_ASSERT(static_cast<SgNode *>(identity.var) == allocation &&
                identity.derefLevel == -1);
    ROSE_ASSERT(SgVariableDeclaration::numberOfNodes() == declarations &&
                SgVariableSymbol::numberOfNodes() == symbols);
    ROSE_ASSERT(allocation->get_allocation_implicit_argument_count() == 1);
    auto *arguments = allocation->get_placement_args();
    ROSE_ASSERT(arguments != nullptr && arguments->get_parent() == allocation);
    const auto &elements = arguments->get_expressions();
    ROSE_ASSERT(elements.size() == (allocation->get_array_bound() ? 2 : 1));
    auto *use = isSgCxxDefaultInitExp(elements.back());
    ROSE_ASSERT(use != nullptr && use->get_parent() == arguments);
    use->validate();
    auto *declaration = allocation->get_newOperatorDeclaration();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_parameterList()->get_args()[elements.size()] ==
                use->get_source_declaration());
    ROSE_ASSERT(arguments->get_file_info()->isCompilerGenerated() ==
                (elements.size() == 1));
    SgCopyHelp::copiedNodeMapType mapping;
    auto *copy = isSgNewExp(
        SageInterface::deepCopyNodeWithIdentityMap(allocation, mapping));
    ROSE_ASSERT(copy != nullptr && copy != allocation);
    ROSE_ASSERT(copy->get_allocation_implicit_argument_count() == 1);
    ROSE_ASSERT(copy->get_newOperatorDeclaration() == declaration);
    auto *copiedUse = isSgCxxDefaultInitExp(
        copy->get_placement_args()->get_expressions().back());
    ROSE_ASSERT(copiedUse != nullptr && copiedUse != use);
    ROSE_ASSERT(copiedUse->get_expression() != use->get_expression());
    ROSE_ASSERT(copiedUse->get_source_declaration() ==
                use->get_source_declaration());
    SageInterface::deepDelete(copy);
    if (allocation->get_array_bound() == nullptr &&
        !allocation->get_type_id_is_parenthesized()) {
      SgCopyHelp::copiedNodeMapType declarationMapping;
      auto *copiedDeclaration =
          isSgFunctionDeclaration(SageInterface::deepCopyNodeWithIdentityMap(
              declaration, declarationMapping));
      ROSE_ASSERT(copiedDeclaration != nullptr &&
                  copiedDeclaration != declaration);
      auto *copiedParameter = isSgInitializedName(
          declarationMapping.at(use->get_source_declaration()));
      ROSE_ASSERT(SgCxxDefaultInitExp::is_valid_use(copiedParameter,
                                                    use->get_expression()));
      SgTreeCopy help;
      for (const auto &[original, target] : declarationMapping)
        help.insertCopiedNodePair(original, target);
      auto *mappedCopy = isSgNewExp(allocation->copy(help));
      ROSE_ASSERT(mappedCopy != nullptr);
      ROSE_ASSERT(mappedCopy->get_newOperatorDeclaration() ==
                  copiedDeclaration);
      auto *mappedUse = isSgCxxDefaultInitExp(
          mappedCopy->get_placement_args()->get_expressions().back());
      ROSE_ASSERT(mappedUse != nullptr &&
                  mappedUse->get_source_declaration() == copiedParameter);
      mappedUse->validate();
      SageInterface::deepDelete(mappedCopy);
      SageInterface::deepDelete(copiedDeclaration);
    }
  }

  const auto loops = NodeQuery::querySubTree(project, V_SgCxxArrayInitLoop);
  ROSE_ASSERT(loops.size() == (project->get_Cxx14_only() ? 4 : 2));
  for (SgNode *node : loops) {
    auto *loop = isSgCxxArrayInitLoop(node);
    loop->validate();
    ROSE_ASSERT(loop->get_element_count() == 2);
    auto *binding = loop->get_binding();
    ROSE_ASSERT(binding->get_parent() == loop);
    ROSE_ASSERT(binding->get_common_source()->get_parent() == binding);
    for (SgNode *child : NodeQuery::querySubTree(
             loop->get_element_initializer(), V_SgCxxArrayInitReference)) {
      auto *reference = isSgCxxArrayInitReference(child);
      reference->validate();
      auto *owner =
          isSgCxxArrayInitLoop(reference->get_binding()->get_parent());
      ROSE_ASSERT(owner != nullptr &&
                  owner->get_binding() == reference->get_binding());
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  bool elisionPermitted = true;
  for (int argument = 1; argument < argc; ++argument) {
    if (std::string(argv[argument]) == "-fno-elide-constructors")
      elisionPermitted = false;
  }
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);
  validateConstructorPreinitializers(project, elisionPermitted);
  requireLambdaRecipePreflight(project);
  AstTests::runAllTests(project);
  return backend(project);
}
