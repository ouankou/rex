#include "astJson/sageAstJson.h"
#include "rose.h"
#include "unparser.h"

#include "manglingSupport.h"

#include <cstdlib>
#include <memory>
#include <set>
#include <sstream>

namespace {
void checkCanonicalTemplateConsumers(SgNode *root) {
  std::set<std::string> scopes[2];
  unsigned definitions[2] = {};
  unsigned ordinary_members = 0, empty_packs = 0;
  bool function_roles_match = true;
  for (auto *node : NodeQuery::querySubTree(root, V_SgFunctionDefinition)) {
    auto *definition = isSgFunctionDefinition(node);
    const auto observe = [&](auto *instantiation, const char *name,
                             unsigned index) {
      if (instantiation == nullptr || instantiation->get_templateName() != name)
        return;
      ROSE_ASSERT(instantiation->get_templateArguments().empty());
      ROSE_ASSERT(instantiation->get_deducedTemplateArguments().size() == 1);
      function_roles_match &= instantiation->isTemplateFunction();
      scopes[index].insert(mangleQualifiersToString(definition));
      ++definitions[index];
    };
    observe(
        isSgTemplateInstantiationFunctionDecl(definition->get_declaration()),
        "rex_scope_identity", 0);
    observe(isSgTemplateInstantiationMemberFunctionDecl(
                definition->get_declaration()),
            "rex_member_scope_identity", 1);
    if (auto *member = isSgTemplateInstantiationMemberFunctionDecl(
            definition->get_declaration())) {
      if (member->get_templateName() == "read") {
        function_roles_match &= !member->isTemplateFunction();
        ++ordinary_members;
      }
      if (member->get_templateName() == "rex_empty_member_scope_identity") {
        ROSE_ASSERT(member->get_templateArguments().empty());
        ROSE_ASSERT(member->get_deducedTemplateArguments().empty());
        function_roles_match &= member->isTemplateFunction();
        ++empty_packs;
      }
    }
  }
  unsigned source_types = 0;
  bool source_types_match = true;
  bool class_arguments_match = true;
  for (auto *node : NodeQuery::querySubTree(root, V_SgTemplateArgument)) {
    auto *argument = isSgTemplateArgument(node);
    auto *source = isSgNonrealType(argument->get_sourceSpelledType());
    auto *declaration = source != nullptr
                            ? isSgNonrealDecl(source->get_declaration())
                            : nullptr;
    if (declaration == nullptr ||
        declaration->get_name() != "RexCanonicalSourceBox")
      continue;
    ROSE_ASSERT(declaration->get_tpl_args().size() == 1);
    auto *semantic = isSgClassType(argument->get_type());
    ROSE_ASSERT(semantic != nullptr);
    auto *instance = isSgTemplateInstantiationDecl(semantic->get_declaration());
    ROSE_ASSERT(instance != nullptr &&
                instance->get_templateArguments().empty());
    ROSE_ASSERT(instance->get_semanticTemplateArguments().size() == 2);
    auto *table = instance->get_scope()->get_symbol_table();
    auto *symbol = isSgClassSymbol(
        instance->get_scope()->find_symbol_from_declaration(instance));
    ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == instance);
    class_arguments_match &=
        table->hasMatchingTemplateArgumentsOrNoTemplateArguments(
            &instance->get_semanticTemplateArguments(), symbol);
    SgTemplateArgumentPtrList empty;
    class_arguments_match &=
        !table->hasMatchingTemplateArgumentsOrNoTemplateArguments(&empty,
                                                                  symbol);
    source_types_match &=
        SageInterface::cxxSourceTypeMatchesSemanticType(source, semantic);
    ++source_types;
  }
  std::cerr << "canonical template scopes: " << definitions[0] << '/'
            << scopes[0].size() << ' ' << definitions[1] << '/'
            << scopes[1].size() << "; source types: " << source_types << '/'
            << source_types_match << '\n';
  std::cerr << "canonical template roles: " << function_roles_match
            << "; ordinary members: " << ordinary_members
            << "; empty packs: " << empty_packs
            << "; class arguments: " << class_arguments_match << '\n';
  ROSE_ASSERT(definitions[0] == 2 && scopes[0].size() == 2);
  ROSE_ASSERT(definitions[1] == 2 && scopes[1].size() == 2);
  ROSE_ASSERT(source_types != 0 && source_types_match);
  ROSE_ASSERT(ordinary_members != 0 && empty_packs == 1);
  ROSE_ASSERT(function_roles_match && class_arguments_match);
}

