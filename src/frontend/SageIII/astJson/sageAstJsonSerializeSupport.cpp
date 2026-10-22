#include "AstNodes/Expression/ExpressionTypeClassification.h"
#include "openMPConstantInteger.h"
#include "sageAstJsonPrivate.h"

#include <atomic>
#include <set>
#include <utility>
#include <variant>

namespace Rose {
namespace AstJson {

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
  if (SageInterface::containsUnknownType(type)) {
    throw std::runtime_error("AST JSON " + expression->class_name() +
                             " has no exact semantic value type during " +
                             phase);
  }
}

static const JsonValue *
validatedExpressionTypeProperty(bool semantic, const JsonValue &properties) {
  const JsonValue *type = properties.find("type");
  if (semantic) {
    if (type == nullptr) {
      throw std::runtime_error(
          "AST JSON semantic expression is missing its semantic type "
          "(missing key: type)");
    }
  } else if (type != nullptr) {
    throw std::runtime_error(
        "AST JSON syntax expression has a serialized semantic type");
  }
  return type;
}

const JsonValue *validatedExpressionTypeProperty(const SgExpression *expression,
                                                 const JsonValue &properties) {
  if (expression == nullptr)
    throw std::runtime_error(
        "AST JSON expression type validation received a null expression");
  return validatedExpressionTypeProperty(
      expressionCarriesSemanticType(expression), properties);
}

const JsonValue *validatedExpressionTypeProperty(const NodeRecord &record) {
  static const VariantVector expressions(V_SgExpression);
  const auto kind = std::find_if(
      expressions.begin(), expressions.end(), [&](VariantT variant) {
        return record.kind == roseGlobalVariantNameList[variant];
      });
  if (kind == expressions.end())
    throw std::runtime_error(
        "AST JSON expression type validation received a non-expression kind");
  const bool unevaluated_string =
      *kind == V_SgStringVal &&
      record.properties.requiredBool("cxx_unevaluated");
  return validatedExpressionTypeProperty(
      Rose::AST::expressionHasSemanticValueType(*kind, unevaluated_string),
      record.properties);
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

OmpContextPropertyIdentity ompContextPropertyIdentity(
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind,
    const SgOmpContextSelectorProperty *property) {
  switch (selector_kind) {
  case SgOmpClause::e_omp_context_trait_kind:
    return property->get_context_kind();
  case SgOmpClause::e_omp_context_trait_vendor:
    return property->get_context_vendor();
  case SgOmpClause::e_omp_context_trait_atomic_default_mem_order:
    return property->get_atomic_default_mem_order();
  case SgOmpClause::e_omp_context_trait_arch:
  case SgOmpClause::e_omp_context_trait_isa:
  case SgOmpClause::e_omp_context_trait_uid:
  case SgOmpClause::e_omp_context_trait_extension:
    // OpenMP treats an identifier and the corresponding string literal as
    // the same name-list property value.
    return ompContextPropertyName(property->get_expression());
  case SgOmpClause::e_omp_context_trait_requires: {
    const auto kind = property->get_requires_kind();
    if (kind == SgOmpClause::e_omp_requires_property_implementation_defined)
      return std::make_pair(kind,
                            property->get_requires_extension().getString());
    return kind;
  }
  case SgOmpClause::e_omp_context_trait_implementation_user:
    return ompContextSyntaxProperty(property->get_expression());
  case SgOmpClause::e_omp_context_trait_condition:
  case SgOmpClause::e_omp_context_trait_device_num:
    // These selectors require exactly one property, so cardinality is their
    // stronger and language-independent uniqueness check.
    return std::monostate{};
  case SgOmpClause::e_omp_context_trait_construct:
  default:
    throw std::runtime_error(
        "AST JSON OpenMP context property has no valid selector identity");
  }
}

} // namespace

void validateOmpContextPropertyPayload(
    bool has_expression, SgOmpClause::omp_when_context_kind_enum kind,
    SgOmpClause::omp_when_context_vendor_enum vendor,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum atomic_order,
    SgOmpClause::omp_requires_property_kind_enum requires_kind,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum requires_atomic_order,
    bool has_requires_expression, bool has_requires_extension) {
  const unsigned count =
      static_cast<unsigned>(has_expression) +
      static_cast<unsigned>(kind !=
                            SgOmpClause::e_omp_when_context_kind_unknown) +
      static_cast<unsigned>(
          vendor != SgOmpClause::e_omp_when_context_vendor_unspecified) +
      static_cast<unsigned>(
          atomic_order !=
          SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified) +
      static_cast<unsigned>(requires_kind !=
                            SgOmpClause::e_omp_requires_property_unspecified);
  if (count != 1)
    throw std::runtime_error("AST JSON OpenMP context selector property does "
                             "not own exactly one typed payload");
  switch (requires_kind) {
  case SgOmpClause::e_omp_requires_property_unspecified:
    if (has_requires_expression ||
        requires_atomic_order !=
            SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
        has_requires_extension)
      throw std::runtime_error(
          "AST JSON OpenMP non-requires property owns a requires payload");
    break;
  case SgOmpClause::e_omp_requires_property_reverse_offload:
  case SgOmpClause::e_omp_requires_property_unified_address:
  case SgOmpClause::e_omp_requires_property_unified_shared_memory:
  case SgOmpClause::e_omp_requires_property_dynamic_allocators:
  case SgOmpClause::e_omp_requires_property_self_maps:
  case SgOmpClause::e_omp_requires_property_device_safesync:
    if (requires_atomic_order !=
            SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
        has_requires_extension)
      throw std::runtime_error(
          "AST JSON OpenMP ordinary requires property owns a mismatched "
          "payload");
    break;
  case SgOmpClause::e_omp_requires_property_atomic_default_mem_order:
    if (has_requires_expression || has_requires_extension)
      throw std::runtime_error(
          "AST JSON OpenMP atomic_default_mem_order requires property "
          "owns a mismatched payload");
    switch (requires_atomic_order) {
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
    if (has_requires_expression ||
        requires_atomic_order !=
            SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
        !has_requires_extension)
      throw std::runtime_error(
          "AST JSON OpenMP implementation-defined requires property is "
          "malformed");
    break;
  default:
    throw std::runtime_error(
        "AST JSON OpenMP requires property has an invalid typed kind");
  }
}

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

  validateOmpContextSelectorPropertyPayload(property,
                                            selector->get_selector_kind());
}

void validateOmpContextPropertySelectorKind(
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind,
    bool has_expression, SgOmpClause::omp_when_context_kind_enum kind,
    SgOmpClause::omp_when_context_vendor_enum vendor,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum atomic_order,
    SgOmpClause::omp_requires_property_kind_enum requires_kind) {
  bool matches = false;
  switch (selector_kind) {
  case SgOmpClause::e_omp_context_trait_condition:
  case SgOmpClause::e_omp_context_trait_arch:
  case SgOmpClause::e_omp_context_trait_isa:
  case SgOmpClause::e_omp_context_trait_device_num:
  case SgOmpClause::e_omp_context_trait_uid:
  case SgOmpClause::e_omp_context_trait_extension:
  case SgOmpClause::e_omp_context_trait_implementation_user:
    matches = has_expression;
    break;
  case SgOmpClause::e_omp_context_trait_kind:
    matches = kind != SgOmpClause::e_omp_when_context_kind_unknown;
    break;
  case SgOmpClause::e_omp_context_trait_vendor:
    matches = vendor != SgOmpClause::e_omp_when_context_vendor_unspecified;
    break;
  case SgOmpClause::e_omp_context_trait_atomic_default_mem_order:
    matches = atomic_order !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified;
    break;
  case SgOmpClause::e_omp_context_trait_requires:
    matches = requires_kind != SgOmpClause::e_omp_requires_property_unspecified;
    break;
  case SgOmpClause::e_omp_context_trait_construct:
  default:
    throw std::runtime_error("AST JSON OpenMP context selector property is "
                             "illegal for its selector kind");
  }
  if (!matches)
    throw std::runtime_error("AST JSON OpenMP context selector property has "
                             "the wrong typed payload for its selector");
}

OmpContextPropertyIdentity validateOmpContextSelectorPropertyPayload(
    const SgOmpContextSelectorProperty *property,
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind) {
  if (property == nullptr)
    throw std::runtime_error(
        "AST JSON has a null OpenMP context selector property or owner");
  const bool has_expression = property->get_expression() != nullptr;
  validateOmpContextPropertyPayload(
      has_expression, property->get_context_kind(),
      property->get_context_vendor(), property->get_atomic_default_mem_order(),
      property->get_requires_kind(),
      property->get_requires_atomic_default_mem_order(),
      property->get_requires_expression() != nullptr,
      !property->get_requires_extension().getString().empty());
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

  validateOmpContextPropertySelectorKind(
      selector_kind, has_expression, property->get_context_kind(),
      property->get_context_vendor(), property->get_atomic_default_mem_order(),
      property->get_requires_kind());

  if (property->get_requires_expression() != nullptr &&
      !expressionCarriesSemanticType(property->get_requires_expression()))
    throw std::runtime_error(
        "AST JSON OpenMP requires logical expression is not a semantic "
        "expression");

  // This also validates the exact syntax-node kind for name-list and syntax
  // properties without ever asking those nodes for a fabricated value type.
  if ((selector_kind == SgOmpClause::e_omp_context_trait_condition ||
       selector_kind == SgOmpClause::e_omp_context_trait_device_num) &&
      !expressionCarriesSemanticType(property->get_expression())) {
    throw std::runtime_error(
        "AST JSON OpenMP semantic context property is not a semantic "
        "expression");
  }
  return ompContextPropertyIdentity(selector_kind, property);
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

  for (const auto *property : selector->get_properties()) {
    if (property == nullptr)
      throw std::runtime_error(
          "AST JSON has a null OpenMP context selector property or owner");
    if (property->get_parent() != selector)
      throw std::runtime_error(
          "AST JSON OpenMP context selector property has a different owner");
  }
  validateOmpContextSelectorPayload(
      selector->get_selector_kind(),
      selector->get_implementation_defined_name(), selector->get_score(),
      selector->get_construct_directive(), selector->get_properties());
}

void validateOmpContextSelectorShape(
    SgOmpClause::omp_context_trait_selector_kind_enum kind,
    const SgName &implementation_name, bool has_score, bool has_construct,
    size_t property_count) {
  const bool custom =
      kind == SgOmpClause::e_omp_context_trait_implementation_user;
  const bool has_custom_name = !implementation_name.is_null() &&
                               !implementation_name.getString().empty();
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
  switch (kind) {
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

  if (has_score && !score_allowed) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector has a prohibited score");
  }
  if (has_construct != construct_required) {
    throw std::runtime_error(
        construct_required
            ? "AST JSON OpenMP construct selector is missing its directive"
            : "AST JSON OpenMP context selector has an unexpected directive");
  }
  if (property_count < minimum_properties ||
      property_count > maximum_properties ||
      (has_score && property_count == 0)) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector has invalid property cardinality");
  }
}

