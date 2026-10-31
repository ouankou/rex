#include "rose.h"

#include <algorithm>
#include <iostream>
#include <set>

bool unparse_all = false;

bool isUnspelledTemplateParameterValue(SgExpression *expression) {
  auto *value = isSgTemplateParameterVal(expression);
  if (value == nullptr)
    return false;
  auto *parameter = value->get_template_parameter();
  ROSE_ASSERT(parameter != nullptr &&
              parameter->get_initializedName() != nullptr);
  if (!parameter->get_initializedName()->get_name().is_null())
    return false;
  ROSE_ASSERT(value->get_literal_spelling_form() ==
              SgValueExp::e_literal_canonical_generated);
  ROSE_ASSERT(value->get_type() == parameter->get_type());
  auto *argument = isSgTemplateArgument(value->get_parent());
  ROSE_ASSERT(argument != nullptr &&
              argument->get_argumentType() ==
                  SgTemplateArgument::nontype_argument &&
              argument->get_expression() == value);
  auto *owner = isSgNonrealDecl(argument->get_parent());
  auto *declaration = isSgTemplateClassDeclaration(parameter->get_parent());
  ROSE_ASSERT(owner != nullptr && declaration != nullptr &&
              owner->get_nonreal_template_role() ==
                  SgNonrealDecl::e_nonreal_template_id);
  auto *target = owner->get_templateDeclaration();
  auto *canonical = declaration->get_firstNondefiningDeclaration();
  ROSE_ASSERT(target != nullptr && canonical != nullptr &&
              target->get_firstNondefiningDeclaration() == canonical);
  const auto &parameters = declaration->get_templateParameters();
  const auto &arguments = owner->get_tpl_args();
  const int position = value->get_template_parameter_position();
  ROSE_ASSERT(position >= 0 &&
              static_cast<size_t>(position) < parameters.size());
  ROSE_ASSERT(arguments.size() == parameters.size() &&
              parameters[position] == parameter &&
              arguments[position] == argument);
  ROSE_ASSERT(std::count(arguments.begin(), arguments.end(), argument) == 1);
  return true;
}

SgFunctionDeclaration *unnamedReferenceOwner(SgVarRefExp *reference) {
  ROSE_ASSERT(reference != nullptr && reference->get_symbol() != nullptr);
  auto *name = reference->get_symbol()->get_declaration();
  ROSE_ASSERT(name != nullptr);
  if (!name->get_name().is_null())
    return nullptr;
  if (auto *variable = isSgVariableDeclaration(name->get_parent())) {
    auto *scope = isSgClassDefinition(variable->get_scope());
    auto *record =
        isSgClassDeclaration(variable->get_baseTypeDefiningDeclaration());
    auto *function = isSgMemberFunctionDeclaration(
        SageInterface::getEnclosingFunctionDeclaration(reference));
    ROSE_ASSERT(scope != nullptr && record != nullptr && function != nullptr);
    ROSE_ASSERT(record->get_isUnNamed() && record->get_parent() == variable &&
                record->get_definingDeclaration() == record &&
                name->get_type() == record->get_type());
    ROSE_ASSERT(variable->get_variables().size() == 1 &&
                variable->get_variables().front() == name &&
                name->get_declptr() == variable && name->get_scope() == scope &&
                variable->get_scope() == scope);
    auto *owner =
        isSgClassDeclaration(function->get_associatedClassDeclaration());
    ROSE_ASSERT(
        owner != nullptr &&
        owner->get_firstNondefiningDeclaration() ==
            scope->get_declaration()->get_firstNondefiningDeclaration());
    ROSE_ASSERT(function->get_functionModifier().isMarkedDefault() ||
                function->get_file_info()->isCompilerGenerated());
    return function;
  }
  auto *parameters = isSgFunctionParameterList(name->get_parent());
  auto *function = parameters != nullptr
                       ? isSgFunctionDeclaration(parameters->get_parent())
                       : nullptr;
  if (function == nullptr || function->get_definition() == nullptr)
    return nullptr;
  ROSE_ASSERT(function->get_parameterList() == parameters &&
              function->get_definingDeclaration() == function &&
              name->get_declptr() == function &&
              name->get_scope() == function->get_definition());
  const auto &arguments = parameters->get_args();
  ROSE_ASSERT(std::count(arguments.begin(), arguments.end(), name) == 1);
  ROSE_ASSERT(SageInterface::getEnclosingFunctionDeclaration(reference) ==
              function);
  return function;
}