void checkSubstitutedPackSpelling(SgNode *root) {
  unsigned checked = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateClassDeclaration)) {
    auto *declaration = isSgTemplateClassDeclaration(node);
    if (declaration->get_name() != "RexSpellingPack")
      continue;
    ROSE_ASSERT(declaration->get_templateParameters().size() == 1);
    auto *parameter = declaration->get_templateParameters().front();
    ROSE_ASSERT(parameter->get_is_parameter_pack());
    auto *type = isSgTemplateType(parameter->get_type());
    ROSE_ASSERT(type != nullptr && type->get_name() == "Types");
    SgTemplateType substitution(type->get_name());
    substitution.set_template_parameter(parameter);
    substitution.set_template_parameter_depth(
        type->get_template_parameter_depth());
    substitution.set_template_parameter_position(
        type->get_template_parameter_position());
    ROSE_ASSERT(type->get_canonical_source_identity().has_value());
    substitution.initialize_canonical_source_identity(
        *type->get_canonical_source_identity());
    SgTemplateType other(type->get_name());
    other.set_template_parameter(parameter);
    other.set_template_parameter_depth(type->get_template_parameter_depth());
    other.set_template_parameter_position(
        type->get_template_parameter_position());
    other.initialize_canonical_source_identity(
        *type->get_canonical_source_identity());
    auto checkEquivalence = [&](bool expected) {
      ROSE_ASSERT((substitution.get_mangled() == other.get_mangled()) ==
                  expected);
      ROSE_ASSERT(SageInterface::isEquivalentType(&substitution, &other) ==
                  expected);
      ROSE_ASSERT(SageInterface::isEquivalentType(&other, &substitution) ==
                  expected);
      ROSE_ASSERT(SageInterface::cxxSourceTypeMatchesSemanticType(
                      &substitution, &other) == expected);
      ROSE_ASSERT(SageInterface::cxxSourceTypeMatchesSemanticType(
                      &other, &substitution) == expected);
    };
    checkEquivalence(true);
    other.set_name("RenamedTypes");
    checkEquivalence(true);
    other.set_name(type->get_name());
    SgTemplateType unrelated(type->get_name());
    unrelated.set_template_parameter_depth(
        type->get_template_parameter_depth());
    unrelated.set_template_parameter_position(
        type->get_template_parameter_position());
    auto unrelatedIdentity = *type->get_canonical_source_identity();
    ++unrelatedIdentity.expansion_file_offset;
    unrelated.initialize_canonical_source_identity(unrelatedIdentity);
    ROSE_ASSERT(!SageInterface::isEquivalentType(&substitution, &unrelated));
    ROSE_ASSERT(!SageInterface::isEquivalentType(&unrelated, &substitution));
    other.set_packed(true);
    checkEquivalence(false);
    other.set_packed(false);
    checkEquivalence(true);
    auto render = [&]() {
      auto *source = SageInterface::getEnclosingSourceFile(declaration);
      ROSE_ASSERT(source != nullptr);
      std::ostringstream output;
      Unparser_Opt options;
      Unparser unparser(&output, source->getFileName(), options);
      unparser.currentFile = source;
      SgUnparse_Info info;
      info.set_current_source_file(source);
      info.set_language(SgFile::e_Cxx_language);
      unparser.u_type->unparseType(&substitution, info);
      return output.str();
    };
    const std::string spelling = render();
    ROSE_ASSERT(spelling.find("Types") == 0);
    substitution.set_is_pack_substitution(true);
    checkEquivalence(false);
    other.set_is_pack_substitution(true);
    checkEquivalence(true);
    auto integer = std::make_unique<SgTemplateArgument>(
        SageBuilder::buildIntType(), false);
    auto real = std::make_unique<SgTemplateArgument>(
        SageBuilder::buildDoubleType(), false);
    integer->set_parent(&substitution);
    real->set_parent(&substitution);
    substitution.get_tpl_args() = {integer.get(), real.get()};
    checkEquivalence(false);
    auto otherInteger = std::make_unique<SgTemplateArgument>(
        SageBuilder::buildIntType(), false);
    auto otherReal = std::make_unique<SgTemplateArgument>(
        SageBuilder::buildDoubleType(), false);
    otherInteger->set_parent(&other);
    otherReal->set_parent(&other);
    other.get_tpl_args() = {otherInteger.get(), otherReal.get()};
    checkEquivalence(true);
    other.get_tpl_args() = {otherReal.get(), otherInteger.get()};
    checkEquivalence(false);
    other.get_tpl_args() = {otherInteger.get()};
    checkEquivalence(false);
    other.get_tpl_args().clear();
    ROSE_ASSERT(render() == spelling);
    ROSE_ASSERT(substitution.get_tpl_args() ==
                SgTemplateArgumentPtrList({integer.get(), real.get()}));
    ROSE_ASSERT(integer->get_type() == SageBuilder::buildIntType());
    ROSE_ASSERT(real->get_type() == SageBuilder::buildDoubleType());
    SgTreeCopy copier;
    auto *copy = isSgTemplateType(substitution.copy(copier));
    ROSE_ASSERT(copy != nullptr && copy != &substitution);
    ROSE_ASSERT(copy->get_template_parameter() == parameter);
    ROSE_ASSERT(copy->get_is_pack_substitution());
    ROSE_ASSERT(copy->get_tpl_args().size() == 2);
    ROSE_ASSERT(copy->get_tpl_args()[0] != integer.get());
    ROSE_ASSERT(copy->get_tpl_args()[1] != real.get());
    ROSE_ASSERT(copy->get_tpl_args()[0]->get_parent() == copy);
    ROSE_ASSERT(copy->get_tpl_args()[1]->get_parent() == copy);
    ROSE_ASSERT(copy->get_tpl_args()[0]->get_type() == integer->get_type());
    ROSE_ASSERT(copy->get_tpl_args()[1]->get_type() == real->get_type());
    ROSE_ASSERT(copy->get_mangled() == substitution.get_mangled());
    delete copy;
    ROSE_ASSERT(integer->get_parent() == &substitution);
    ROSE_ASSERT(real->get_parent() == &substitution);
    substitution.get_tpl_args().clear();
    ++checked;
  }
  ROSE_ASSERT(checked != 0);
}

