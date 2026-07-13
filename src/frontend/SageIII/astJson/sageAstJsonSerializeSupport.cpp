#include "openMPConstantInteger.h"
#include "sageAstJsonPrivate.h"

#include <atomic>

namespace Rose {
namespace AstJson {

uint64_t idFor(const std::unordered_map<const SgNode *, uint64_t> &ids,
               const SgNode *node);

bool expressionCarriesSemanticType(const SgExpression *expression) {
  return expression != nullptr && expression->has_semantic_value_type();
}

void validateExactSemanticExpressionType(const SgExpression *expression,
                                         const SgType *type,
                                         const std::string &phase) {
  if (expression == nullptr) {
    throw std::runtime_error(
        "AST JSON semantic expression type validation received a null "
        "expression");
  }
  if (!expressionCarriesSemanticType(expression)) {
    return;
  }
  if (type == nullptr || isSgTypeUnknown(type) != nullptr ||
      isSgTypeDefault(type) != nullptr) {
    throw std::runtime_error("AST JSON " + expression->class_name() +
                             " has no exact semantic value type during " +
                             phase);
  }
}

const JsonValue *validatedExpressionTypeProperty(const SgExpression *expression,
                                                 const JsonValue &properties) {
  if (expression == nullptr) {
    throw std::runtime_error(
        "AST JSON expression type validation received a null expression");
  }

  const JsonValue *type = properties.find("type");
  if (expressionCarriesSemanticType(expression)) {
    if (type == nullptr) {
      throw std::runtime_error(
          "AST JSON semantic expression is missing its semantic type");
    }
  } else if (type != nullptr) {
    throw std::runtime_error(
        "AST JSON syntax expression has a serialized semantic type");
  }
  return type;
}

namespace {

void validateOmpContextScore(SgExpression *score) {
  if (!expressionCarriesSemanticType(score)) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector score is not a semantic "
        "expression");
  }
  SgType *type = score->get_type();
  if (type == nullptr ||
      (!SageInterface::isStrictIntegerType(type) &&
       isSgEnumType(type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                    SgType::STRIP_TYPEDEF_TYPE)) == nullptr)) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector score is not an integer or enum "
        "expression");
  }
  if (!Rose::OpenMP::isNonnegativeConstantInteger(score)) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector score is not a nonnegative "
        "constant integer expression");
  }
}

std::string ompContextPropertyName(const SgExpression *expression) {
  if (const SgOmpNameExpression *name = isSgOmpNameExpression(expression)) {
    const std::string spelling = trim(name->get_spelling());
    if (spelling.empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector has an empty name property");
    }
    return spelling;
  }
  if (const SgOmpSourceExpression *source =
          isSgOmpSourceExpression(expression)) {
    const std::string spelling = trim(source->get_spelling());
    const bool double_quoted = spelling.size() >= 2 &&
                               spelling.front() == '"' &&
                               spelling.back() == '"';
    const bool single_quoted = spelling.size() >= 2 &&
                               spelling.front() == '\'' &&
                               spelling.back() == '\'';
    if (!double_quoted && !single_quoted) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector string property is not exactly "
          "quoted");
    }
    const std::string value = spelling.substr(1, spelling.size() - 2);
    if (value.empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector has an empty string property");
    }
    return value;
  }
  throw std::runtime_error(
      "AST JSON OpenMP name-list selector property is not an exact name or "
      "quoted syntax node");
}

std::string ompContextSyntaxProperty(const SgExpression *expression) {
  if (const SgOmpSourceExpression *source =
          isSgOmpSourceExpression(expression)) {
    const std::string spelling = trim(source->get_spelling());
    if (spelling.empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector has an empty syntax property");
    }
    return spelling;
  }
  return ompContextPropertyName(expression);
}

std::string
ompContextPropertyIdentity(const SgOmpContextSelector *selector,
                           const SgOmpContextSelectorProperty *property) {
  switch (selector->get_selector_kind()) {
  case SgOmpClause::e_omp_context_trait_kind:
    return "kind:" + std::to_string(property->get_context_kind());
  case SgOmpClause::e_omp_context_trait_vendor:
    return "vendor:" + std::to_string(property->get_context_vendor());
  case SgOmpClause::e_omp_context_trait_atomic_default_mem_order:
    return "atomic-default-mem-order:" +
           std::to_string(property->get_atomic_default_mem_order());
  case SgOmpClause::e_omp_context_trait_arch:
  case SgOmpClause::e_omp_context_trait_isa:
  case SgOmpClause::e_omp_context_trait_uid:
  case SgOmpClause::e_omp_context_trait_extension:
    // OpenMP treats an identifier and the corresponding string literal as
    // the same name-list property value.
    return "name:" + ompContextPropertyName(property->get_expression());
  case SgOmpClause::e_omp_context_trait_requires: {
    const auto kind = property->get_requires_kind();
    switch (kind) {
    case SgOmpClause::e_omp_requires_property_reverse_offload:
    case SgOmpClause::e_omp_requires_property_unified_address:
    case SgOmpClause::e_omp_requires_property_unified_shared_memory:
    case SgOmpClause::e_omp_requires_property_dynamic_allocators:
    case SgOmpClause::e_omp_requires_property_self_maps:
    case SgOmpClause::e_omp_requires_property_device_safesync:
    case SgOmpClause::e_omp_requires_property_atomic_default_mem_order:
      return "requires:" + std::to_string(kind);
    case SgOmpClause::e_omp_requires_property_implementation_defined:
      return "requires:" + std::to_string(kind) + ":" +
             property->get_requires_extension().getString();
    case SgOmpClause::e_omp_requires_property_unspecified:
    default:
      throw std::runtime_error(
          "AST JSON OpenMP requires property has an invalid typed kind");
    }
  }
  case SgOmpClause::e_omp_context_trait_implementation_user:
    return "syntax:" + ompContextSyntaxProperty(property->get_expression());
  case SgOmpClause::e_omp_context_trait_condition:
  case SgOmpClause::e_omp_context_trait_device_num:
    // These selectors require exactly one property, so cardinality is their
    // stronger and language-independent uniqueness check.
    return "single";
  case SgOmpClause::e_omp_context_trait_construct:
  default:
    throw std::runtime_error(
        "AST JSON OpenMP context property has no valid selector identity");
  }
}

} // namespace