SgExpression *
recordSemanticExpressionPath(SgExpression *marker,
                             std::set<SgExpression *> &non_source_expressions) {
  ROSE_ASSERT(marker != nullptr);
  if (isSgTemplateParameterVal(marker) != nullptr) {
    ROSE_ASSERT(isUnspelledTemplateParameterValue(marker));
    non_source_expressions.insert(marker);
    return marker;
  }
  SgExpression *root = marker;
  bool hidden_by_source_projection = false;
  non_source_expressions.insert(marker);
  while (true) {
    auto *parent = isSgExpression(root->get_parent());
    if (auto *binding = isSgCxxArrayInitBinding(root->get_parent());
        binding != nullptr && (isSgVarRefExp(marker) != nullptr ||
                               isSgCxxArrayInitReference(marker) != nullptr)) {
      ROSE_ASSERT(binding->get_common_source() == root);
      auto *loop = isSgCxxArrayInitLoop(binding->get_parent());
      ROSE_ASSERT(loop != nullptr && loop->get_binding() == binding);
      loop->validate_bindings();
      parent = loop;
    }
    if (parent == nullptr)
      break;
    auto *original = parent->get_originalExpressionTree();
    if (original != nullptr && original != root)
      hidden_by_source_projection = true;
    if (auto *pseudo = isSgPseudoObjectExp(parent))
      if (pseudo->get_written_expression_checked() != root)
        hidden_by_source_projection = true;
    if (auto *loop = isSgCxxArrayInitLoop(parent))
      if (loop->get_element_initializer() == root)
        hidden_by_source_projection = true;
    root = parent;
    if (!hidden_by_source_projection)
      non_source_expressions.insert(root);
  }
  if (auto *reference = isSgCxxDecompositionRefExp(marker)) {
    reference->validate();
    auto *binding = isSgInitializedName(root->get_parent());
    ROSE_ASSERT(binding != nullptr);
    binding->validate_structured_binding();
    ROSE_ASSERT(binding->get_structured_binding_owner() ==
                reference->get_decomposition());
    if (binding->get_structured_binding_kind() ==
        SgInitializedName::e_resolved_structured_binding) {
      ROSE_ASSERT(binding->get_structured_binding_value() == root);
    } else {
      ROSE_ASSERT(binding->get_structured_binding_kind() ==
                  SgInitializedName::e_structured_binding_holding_variable);
      ROSE_ASSERT(binding->get_initializer() == root);
      auto *alias = binding->get_structured_binding_alias();
      ROSE_ASSERT(alias != nullptr &&
                  alias->get_structured_binding_holding_variable() == binding);
      ROSE_ASSERT(alias->get_structured_binding_owner() ==
                  reference->get_decomposition());
    }
  } else if (auto *reference = isSgVarRefExp(marker)) {
    ROSE_ASSERT(unnamedReferenceOwner(reference) != nullptr);
  } else if (auto *reference = isSgCxxArrayInitReference(marker)) {
    reference->validate();
    auto *binding = reference->get_binding();
    auto *loop = isSgCxxArrayInitLoop(binding->get_parent());
    ROSE_ASSERT(loop != nullptr && loop->get_binding() == binding);
    loop->validate_bindings();
    ROSE_ASSERT(
        SageInterface::isAncestor(loop->get_element_initializer(), reference));
  } else if (auto *initializer = isSgConstructorInitializer(marker)) {
    initializer->validate_argument_context();
    ROSE_ASSERT(
        root == marker &&
        initializer->get_argument_source() !=
            SgConstructorInitializer::e_constructor_argument_expressions);
  } else if (auto *reference = isSgFunctionParameterRefExp(marker)) {
    auto *signature = isSgFunctionType(root->get_parent());
    ROSE_ASSERT(signature != nullptr && !signature->has_source_prototype());
    ROSE_ASSERT(signature->get_exception_specification_expression() == root);
    const auto &arguments = signature->get_arguments();
    const int position = reference->get_parameter_number();
    ROSE_ASSERT(reference->get_parameter_levels_up() == 0 && position >= 0 &&
                static_cast<size_t>(position) < arguments.size());
    ROSE_ASSERT(SageInterface::isEquivalentType(
        SageBuilder::functionParameterTypeFromDeclaredType(
            reference->get_type()),
        arguments[position]));
  } else {
    auto *destruction = isSgCxxSubobjectDestruction(marker);
    ROSE_ASSERT(destruction != nullptr);
    destruction->validate();
    auto *epilogue = isSgExprListExp(root);
    auto *definition = isSgFunctionDefinition(root->get_parent());
    ROSE_ASSERT(epilogue != nullptr && definition != nullptr &&
                definition->get_destructor_epilogue() == epilogue);
    ROSE_ASSERT(destruction->get_parent() == epilogue);
  }
  return root;
}

