#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  unsigned adjustedArrayParameters = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *variable = isSgInitializedName(node);
    if (variable->get_name() != "rex_array_parameter_adjustment")
      continue;
    auto *initializer = isSgAssignInitializer(variable->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    auto *expression = isSgRequiresExpr(initializer->get_operand());
    ROSE_ASSERT(expression != nullptr);
    auto *parameters = expression->get_local_parameter_list();
    ROSE_ASSERT(parameters != nullptr && parameters->get_args().size() == 1);
    ROSE_ASSERT(isSgPointerType(parameters->get_args().front()->get_type()) !=
                nullptr);
    ++adjustedArrayParameters;
  }
  ROSE_ASSERT(adjustedArrayParameters == 1);

  auto validateAdjustedParameters = [](SgNode *root) {
    unsigned found = 0;
    for (auto *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
      auto *variable = isSgInitializedName(node);
      const auto name = variable->get_name().getString();
      if (name != "rex_array_parameter_adjustment" &&
          name != "rex_function_parameter_adjustment" &&
          name != "rex_alias_array_parameter_adjustment" &&
          name != "rex_qualified_array_parameter_adjustment")
        continue;
      auto *initializer = isSgAssignInitializer(variable->get_initializer());
      ROSE_ASSERT(initializer != nullptr);
      auto *expression = isSgRequiresExpr(initializer->get_operand());
      ROSE_ASSERT(expression != nullptr);
      auto *semantic = expression->get_local_parameter_list();
      auto *written = expression->get_local_parameter_list_syntax();
      auto *scope = expression->get_local_parameter_scope();
      ROSE_ASSERT(semantic != nullptr && written != nullptr &&
                  semantic != written && semantic->get_args().size() == 1 &&
                  written->get_args().size() == 1 && scope != nullptr &&
                  semantic->get_parent() == expression &&
                  written->get_parent() == expression);
      auto *object = semantic->get_args().front();
      auto *source = written->get_args().front();
      ROSE_ASSERT(object != source && object->get_parent() == semantic &&
                  source->get_parent() == written &&
                  object->get_scope() == scope &&
                  source->get_scope() == scope &&
                  scope->find_symbol_from_declaration(object) != nullptr &&
                  scope->find_symbol_from_declaration(source) == nullptr &&
                  isSgPointerType(object->get_type()) != nullptr);
      if (name == "rex_function_parameter_adjustment") {
        auto *function = isSgFunctionType(source->get_type());
        ROSE_ASSERT(function != nullptr && function->has_source_prototype() &&
                    function->get_arguments().size() == 1);
        auto *array = isSgArrayType(function->get_arguments().front());
        auto *bound =
            array != nullptr ? isSgIntVal(array->get_index()) : nullptr;
        ROSE_ASSERT(bound != nullptr && bound->get_value() == 3);
      } else {
        auto *array = isSgArrayType(source->get_type()->stripType(
            SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE));
        ROSE_ASSERT(array != nullptr);
        auto *bound = isSgIntVal(array->get_index());
        ROSE_ASSERT(
            bound != nullptr &&
            bound->get_value() ==
                (name == "rex_qualified_array_parameter_adjustment" ? 2 : 3));
        if (name == "rex_alias_array_parameter_adjustment") {
          auto *element = isSgModifierType(
              isSgPointerType(object->get_type())->get_base_type());
          ROSE_ASSERT(element != nullptr &&
                      element->get_typeModifier()
                          .get_constVolatileModifier()
                          .isConst() &&
                      element->get_typeModifier()
                          .get_constVolatileModifier()
                          .isVolatile());
        }
      }
      const auto spelling = expression->unparseToString();
      if (name == "rex_function_parameter_adjustment")
        ROSE_ASSERT(spelling.find("[3]") != std::string::npos);
      if (name == "rex_array_parameter_adjustment")
        ROSE_ASSERT(spelling.find("[3]") != std::string::npos);
      if (name == "rex_qualified_array_parameter_adjustment")
        ROSE_ASSERT(spelling.find("[2][3]") != std::string::npos);
      unsigned references = 0;
      for (auto *referenceNode :
           NodeQuery::querySubTree(expression, V_SgVarRefExp)) {
        auto *reference = isSgVarRefExp(referenceNode);
        references += reference->get_symbol()->get_declaration() == object;
      }
      ROSE_ASSERT(references == 1);
      ++found;
    }
    ROSE_ASSERT(found == 4);
  };
  auto validateFunctionParameters = [](SgNode *root) {
    unsigned declarations = 0;
    for (auto *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
      auto *function = isSgFunctionDeclaration(node);
      if (function->get_name() != "rex_signature_parameter_owner")
        continue;
      auto *parameters = function->get_parameterList();
      auto *constraint = function->get_trailingRequiresClause();
      ROSE_ASSERT(parameters != nullptr && parameters->get_args().size() == 1);
      ROSE_ASSERT(parameters->get_parent() == function &&
                  constraint != nullptr);
      ROSE_ASSERT(constraint->get_parent() == function);
      auto *parameter = parameters->get_args().front();
      ROSE_ASSERT(parameter->get_parent() == parameters &&
                  parameter->get_declptr() == function);
      auto *first =
          isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
      ROSE_ASSERT(first != nullptr);
      if (first != function)
        ROSE_ASSERT(parameter->get_prev_decl_item() ==
                    first->get_parameterList()->get_args().front());
      auto *scope = parameter->get_scope();
      ROSE_ASSERT(scope != nullptr &&
                  (scope == function->get_definition() ||
                   scope == function->get_functionParameterScope()));
      auto references = NodeQuery::querySubTree(constraint, V_SgVarRefExp);
      ROSE_ASSERT(references.size() == 2);
      for (auto *node : references) {
        auto *symbol = isSgVarRefExp(node)->get_symbol();
        ROSE_ASSERT(symbol != nullptr &&
                    symbol->get_declaration() == parameter);
        ROSE_ASSERT(scope->find_symbol_from_declaration(parameter) == symbol);
      }
      ++declarations;
    }
    ROSE_ASSERT(declarations == 2);
  };
  validateFunctionParameters(project);
  validateAdjustedParameters(project);
  {
    const auto scopes = SgFunctionParameterScope::numberOfNodes();
    const auto lists = SgFunctionParameterList::numberOfNodes();
    const auto names = SgInitializedName::numberOfNodes();
    auto *copy = SageInterface::deepCopy(project);
    validateFunctionParameters(copy);
    validateAdjustedParameters(copy);
    SageInterface::deleteAST(copy,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(SgFunctionParameterScope::numberOfNodes() == scopes &&
                SgFunctionParameterList::numberOfNodes() == lists &&
                SgInitializedName::numberOfNodes() == names);
  }
  {
    using namespace Rose::AstJson;
    constexpr auto checkpoint = Checkpoint::PreOmpConstruction;
    auto *source = isSgSourceFile(&project->get_file(0));
    auto graph = parseAstFileJson(buildJson(source, checkpoint, source),
                                  checkpointName(checkpoint));
    for (const auto &record : graph.nodes) {
      if (record.kind != "SgRequiresExpr")
        continue;
      const auto syntax =
          singleEdgeTarget(record, "local_parameter_list_syntax");
      if (syntax == 0)
        continue;
      for (unsigned mutation = 0; mutation != 3; ++mutation) {
        auto malformed = graph;
        auto &list = malformed.nodes.at(malformed.index_by_id.at(syntax));
        const auto argument = edgesFor(list, "args").front().target;
        auto &parameter =
            malformed.nodes.at(malformed.index_by_id.at(argument));
        if (mutation == 0) {
          list.edges.erase(std::remove_if(list.edges.begin(), list.edges.end(),
                                          [](const auto &edge) {
                                            return edge.field == "args";
                                          }),
                           list.edges.end());
        } else if (mutation == 1) {
          parameter.properties.object.at("name") = JsonValue::string("wrong");
        } else {
          for (auto &edge : parameter.edges)
            if (edge.field == "scope")
              edge.target = syntax;
        }
        const auto names = SgInitializedName::numberOfNodes();
        const auto lists = SgFunctionParameterList::numberOfNodes();
        const auto scopes = SgFunctionParameterScope::numberOfNodes();
        const auto positions = Sg_File_Info::numberOfNodes();
        const auto revision = SgNode::get_globalAstModificationSequence();
        for (bool full_reader : {false, true}) {
          NodeMap nodes;
          bool rejected = false;
          try {
            if (full_reader)
              (void)reconstructSourceFile(malformed, source);
            else
              rebuildConstructorOnlyNodes(malformed, nodes);
          } catch (const std::runtime_error &error) {
            const std::string expected =
                mutation == 0 ? "AST JSON requires-expression written "
                                "parameters lack exact ownership"
                              : "AST JSON requires-expression written "
                                "parameter differs from its lookup object";
            if (error.what() != expected)
              throw;
            rejected = true;
          }
          ROSE_ASSERT(rejected && nodes.empty() &&
                      SgInitializedName::numberOfNodes() == names &&
                      SgFunctionParameterList::numberOfNodes() == lists &&
                      SgFunctionParameterScope::numberOfNodes() == scopes &&
                      Sg_File_Info::numberOfNodes() == positions &&
                      SgNode::get_globalAstModificationSequence() == revision);
        }
      }
      break;
    }
    auto *copy = reconstructSourceFile(graph, source);
    replaceFileInProject(source, copy);
    validateAdjustedParameters(project);
  }

  {
    auto *source = isSgSourceFile(&project->get_file(0));
    ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
    auto *enclosing = source->get_globalScope();
    const size_t scopes = SgFunctionParameterScope::numberOfNodes();
    const size_t expressions = SgRequiresExpr::numberOfNodes();
    auto *scope = new SgFunctionParameterScope();
    SageInterface::setSemanticOnlyFrontendSourcePosition(scope);
    SageInterface::beginDetachedFunctionParameterScopeConstruction(
        scope, enclosing, enclosing);
    auto *requirements = SageBuilder::buildExprListExp_nfi();
    requirements->append_expression(SageBuilder::buildSimpleRequirement_nfi(
        SageBuilder::buildIntVal_nfi(1)));
    auto *expression = new SgRequiresExpr(
        nullptr, new SgFunctionParameterList(), scope, requirements);
    ROSE_ASSERT(scope->get_parent() == expression &&
                scope->get_construction_physical_output_owner() == nullptr &&
                scope->get_construction_semantic_scope() == nullptr &&
                scope->get_completed_semantic_scope() == enclosing &&
                scope->get_scope() == enclosing);
    SageInterface::deleteAST(expression,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(SgFunctionParameterScope::numberOfNodes() == scopes &&
                SgRequiresExpr::numberOfNodes() == expressions);
  }

  size_t matchingRequiresExpressions = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgRequiresExpr)) {
    SgRequiresExpr *requiresExpression = isSgRequiresExpr(node);
    SgFunctionParameterList *parameters =
        requiresExpression != nullptr
            ? requiresExpression->get_local_parameter_list()
            : nullptr;
    if (parameters == nullptr || parameters->get_args().size() != 2) {
      continue;
    }
    SgFunctionParameterScope *scope =
        requiresExpression->get_local_parameter_scope();
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(scope->get_parent() == requiresExpression);
    ROSE_ASSERT(parameters->get_parent() == requiresExpression);

    std::set<SgInitializedName *> parameterIdentities;
    for (SgInitializedName *parameter : parameters->get_args()) {
      ROSE_ASSERT(parameter != nullptr);
      ROSE_ASSERT(parameter->get_parent() == parameters);
      ROSE_ASSERT(parameter->get_scope() == scope);
      ROSE_ASSERT(scope->find_symbol_from_declaration(parameter) != nullptr);
      ROSE_ASSERT(parameterIdentities.insert(parameter).second);
    }

    size_t exactReferences = 0;
    for (SgNode *referenceNode :
         NodeQuery::querySubTree(requiresExpression, V_SgVarRefExp)) {
      SgVarRefExp *reference = isSgVarRefExp(referenceNode);
      SgVariableSymbol *symbol =
          reference != nullptr ? reference->get_symbol() : nullptr;
      SgInitializedName *declaration =
          symbol != nullptr ? symbol->get_declaration() : nullptr;
      exactReferences += parameterIdentities.count(declaration);
    }
    // Each of the four requirements has one evaluated reference to each local
    // parameter.  References contained in decltype type operands are not AST
    // expression children and therefore are intentionally outside this query.
    ROSE_ASSERT(exactReferences >= 8);
    ++matchingRequiresExpressions;
  }
  ROSE_ASSERT(matchingRequiresExpressions >= 1);
  std::map<std::string, SgTemplateArgument *> arguments;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *declaration = isSgInitializedName(node);
    const std::string name = declaration->get_name().getString();
    if (name.find("rex_requires_") != 0)
      continue;
    auto *initializer = isSgAssignInitializer(declaration->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    auto *expression = isSgRequiresExpr(initializer->get_operand());
    ROSE_ASSERT(expression != nullptr);
    arguments.emplace(name, SageBuilder::buildTemplateArgument(
                                SageInterface::copyExpression(expression)));
  }
  ROSE_ASSERT(arguments.size() == 6);
  auto verifyGroup = [&](const char *originalName, const char *renamedName,
                         const char *wrongName) {
    auto *original = arguments.at(originalName);
    auto *renamed = arguments.at(renamedName);
    auto *wrong = arguments.at(wrongName);
    ROSE_ASSERT(SageInterface::templateArgumentEquivalence(original, renamed));
    ROSE_ASSERT(SageInterface::templateArgumentEquivalence(renamed, original));
    ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(original, wrong));
    ROSE_ASSERT(!SageInterface::templateArgumentEquivalence(wrong, original));
  };
  verifyGroup("rex_requires_original", "rex_requires_renamed",
              "rex_requires_wrong_binding");
  verifyGroup("rex_requires_call_original", "rex_requires_call_renamed",
              "rex_requires_call_wrong");
  for (const auto &argument : arguments)
    SageInterface::deleteAST(argument.second);
  AstTests::runAllTests(project);
  return 0;
}