void validateOmpContextSelectorProperty(
    const SgOmpContextSelectorProperty *property,
    const SgOmpContextSelector *selector) {
  if (property == nullptr || selector == nullptr) {
    throw std::runtime_error(
        "AST JSON has a null OpenMP context selector property or owner");
  }
  if (property->get_parent() != selector) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector property has a different owner");
  }

  const bool has_expression = property->get_expression() != nullptr;
  const bool has_context_kind = property->get_context_kind() !=
                                SgOmpClause::e_omp_when_context_kind_unknown;
  const bool has_context_vendor =
      property->get_context_vendor() !=
      SgOmpClause::e_omp_when_context_vendor_unspecified;
  const bool has_atomic_default_mem_order =
      property->get_atomic_default_mem_order() !=
      SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified;
  const bool has_requires = property->get_requires_kind() !=
                            SgOmpClause::e_omp_requires_property_unspecified;
  const unsigned payload_count =
      static_cast<unsigned>(has_expression) +
      static_cast<unsigned>(has_context_kind) +
      static_cast<unsigned>(has_context_vendor) +
      static_cast<unsigned>(has_atomic_default_mem_order) +
      static_cast<unsigned>(has_requires);
  if (payload_count != 1) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector property does not own exactly one "
        "typed payload");
  }
  if (has_expression && property->get_expression()->get_parent() != property) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector property expression has a "
        "different owner");
  }
  if (property->get_requires_expression() != nullptr &&
      property->get_requires_expression()->get_parent() != property) {
    throw std::runtime_error(
        "AST JSON OpenMP requires logical expression has a different owner");
  }

  bool expression_required = false;
  bool context_kind_required = false;
  bool context_vendor_required = false;
  bool atomic_default_mem_order_required = false;
  bool requires_required = false;
  switch (selector->get_selector_kind()) {
  case SgOmpClause::e_omp_context_trait_condition:
  case SgOmpClause::e_omp_context_trait_arch:
  case SgOmpClause::e_omp_context_trait_isa:
  case SgOmpClause::e_omp_context_trait_device_num:
  case SgOmpClause::e_omp_context_trait_uid:
  case SgOmpClause::e_omp_context_trait_extension:
  case SgOmpClause::e_omp_context_trait_implementation_user:
    expression_required = true;
    break;
  case SgOmpClause::e_omp_context_trait_requires:
    requires_required = true;
    break;
  case SgOmpClause::e_omp_context_trait_kind:
    context_kind_required = true;
    break;
  case SgOmpClause::e_omp_context_trait_vendor:
    context_vendor_required = true;
    break;
  case SgOmpClause::e_omp_context_trait_atomic_default_mem_order:
    atomic_default_mem_order_required = true;
    break;
  case SgOmpClause::e_omp_context_trait_construct:
  default:
    throw std::runtime_error(
        "AST JSON OpenMP context selector property is illegal for its "
        "selector kind");
  }
  if (has_expression != expression_required ||
      has_context_kind != context_kind_required ||
      has_context_vendor != context_vendor_required ||
      has_atomic_default_mem_order != atomic_default_mem_order_required ||
      has_requires != requires_required) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector property has the wrong typed "
        "payload for its selector");
  }

  if (!has_requires) {
    if (property->get_requires_expression() != nullptr ||
        property->get_requires_atomic_default_mem_order() !=
            SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
        !property->get_requires_extension().getString().empty()) {
      throw std::runtime_error(
          "AST JSON OpenMP non-requires property owns a requires payload");
    }
  } else {
    SgExpression *requires_expression = property->get_requires_expression();
    const auto requires_atomic =
        property->get_requires_atomic_default_mem_order();
    const bool has_requires_extension =
        !property->get_requires_extension().getString().empty();
    switch (property->get_requires_kind()) {
    case SgOmpClause::e_omp_requires_property_reverse_offload:
    case SgOmpClause::e_omp_requires_property_unified_address:
    case SgOmpClause::e_omp_requires_property_unified_shared_memory:
    case SgOmpClause::e_omp_requires_property_dynamic_allocators:
    case SgOmpClause::e_omp_requires_property_self_maps:
    case SgOmpClause::e_omp_requires_property_device_safesync:
      if (requires_atomic !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
          has_requires_extension) {
        throw std::runtime_error(
            "AST JSON OpenMP ordinary requires property owns a mismatched "
            "payload");
      }
      if (requires_expression != nullptr &&
          !expressionCarriesSemanticType(requires_expression)) {
        throw std::runtime_error(
            "AST JSON OpenMP requires logical expression is not a semantic "
            "expression");
      }
      break;
    case SgOmpClause::e_omp_requires_property_atomic_default_mem_order:
      if (requires_expression != nullptr || has_requires_extension) {
        throw std::runtime_error(
            "AST JSON OpenMP atomic_default_mem_order requires property "
            "owns a mismatched payload");
      }
      switch (requires_atomic) {
      case SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst:
      case SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel:
      case SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire:
      case SgOmpClause::e_omp_atomic_default_mem_order_kind_release:
      case SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed:
        break;
      case SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified:
      default:
        throw std::runtime_error(
            "AST JSON OpenMP atomic_default_mem_order requires property has "
            "an invalid order");
      }
      break;
    case SgOmpClause::e_omp_requires_property_implementation_defined:
      if (requires_expression != nullptr ||
          requires_atomic !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
          !has_requires_extension) {
        throw std::runtime_error(
            "AST JSON OpenMP implementation-defined requires property is "
            "malformed");
      }
      break;
    case SgOmpClause::e_omp_requires_property_unspecified:
    default:
      throw std::runtime_error(
          "AST JSON OpenMP requires property has an invalid typed kind");
    }
  }

  // This also validates the exact syntax-node kind for name-list and syntax
  // properties without ever asking those nodes for a fabricated value type.
  if ((selector->get_selector_kind() ==
           SgOmpClause::e_omp_context_trait_condition ||
       selector->get_selector_kind() ==
           SgOmpClause::e_omp_context_trait_device_num) &&
      !expressionCarriesSemanticType(property->get_expression())) {
    throw std::runtime_error(
        "AST JSON OpenMP semantic context property is not a semantic "
        "expression");
  }
  (void)ompContextPropertyIdentity(selector, property);
}