class TypeUnparserOnAST : public AstSimpleProcessing {
  std::set<SgExpression *> non_source_expressions;

public:
  explicit TypeUnparserOnAST(SgProject *project) {
    VariantVector variants(V_SgCxxDecompositionRefExp);
    variants.push_back(V_SgCxxSubobjectDestruction);
    variants.push_back(V_SgVarRefExp);
    variants.push_back(V_SgTemplateParameterVal);
    variants.push_back(V_SgFunctionParameterRefExp);
    variants.push_back(V_SgCxxArrayInitReference);
    variants.push_back(V_SgConstructorInitializer);
    for (SgNode *node : NodeQuery::querySubTree(project, variants)) {
      if (auto *initializer = isSgConstructorInitializer(node))
        if (initializer->get_argument_source() ==
            SgConstructorInitializer::e_constructor_argument_expressions)
          continue;
      if (auto *reference = isSgVarRefExp(node))
        if (unnamedReferenceOwner(reference) == nullptr)
          continue;
      if (isSgTemplateParameterVal(node) != nullptr &&
          !isUnspelledTemplateParameterValue(isSgExpression(node)))
        continue;
      recordSemanticExpressionPath(isSgExpression(node),
                                   non_source_expressions);
    }
  }

  void visit(SgNode *n) {
    SgStatement *statement = isSgStatement(n);
    Sg_File_Info *statement_info =
        statement != nullptr ? statement->get_file_info() : nullptr;
    const bool exact_source_statement =
        statement_info != nullptr && statement_info->get_line() > 0 &&
        statement_info->get_physical_file_id() >= 0 &&
        !statement_info->isCompilerGenerated() &&
        !statement_info->isFrontendSpecific() &&
        !statement_info->isSourcePositionUnavailableInFrontend() &&
        statement_info->isOutputInCodeGeneration() &&
        isSgAuxiliaryDeclarationList(statement->get_parent()) == nullptr &&
        isSgCatchStatementSeq(statement) == nullptr &&
        isSgFunctionParameterList(statement) == nullptr &&
        isSgCtorInitializerList(statement) == nullptr;
    const bool hasTypedStandaloneEmitter =
        isSgProject(n) != nullptr || isSgSourceFile(n) != nullptr ||
        isSgExpression(n) != nullptr || exact_source_statement;
    if (unparse_all && hasTypedStandaloneEmitter &&
        non_source_expressions.count(isSgExpression(n)) == 0) {
      if (SgFunctionDeclaration *d = isSgFunctionDeclaration(n)) {
        std::cout << "function declaration, name = "
                  << d->get_name().getString() << std::endl;
      }
      std::cout << n->unparseToString() << std::endl;
    }

    SgInitializedName *initialized_name = isSgInitializedName(n);
    Sg_File_Info *name_info = initialized_name != nullptr
                                  ? initialized_name->get_file_info()
                                  : nullptr;
    if (initialized_name != nullptr &&
        initialized_name->get_type() != nullptr && name_info != nullptr &&
        name_info->get_line() > 0 && name_info->get_physical_file_id() >= 0 &&
        !name_info->isCompilerGenerated() && !name_info->isFrontendSpecific() &&
        !name_info->isSourcePositionUnavailableInFrontend()) {
      SgSourceFile *source_file =
          SageInterface::getEnclosingSourceFile(initialized_name);
      SgScopeStatement *scope = initialized_name->get_scope();
      ROSE_ASSERT(source_file != nullptr);
      ROSE_ASSERT(scope != nullptr);
      SgUnparse_Info info;
      info.set_current_source_file(source_file);
      info.set_current_scope(scope);
      info.set_template_argument_qualification_context(
          SageInterface::getEnclosingStatement(initialized_name));
      info.set_reference_node_for_qualification(initialized_name);
      std::cout << initialized_name->get_type()->unparseToString(&info)
                << std::endl;
    }
  }
};