void validateOmpContextSelectorPayload(
    SgOmpClause::omp_context_trait_selector_kind_enum kind,
    const SgName &implementation_name, SgExpression *score,
    SgStatement *construct_directive,
    const SgOmpContextSelectorPropertyPtrList &properties) {
  validateOmpContextSelectorShape(kind, implementation_name, score != nullptr,
                                  construct_directive != nullptr,
                                  properties.size());
  if (score != nullptr)
    validateOmpContextScore(score);
  std::set<OmpContextPropertyIdentity> property_identities;
  for (const SgOmpContextSelectorProperty *property : properties) {
    if (!property_identities
             .insert(validateOmpContextSelectorPropertyPayload(property, kind))
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
  for (const auto *selector : set->get_selectors())
    if (selector == nullptr || selector->get_parent() != set)
      throw std::runtime_error(
          "AST JSON OpenMP context selector has a different set owner");
  validateOmpContextSelectorSetPayload(set->get_set_kind(),
                                       set->get_selectors());
}

void validateOmpContextTraitInSet(
    SgOmpClause::omp_context_selector_set_kind_enum set_kind,
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind) {
  bool selector_allowed = false;
  switch (set_kind) {
  case SgOmpClause::e_omp_context_selector_set_user:
    selector_allowed =
        selector_kind == SgOmpClause::e_omp_context_trait_condition;
    break;
  case SgOmpClause::e_omp_context_selector_set_construct:
    selector_allowed =
        selector_kind == SgOmpClause::e_omp_context_trait_construct;
    break;
  case SgOmpClause::e_omp_context_selector_set_device:
    selector_allowed = selector_kind == SgOmpClause::e_omp_context_trait_kind ||
                       selector_kind == SgOmpClause::e_omp_context_trait_arch ||
                       selector_kind == SgOmpClause::e_omp_context_trait_isa;
    break;
  case SgOmpClause::e_omp_context_selector_set_target_device:
    selector_allowed =
        selector_kind == SgOmpClause::e_omp_context_trait_kind ||
        selector_kind == SgOmpClause::e_omp_context_trait_arch ||
        selector_kind == SgOmpClause::e_omp_context_trait_isa ||
        selector_kind == SgOmpClause::e_omp_context_trait_device_num ||
        selector_kind == SgOmpClause::e_omp_context_trait_uid;
    break;
  case SgOmpClause::e_omp_context_selector_set_implementation:
    selector_allowed =
        selector_kind == SgOmpClause::e_omp_context_trait_vendor ||
        selector_kind == SgOmpClause::e_omp_context_trait_extension ||
        selector_kind == SgOmpClause::e_omp_context_trait_requires ||
        selector_kind ==
            SgOmpClause::e_omp_context_trait_atomic_default_mem_order ||
        selector_kind == SgOmpClause::e_omp_context_trait_implementation_user;
    break;
  default:
    throw std::runtime_error(
        "AST JSON OpenMP context selector set has an invalid set kind");
  }
  if (!selector_allowed) {
    throw std::runtime_error(
        "AST JSON OpenMP trait selector is illegal in its selector set");
  }
}

OmpContextSelectorIdentity ompContextSelectorIdentity(
    SgOmpClause::omp_context_trait_selector_kind_enum kind,
    const SgName &implementation_name, std::optional<VariantT> construct_kind) {
  if (kind == SgOmpClause::e_omp_context_trait_construct) {
    if (!construct_kind)
      throw std::runtime_error(
          "AST JSON OpenMP construct selector is missing its directive");
    return *construct_kind;
  }
  if (kind == SgOmpClause::e_omp_context_trait_implementation_user) {
    if (implementation_name.is_null() ||
        implementation_name.getString().empty())
      throw std::runtime_error(
          "AST JSON OpenMP implementation-defined selector has no exact name");
    return implementation_name.getString();
  }
  return kind;
}

void validateOmpContextKindAny(bool has_any, size_t property_count,
                               size_t selector_count) {
  if (!has_any)
    return;
  if (property_count != 1)
    throw std::runtime_error(
        "AST JSON OpenMP kind(any) is not the only property in its selector");
  if (selector_count != 1)
    throw std::runtime_error("AST JSON OpenMP kind(any) selector is not the "
                             "only selector in its set");
}

void validateOmpContextSelectorSetPayload(
    SgOmpClause::omp_context_selector_set_kind_enum kind,
    const SgOmpContextSelectorPtrList &selectors) {
  if (selectors.empty()) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector set has no selectors");
  }

  std::set<OmpContextSelectorIdentity> selector_identities;
  for (const SgOmpContextSelector *selector : selectors) {
    if (selector == nullptr)
      throw std::runtime_error(
          "AST JSON OpenMP context selector has a different set owner");

    validateOmpContextTraitInSet(kind, selector->get_selector_kind());
    validateOmpContextSelector(selector);

    std::optional<VariantT> construct_kind;
    if (selector->get_construct_directive() != nullptr)
      construct_kind = selector->get_construct_directive()->variantT();
    if (!selector_identities
             .insert(ompContextSelectorIdentity(
                 selector->get_selector_kind(),
                 selector->get_implementation_defined_name(), construct_kind))
             .second) {
      throw std::runtime_error(
          "AST JSON OpenMP context selector set has a duplicate selector");
    }

    if (selector->get_selector_kind() ==
        SgOmpClause::e_omp_context_trait_kind) {
      const auto &properties = selector->get_properties();
      const bool has_any =
          std::any_of(properties.begin(), properties.end(),
                      [](const SgOmpContextSelectorProperty *property) {
                        return property->get_context_kind() ==
                               SgOmpClause::e_omp_when_context_kind_any;
                      });
      validateOmpContextKindAny(has_any, properties.size(), selectors.size());
    }
  }
}

void validateOmpContextSelectorSets(const SgOmpContextSelectorSetPtrList &sets,
                                    const SgNode *owner) {
  if (isSgOmpWhenClause(owner) == nullptr &&
      isSgOmpMatchClause(owner) == nullptr) {
    throw std::runtime_error(
        "AST JSON OpenMP context selector sets have an invalid owner");
  }
  for (const auto *set : sets)
    if (set == nullptr || set->get_parent() != owner)
      throw std::runtime_error(
          "AST JSON OpenMP context selector set has a different clause owner");
  validateOmpContextSelectorSetsPayload(sets);
}

void validateOmpContextSelectorSetsPayload(
    const SgOmpContextSelectorSetPtrList &sets) {
  if (sets.empty()) {
    throw std::runtime_error(
        "AST JSON OpenMP variant clause has no context selector sets");
  }

  std::unordered_set<SgOmpClause::omp_context_selector_set_kind_enum> set_kinds;
  for (const SgOmpContextSelectorSet *set : sets) {
    if (set == nullptr)
      throw std::runtime_error(
          "AST JSON OpenMP context selector set has a different clause owner");
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

std::atomic<uint64_t> nextTypeJsonIdentity{1};

uint64_t typeJsonIdentity(const SgType *type, const char *description) {
  if (type == nullptr) {
    return 0;
  }
  const uint64_t identity = type->get_ast_json_type_identity();
  if (identity > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    throw std::runtime_error(std::string("AST JSON type has invalid ") +
                             description + " identity");
  }
  return identity;
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
    type->set_ast_json_type_identity(identity);
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
    advanceTypeJsonIdentity(next, existing);
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

bool isAstJsonExternalFunction(SgFunctionDeclaration *decl) {
  return decl != nullptr &&
         decl->get_ast_json_external_role() ==
             SgDeclarationStatement::e_ast_json_external_function;
}

bool isAstJsonExternalModule(SgModuleStatement *module) {
  return module != nullptr &&
         module->get_ast_json_external_role() ==
             SgDeclarationStatement::e_ast_json_external_module;
}

bool isAstJsonExternalClassDeclaration(SgClassDeclaration *decl) {
  return decl != nullptr &&
         decl->get_ast_json_external_role() ==
             SgDeclarationStatement::e_ast_json_external_class;
}

void initializeExternalProvenance(
    SgDeclarationStatement *decl,
    SgDeclarationStatement::ast_json_external_role_enum role,
    const std::string &source_file) {
  if (decl == nullptr || source_file.empty() ||
      source_file.find('\0') != std::string::npos) {
    throw std::runtime_error(
        "AST JSON external declaration requires a node and valid source_file");
  }
  if (decl->get_ast_json_external_role() !=
      SgDeclarationStatement::e_ast_json_local) {
    throw std::runtime_error(
        "AST JSON external declaration provenance is already initialized");
  }
  decl->initialize_ast_json_external_provenance(role, source_file);
}

void markAstJsonExternalFunction(SgFunctionDeclaration *decl,
                                 const std::string &source_file) {
  initializeExternalProvenance(
      decl, SgDeclarationStatement::e_ast_json_external_function, source_file);
}

void markAstJsonExternalModule(SgModuleStatement *module,
                               const std::string &source_file) {
  initializeExternalProvenance(
      module, SgDeclarationStatement::e_ast_json_external_module, source_file);
}

void markAstJsonExternalClassDeclaration(SgClassDeclaration *decl,
                                         const std::string &source_file) {
  initializeExternalProvenance(
      decl, SgDeclarationStatement::e_ast_json_external_class, source_file);
}

void attachJsonNodeId(SgNode *node, uint64_t id) {
  if (node == nullptr || id == 0) {
    throw std::runtime_error(
        "AST JSON node identity requires a node and positive ID");
  }
  node->set_ast_json_node_id(id);
}

uint64_t preservedJsonNodeId(SgNode *node) {
  if (node == nullptr) {
    return 0;
  }
  return node->get_ast_json_node_id();
}

uint64_t semanticArrayJsonIdentity(SgArrayType *type) {
  if (type == nullptr || type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON cannot assign semantic identity to a source array type");
  }
  return assignTypeJsonIdentity(type, "semantic array", nextTypeJsonIdentity);
}

uint64_t preservedSemanticArrayJsonIdentity(const SgArrayType *type) {
  return typeJsonIdentity(type, "semantic array");
}

void attachSemanticArrayJsonIdentity(SgArrayType *type, uint64_t identity) {
  if (type == nullptr || type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON cannot restore semantic identity on a source array type");
  }
  attachTypeJsonIdentity(type, "semantic array", identity,
                         nextTypeJsonIdentity);
}

uint64_t sourceTypeJsonIdentity(SgType *type) {
  if (!typeOwnsSourceJsonIdentity(type))
    throw std::runtime_error(
        "AST JSON source identity requires a source-syntax type");
  return assignTypeJsonIdentity(type, "source type", nextTypeJsonIdentity);
}

uint64_t preservedSourceTypeJsonIdentity(const SgType *type) {
  return typeJsonIdentity(type, "source type");
}

void attachSourceTypeJsonIdentity(SgType *type, uint64_t identity) {
  if (!typeOwnsSourceJsonIdentity(type))
    throw std::runtime_error(
        "AST JSON source identity requires a source-syntax type");
  attachTypeJsonIdentity(type, "source type", identity, nextTypeJsonIdentity);
}

uint64_t pointerMemberJsonIdentity(SgPointerMemberType *type) {
  if (type == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot assign identity to a null pointer-member type");
  }
  return assignTypeJsonIdentity(type, "pointer member", nextTypeJsonIdentity);
}

uint64_t preservedPointerMemberJsonIdentity(const SgPointerMemberType *type) {
  return typeJsonIdentity(type, "pointer member");
}

void attachPointerMemberJsonIdentity(SgPointerMemberType *type,
                                     uint64_t identity) {
  if (type == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot restore identity on a null pointer-member type");
  }
  attachTypeJsonIdentity(type, "pointer member", identity,
                         nextTypeJsonIdentity);
}

SgPointerType *buildCachedJsonPointerType(SgType *base) {
  ROSE_ASSERT(base != nullptr);
  if (isSgFunctionType(base) != nullptr) {
    SgPointerType *pointer = base->get_ptr_to();
    if (pointer == nullptr) {
      pointer = new SgPointerType(base);
      base->set_ptr_to(pointer);
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
  ASSERT_not_null(node);
  SgExpression *expression = isSgExpression(node);
  std::ostringstream out;
  out << "{\n";
  indent(out, 2);
  out << jsonString("start") << ": "
      << rawFileInfoJson(node->get_startOfConstruct()) << ",\n";
  indent(out, 2);
  out << jsonString("end") << ": "
      << rawFileInfoJson(node->get_endOfConstruct());
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
  ASSERT_not_null(node);
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("is_modified", node->get_isModified()));
  fields.push_back(rawBoolField("contains_transformation",
                                node->get_containsTransformation()));
  const SgLocatedNode *located = isSgLocatedNode(node);
  fields.push_back(rawBoolField(
      "contains_transformation_to_surrounding_whitespace",
      located != nullptr &&
          located->get_containsTransformationToSurroundingWhitespace()));
  fields.push_back(
      rawBoolField("source_range_ends_in_macro_expansion",
                   located != nullptr &&
                       located->get_source_range_ends_in_macro_expansion()));
  fields.push_back(rawBoolField(
      "source_range_is_macro_expansion_fragment",
      located != nullptr &&
          located->get_source_range_is_macro_expansion_fragment()));
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

static std::string rawOriginalExpressionField(
    SgExpression *expression,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  SgExpression *source = expression->get_originalExpressionTree();
  if (source != nullptr &&
      (source == expression || source->get_parent() != expression ||
       source->get_originalExpressionTree() != nullptr))
    throw std::runtime_error("AST JSON original expression requires one "
                             "exclusively owned source tree");
  return jsonString("original_expression") + ": " +
         rawTypeOwnedExpressionRef(source, ids);
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
  fields.push_back(rawOriginalExpressionField(expression, ids));
  TypeOwnedExpressionSerializationContext owned_expression_context;
  fields.push_back(jsonString("properties") + ": " +
                   rawNodeProperties(expression, ids));
  if (auto *list = isSgExprListExp(expression))
    fields.push_back(
        jsonString("expressions") + ": " +
        rawTypeOwnedExpressionListJson(list->get_expressions(), ids));

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
    fields.push_back(rawOriginalExpressionField(expression_list, ids));
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
  return symbol->get_symbol_basis();
}

std::string symbolName(const SgSymbol *symbol) {
  if (const auto *label = isSgLabelSymbol(symbol))
    return label->get_name().getString();
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
    fields.push_back(
        rawIntegerField("label_basis_kind",
                        static_cast<int>(labelSymbolBasisKind(label_symbol))));
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

std::string
rawExactBoundSymbolRef(SgSymbol *symbol,
                       const std::unordered_map<const SgNode *, uint64_t> &ids,
                       const SgScopeStatement *inline_scope) {
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
  if (scope == nullptr || (scope_id == 0 && scope != inline_scope) ||
      binding_name == nullptr || occurrences != 1) {
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
  if (scope == inline_scope)
    fields.push_back(rawBoolField("binding_local", true));
  else
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

std::vector<std::string> rawVisibilitySymbolFields(
    SgSymbol *symbol, const std::unordered_map<const SgNode *, uint64_t> &ids,
    const SgScopeStatement *inline_scope) {
  auto *alias = isSgAliasSymbol(symbol);
  auto *rename = isSgRenameSymbol(symbol);
  ROSE_ASSERT(alias != nullptr || rename != nullptr);
  auto *target =
      alias != nullptr ? alias->get_alias() : rename->get_original_symbol();
  if (target == nullptr)
    throw std::runtime_error("AST JSON alias/rename has no target");
  std::vector<std::string> fields;
  if (isSgAliasSymbol(target) || isSgRenameSymbol(target))
    fields.push_back(jsonString(alias != nullptr ? "alias_target_binding"
                                                 : "original_symbol_binding") +
                     ": " + rawExactBoundSymbolRef(target, ids, inline_scope));
  else
    fields.push_back(
        jsonString(alias != nullptr ? "alias_target" : "original_symbol") +
        ": " + rawSymbolRef(target, ids));
  if (alias != nullptr)
    fields.push_back(rawBoolField("alias_is_renamed", alias->get_isRenamed()));
  fields.push_back(rawStringField(
      alias != nullptr ? "alias_new_name" : "rename_new_name",
      (alias != nullptr ? alias->get_new_name() : rename->get_new_name())
          .getString()));
  return fields;
}

VariantT symbolLookupPreferenceCategory(VariantT kind) {
  static const auto categories = [] {
    std::unordered_map<VariantT, VariantT> result;
    for (VariantT category :
         {V_SgVariableSymbol, V_SgClassSymbol, V_SgEnumSymbol,
          V_SgEnumFieldSymbol, V_SgTypedefSymbol, V_SgLabelSymbol,
          V_SgNamespaceSymbol, V_SgFunctionSymbol})
      for (VariantT variant : VariantVector(category)) {
        const bool inserted = result.emplace(variant, category).second;
        ROSE_ASSERT(inserted);
      }
    return result;
  }();
  const auto category = categories.find(kind);
  return category == categories.end() ? V_SgSymbol : category->second;
}

VariantT symbolLookupPreferenceCategory(SgSymbol *symbol) {
  return symbolLookupPreferenceCategory(symbol != nullptr ? symbol->variantT()
                                                          : V_SgSymbol);
}

bool symbolIsLookupPreferred(SgSymbolTable *table, const SgName &name,
                             SgSymbol *symbol) {
  if (table == nullptr)
    return false;
  switch (symbolLookupPreferenceCategory(symbol)) {
  case V_SgVariableSymbol:
    return table->find_variable(name) == symbol;
  case V_SgClassSymbol:
    return table->find_class(name, nullptr) == symbol;
  case V_SgEnumSymbol:
    return table->find_enum(name) == symbol;
  case V_SgEnumFieldSymbol:
    return table->find_enum_field(name) == symbol;
  case V_SgTypedefSymbol:
    return table->find_typedef(name) == symbol;
  case V_SgLabelSymbol:
    return table->find_label(name) == symbol;
  case V_SgNamespaceSymbol:
    return table->find_namespace(name) == symbol;
  case V_SgFunctionSymbol:
    return table->find_function(name) == symbol;
  default:
    return false;
  }
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

    if (auto *namelist = isSgNamelistSymbol(symbol)) {
      std::ostringstream declarations;
      declarations << "[";
      const auto &groups = namelist->get_declarations();
      for (size_t index = 0; index < groups.size(); ++index) {
        const auto id = idFor(ids, groups[index]);
        if (id == 0)
          throw std::runtime_error(
              "AST JSON NAMELIST declaration was not collected");
        if (index != 0)
          declarations << ", ";
        declarations << id;
      }
      declarations << "]";
      fields.push_back(jsonString("namelist_declarations") + ": " +
                       declarations.str());
    }

    SgAliasSymbol *alias = isSgAliasSymbol(symbol);
    SgRenameSymbol *rename = isSgRenameSymbol(symbol);
    if (alias != nullptr || rename != nullptr) {
      auto visibility_fields = rawVisibilitySymbolFields(symbol, ids);
      fields.insert(fields.end(), visibility_fields.begin(),
                    visibility_fields.end());
    }
    const SgNodePtrList *causes = alias != nullptr ? &alias->get_causal_nodes()
                                  : rename != nullptr
                                      ? &rename->get_causal_nodes()
                                      : nullptr;
    if (causes != nullptr) {
      if (alias != nullptr && causes->empty()) {
        throw std::runtime_error(
            "AST JSON encountered a visibility symbol without causal "
            "provenance");
      }
      std::ostringstream causal_nodes;
      causal_nodes << "[";
      for (size_t i = 0; i < causes->size(); ++i) {
        SgNode *causal_node = (*causes)[i];
        const uint64_t causal_id = idFor(ids, causal_node);
        if (causal_node == nullptr || causal_id == 0) {
          throw std::runtime_error(
              "AST JSON visibility-symbol causal node is null or was not "
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
