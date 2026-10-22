#include "rose.h"

#include "fixupCopy.h"
#include "nodeQuery.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace {
void requireExactSourceSurface(SgDeclarationStatement *declaration,
                               unsigned expectedStartLine,
                               unsigned expectedEndLine = 0) {
  if (expectedEndLine == 0) {
    expectedEndLine = expectedStartLine;
  }
  ROSE_ASSERT(declaration != nullptr);
  Sg_File_Info *primary = declaration->get_file_info();
  Sg_File_Info *start = declaration->get_startOfConstruct();
  Sg_File_Info *end = declaration->get_endOfConstruct();
  ROSE_ASSERT(primary != nullptr && start != nullptr && end != nullptr);
  ROSE_ASSERT(!primary->isCompilerGenerated());
  ROSE_ASSERT(!start->isCompilerGenerated());
  ROSE_ASSERT(!end->isCompilerGenerated());
  ROSE_ASSERT(!primary->isFrontendSpecific());
  ROSE_ASSERT(!start->isFrontendSpecific());
  ROSE_ASSERT(!end->isFrontendSpecific());
  ROSE_ASSERT(primary->get_physical_file_id() >= 0);
  ROSE_ASSERT(primary->get_physical_file_id() == start->get_physical_file_id());
  ROSE_ASSERT(start->get_physical_file_id() == end->get_physical_file_id());
  ROSE_ASSERT(primary->get_line() == expectedStartLine);
  ROSE_ASSERT(start->get_line() == expectedStartLine);
  ROSE_ASSERT(end->get_line() == expectedEndLine);
  ROSE_ASSERT(primary->get_col() == start->get_col());
  ROSE_ASSERT(declaration->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(start->get_source_sequence_number() ==
              *declaration->get_translation_unit_source_order());
}

bool sourcePositionBefore(const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
  ROSE_ASSERT(lhs != nullptr && rhs != nullptr);
  ROSE_ASSERT(lhs->get_physical_file_id() == rhs->get_physical_file_id());
  return lhs->get_line() < rhs->get_line() ||
         (lhs->get_line() == rhs->get_line() &&
          lhs->get_col() < rhs->get_col());
}

bool isSemanticIdentity(SgFunctionDeclaration *declaration) {
  SgAuxiliaryDeclarationList *owner =
      declaration != nullptr
          ? isSgAuxiliaryDeclarationList(declaration->get_parent())
          : nullptr;
  SgScopeStatement *scope =
      owner != nullptr ? isSgScopeStatement(owner->get_parent()) : nullptr;
  return scope != nullptr && scope->get_auxiliary_declarations() == owner &&
         declaration->get_scope() == scope &&
         std::count(owner->get_declarations().begin(),
                    owner->get_declarations().end(), declaration) == 1 &&
         declaration->get_file_info() != nullptr &&
         declaration->get_file_info()->isCompilerGenerated();
}

void requireExactSemanticProvenance(SgLocatedNode *node) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *primary = node->get_file_info();
  Sg_File_Info *start = node->get_startOfConstruct();
  Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(primary != nullptr && start != nullptr && end != nullptr);
  ROSE_ASSERT(primary == start);
  ROSE_ASSERT(start != end);
  for (Sg_File_Info *position : {primary, start, end}) {
    ROSE_ASSERT(position->get_parent() == node);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_raw_line() == 0);
    ROSE_ASSERT(position->get_raw_col() == 0);
  }
  if (SgExpression *expression = isSgExpression(node)) {
    Sg_File_Info *operatorPosition = expression->get_operatorPosition();
    ROSE_ASSERT(operatorPosition != nullptr);
    ROSE_ASSERT(operatorPosition->get_parent() == expression);
    ROSE_ASSERT(operatorPosition->isCompilerGenerated());
    ROSE_ASSERT(operatorPosition->isFrontendSpecific());
    ROSE_ASSERT(!operatorPosition->isTransformation());
    ROSE_ASSERT(!operatorPosition->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(operatorPosition->isOutputInCodeGeneration());
    ROSE_ASSERT(operatorPosition->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(operatorPosition->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(operatorPosition->get_raw_line() == 0);
    ROSE_ASSERT(operatorPosition->get_raw_col() == 0);
  }
}

void requireExactSemanticOwnedSubtree(SgNode *root) {
  ROSE_ASSERT(root != nullptr);
  std::unordered_set<SgNode *> visited;
  std::vector<SgNode *> worklist{root};
  std::size_t locatedCount = 0;
  while (!worklist.empty()) {
    SgNode *current = worklist.back();
    worklist.pop_back();
    if (current == nullptr || !visited.insert(current).second) {
      continue;
    }
    if (SgLocatedNode *located = isSgLocatedNode(current)) {
      requireExactSemanticProvenance(located);
      ++locatedCount;
    }
    for (SgNode *child : current->get_traversalSuccessorContainer()) {
      if (child != nullptr && child->get_parent() == current) {
        worklist.push_back(child);
      }
    }
  }
  ROSE_ASSERT(locatedCount > 0);
}

void checkImplicitConstructorSemanticSubtree(SgProject *project) {
  SgMemberFunctionDeclaration *constructor = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    SgMemberFunctionDeclaration *candidate =
        isSgMemberFunctionDeclaration(node);
    if (candidate == nullptr || candidate->get_name() != "defaults" ||
        !candidate->get_specialFunctionModifier().isConstructor() ||
        candidate->get_definition() == nullptr ||
        !isSemanticIdentity(candidate)) {
      continue;
    }
    ROSE_ASSERT(candidate->get_parameterList() != nullptr);
    if (!candidate->get_parameterList()->get_args().empty()) {
      continue;
    }
    ROSE_ASSERT(constructor == nullptr);
    constructor = candidate;
  }
  ROSE_ASSERT(constructor != nullptr);

  SgFunctionDeclaration *first =
      isSgFunctionDeclaration(constructor->get_firstNondefiningDeclaration());
  SgFunctionDefinition *definition = constructor->get_definition();
  SgBasicBlock *body = definition != nullptr ? definition->get_body() : nullptr;
  ROSE_ASSERT(first != nullptr && first != constructor);
  ROSE_ASSERT(isSemanticIdentity(first));
  ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
  ROSE_ASSERT(first->get_definingDeclaration() == constructor);
  ROSE_ASSERT(constructor->get_definingDeclaration() == constructor);
  ROSE_ASSERT(first->get_frontend_declaration_origin() ==
              SgFunctionDeclaration::e_frontend_declaration_implicit);
  ROSE_ASSERT(constructor->get_frontend_declaration_origin() ==
              SgFunctionDeclaration::e_frontend_declaration_implicit);
  ROSE_ASSERT(first->get_frontend_source_ownership() ==
              SgFunctionDeclaration::e_frontend_source_implicit);
  ROSE_ASSERT(constructor->get_frontend_source_ownership() ==
              SgFunctionDeclaration::e_frontend_source_implicit);
  ROSE_ASSERT(definition != nullptr);
  ROSE_ASSERT(definition->get_declaration() == constructor);
  ROSE_ASSERT(definition->get_parent() == constructor);
  ROSE_ASSERT(body != nullptr);
  ROSE_ASSERT(definition->get_body() == body);
  ROSE_ASSERT(body->get_parent() == definition);

  requireExactSemanticOwnedSubtree(first);
  requireExactSemanticOwnedSubtree(constructor);
}

void checkExplicitInstantiationOwners(SgProject *project) {
  std::size_t directiveCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationDirectiveStatement)) {
    SgTemplateInstantiationDirectiveStatement *directive =
        isSgTemplateInstantiationDirectiveStatement(node);
    SgFunctionDeclaration *function =
        directive != nullptr
            ? isSgFunctionDeclaration(directive->get_declaration())
            : nullptr;
    SgTemplateInstantiationFunctionDecl *instantiation =
        isSgTemplateInstantiationFunctionDecl(function);
    if (instantiation == nullptr ||
        instantiation->get_templateName() != "selected") {
      continue;
    }
    ++directiveCount;
    ROSE_ASSERT(function->get_parent() == directive);
    ROSE_ASSERT(directive->get_declaration() == function);
    SgFunctionDeclaration *semanticIdentity =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(semanticIdentity != nullptr);
    ROSE_ASSERT(semanticIdentity != function);
    ROSE_ASSERT(semanticIdentity->get_firstNondefiningDeclaration() ==
                semanticIdentity);
    ROSE_ASSERT(isSemanticIdentity(semanticIdentity));
    ROSE_ASSERT(function->get_definingDeclaration() ==
                semanticIdentity->get_definingDeclaration());
    ROSE_ASSERT(function->get_scope() == semanticIdentity->get_scope());
    SgScopeStatement *scope = function->get_scope();
    ROSE_ASSERT(scope != nullptr);
    ROSE_ASSERT(scope->find_symbol_from_declaration(function) == nullptr);
    SgSymbol *symbol = scope->find_symbol_from_declaration(semanticIdentity);
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(symbol->get_symbol_basis() == semanticIdentity);
    ROSE_ASSERT(symbol->get_parent() == scope->get_symbol_table());
    ROSE_ASSERT(function->get_symbol_from_symbol_table() == symbol);
    ROSE_ASSERT(function->get_parameterList() != nullptr);
    ROSE_ASSERT(function->get_parameterList()->get_parent() == function);
    for (SgInitializedName *parameter :
         function->get_parameterList()->get_args()) {
      ROSE_ASSERT(parameter != nullptr);
      ROSE_ASSERT(parameter->get_parent() == function->get_parameterList());
      ROSE_ASSERT(parameter->get_declptr() == function);
      ROSE_ASSERT(parameter->get_scope() != nullptr);
      ROSE_ASSERT(parameter->get_initializer() == nullptr);
    }
    SgFunctionParameterList *syntax = function->get_parameterList_syntax();
    ROSE_ASSERT(syntax != nullptr && syntax != function->get_parameterList());
    ROSE_ASSERT(syntax->get_parent() == function);
    ROSE_ASSERT(syntax->get_args().size() == function->get_args().size());
    for (size_t index = 0; index < syntax->get_args().size(); ++index) {
      SgInitializedName *parameter = syntax->get_args()[index];
      ROSE_ASSERT(parameter->get_parent() == syntax);
      ROSE_ASSERT(parameter->get_declptr() == function);
      ROSE_ASSERT(parameter->get_scope() ==
                  function->get_args()[index]->get_scope());
    }
    SgScopeStatement *owner = isSgScopeStatement(directive->get_parent());
    ROSE_ASSERT(owner != nullptr);
    ROSE_ASSERT(directive->get_scope() == owner);

    ROSE_ASSERT(directive->get_file_info() != nullptr);
    const unsigned expectedLine = directive->get_file_info()->get_line();
    ROSE_ASSERT(expectedLine == 5 || expectedLine == 6 || expectedLine == 9);
    const char *expectedSpecializedName = expectedLine == 5 ? "selected<long>"
                                          : expectedLine == 6
                                              ? "selected<char>"
                                              : "selected<short>";
    ROSE_ASSERT(instantiation->get_name() == expectedSpecializedName);
    ROSE_ASSERT(instantiation->get_templateArguments().size() == 1);
    ROSE_ASSERT(instantiation->get_templateArguments().front() != nullptr);
    ROSE_ASSERT(instantiation->get_templateArguments()
                    .front()
                    ->get_explicitlySpecified());
    ROSE_ASSERT(directive->get_do_not_instantiate() ==
                (expectedLine == 5 || expectedLine == 9));
    requireExactSourceSurface(directive, expectedLine);
    requireExactSourceSurface(function, expectedLine);
    ROSE_ASSERT(sourcePositionBefore(directive->get_startOfConstruct(),
                                     function->get_startOfConstruct()));
    ROSE_ASSERT(sourcePositionBefore(function->get_endOfConstruct(),
                                     directive->get_endOfConstruct()));
    ROSE_ASSERT(*directive->get_translation_unit_source_order() <
                *function->get_translation_unit_source_order());
  }
  ROSE_ASSERT(directiveCount == 3);

  std::size_t semanticDefinitionCount = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationFunctionDecl)) {
    SgTemplateInstantiationFunctionDecl *function =
        isSgTemplateInstantiationFunctionDecl(node);
    if (function == nullptr || function->get_templateName() != "selected" ||
        function->get_definition() == nullptr ||
        !isSemanticIdentity(function)) {
      continue;
    }
    ++semanticDefinitionCount;
    ROSE_ASSERT(!function->get_translation_unit_source_order().has_value());
    SgFunctionDeclaration *first =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr && first != function);
    ROSE_ASSERT(isSemanticIdentity(first));
    ROSE_ASSERT(!first->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(first->get_definingDeclaration() == function);
    ROSE_ASSERT(function->get_definingDeclaration() == function);
    for (SgFunctionDeclaration *semantic :
         {first, static_cast<SgFunctionDeclaration *>(function)}) {
      auto *instantiation = isSgTemplateInstantiationFunctionDecl(semantic);
      ROSE_ASSERT(instantiation != nullptr);
      ROSE_ASSERT(instantiation->get_templateArguments().empty());
      ROSE_ASSERT(!instantiation->get_template_argument_list_is_explicit());
      SgFunctionParameterList *parameters = semantic->get_parameterList();
      ROSE_ASSERT(parameters != nullptr &&
                  parameters->get_parent() == semantic);
      requireExactSemanticProvenance(parameters);
      for (SgInitializedName *parameter : parameters->get_args()) {
        ROSE_ASSERT(parameter != nullptr &&
                    parameter->get_parent() == parameters);
        requireExactSemanticProvenance(parameter);
      }
    }
  }
  if (semanticDefinitionCount != 1) {
    std::cerr << "REX_TEST_FAILURE[semantic-instantiation-definition-count]: "
              << semanticDefinitionCount << "\n";
    for (SgNode *node : NodeQuery::querySubTree(
             project, V_SgTemplateInstantiationFunctionDecl)) {
      SgTemplateInstantiationFunctionDecl *function =
          isSgTemplateInstantiationFunctionDecl(node);
      if (function != nullptr && function->get_templateName() == "selected") {
        std::cerr << "  function=" << function
                  << " name=" << function->get_name()
                  << " definition=" << function->get_definition()
                  << " semantic=" << isSemanticIdentity(function) << " parent="
                  << (function->get_parent() != nullptr
                          ? function->get_parent()->class_name()
                          : "<null>")
                  << "\n";
      }
    }
  }
  ROSE_ASSERT(semanticDefinitionCount == 1);
}