void validateOmpContextSelector(const SgOmpContextSelector *selector) {
  if (selector == nullptr) {
    throw std::runtime_error("AST JSON has a null OpenMP context selector");
  }

  if (selector->get_score() != nullptr &&
      selector->get_score()->get_parent() != selector) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector score has a different owner");
  }
  if (selector->get_construct_directive() != nullptr &&
      selector->get_construct_directive()->get_parent() != selector) {
    throw std::runtime_error(
        "AST JSON OpenMP construct selector directive has a different owner");
  }

  const bool custom = selector->get_selector_kind() ==
                      SgOmpClause::e_omp_context_trait_implementation_user;
  const bool has_custom_name =
      !selector->get_implementation_defined_name().is_null() &&
      !selector->get_implementation_defined_name().getString().empty();
  if (has_custom_name != custom) {
    throw std::runtime_error(
        custom ? "AST JSON OpenMP implementation-defined selector has no "
                 "exact name"
               : "AST JSON OpenMP built-in selector has an unexpected "
                 "implementation-defined name");
  }

  bool score_allowed = false;
  bool construct_required = false;
  size_t minimum_properties = 1;
  size_t maximum_properties = std::numeric_limits<size_t>::max();
  switch (selector->get_selector_kind()) {
  case SgOmpClause::e_omp_context_trait_condition:
    score_allowed = true;
    maximum_properties = 1;
    break;
  case SgOmpClause::e_omp_context_trait_construct:
    construct_required = true;
    minimum_properties = 0;
    maximum_properties = 0;
    break;
  case SgOmpClause::e_omp_context_trait_kind:
  case SgOmpClause::e_omp_context_trait_arch:
  case SgOmpClause::e_omp_context_trait_isa:
    break;
  case SgOmpClause::e_omp_context_trait_device_num:
  case SgOmpClause::e_omp_context_trait_uid:
    maximum_properties = 1;
    break;
  case SgOmpClause::e_omp_context_trait_vendor:
  case SgOmpClause::e_omp_context_trait_extension:
  case SgOmpClause::e_omp_context_trait_requires:
    score_allowed = true;
    break;
  case SgOmpClause::e_omp_context_trait_atomic_default_mem_order:
    score_allowed = true;
    maximum_properties = 1;
    break;
  case SgOmpClause::e_omp_context_trait_implementation_user:
    score_allowed = true;
    minimum_properties = 0;
    break;
  default:
    throw std::runtime_error(
        "AST JSON OpenMP context selector has an invalid selector kind");
  }

  if (selector->get_score() != nullptr && !score_allowed) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector has a prohibited score");
  }
  if (selector->get_score() != nullptr) {
    validateOmpContextScore(selector->get_score());
  }
  const bool has_construct = selector->get_construct_directive() != nullptr;
  if (has_construct != construct_required) {
    throw std::runtime_error(
        construct_required
            ? "AST JSON OpenMP construct selector is missing its directive"
            : "AST JSON OpenMP context selector has an unexpected directive");
  }
  const size_t property_count = selector->get_properties().size();
  if (property_count < minimum_properties ||
      property_count > maximum_properties ||
      (selector->get_score() != nullptr && property_count == 0)) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector has invalid property cardinality");
  }

  std::unordered_set<std::string> property_identities;
  for (const SgOmpContextSelectorProperty *property :
       selector->get_properties()) {
    validateOmpContextSelectorProperty(property, selector);
    if (!property_identities
             .insert(ompContextPropertyIdentity(selector, property))
             .second) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector has a duplicate property");
    }
  }
}

void validateOmpContextSelectorSet(const SgOmpContextSelectorSet *set) {
  if (set == nullptr) {
    throw std::runtime_error("AST JSON has a null OpenMP context selector set");
  }
  if (set->get_selectors().empty()) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector set has no selectors");
  }

  std::unordered_set<int> singleton_selector_kinds;
  std::unordered_set<int> construct_selector_kinds;
  std::unordered_set<std::string> implementation_selector_names;
  bool has_kind_any = false;
  for (const SgOmpContextSelector *selector : set->get_selectors()) {
    if (selector == nullptr || selector->get_parent() != set) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector has a different set owner");
    }

    bool selector_allowed = false;
    switch (set->get_set_kind()) {
    case SgOmpClause::e_omp_context_selector_set_user:
      selector_allowed = selector->get_selector_kind() ==
                         SgOmpClause::e_omp_context_trait_condition;
      break;
    case SgOmpClause::e_omp_context_selector_set_construct:
      selector_allowed = selector->get_selector_kind() ==
                         SgOmpClause::e_omp_context_trait_construct;
      break;
    case SgOmpClause::e_omp_context_selector_set_device:
      selector_allowed =
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_kind ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_arch ||
          selector->get_selector_kind() == SgOmpClause::e_omp_context_trait_isa;
      break;
    case SgOmpClause::e_omp_context_selector_set_target_device:
      selector_allowed =
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_kind ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_arch ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_isa ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_device_num ||
          selector->get_selector_kind() == SgOmpClause::e_omp_context_trait_uid;
      break;
    case SgOmpClause::e_omp_context_selector_set_implementation:
      selector_allowed =
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_vendor ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_extension ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_requires ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_atomic_default_mem_order ||
          selector->get_selector_kind() ==
              SgOmpClause::e_omp_context_trait_implementation_user;
      break;
    default:
      throw std::runtime_error(
          "AST JSON OpenMP context selector set has an invalid set kind");
    }
    if (!selector_allowed) {
      throw std::runtime_error(
          "AST JSON OpenMP trait selector is illegal in its selector set");
    }
    validateOmpContextSelector(selector);

    bool unique = false;
    if (selector->get_selector_kind() ==
        SgOmpClause::e_omp_context_trait_construct) {
      unique = construct_selector_kinds
                   .insert(selector->get_construct_directive()->variantT())
                   .second;
    } else if (selector->get_selector_kind() ==
               SgOmpClause::e_omp_context_trait_implementation_user) {
      if (selector->get_implementation_defined_name().is_null()) {
        throw std::runtime_error(
            "AST JSON OpenMP implementation-defined selector has no exact "
            "name");
      }
      unique =
          implementation_selector_names
              .insert(selector->get_implementation_defined_name().getString())
              .second;
    } else {
      unique =
          singleton_selector_kinds.insert(selector->get_selector_kind()).second;
    }
    if (!unique) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector set has a duplicate selector");
    }

    if (selector->get_selector_kind() ==
        SgOmpClause::e_omp_context_trait_kind) {
      bool selector_has_any = false;
      for (const SgOmpContextSelectorProperty *property :
           selector->get_properties()) {
        selector_has_any =
            selector_has_any || property->get_context_kind() ==
                                    SgOmpClause::e_omp_when_context_kind_any;
      }
      if (selector_has_any && selector->get_properties().size() != 1) {
        throw std::runtime_error(
            "AST JSON OpenMP kind(any) is not the only property in its "
            "selector");
      }
      has_kind_any = has_kind_any || selector_has_any;
    }
  }
  if (has_kind_any && set->get_selectors().size() != 1) {
    throw std::runtime_error(
        "AST JSON OpenMP kind(any) selector is not the only selector in its "
        "set");
  }
}

void validateOmpContextSelectorSets(const SgOmpContextSelectorSetPtrList &sets,
                                    const SgNode *owner) {
  if (isSgOmpWhenClause(owner) == nullptr &&
      isSgOmpMatchClause(owner) == nullptr) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector sets have an invalid owner");
  }
  if (sets.empty()) {
    throw std::runtime_error(
        "AST JSON OpenMP variant clause has no context selector sets");
  }

  std::unordered_set<int> set_kinds;
  for (const SgOmpContextSelectorSet *set : sets) {
    if (set == nullptr || set->get_parent() != owner) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector set has a different clause owner");
    }
    if (!set_kinds.insert(set->get_set_kind()).second) {
      throw std::runtime_error(
          "AST JSON OpenMP variant clause has a duplicate selector set");
    }
    validateOmpContextSelectorSet(set);
  }
}

bool edgeTargetIsInParentChain(const SgNode *node, const SgNode *target) {
  if (node == nullptr || target == nullptr) {
    return false;
  }
  for (const SgNode *current = node->get_parent(); current != nullptr;
       current = current->get_parent()) {
    if (current == target) {
      return true;
    }
  }
  return false;
}

bool isRightHandSideOfMemberAccess(const SgNode *node) {
  if (node == nullptr) {
    return false;
  }
  const SgNode *parent = node->get_parent();
  if (const SgDotExp *dot = isSgDotExp(parent)) {
    return dot->get_rhs_operand() == node;
  }
  if (const SgArrowExp *arrow = isSgArrowExp(parent)) {
    return arrow->get_rhs_operand() == node;
  }
  return false;
}