void checkTemplateTemplateIdentity(SgNode *root) {
  unsigned named = 0, unnamed = 0;
  unsigned nested = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateClassDeclaration)) {
    auto *declaration = isSgTemplateClassDeclaration(node);
    const bool named_case = declaration->get_name() == "RexTemplateSpellingUse";
    if (!named_case && declaration->get_name() != "RexAnonymousTemplateSlots")
      continue;
    const auto &parameters = declaration->get_templateParameters();
    ROSE_ASSERT(parameters.size() == (named_case ? 1 : 2));
    if (named_case && declaration->get_definition() != nullptr) {
      for (SgNode *node : NodeQuery::querySubTree(declaration->get_definition(),
                                                  V_SgTypedefDeclaration)) {
        auto *alias = isSgTypedefDeclaration(node);
        if (alias->get_name() != "nested_type")
          continue;
        auto *outer = isSgNonrealType(alias->get_base_type());
        ROSE_ASSERT(outer != nullptr);
        auto *outerDeclaration = isSgNonrealDecl(outer->get_declaration());
        ROSE_ASSERT(outerDeclaration != nullptr &&
                    outerDeclaration->get_tpl_args().size() == 2);
        auto *argument =
            isSgNonrealType(outerDeclaration->get_tpl_args()[0]->get_type());
        ROSE_ASSERT(argument != nullptr);
        auto *argumentDeclaration =
            isSgNonrealDecl(argument->get_declaration());
        ROSE_ASSERT(argumentDeclaration != nullptr &&
                    argumentDeclaration->get_templateDeclaration() ==
                        parameters.front()->get_templateDeclaration());
        ++nested;
      }
    }
    for (auto *parameter : parameters) {
      ROSE_ASSERT(parameter->get_parent() == declaration);
      ROSE_ASSERT(parameter->get_parameterType() ==
                  SgTemplateParameter::template_parameter);
      auto *type = isSgTemplateType(parameter->get_type());
      auto *identity =
          isSgTemplateDeclaration(parameter->get_templateDeclaration());
      ROSE_ASSERT(type != nullptr &&
                  type->get_template_parameter() == parameter);
      ROSE_ASSERT(identity != nullptr);
      auto *scope = SageBuilder::getNonrealDeclarationScope(declaration);
      ROSE_ASSERT(scope != nullptr && scope->get_parent() == declaration);
      ROSE_ASSERT(identity->get_scope() == scope &&
                  identity->get_parent() == scope);
      auto *symbol =
          isSgTemplateSymbol(identity->get_symbol_from_symbol_table());
      ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == identity);
      ROSE_ASSERT(symbol->get_parent() == scope->get_symbol_table());
      if (named_case) {
        ROSE_ASSERT(type->get_name() ==
                    "__unnamed_template_template_parameter_real");
        ROSE_ASSERT(identity->get_name() == type->get_name());
        ++named;
      } else {
        ROSE_ASSERT(type->get_name().is_null());
        ++unnamed;
      }
    }
  }
  ROSE_ASSERT(named != 0 && unnamed >= 2 && nested != 0);
}