void checkSpecializationIdentity(SgProject *project) {
  std::size_t sourceSpecializations = 0;
  std::size_t defaultedSpecializations = 0;
  std::size_t defaultedInstantiations = 0;
  std::size_t expressionInstantiations = 0;
  SgTemplateInstantiationFunctionDecl *defaultedCanonical = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgTemplateInstantiationFunctionDecl *function =
        isSgTemplateInstantiationFunctionDecl(node);
    if (function != nullptr &&
        (function->get_templateName() == "rex_defaulted_instantiation" ||
         function->get_templateName() == "rex_expression_instantiation")) {
      const bool defaulted =
          function->get_templateName() == "rex_defaulted_instantiation";
      ROSE_ASSERT(function->get_deducedTemplateArguments().size() ==
                  (defaulted ? 2 : 1));
      if (isSemanticIdentity(function)) {
        ROSE_ASSERT(function->get_templateArguments().empty());
      } else {
        ROSE_ASSERT(isSgTemplateInstantiationDirectiveStatement(
                        function->get_parent()) != nullptr);
        ROSE_ASSERT(function->get_template_argument_list_is_explicit());
        ROSE_ASSERT(function->get_templateArguments().size() == 1);
        if (defaulted) {
          ROSE_ASSERT(
              function->get_templateArguments().front()->get_type() ==
              function->get_deducedTemplateArguments().front()->get_type());
          auto *syntax = function->get_type_syntax();
          ROSE_ASSERT(syntax != nullptr);
          ROSE_ASSERT(isSgTypeInt(syntax->get_return_type()->stripType(
                          SgType::STRIP_TYPEDEF_TYPE)) != nullptr);
          ++defaultedInstantiations;
        } else {
          auto *expression =
              function->get_templateArguments().front()->get_expression();
          ROSE_ASSERT(expression != nullptr);
          auto *source = expression->get_originalExpressionTree();
          auto *addition = isSgAddOp(source != nullptr ? source : expression);
          ROSE_ASSERT(addition != nullptr);
          ROSE_ASSERT(isSgIntVal(addition->get_lhs_operand()) != nullptr &&
                      isSgIntVal(addition->get_lhs_operand())->get_value() ==
                          1);
          ROSE_ASSERT(isSgIntVal(addition->get_rhs_operand()) != nullptr &&
                      isSgIntVal(addition->get_rhs_operand())->get_value() ==
                          1);
          ++expressionInstantiations;
        }
      }
    }
    if (function != nullptr &&
        function->get_templateName() == "rex_defaulted_specialization" &&
        function->get_specialization() ==
            SgDeclarationStatement::e_specialization &&
        !isSemanticIdentity(function)) {
      auto *canonical = isSgTemplateInstantiationFunctionDecl(
          function->get_firstNondefiningDeclaration());
      ROSE_ASSERT(canonical != nullptr);
      if (defaultedCanonical == nullptr)
        defaultedCanonical = canonical;
      ROSE_ASSERT(canonical == defaultedCanonical);
      ROSE_ASSERT(function->get_templateArguments().size() ==
                  (function->get_definition() == nullptr ? 2 : 1));
      ROSE_ASSERT(function->get_deducedTemplateArguments().size() == 2);
      ROSE_ASSERT(function->get_definingDeclaration() ==
                  canonical->get_definingDeclaration());
      ROSE_ASSERT(function->get_mangled_name() ==
                  canonical->get_mangled_name());
      ++defaultedSpecializations;
    }
    if (function == nullptr || function->get_templateName() != "selected" ||
        function->get_specialization() !=
            SgDeclarationStatement::e_specialization) {
      continue;
    }
    if (isSemanticIdentity(function)) {
      continue;
    }
    ++sourceSpecializations;
    ROSE_ASSERT(function->get_firstNondefiningDeclaration() == function);
    ROSE_ASSERT(function->get_definingDeclaration() == nullptr);
    ROSE_ASSERT(isSgNamespaceDefinitionStatement(function->get_parent()) !=
                nullptr);
    requireExactSourceSurface(function, 4);
  }
  ROSE_ASSERT(sourceSpecializations == 1);
  ROSE_ASSERT(defaultedSpecializations == 2);
  ROSE_ASSERT(defaultedInstantiations == 6);
  ROSE_ASSERT(expressionInstantiations == 1);
  std::size_t memberInstantiations = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           project, V_SgTemplateInstantiationMemberFunctionDecl)) {
    auto *member = isSgTemplateInstantiationMemberFunctionDecl(node);
    if (member->get_templateName() != "rex_member_instantiation")
      continue;
    ROSE_ASSERT(member->get_deducedTemplateArguments().size() == 2);
    if (isSemanticIdentity(member)) {
      ROSE_ASSERT(member->get_templateArguments().empty());
    } else {
      ROSE_ASSERT(isSgTemplateInstantiationDirectiveStatement(
                      member->get_parent()) != nullptr);
      ROSE_ASSERT(member->get_template_argument_list_is_explicit());
      ROSE_ASSERT(member->get_templateArguments().size() == 1);
      ++memberInstantiations;
    }
  }
  ROSE_ASSERT(memberInstantiations == 1);
}