bool isAnonymousDataMemberReference(SgVarRefExp *ref) {
  if (ref == nullptr || !isRightHandSideOfMemberAccess(ref)) {
    return false;
  }
  SgVariableSymbol *symbol = ref->get_symbol();
  SgInitializedName *name =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  SgClassDefinition *definition =
      name != nullptr ? isSgClassDefinition(name->get_scope()) : nullptr;
  SgClassDeclaration *declaration =
      definition != nullptr ? definition->get_declaration() : nullptr;
  if (declaration == nullptr) {
    return false;
  }
  return declaration->get_isUnNamed();
}

void validateAnonymousDataMemberReferenceQualification(SgVarRefExp *ref) {
  if (isAnonymousDataMemberReference(ref) &&
      (ref->get_name_qualification_length() != 0 ||
       ref->get_type_elaboration_required() ||
       ref->get_global_qualification_required() ||
       ref->get_explicit_name_qualification_length() != -1 ||
       ref->get_explicit_global_qualification() ||
       !ref->get_explicit_name_qualification_tokens().empty())) {
    throw std::runtime_error(
        "AST JSON anonymous data-member reference has qualification state");
  }
}

void writeRawObject(std::ostream &out, int level,
                    const std::vector<std::string> &fields, bool comma) {
  indent(out, level);
  out << "{";
  if (!fields.empty()) {
    out << '\n';
    for (size_t i = 0; i < fields.size(); ++i) {
      indent(out, level + 2);
      out << fields[i];
      if (i + 1 != fields.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, level);
  }
  out << '}';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

std::string
rawTypeJson(SgType *type,
            const std::unordered_map<const SgNode *, uint64_t> &ids);

std::string safeNodeText(SgNode *node);
bool insideCollectionBoundary(SgNode *node);
std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl);
std::string
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external);

const SgNode *currentTypeSerializationNode = nullptr;
bool serializingTypeOwnedExpression = false;

TypeSerializationContext::TypeSerializationContext(const SgNode *node)
    : previous_(currentTypeSerializationNode) {
  currentTypeSerializationNode = node;
}

TypeSerializationContext::~TypeSerializationContext() {
  currentTypeSerializationNode = previous_;
}

class TypeOwnedExpressionSerializationContext {
public:
  TypeOwnedExpressionSerializationContext()
      : previous_(serializingTypeOwnedExpression) {
    serializingTypeOwnedExpression = true;
  }

  ~TypeOwnedExpressionSerializationContext() {
    serializingTypeOwnedExpression = previous_;
  }

private:
  bool previous_;
};

uint64_t idFor(const std::unordered_map<const SgNode *, uint64_t> &ids,
               const SgNode *node) {
  if (node == nullptr) {
    return 0;
  }
  auto found = ids.find(node);
  return found == ids.end() ? 0 : found->second;
}

uint64_t canonicalTypedefDeclarationId(
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const SgTypedefDeclaration *decl) {
  if (decl == nullptr) {
    return 0;
  }

  const uint64_t direct_id = idFor(ids, decl);
  if (direct_id != 0) {
    return direct_id;
  }
  if (const SgTypedefDeclaration *first_nondef =
          isSgTypedefDeclaration(decl->get_firstNondefiningDeclaration())) {
    if (const uint64_t id = idFor(ids, first_nondef)) {
      return id;
    }
  }
  if (const SgTypedefDeclaration *defining =
          isSgTypedefDeclaration(decl->get_definingDeclaration())) {
    if (const uint64_t id = idFor(ids, defining)) {
      return id;
    }
  }
  return 0;
}

const char *const kAstJsonExternalFunctionAttribute =
    "rex_ast_json_external_function";
const char *const kAstJsonExternalModuleAttribute =
    "rex_ast_json_external_module";
const char *const kAstJsonExternalClassDeclarationAttribute =
    "rex_ast_json_external_class_declaration";
const char *const kAstJsonExternalSourceFileAttribute =
    "rex_ast_json_external_source_file";
const char *const kAstJsonSemanticArrayIdentityAttribute =
    "rex_ast_json_semantic_array_identity";
const char *const kAstJsonPointerMemberIdentityAttribute =
    "rex_ast_json_pointer_member_identity";

std::unordered_map<const SgNode *, uint64_t> preservedJsonNodeIds;
std::atomic<uint64_t> nextSemanticArrayJsonIdentity{1};
std::atomic<uint64_t> nextPointerMemberJsonIdentity{1};

class AstJsonStringAttribute : public AstAttribute {
public:
  explicit AstJsonStringAttribute(std::string value)
      : value_(std::move(value)) {}

  AstAttribute *copy() const override {
    return new AstJsonStringAttribute(value_);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "AstJsonStringAttribute";
  }

  const std::string &value() const { return value_; }

private:
  std::string value_;
};

class AstJsonIdentityAttribute : public AstAttribute {
public:
  explicit AstJsonIdentityAttribute(uint64_t value) : value_(value) {}