void checkConversionTypes(SgNode *root) {
  checkSubstitutedPackSpelling(root);
  checkTemplateTemplateIdentity(root);
  unsigned aliases = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateTypedefDeclaration)) {
    auto *declaration = isSgTemplateTypedefDeclaration(node);
    if (declaration->get_name() != "RexSpellingAlias")
      continue;
    auto *table = declaration->get_scope()->get_symbol_table();
    auto *symbol = table->find_template_typedef(declaration->get_name());
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(table->hasMatchingTemplateParametersOrNoTemplateParameters(
        &declaration->get_templateParameters(), symbol));
    SgTemplateParameterPtrList empty;
    ROSE_ASSERT(!table->hasMatchingTemplateParametersOrNoTemplateParameters(
        &empty, symbol));
    ++aliases;
  }
  ROSE_ASSERT(aliases == 1);
  unsigned instantiatedAliases = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           root, V_SgTemplateInstantiationTypedefDeclaration)) {
    auto *instance = isSgTemplateInstantiationTypedefDeclaration(node);
    auto *declaration = instance->get_templateDeclaration();
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != "RexSpellingAlias")
      continue;
    SgTemplateTypedefSymbol symbol(instance);
    auto *table = instance->get_scope()->get_symbol_table();
    ROSE_ASSERT(table->hasMatchingTemplateParametersOrNoTemplateParameters(
        &declaration->get_templateParameters(), &symbol));
    ROSE_ASSERT(table->hasMatchingTemplateArgumentsOrNoTemplateArguments(
        &instance->get_templateArguments(), &symbol));
    SgTemplateArgumentPtrList empty;
    ROSE_ASSERT(!table->hasMatchingTemplateArgumentsOrNoTemplateArguments(
        &empty, &symbol));
    ++instantiatedAliases;
  }
  ROSE_ASSERT(instantiatedAliases != 0);

  unsigned dependent_conversions = 0;
  unsigned templated_conversions = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (!function->get_specialFunctionModifier().isConversion()) {
      continue;
    }
    SgType *target = function->get_type()->get_return_type();
    ROSE_ASSERT(target != nullptr);
    if (auto *parameter_type = isSgTemplateType(target)) {
      ROSE_ASSERT(parameter_type->get_template_parameter() != nullptr);
      ROSE_ASSERT(parameter_type->get_template_parameter_depth() == 0);
      if (parameter_type->get_name() == "Target") {
        ROSE_ASSERT(parameter_type->get_template_parameter_position() == 0);
        ++templated_conversions;
      } else {
        ROSE_ASSERT(parameter_type->get_template_parameter_position() == 1);
        ROSE_ASSERT(parameter_type->get_name() == "type_parameter_0_0");
        ++dependent_conversions;
      }
    }
  }
  ROSE_ASSERT(dependent_conversions >= 2);
  ROSE_ASSERT(templated_conversions != 0);
  unsigned conversion_references = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgNonrealRefExp)) {
    auto *reference = isSgNonrealRefExp(node);
    if (reference->get_name_kind() !=
        SgNonrealRefExp::e_conversion_function_id) {
      continue;
    }
    SgTypeExpression *type_id = reference->get_conversion_type_id();
    ROSE_ASSERT(type_id != nullptr && type_id->get_parent() == reference);
    auto *target = isSgTemplateType(type_id->get_represented_type());
    ROSE_ASSERT(target != nullptr &&
                target->get_template_parameter() != nullptr);
    if (target->get_name() == "Target") {
      ROSE_ASSERT(target->get_template_parameter_position() == 0);
    } else {
      ROSE_ASSERT(target->get_template_parameter_position() == 1);
      ROSE_ASSERT(target->get_name() == "type_parameter_0_0");
      ++conversion_references;
    }
    auto *copy = isSgNonrealRefExp(SageInterface::copyExpression(reference));
    ROSE_ASSERT(copy != nullptr &&
                copy->get_name_kind() == reference->get_name_kind());
    ROSE_ASSERT(copy->get_conversion_type_id() != type_id);
    ROSE_ASSERT(copy->get_conversion_type_id()->get_parent() == copy);
    ROSE_ASSERT(copy->get_conversion_type_id()->get_represented_type() ==
                target);
    SageInterface::deleteAST(copy);
  }
  ROSE_ASSERT(conversion_references == 2);
  unsigned member_conversions = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgMemberFunctionRefExp)) {
    auto *reference = isSgMemberFunctionRefExp(node);
    if (auto *operand = reference->get_conversion_type_id()) {
      ROSE_ASSERT(operand->get_parent() == reference);
      ROSE_ASSERT(operand->get_represented_type() != nullptr);
      ++member_conversions;
    }
  }
  ROSE_ASSERT(member_conversions != 0);
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateMemberFunctionRefExp)) {
    auto *reference = isSgTemplateMemberFunctionRefExp(node);
    if (auto *operand = reference->get_conversion_type_id()) {
      ROSE_ASSERT(operand->get_parent() == reference);
      ROSE_ASSERT(operand->get_represented_type() != nullptr);
    }
  }

  unsigned named_parameter_uses = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    auto *variable = isSgInitializedName(node);
    if (variable->get_name() != "rex_named_value") {
      continue;
    }
    if (auto *type = isSgTemplateType(variable->get_type())) {
      ROSE_ASSERT(type->get_name() == "type_parameter_0_0");
      ROSE_ASSERT(type->get_template_parameter() != nullptr);
      ROSE_ASSERT(type->get_template_parameter_depth() == 0);
      ROSE_ASSERT(type->get_template_parameter_position() == 0);
      ++named_parameter_uses;
    }
  }
  ROSE_ASSERT(named_parameter_uses > 0);

  unsigned nested_parameters = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    auto *variable = isSgInitializedName(node);
    const bool outer = variable->get_name() == "rex_outer_value";
    if (!outer && variable->get_name() != "rex_inner_value") {
      continue;
    }
    SgType *type = variable->get_type();
    if (!outer) {
      auto *pointer = isSgPointerType(type);
      ROSE_ASSERT(pointer != nullptr);
      type = pointer->get_base_type();
    }
    if (auto *parameterType = isSgTemplateType(type)) {
      ROSE_ASSERT(parameterType->get_template_parameter() != nullptr);
      ROSE_ASSERT(parameterType->get_name() == (outer ? "Outer" : "Inner"));
      ROSE_ASSERT(parameterType->get_template_parameter_position() == 0);
      ROSE_ASSERT(parameterType->get_template_parameter_depth() ==
                  (outer ? 0 : 1));
      ++nested_parameters;
    }
  }
  ROSE_ASSERT(nested_parameters >= 2);

  unsigned static_member_headers = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    auto *declaration = isSgVariableDeclaration(node);
    if (declaration->get_variables().size() != 1 ||
        declaration->get_variables().front()->get_name() != "maximum") {
      continue;
    }
    for (auto *header : declaration->get_sourceSpelledTemplateHeaders()) {
      ROSE_ASSERT(header != nullptr && header->get_parent() == declaration);
      ROSE_ASSERT(header->get_args().size() == 1);
      auto *parameter = header->get_args().front();
      ROSE_ASSERT(parameter != nullptr && parameter->get_parent() == header);
      auto *type = isSgTemplateType(parameter->get_type());
      ROSE_ASSERT(type != nullptr && type->get_name() == "Storage");
      ROSE_ASSERT(type->get_template_parameter() == parameter);
      ROSE_ASSERT(type->get_template_parameter_depth() == 0);
      ROSE_ASSERT(type->get_template_parameter_position() == 0);
      ++static_member_headers;
    }
  }
  ROSE_ASSERT(static_member_headers > 0);

  unsigned partial_member_headers = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    auto *declaration = isSgVariableDeclaration(node);
    if (declaration->get_variables().size() != 1 ||
        declaration->get_variables().front()->get_name() != "rex_dimensions") {
      continue;
    }
    ROSE_ASSERT(declaration->get_is_constexpr());
    for (auto *header : declaration->get_sourceSpelledTemplateHeaders()) {
      ROSE_ASSERT(header != nullptr && header->get_parent() == declaration);
      ROSE_ASSERT(header->get_args().size() == 2);
      auto *dimensions = header->get_args()[0];
      auto *offset = header->get_args()[1];
      ROSE_ASSERT(dimensions != nullptr && dimensions->get_parent() == header);
      ROSE_ASSERT(dimensions->get_parameterType() ==
                  SgTemplateParameter::nontype_parameter);
      ROSE_ASSERT(dimensions->get_is_parameter_pack());
      auto *name = dimensions->get_initializedName();
      ROSE_ASSERT(name != nullptr && name->get_parent() == dimensions);
      ROSE_ASSERT(name->get_name() == "Dimensions");
      ROSE_ASSERT(name->get_scope() != nullptr);
      ROSE_ASSERT(name->get_is_parameter_pack());
      ROSE_ASSERT(!name->get_is_pack_element());
      ROSE_ASSERT(offset != nullptr && offset->get_parent() == header);
      ROSE_ASSERT(offset->get_parameterType() ==
                  SgTemplateParameter::type_parameter);
      auto *type = isSgTemplateType(offset->get_type());
      ROSE_ASSERT(type != nullptr && type->get_name() == "Offset");
      ROSE_ASSERT(type->get_template_parameter() == offset);
      ROSE_ASSERT(type->get_template_parameter_depth() == 0);
      ROSE_ASSERT(type->get_template_parameter_position() == 1);
      ++partial_member_headers;
    }
  }
  ROSE_ASSERT(partial_member_headers > 0);
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  VariantVector parameters(V_SgTemplateParameter);
  for (SgNode *node : NodeQuery::queryMemoryPool(parameters)) {
    auto *parameter = isSgTemplateParameter(node);
    if (parameter->get_parameterType() !=
        SgTemplateParameter::nontype_parameter)
      ROSE_ASSERT(parameter->get_parent() != nullptr);
  }
  checkConversionTypes(project);
  checkCanonicalTemplateConsumers(project);
  struct ParameterCopy final : SgTreeCopy {
    size_t checked = 0;
    size_t dependent_non_type = 0;
    size_t dependent_defaults = 0;
    SgNode *copyAst(const SgNode *node) override {
      const auto *owner = isSgInitializedName(node) != nullptr
                              ? isSgTemplateParameter(node->get_parent())
                              : nullptr;
      if (owner != nullptr && owner->get_parameterType() ==
                                  SgTemplateParameter::nontype_parameter) {
        const auto mappedOwner = get_copiedNodeMap().find(owner);
        ROSE_ASSERT(mappedOwner != get_copiedNodeMap().end());
        auto *copy = isSgTemplateParameter(mappedOwner->second);
        ROSE_ASSERT(copy != nullptr && copy != owner);
        auto *sourceType = owner->get_type();
        auto *parameterType = isSgTemplateType(sourceType);
        auto *pointer = isSgPointerType(sourceType);
        if (pointer != nullptr)
          parameterType = isSgTemplateType(pointer->get_base_type());
        if (parameterType != nullptr) {
          const auto mappedParameter =
              get_copiedNodeMap().find(parameterType->get_template_parameter());
          ROSE_ASSERT(mappedParameter != get_copiedNodeMap().end());
          auto *type = copy->get_type();
          ROSE_ASSERT(type != sourceType);
          if (pointer != nullptr) {
            auto *copiedPointer = isSgPointerType(type);
            ROSE_ASSERT(copiedPointer != nullptr && copiedPointer != pointer);
            type = copiedPointer->get_base_type();
          }
          auto *copiedType = isSgTemplateType(type);
          ROSE_ASSERT(copiedType != nullptr && copiedType != parameterType);
          ROSE_ASSERT(copiedType->get_template_parameter() ==
                      mappedParameter->second);
          ++dependent_non_type;
        }
      }
      SgNode *result = SgTreeCopy::copyAst(node);
      const auto *parameter = isSgTemplateParameter(node);
      if (parameter != nullptr && parameter->get_parameterType() !=
                                      SgTemplateParameter::nontype_parameter) {
        auto *copy = isSgTemplateParameter(result);
        const auto mapped = get_copiedNodeMap().find(parameter->get_type());
        ROSE_ASSERT(copy != nullptr && mapped != get_copiedNodeMap().end());
        auto *type = isSgTemplateType(mapped->second);
        ROSE_ASSERT(type != nullptr && type != parameter->get_type());
        ROSE_ASSERT(copy->get_type() == type &&
                    type->get_template_parameter() == copy);
        auto *sourceDefault =
            isSgPointerType(parameter->get_defaultTypeParameter());
        auto *defaultParameter =
            sourceDefault != nullptr
                ? isSgTemplateType(sourceDefault->get_base_type())
                : nullptr;
        if (defaultParameter != nullptr) {
          auto *copiedDefault =
              isSgPointerType(copy->get_defaultTypeParameter());
          ROSE_ASSERT(copiedDefault != nullptr &&
                      copiedDefault != sourceDefault);
          auto *copiedBase = isSgTemplateType(copiedDefault->get_base_type());
          ROSE_ASSERT(copiedBase != nullptr && copiedBase != defaultParameter);
          ROSE_ASSERT(copiedBase->get_template_parameter() ==
                      get_copiedNodeMap().at(
                          defaultParameter->get_template_parameter()));
          ++dependent_defaults;
        }
        ++checked;
      }
      return result;
    }
  } copier;
  auto *copied = isSgProject(project->copy(copier));
  ROSE_ASSERT(copied != nullptr && copied != project && copier.checked > 0 &&
              copier.dependent_non_type >= 2 && copier.dependent_defaults > 0);
  checkConversionTypes(copied);
  auto *copied_files = copied->get_fileList_ptr();
  SageInterface::deleteAST(copied,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  checkTemplateTemplateIdentity(project);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_template_parameter_spelling_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  checkConversionTypes(restored);
  project->skipfinalCompileStep(true);
  const int status = backend(project);
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
  return status;
}