void checkSyntheticFirstParameterOwnership(SgProject *project) {
  SgFunctionDeclaration *definition = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate != nullptr && candidate->get_name() == "owned_default" &&
        candidate->get_definition() != nullptr &&
        isSgClassDefinition(candidate->get_parent()) != nullptr) {
      ROSE_ASSERT(definition == nullptr);
      definition = candidate;
    }
  }
  ROSE_ASSERT(definition != nullptr);
  requireExactSourceSurface(definition, 12);
  SgFunctionDeclaration *first =
      isSgFunctionDeclaration(definition->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr && first != definition);
  ROSE_ASSERT(isSemanticIdentity(first));
  ROSE_ASSERT(!first->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(first->get_definingDeclaration() == definition);
  ROSE_ASSERT(definition->get_definingDeclaration() == definition);
  ROSE_ASSERT(first->get_parameterList() != nullptr);
  ROSE_ASSERT(definition->get_parameterList() != nullptr);
  ROSE_ASSERT(first->get_parameterList() != definition->get_parameterList());
  ROSE_ASSERT(first->get_parameterList()->get_args().size() == 1);
  ROSE_ASSERT(definition->get_parameterList()->get_args().size() == 1);
  requireExactSemanticProvenance(first->get_parameterList());
  requireExactSemanticProvenance(
      first->get_parameterList()->get_args().front());
  ROSE_ASSERT(
      first->get_parameterList()->get_args().front()->get_initializer() ==
      nullptr);
  ROSE_ASSERT(
      definition->get_parameterList()->get_args().front()->get_initializer() !=
      nullptr);
}