  AstAttribute *copy() const override {
    return new AstJsonIdentityAttribute(value_);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string attribute_class_name() const override {
    return "AstJsonIdentityAttribute";
  }

  uint64_t value() const { return value_; }

private:
  uint64_t value_;
};

uint64_t typeJsonIdentity(const SgType *type, const char *name) {
  SgType *mutable_type = const_cast<SgType *>(type);
  if (mutable_type == nullptr || !mutable_type->attributeExists(name)) {
    return 0;
  }
  const AstJsonIdentityAttribute *attribute =
      dynamic_cast<const AstJsonIdentityAttribute *>(
          mutable_type->getAttribute(name));
  if (attribute == nullptr || attribute->value() == 0 ||
      attribute->value() >
          static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    throw std::runtime_error(std::string("AST JSON type has malformed ") +
                             name + " metadata");
  }
  return attribute->value();
}

void advanceTypeJsonIdentity(std::atomic<uint64_t> &next, uint64_t identity) {
  const uint64_t successor = identity + 1;
  uint64_t candidate = next.load(std::memory_order_relaxed);
  while (candidate < successor &&
         !next.compare_exchange_weak(candidate, successor,
                                     std::memory_order_relaxed,
                                     std::memory_order_relaxed)) {
  }
}

void attachTypeJsonIdentity(SgType *type, const char *name, uint64_t identity,
                            std::atomic<uint64_t> &next) {
  if (type == nullptr || identity == 0 ||
      identity > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    throw std::runtime_error(std::string("AST JSON cannot attach invalid ") +
                             name + " metadata");
  }
  const uint64_t existing = typeJsonIdentity(type, name);
  if (existing != 0 && existing != identity) {
    throw std::runtime_error(std::string("AST JSON type has conflicting ") +
                             name + " metadata");
  }
  if (existing == 0) {
    type->setAttribute(name, new AstJsonIdentityAttribute(identity));
  }
  advanceTypeJsonIdentity(next, identity);
}

uint64_t assignTypeJsonIdentity(SgType *type, const char *name,
                                std::atomic<uint64_t> &next) {
  if (type == nullptr) {
    throw std::runtime_error(std::string("AST JSON cannot assign ") + name +
                             " metadata to a null type");
  }
  if (const uint64_t existing = typeJsonIdentity(type, name)) {
    return existing;
  }
  const uint64_t identity = next.fetch_add(1, std::memory_order_relaxed);
  if (identity == 0 ||
      identity > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    throw std::runtime_error(std::string("AST JSON exhausted ") + name +
                             " metadata");
  }
  attachTypeJsonIdentity(type, name, identity, next);
  return identity;
}

void setAstJsonStringAttribute(SgNode *node, const char *name,
                               std::string value) {
  if (node == nullptr) {
    return;
  }
  node->setAttribute(name, new AstJsonStringAttribute(std::move(value)));
}

std::string astJsonStringAttribute(SgNode *node, const char *name) {
  if (node == nullptr || !node->attributeExists(name)) {
    return "";
  }
  AstJsonStringAttribute *attribute =
      dynamic_cast<AstJsonStringAttribute *>(node->getAttribute(name));
  return attribute != nullptr ? attribute->value() : "";
}

bool hasAstJsonAttribute(SgNode *node, const char *name) {
  return node != nullptr && node->attributeExists(name);
}

bool isAstJsonExternalFunction(SgFunctionDeclaration *decl) {
  return hasAstJsonAttribute(decl, kAstJsonExternalFunctionAttribute);
}

bool isAstJsonExternalModule(SgModuleStatement *module) {
  return hasAstJsonAttribute(module, kAstJsonExternalModuleAttribute);
}

bool isAstJsonExternalClassDeclaration(SgClassDeclaration *decl) {
  return hasAstJsonAttribute(decl, kAstJsonExternalClassDeclarationAttribute);
}

void markAstJsonExternalFunction(SgFunctionDeclaration *decl,
                                 const std::string &source_file) {
  if (decl == nullptr) {
    return;
  }
  setAstJsonStringAttribute(decl, kAstJsonExternalFunctionAttribute, "true");
  setAstJsonStringAttribute(decl, kAstJsonExternalSourceFileAttribute,
                            source_file);
}

void markAstJsonExternalModule(SgModuleStatement *module,
                               const std::string &source_file) {
  if (module == nullptr) {
    return;
  }
  setAstJsonStringAttribute(module, kAstJsonExternalModuleAttribute, "true");
  setAstJsonStringAttribute(module, kAstJsonExternalSourceFileAttribute,
                            source_file);
}

void markAstJsonExternalClassDeclaration(SgClassDeclaration *decl,
                                         const std::string &source_file) {
  if (decl == nullptr) {
    return;
  }
  setAstJsonStringAttribute(decl, kAstJsonExternalClassDeclarationAttribute,
                            "true");
  setAstJsonStringAttribute(decl, kAstJsonExternalSourceFileAttribute,
                            source_file);
}

void attachJsonNodeId(SgNode *node, uint64_t id) {
  if (node == nullptr || id == 0) {
    return;
  }
  preservedJsonNodeIds[node] = id;
}

uint64_t preservedJsonNodeId(SgNode *node) {
  if (node == nullptr) {
    return 0;
  }
  auto found = preservedJsonNodeIds.find(node);
  return found == preservedJsonNodeIds.end() ? 0 : found->second;
}

uint64_t semanticArrayJsonIdentity(SgArrayType *type) {
  if (type == nullptr || type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON cannot assign semantic identity to a source array type");
  }
  return assignTypeJsonIdentity(type, kAstJsonSemanticArrayIdentityAttribute,
                                nextSemanticArrayJsonIdentity);
}

uint64_t preservedSemanticArrayJsonIdentity(const SgArrayType *type) {
  return typeJsonIdentity(type, kAstJsonSemanticArrayIdentityAttribute);
}

void attachSemanticArrayJsonIdentity(SgArrayType *type, uint64_t identity) {
  if (type == nullptr || type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON cannot restore semantic identity on a source array type");
  }
  attachTypeJsonIdentity(type, kAstJsonSemanticArrayIdentityAttribute, identity,
                         nextSemanticArrayJsonIdentity);
}

uint64_t pointerMemberJsonIdentity(SgPointerMemberType *type) {
  if (type == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot assign identity to a null pointer-member type");
  }
  return assignTypeJsonIdentity(type, kAstJsonPointerMemberIdentityAttribute,
                                nextPointerMemberJsonIdentity);
}

uint64_t preservedPointerMemberJsonIdentity(const SgPointerMemberType *type) {
  return typeJsonIdentity(type, kAstJsonPointerMemberIdentityAttribute);
}

void attachPointerMemberJsonIdentity(SgPointerMemberType *type,
                                     uint64_t identity) {
  if (type == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot restore identity on a null pointer-member type");
  }
  attachTypeJsonIdentity(type, kAstJsonPointerMemberIdentityAttribute, identity,
                         nextPointerMemberJsonIdentity);
}

void installPointerCache(SgType *base, SgPointerType *pointer) {
  if (base == nullptr || pointer == nullptr) {
    return;
  }
  SgPointerType *cached = base->get_ptr_to();
  if (cached == pointer) {
    return;
  }
  if (cached == nullptr || cached->get_base_type() != base) {
    base->set_ptr_to(pointer);
  }
}

void installReferenceCache(SgType *base, SgReferenceType *reference) {
  if (base == nullptr || reference == nullptr) {
    return;
  }
  SgReferenceType *cached = base->get_ref_to();
  if (cached == reference) {
    return;
  }
  if (cached == nullptr || cached->get_base_type() != base) {
    base->set_ref_to(reference);
  }
}

void installRvalueReferenceCache(SgType *base,
                                 SgRvalueReferenceType *reference) {
  if (base == nullptr || reference == nullptr) {
    return;
  }
  SgRvalueReferenceType *cached = base->get_rvalue_ref_to();
  if (cached == reference) {
    return;
  }
  if (cached == nullptr || cached->get_base_type() != base) {
    base->set_rvalue_ref_to(reference);
  }
}

SgPointerType *buildCachedJsonPointerType(SgType *base) {
  ROSE_ASSERT(base != nullptr);
  if (isSgFunctionType(base) != nullptr) {
    SgPointerType *pointer = base->get_ptr_to();
    if (pointer == nullptr) {
      pointer = new SgPointerType(base);
      installPointerCache(base, pointer);
    }
    if (pointer->get_base_type() != base || base->get_ptr_to() != pointer) {
      throw std::runtime_error(
          "AST JSON pointer-to-function cache does not preserve the exact "
          "non-interned function type identity");
    }
    return pointer;
  }
  SgPointerType *pointer = SgPointerType::createType(base);
  if (pointer == nullptr || pointer->get_base_type() != base ||
      base->get_ptr_to() != pointer) {
    throw std::runtime_error(
        "AST JSON pointer factory did not preserve the exact canonical base "
        "type identity");
  }
  return pointer;
}

void installNewExpressionResultType(SgNewExp *expr, SgType *restored_type,
                                    const JsonValue &json) {
  ROSE_ASSERT(expr != nullptr);
  if (json.kind != JsonValue::Kind::Object ||
      json.requiredString("kind") != "SgPointerType") {
    throw std::runtime_error("AST JSON SgNewExp type must be SgPointerType");
  }
  SgPointerType *pointer = isSgPointerType(restored_type);
  if (pointer == nullptr) {
    throw std::runtime_error(
        "AST JSON SgNewExp restored type is not SgPointerType");
  }
  SgType *specified_type = expr->get_specified_type();
  if (specified_type == nullptr) {
    throw std::runtime_error(
        "AST JSON SgNewExp requires specified_type before result type");
  }
  if (pointer->get_base_type() != specified_type) {
    throw std::runtime_error(
        "AST JSON SgNewExp result type does not point to its exact "
        "specified_type");
  }
  specified_type->set_ptr_to(pointer);
}

void writeFileInfoJson(std::ostream &out, int level, const Sg_File_Info *info,
                       bool comma);

std::string
rawNodeProperties(SgNode *node,
                  const std::unordered_map<const SgNode *, uint64_t> &ids);

std::string rawFileInfoJson(const Sg_File_Info *info) {
  std::ostringstream out;
  writeFileInfoJson(out, 0, info, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawLocationJson(SgNode *node) {
  SgExpression *expression = isSgExpression(node);
  std::ostringstream out;
  out << "{\n";
  indent(out, 2);
  out << jsonString("start") << ": "
      << rawFileInfoJson(node != nullptr ? node->get_startOfConstruct()
                                         : nullptr)
      << ",\n";
  indent(out, 2);
  out << jsonString("end") << ": "
      << rawFileInfoJson(node != nullptr ? node->get_endOfConstruct()
                                         : nullptr);
  if (expression != nullptr) {
    out << ",\n";
    indent(out, 2);
    out << jsonString("operator") << ": "
        << rawFileInfoJson(expression->get_operatorPosition());
  }
  out << '\n';
  out << "}";
  return out.str();
}

std::string rawRequiredLocationJson(SgNode *node, const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a node with source position");
  }
  if (node->get_startOfConstruct() == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires startOfConstruct");
  }
  return rawLocationJson(node);
}

std::string rawNodeFlagsJson(SgNode *node) {
  std::vector<std::string> fields;
  fields.push_back(
      rawBoolField("contains_transformation",
                   node != nullptr && node->get_containsTransformation()));
  const SgLocatedNode *located = isSgLocatedNode(node);
  fields.push_back(rawBoolField(
      "contains_transformation_to_surrounding_whitespace",
      located != nullptr &&
          located->get_containsTransformationToSurroundingWhitespace()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

uint64_t
expressionIdFor(SgExpression *expression,
                const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return idFor(ids, expression);
}

std::string
rawExpressionRef(SgExpression *expression,
                 const std::unordered_map<const SgNode *, uint64_t> &ids) {
  const uint64_t id = expressionIdFor(expression, ids);
  if (expression != nullptr && id == 0) {
    std::ostringstream message;
    message << "AST JSON expression reference target was not collected: "
            << expression->sage_class_name();
    if (SgNode *parent = expression->get_parent()) {
      message << " parent=" << parent->sage_class_name();
    }
    if (currentTypeSerializationNode != nullptr) {
      message << " owner=" << currentTypeSerializationNode->sage_class_name()
              << " owner_text="
              << safeNodeText(
                     const_cast<SgNode *>(currentTypeSerializationNode));
    }
    throw std::runtime_error(message.str());
  }
  std::vector<std::string> fields;
  fields.push_back(rawIntegerField("node", id));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawTypeOwnedExpressionRef(
    SgExpression *expression,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (expression == nullptr) {
    return rawExpressionRef(expression, ids);
  }
  if (expressionIdFor(expression, ids) != 0) {
    // A collected expression can also be referenced by a type-owned semantic
    // edge (for example, one placement-new expression shared by a template
    // argument and decltype). Preserve that exact identity and its complete
    // collected subtree instead of manufacturing a disconnected anonymous
    // copy with no edge records.
    return rawExpressionRef(expression, ids);
  }

  std::vector<std::string> fields;
  fields.push_back(rawIntegerField("node", 0));
  fields.push_back(rawStringField("owned_kind", expression->sage_class_name()));
  fields.push_back(jsonString("flags") + ": " + rawNodeFlagsJson(expression));
  fields.push_back(jsonString("location") + ": " + rawLocationJson(expression));
  TypeOwnedExpressionSerializationContext owned_expression_context;
  fields.push_back(jsonString("properties") + ": " +
                   rawNodeProperties(expression, ids));

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawTypeOwnedExpressionListJson(
    const SgExpressionPtrList &expressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!expressions.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgExpression *expr : expressions) {
      indent(out, 6);
      out << rawTypeOwnedExpressionRef(expr, ids);
      if (++index != expressions.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawTypeOwnedExprListExpJson(
    SgExprListExp *expression_list,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("present", expression_list != nullptr));
  if (expression_list != nullptr) {
    fields.push_back(
        rawStringField("kind", expression_list->sage_class_name()));
    fields.push_back(jsonString("flags") + ": " +
                     rawNodeFlagsJson(expression_list));
    fields.push_back(jsonString("location") + ": " +
                     rawLocationJson(expression_list));
    fields.push_back(jsonString("properties") + ": " +
                     rawNodeProperties(expression_list, ids));
    fields.push_back(jsonString("expressions") + ": " +
                     rawTypeOwnedExpressionListJson(
                         expression_list->get_expressions(), ids));
  }

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

const SgNode *symbolBasis(const SgSymbol *symbol) {
  if (symbol == nullptr) {
    return nullptr;
  }
  if (const SgAliasSymbol *alias_symbol = isSgAliasSymbol(symbol)) {
    return symbolBasis(alias_symbol->get_alias());
  }
  if (const SgRenameSymbol *rename_symbol = isSgRenameSymbol(symbol)) {
    return rename_symbol->get_declaration();
  }
  if (const SgEnumFieldSymbol *enum_field = isSgEnumFieldSymbol(symbol)) {
    return enum_field->get_declaration();
  }
  if (const SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
    if (const SgLabelStatement *label = label_symbol->get_declaration()) {
      return label;
    }
    return label_symbol->get_symbol_basis();
  }
  if (const SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
    return namespace_symbol->get_declaration();
  }
  if (const SgIntrinsicSymbol *intrinsic_symbol = isSgIntrinsicSymbol(symbol)) {
    return intrinsic_symbol->get_declaration();
  }
  if (const SgModuleSymbol *module_symbol = isSgModuleSymbol(symbol)) {
    return module_symbol->get_declaration();
  }
  if (const SgInterfaceSymbol *interface_symbol = isSgInterfaceSymbol(symbol)) {
    return interface_symbol->get_declaration();
  }
  if (const SgCommonSymbol *common_symbol = isSgCommonSymbol(symbol)) {
    return common_symbol->get_declaration();
  }
  if (const SgVariableSymbol *variable = isSgVariableSymbol(symbol)) {
    return variable->get_declaration();
  }
  if (const SgFunctionSymbol *function = isSgFunctionSymbol(symbol)) {
    return function->get_declaration();
  }
  if (const SgMemberFunctionSymbol *member_function =
          isSgMemberFunctionSymbol(symbol)) {
    return member_function->get_declaration();
  }
  if (const SgClassSymbol *class_symbol = isSgClassSymbol(symbol)) {
    return class_symbol->get_declaration();
  }
  if (const SgEnumSymbol *enum_symbol = isSgEnumSymbol(symbol)) {
    return enum_symbol->get_declaration();
  }
  if (const SgTypedefSymbol *typedef_symbol = isSgTypedefSymbol(symbol)) {
    return typedef_symbol->get_declaration();
  }
  if (const SgNonrealSymbol *nonreal_symbol = isSgNonrealSymbol(symbol)) {
    return nonreal_symbol->get_declaration();
  }
  if (const SgTemplateSymbol *template_symbol = isSgTemplateSymbol(symbol)) {
    return template_symbol->get_declaration();
  }
  if (const SgNode *basis = symbol->get_symbol_basis()) {
    return basis;
  }
  return nullptr;
}

std::string symbolName(const SgSymbol *symbol) {
  const SgNode *basis = symbolBasis(symbol);
  if (const SgInitializedName *name = isSgInitializedName(basis)) {
    return name->get_name().getString();
  }
  if (const SgFunctionDeclaration *decl = isSgFunctionDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgClassDeclaration *decl = isSgClassDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgEnumDeclaration *decl = isSgEnumDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgTypedefDeclaration *decl = isSgTypedefDeclaration(basis)) {
    return decl->get_name().getString();
  }
  if (const SgNonrealDecl *decl = isSgNonrealDecl(basis)) {
    return decl->get_name().getString();
  }
  return symbol != nullptr ? symbol->get_name().getString() : "";
}

std::string
rawSymbolRef(SgSymbol *symbol,
             const std::unordered_map<const SgNode *, uint64_t> &ids) {
  const SgNode *basis = symbolBasis(symbol);
  const uint64_t basis_id = idFor(ids, basis);
  const bool external_function =
      basis_id == 0 &&
      (isAstJsonExternalFunction(
           isSgFunctionDeclaration(const_cast<SgNode *>(basis))) ||
       (isSgFunctionDeclaration(basis) != nullptr &&
        !insideCollectionBoundary(const_cast<SgNode *>(basis))));
  const bool external_module =
      basis_id == 0 &&
      (isAstJsonExternalModule(
           isSgModuleStatement(const_cast<SgNode *>(basis))) ||
       (isSgModuleStatement(basis) != nullptr &&
        !insideCollectionBoundary(const_cast<SgNode *>(basis))));
  const bool external_class =
      basis_id == 0 &&
      (isAstJsonExternalClassDeclaration(
           isSgClassDeclaration(const_cast<SgNode *>(basis))) ||
       (isSgClassDeclaration(basis) != nullptr &&
        !insideCollectionBoundary(const_cast<SgNode *>(basis))));
  if (symbol != nullptr && basis_id == 0 && !external_function &&
      !external_module && !external_class) {
    std::ostringstream message;
    message << "AST JSON symbol reference target was not collected: "
            << symbol->get_name().getString();
    if (basis != nullptr) {
      message << " basis=" << basis->sage_class_name()
              << " basis_text=" << safeNodeText(const_cast<SgNode *>(basis));
    }
    throw std::runtime_error(message.str());
  }
  std::vector<std::string> fields;
  fields.push_back(rawIntegerField("symbol_declaration", basis_id));
  fields.push_back(rawStringField("symbol_name", symbolName(symbol)));
  fields.push_back(rawStringField("symbol_kind", symbol->class_name()));
  if (const SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
    fields.push_back(rawIntegerField("label_numeric_label_value",
                                     label_symbol->get_numeric_label_value()));
    fields.push_back(rawIntegerField(
        "label_type", static_cast<int>(label_symbol->get_label_type())));
  }
  if (external_function) {
    fields.push_back(
        jsonString("external_function") + ": " +
        rawExternalFunctionJson(
            isSgFunctionDeclaration(const_cast<SgNode *>(basis)), ids));
  }
  if (external_module) {
    fields.push_back(
        jsonString("external_module") + ": " +
        rawExternalModuleJson(isSgModuleStatement(const_cast<SgNode *>(basis)),
                              ids));
  }
  if (external_class) {
    fields.push_back(jsonString("external_class") + ": " +
                     rawExternalClassDeclarationJson(
                         isSgClassDeclaration(const_cast<SgNode *>(basis))));
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExactBoundSymbolRef(
    SgSymbol *symbol, const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (symbol == nullptr) {
    return "null";
  }

  SgSymbolTable *table = isSgSymbolTable(symbol->get_parent());
  SgScopeStatement *scope =
      table != nullptr ? isSgScopeStatement(table->get_parent()) : nullptr;
  const SgName *binding_name = nullptr;
  size_t occurrences = 0;
  if (table != nullptr && table->get_table() != nullptr) {
    for (const auto &entry : *table->get_table()) {
      if (entry.second == symbol) {
        binding_name = &entry.first;
        ++occurrences;
      }
    }
  }
  const uint64_t scope_id = idFor(ids, scope);
  if (scope == nullptr || scope_id == 0 || binding_name == nullptr ||
      occurrences != 1) {
    std::ostringstream message;
    message << "AST JSON exact symbol has no unique collected visibility "
               "owner";
    message << " symbol=" << symbol->class_name();
    message << " name=" << symbol->get_name().getString();
    message << " table=" << table;
    message << " scope=" << scope;
    message << " scope_id=" << scope_id;
    message << " occurrences=" << occurrences;
    throw std::runtime_error(message.str());
  }

  std::vector<std::string> fields;
  fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));
  fields.push_back(rawIntegerField("binding_scope", scope_id));
  fields.push_back(rawStringField("binding_name", binding_name->getString()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool symbolIsLookupPreferred(SgSymbolTable *table, const SgName &name,
                             SgSymbol *symbol) {
  if (table == nullptr || symbol == nullptr) {
    return false;
  }
  if (isSgVariableSymbol(symbol) != nullptr &&
      table->find_variable(name) == symbol) {
    return true;
  }
  if (isSgClassSymbol(symbol) != nullptr &&
      table->find_class(name, nullptr) == symbol) {
    return true;
  }
  if (isSgEnumSymbol(symbol) != nullptr && table->find_enum(name) == symbol) {
    return true;
  }
  if (isSgEnumFieldSymbol(symbol) != nullptr &&
      table->find_enum_field(name) == symbol) {
    return true;
  }
  if (isSgTypedefSymbol(symbol) != nullptr &&
      table->find_typedef(name) == symbol) {
    return true;
  }
  if (isSgLabelSymbol(symbol) != nullptr && table->find_label(name) == symbol) {
    return true;
  }
  if (isSgNamespaceSymbol(symbol) != nullptr &&
      table->find_namespace(name) == symbol) {
    return true;
  }
  if (isSgFunctionSymbol(symbol) != nullptr &&
      table->find_function(name) == symbol) {
    return true;
  }
  return false;
}

std::string
rawSymbolTableJson(SgScopeStatement *scope,
                   const std::unordered_map<const SgNode *, uint64_t> &ids) {
  SgSymbolTable *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  if (table == nullptr || table->get_table() == nullptr) {
    return "[]";
  }

  std::vector<SymbolTableEntryJson> entries;
  for (const std::pair<const SgName, SgSymbol *> &entry : *table->get_table()) {
    SgSymbol *symbol = entry.second;
    if (symbol == nullptr) {
      throw std::runtime_error(
          "AST JSON encountered a null symbol table entry");
    }
    const SgNode *basis = symbolBasis(symbol);
    if (const SgFunctionDeclaration *function =
            isSgFunctionDeclaration(basis)) {
      if (SageInterface::isFortranProgramUnitWithoutSourceName(function)) {
        const SgName internalKey =
            SageInterface::getFortranProgramUnitSymbolTableKey(function);
        if (isSgFunctionSymbol(symbol) == nullptr ||
            entry.first != internalKey) {
          throw std::runtime_error(
              "AST JSON anonymous Fortran program-unit symbol has an "
              "invalid internal key or symbol kind");
        }
        // This key is implementation identity, not source AST state. Rebuild
        // it from the declaration's exact source anchor after deserialization.
        continue;
      }
    }
    const uint64_t basis_id = idFor(ids, basis);
    const bool external_basis =
        basis_id == 0 &&
        (isAstJsonExternalFunction(
             isSgFunctionDeclaration(const_cast<SgNode *>(basis))) ||
         isAstJsonExternalModule(
             isSgModuleStatement(const_cast<SgNode *>(basis))) ||
         isAstJsonExternalClassDeclaration(
             isSgClassDeclaration(const_cast<SgNode *>(basis))) ||
         (basis != nullptr &&
          !insideCollectionBoundary(const_cast<SgNode *>(basis)) &&
          (isSgFunctionDeclaration(basis) != nullptr ||
           isSgModuleStatement(basis) != nullptr ||
           isSgClassDeclaration(basis) != nullptr)));
    if (basis_id == 0 && !external_basis) {
      std::ostringstream message;
      message << "AST JSON symbol table target was not collected";
      message << " scope=" << scope->sage_class_name();
      message << " entry=" << entry.first.getString();
      message << " symbol=" << symbol->class_name();
      message << " symbol_name=" << symbol->get_name().getString();
      if (basis != nullptr) {
        message << " basis=" << basis->sage_class_name()
                << " basis_text=" << safeNodeText(const_cast<SgNode *>(basis));
      }
      throw std::runtime_error(message.str());
    }

    std::vector<std::string> fields;
    const std::string entry_name = entry.first.getString();
    const std::string symbol_kind = symbol->class_name();
    const bool lookup_preferred =
        symbolIsLookupPreferred(table, entry.first, symbol);
    fields.push_back(rawStringField("entry_name", entry_name));
    fields.push_back(rawStringField("symbol_kind", symbol_kind));
    fields.push_back(rawBoolField("lookup_preferred", lookup_preferred));
    fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));

    if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      if (alias->get_alias() == nullptr) {
        throw std::runtime_error(
            "AST JSON encountered an SgAliasSymbol without an alias target");
      }
      fields.push_back(jsonString("alias_target") + ": " +
                       rawSymbolRef(alias->get_alias(), ids));
      fields.push_back(
          rawBoolField("alias_is_renamed", alias->get_isRenamed()));
      fields.push_back(
          rawStringField("alias_new_name", alias->get_new_name().getString()));
      if (alias->get_causal_nodes().empty()) {
        throw std::runtime_error(
            "AST JSON encountered an SgAliasSymbol without causal "
            "provenance");
      }
      std::ostringstream causal_nodes;
      causal_nodes << "[";
      for (size_t i = 0; i < alias->get_causal_nodes().size(); ++i) {
        SgNode *causal_node = alias->get_causal_nodes()[i];
        const uint64_t causal_id = idFor(ids, causal_node);
        if (causal_node == nullptr || causal_id == 0) {
          throw std::runtime_error(
              "AST JSON SgAliasSymbol causal node is null or was not "
              "collected");
        }
        if (i != 0) {
          causal_nodes << ", ";
        }
        causal_nodes << causal_id;
      }
      causal_nodes << "]";
      fields.push_back(jsonString("alias_causal_nodes") + ": " +
                       causal_nodes.str());
    }
    if (SgRenameSymbol *rename = isSgRenameSymbol(symbol)) {
      if (rename->get_original_symbol() == nullptr) {
        throw std::runtime_error(
            "AST JSON encountered an SgRenameSymbol without an original "
            "symbol");
      }
      fields.push_back(jsonString("original_symbol") + ": " +
                       rawSymbolRef(rename->get_original_symbol(), ids));
      fields.push_back(rawStringField("rename_new_name",
                                      rename->get_new_name().getString()));
    }
    if (SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
      fields.push_back(rawStringField(
          "namespace_name", namespace_symbol->get_name().getString()));
      fields.push_back(
          rawBoolField("namespace_is_alias", namespace_symbol->get_isAlias()));
      fields.push_back(rawIntegerField(
          "namespace_alias_declaration",
          idFor(ids, namespace_symbol->get_aliasDeclaration())));
    }

    std::ostringstream entry_out;
    writeRawObject(entry_out, 0, fields, false);
    std::string entry_json = entry_out.str();
    if (!entry_json.empty() && entry_json.back() == '\n') {
      entry_json.pop_back();
    }

    SymbolTableEntryJson serialized;
    serialized.entry_name = entry_name;
    serialized.symbol_kind = symbol_kind;
    serialized.basis_id = basis_id;
    serialized.lookup_preferred = lookup_preferred;
    serialized.json = std::move(entry_json);
    entries.push_back(std::move(serialized));
  }

  std::stable_sort(
      entries.begin(), entries.end(),
      [](const SymbolTableEntryJson &lhs, const SymbolTableEntryJson &rhs) {
        if (lhs.basis_id != rhs.basis_id) {
          if (lhs.basis_id == 0) {
            return false;
          }
          if (rhs.basis_id == 0) {
            return true;
          }
          return lhs.basis_id < rhs.basis_id;
        }
        if (lhs.lookup_preferred != rhs.lookup_preferred) {
          return lhs.lookup_preferred && !rhs.lookup_preferred;
        }
        if (lhs.entry_name != rhs.entry_name) {
          return lhs.entry_name < rhs.entry_name;
        }
        return lhs.symbol_kind < rhs.symbol_kind;
      });

  std::ostringstream out;
  out << "[";
  if (!entries.empty()) {
    out << '\n';
    for (size_t i = 0; i < entries.size(); ++i) {
      indent(out, 4);
      out << entries[i].json;
      if (i + 1 != entries.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 2);
  }
  out << "]";
  return out.str();
}

} // namespace AstJson
} // namespace Rose