int main(int argc, char **argv) {
  VariantT rejected_expression = V_SgNode;
  bool reject_semantic_owner = false;
  // Parse command line args for things we recognize
  for (int argno = 1; argno < argc; ++argno) {
    if (!strcmp(argv[argno], "--all")) {
      unparse_all = true;
    } else if (!strcmp(argv[argno], "--reject-decomposition-storage")) {
      ROSE_ASSERT(rejected_expression == V_SgNode);
      rejected_expression = V_SgCxxDecompositionRefExp;
    } else if (!strcmp(argv[argno], "--reject-subobject-destruction")) {
      ROSE_ASSERT(rejected_expression == V_SgNode);
      rejected_expression = V_SgCxxSubobjectDestruction;
    } else if (!strcmp(argv[argno], "--reject-unnamed-template-parameter")) {
      ROSE_ASSERT(rejected_expression == V_SgNode);
      rejected_expression = V_SgTemplateParameterVal;
    } else if (!strcmp(argv[argno], "--reject-array-init-reference")) {
      ROSE_ASSERT(rejected_expression == V_SgNode);
      rejected_expression = V_SgCxxArrayInitReference;
    } else if (!strcmp(argv[argno], "--reject-anonymous-field")) {
      ROSE_ASSERT(rejected_expression == V_SgNode);
      rejected_expression = V_SgVarRefExp;
    } else if (!strcmp(argv[argno], "--reject-inherited-constructor")) {
      ROSE_ASSERT(rejected_expression == V_SgNode);
      rejected_expression = V_SgConstructorInitializer;
    } else if (!strcmp(argv[argno], "--reject-semantic-owner")) {
      reject_semantic_owner = true;
    } else {
      continue;
    }
    memmove(argv + argno, argv + argno + 1, (argc - argno) * sizeof(argv[0]));
    --argno;
    --argc;
  }
  ROSE_ASSERT(!unparse_all || rejected_expression == V_SgNode);
  ROSE_ASSERT(!reject_semantic_owner || rejected_expression != V_SgNode);

  // The frontend() will parse the rest of the arguments
  SgProject *p = frontend(argc, argv);
  ROSE_ASSERT(p);

  TypeUnparserOnAST t1(p);
  if (rejected_expression != V_SgNode) {
    const auto references = NodeQuery::querySubTree(p, rejected_expression);
    ROSE_ASSERT(!references.empty());
    std::set<SgExpression *> non_source_expressions;
    const auto position =
        std::find_if(references.begin(), references.end(), [&](SgNode *node) {
          if (rejected_expression == V_SgVarRefExp) {
            auto *reference = isSgVarRefExp(node);
            return unnamedReferenceOwner(reference) != nullptr &&
                   isSgVariableDeclaration(reference->get_symbol()
                                               ->get_declaration()
                                               ->get_parent()) != nullptr;
          }
          if (auto *initializer = isSgConstructorInitializer(node))
            return initializer->get_argument_source() !=
                   SgConstructorInitializer::e_constructor_argument_expressions;
          return rejected_expression != V_SgTemplateParameterVal ||
                 isUnspelledTemplateParameterValue(isSgExpression(node));
        });
    ROSE_ASSERT(position != references.end());
    auto *marker = isSgExpression(*position);
    auto *root = recordSemanticExpressionPath(marker, non_source_expressions);
    ROSE_ASSERT(!reject_semantic_owner ||
                non_source_expressions.count(root) == 1);
    (void)(reject_semantic_owner ? root : marker)->unparseToString();
    return 1;
  }

  // The traversal only unparses, so one qualification session serves every
  // request instead of recomputing each file's qualification per request.
  UnparseQualificationSession qualificationSession;
  t1.traverse(p, preorder);
}