void checkSourceMemberStructuralCtorListProvenance(SgProject *project) {
  SgMemberFunctionDeclaration *constructor = nullptr;
  SgMemberFunctionDeclaration *ordinaryMethod = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    SgMemberFunctionDeclaration *candidate =
        isSgMemberFunctionDeclaration(node);
    if (candidate == nullptr || candidate->get_definition() == nullptr ||
        isSemanticIdentity(candidate)) {
      continue;
    }
    if (candidate->get_name() == "rex_structural_ctor_list_owner" &&
        candidate->get_specialFunctionModifier().isConstructor()) {
      ROSE_ASSERT(constructor == nullptr);
      constructor = candidate;
    } else if (candidate->get_name() == "rex_structural_nonconstructor") {
      ROSE_ASSERT(!candidate->get_specialFunctionModifier().isConstructor());
      ROSE_ASSERT(ordinaryMethod == nullptr);
      ordinaryMethod = candidate;
    }
  }
  ROSE_ASSERT(constructor != nullptr);
  ROSE_ASSERT(ordinaryMethod != nullptr);

  SgCtorInitializerList *constructorInitializers =
      constructor->get_CtorInitializerList();
  ROSE_ASSERT(constructorInitializers != nullptr);
  ROSE_ASSERT(constructorInitializers->get_parent() == constructor);
  ROSE_ASSERT(constructorInitializers->get_ctors().size() == 1);
  requireExactSourceSurface(constructorInitializers, 24);

  SgCtorInitializerList *ordinaryMethodInitializers =
      ordinaryMethod->get_CtorInitializerList();
  ROSE_ASSERT(ordinaryMethodInitializers != nullptr);
  ROSE_ASSERT(ordinaryMethodInitializers->get_parent() == ordinaryMethod);
  ROSE_ASSERT(ordinaryMethodInitializers->get_ctors().empty());
  ROSE_ASSERT(!ordinaryMethodInitializers->get_translation_unit_source_order()
                   .has_value());
  requireExactSemanticProvenance(ordinaryMethodInitializers);
}

void checkDefiningFunctionSourceSurfaces(SgProject *project) {
  SgFunctionDeclaration *primaryTemplateDefinition = nullptr;
  SgFunctionDeclaration *freeDefinition = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function == nullptr || function->get_definition() == nullptr ||
        isSemanticIdentity(function)) {
      continue;
    }
    if (function->get_name() == "selected" &&
        function->get_file_info() != nullptr &&
        function->get_file_info()->get_line() == 2) {
      ROSE_ASSERT(primaryTemplateDefinition == nullptr);
      primaryTemplateDefinition = function;
    } else if (function->get_name() ==
               "rex_frontend_function_production_contract") {
      ROSE_ASSERT(freeDefinition == nullptr);
      freeDefinition = function;
    }
  }
  requireExactSourceSurface(primaryTemplateDefinition, 2);
  requireExactSourceSurface(freeDefinition, 16, 19);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  checkExplicitInstantiationOwners(project);
  checkSpecializationIdentity(project);
  checkSyntheticFirstParameterOwnership(project);
  checkSourceMemberStructuralCtorListProvenance(project);
  checkDefiningFunctionSourceSurfaces(project);
  checkImplicitConstructorSemanticSubtree(project);
  unsigned writtenTemplateParameters = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_name() != "rex_source_parameter_template")
      continue;
    auto *syntax = function->get_parameterList_syntax();
    ROSE_ASSERT(syntax != nullptr && syntax->get_args().size() == 1);
    auto *written = isSgNonrealType(syntax->get_args().front()->get_type());
    ROSE_ASSERT(written != nullptr);
    auto *declaration = isSgNonrealDecl(written->get_declaration());
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_tpl_args().empty());
    ROSE_ASSERT(declaration->get_nonreal_template_role() ==
                SgNonrealDecl::e_nonreal_template_id);
    ++writtenTemplateParameters;
  }
  ROSE_ASSERT(writtenTemplateParameters == 1);
  unsigned cvParameterDefinitions = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if ((function->get_name() != "rex_cv_parameter" &&
         function->get_name() != "rex_cv_parameter_without_prototype") ||
        function->get_definition() == nullptr)
      continue;
    ROSE_ASSERT(function->get_args().size() == 1);
    auto *parameter = function->get_args().front();
    ROSE_ASSERT(SageInterface::isConstType(parameter->get_type()));
    ROSE_ASSERT(SageInterface::isVolatileType(parameter->get_type()));
    ROSE_ASSERT(function->get_type()->get_arguments().size() == 1);
    ROSE_ASSERT(isSgTypeInt(function->get_type()->get_arguments().front()));
    auto *canonical =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(canonical != nullptr &&
                canonical->get_type() == function->get_type());
    unsigned parameterReads = 0;
    for (SgNode *reference :
         NodeQuery::querySubTree(function->get_definition(), V_SgVarRefExp)) {
      auto *read = isSgVarRefExp(reference);
      if (read->get_symbol()->get_declaration() == parameter) {
        ROSE_ASSERT(read->get_type() == parameter->get_type());
        ++parameterReads;
      }
    }
    ROSE_ASSERT(parameterReads == 1);
    ++cvParameterDefinitions;
  }
  ROSE_ASSERT(cvParameterDefinitions == 2);
  unsigned decltypeParameterDefinitions = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if ((function->get_name() != "rex_decltype_parameter" &&
         function->get_name() != "rex_decltype_array_parameter" &&
         function->get_name() != "rex_decltype_function_parameter") ||
        function->get_definition() == nullptr)
      continue;
    ROSE_ASSERT(function->get_args().size() == 1);
    auto *sourceType = isSgDeclType(function->get_orig_return_type());
    ROSE_ASSERT(sourceType != nullptr);
    auto *reference = isSgVarRefExp(sourceType->get_base_expression());
    ROSE_ASSERT(reference != nullptr);
    ROSE_ASSERT(reference->get_symbol()->get_declaration() ==
                function->get_args().front());
    ROSE_ASSERT(reference->get_type() ==
                function->get_args().front()->get_type());
    if (function->get_name() == "rex_decltype_function_parameter") {
      auto *sourceParameters = function->get_parameterList_syntax();
      ROSE_ASSERT(sourceParameters != nullptr &&
                  sourceParameters->get_args().size() == 1);
      auto *sourceParameter = sourceParameters->get_args().front();
      auto *sourceType = sourceParameter->get_type();
      ROSE_ASSERT(sourceType->has_source_prototype() &&
                  sourceType->get_parent() == sourceParameter);
      const auto scopesBeforeClone = SgFunctionParameterScope::numberOfNodes();
      const auto namesBeforeClone = SgInitializedName::numberOfNodes();
      for (bool semantic : {false, true}) {
        auto *parameters =
            semantic ? SageBuilder::buildSemanticFunctionParameterList(
                           sourceParameters)
                     : SageBuilder::buildGeneratedFunctionParameterList(
                           sourceParameters);
        ROSE_ASSERT(parameters != nullptr &&
                    parameters->get_args().size() == 1);
        auto *parameter = parameters->get_args().front();
        ROSE_ASSERT(parameter != sourceParameter &&
                    parameter->get_type() != sourceType &&
                    parameter->get_type()->has_source_prototype() &&
                    parameter->get_type()->get_parent() == parameter);
        SageInterface::deleteAST(
            parameters, SageInterface::DeleteAstMode::kRequireIsolated);
        ROSE_ASSERT(sourceType->get_parent() == sourceParameter);
        ROSE_ASSERT(SgFunctionParameterScope::numberOfNodes() ==
                    scopesBeforeClone);
        ROSE_ASSERT(SgInitializedName::numberOfNodes() == namesBeforeClone);
      }
    }
    ++decltypeParameterDefinitions;
  }
  ROSE_ASSERT(decltypeParameterDefinitions == 3);
  unsigned prototypes = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTypedefDeclaration)) {
    auto *alias = isSgTypedefDeclaration(node);
    const bool dependentArgument =
        alias->get_name() == "rex_decltype_prototype_dependent_argument";
    const bool nestedArgument =
        alias->get_name() == "rex_decltype_prototype_nested_argument";
    if (alias->get_name() != "rex_decltype_prototype" && !dependentArgument &&
        !nestedArgument)
      continue;
    auto *function = isSgFunctionType(alias->get_base_type());
    ROSE_ASSERT(function != nullptr && function->get_source_trailing_return());
    auto *arguments = function->get_argument_list();
    arguments->validate_source_prototype();
    auto *parameters = arguments->get_source_parameters();
    auto *scope = arguments->get_source_parameter_scope();
    ROSE_ASSERT(parameters != nullptr &&
                parameters->get_args().size() ==
                    (dependentArgument || nestedArgument ? 2 : 1));
    ROSE_ASSERT(scope != nullptr && scope->get_scope() == alias->get_scope());
    ROSE_ASSERT(parameters->get_scope() == scope);
    auto *parameter = parameters->get_args().back();
    ROSE_ASSERT(parameter->get_name() ==
                (dependentArgument || nestedArgument ? "other" : "value"));
    auto *returnType = isSgDeclType(function->get_return_type());
    ROSE_ASSERT(returnType != nullptr);
    auto *reference = isSgVarRefExp(returnType->get_base_expression());
    ROSE_ASSERT(reference != nullptr &&
                reference->get_symbol()->get_declaration() == parameter);
    ROSE_ASSERT(reference->get_type() == parameter->get_type());
    const auto scopesBeforeCopy = SgFunctionParameterScope::numberOfNodes();
    const auto listsBeforeCopy = SgFunctionParameterTypeList::numberOfNodes();
    const auto namesBeforeCopy = SgInitializedName::numberOfNodes();
    const auto decltypesBeforeCopy = SgDeclType::numberOfNodes();
    const auto symbolsBeforeCopy = SgVariableSymbol::numberOfNodes();
    const auto functionsBeforeCopy = SgFunctionType::numberOfNodes();
    const auto pointersBeforeCopy = SgPointerType::numberOfNodes();
    SgTreeCopy copyHelp;
    copyHelp.incrementDepth();
    auto *copy = isSgTypedefDeclaration(alias->copy(copyHelp));
    copyHelp.decrementDepth();
    copyHelp.requireBoundCopiedReferences();
    ROSE_ASSERT(copy != nullptr && copy != alias);
    auto *copiedFunction = isSgFunctionType(copy->get_base_type());
    ROSE_ASSERT(copiedFunction != nullptr && copiedFunction != function &&
                copiedFunction->get_parent() == copy &&
                copiedFunction->get_source_trailing_return());
    auto *copiedArguments = copiedFunction->get_argument_list();
    copiedArguments->validate_source_prototype();
    ROSE_ASSERT(copiedArguments != arguments);
    ROSE_ASSERT(copiedArguments->get_source_parameter_scope() != scope);
    ROSE_ASSERT(copiedArguments->get_source_parameter_scope()->get_scope() ==
                copy->get_scope());
    ROSE_ASSERT(copiedArguments->get_source_parameters()->get_scope() ==
                copiedArguments->get_source_parameter_scope());
    auto *copiedParameter =
        copiedArguments->get_source_parameters()->get_args().back();
    ROSE_ASSERT(copiedParameter != parameter);
    auto *copiedReturn = isSgDeclType(copiedFunction->get_return_type());
    ROSE_ASSERT(copiedReturn != nullptr && copiedReturn != returnType);
    auto *copiedReference = isSgVarRefExp(copiedReturn->get_base_expression());
    ROSE_ASSERT(copiedReference != nullptr && copiedReference != reference);
    ROSE_ASSERT(copiedReference->get_symbol() != reference->get_symbol() &&
                copiedReference->get_symbol()->get_declaration() ==
                    copiedParameter);
    if (dependentArgument) {
      auto *argumentType =
          isSgDeclType(copiedArguments->get_arguments().back());
      ROSE_ASSERT(argumentType != nullptr);
      auto *argumentReference =
          isSgVarRefExp(argumentType->get_base_expression());
      ROSE_ASSERT(
          argumentReference != nullptr &&
          argumentReference->get_symbol()->get_declaration() ==
              copiedArguments->get_source_parameters()->get_args().front());
    }
    if (nestedArgument) {
      auto *pointer = isSgPointerType(arguments->get_arguments().front());
      auto *copiedPointer =
          isSgPointerType(copiedArguments->get_arguments().front());
      ROSE_ASSERT(pointer != nullptr && copiedPointer != nullptr &&
                  copiedPointer != pointer);
      auto *nestedFunction = isSgFunctionType(pointer->get_base_type());
      auto *copiedNested = isSgFunctionType(copiedPointer->get_base_type());
      ROSE_ASSERT(nestedFunction != nullptr && copiedNested != nullptr &&
                  copiedNested != nestedFunction);
      ROSE_ASSERT(pointer->get_parent() ==
                  arguments->get_argument_qualification_use_sites().front());
      ROSE_ASSERT(
          copiedPointer->get_parent() ==
          copiedArguments->get_argument_qualification_use_sites().front());
      ROSE_ASSERT(nestedFunction->get_parent() == pointer &&
                  copiedNested->get_parent() == copiedPointer);
      auto *nestedArguments = nestedFunction->get_argument_list();
      auto *copiedNestedArguments = copiedNested->get_argument_list();
      nestedArguments->validate_source_prototype();
      copiedNestedArguments->validate_source_prototype();
      ROSE_ASSERT(nestedArguments->get_source_parameter_scope()->get_scope() ==
                  scope);
      ROSE_ASSERT(
          copiedNestedArguments->get_source_parameter_scope()->get_scope() ==
          copiedArguments->get_source_parameter_scope());
      auto *nestedReturn = isSgDeclType(copiedNested->get_return_type());
      ROSE_ASSERT(nestedReturn != nullptr);
      auto *nestedReference =
          isSgVarRefExp(nestedReturn->get_base_expression());
      ROSE_ASSERT(nestedReference != nullptr &&
                  nestedReference->get_symbol()->get_declaration() ==
                      copiedNestedArguments->get_source_parameters()
                          ->get_args()
                          .front());
    }
    SageInterface::deleteAST(copy,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    SgTreeCopy projection;
    projection.insertCopiedNodePair(function, function);
    auto *projected = isSgFunctionType(
        SageInterface::materializeTypeWithCopyMap(function, projection));
    ROSE_ASSERT(projected != nullptr && projected != function);
    ROSE_ASSERT(projected->get_argument_list() != arguments);
    projected->get_argument_list()->validate_source_prototype();
    ROSE_ASSERT(projection.get_depth() == 0);
    SageInterface::deleteAST(projected,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    ROSE_ASSERT(SgFunctionParameterScope::numberOfNodes() == scopesBeforeCopy);
    ROSE_ASSERT(SgFunctionParameterTypeList::numberOfNodes() ==
                listsBeforeCopy);
    ROSE_ASSERT(SgInitializedName::numberOfNodes() == namesBeforeCopy);
    ROSE_ASSERT(SgDeclType::numberOfNodes() == decltypesBeforeCopy);
    ROSE_ASSERT(SgVariableSymbol::numberOfNodes() == symbolsBeforeCopy);
    ROSE_ASSERT(SgFunctionType::numberOfNodes() == functionsBeforeCopy);
    ROSE_ASSERT(SgPointerType::numberOfNodes() == pointersBeforeCopy);
    arguments->validate_source_prototype();
    ROSE_ASSERT(reference->get_symbol()->get_declaration() == parameter);
    ++prototypes;
  }
  ROSE_ASSERT(prototypes == 3);
  return 0;
}
