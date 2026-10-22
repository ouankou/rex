#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

void validateNamespaceSymbolKey(const SgName &entry_name, SgNode *basis) {
  auto *declaration = isSgNamespaceDeclarationStatement(basis);
  auto *alias = isSgNamespaceAliasDeclarationStatement(basis);
  if (declaration == nullptr && alias == nullptr) {
    throw std::runtime_error("AST JSON namespace symbol has invalid basis");
  }
  const SgName name =
      alias != nullptr ? alias->get_name() : declaration->get_name();
  if (entry_name != name) {
    throw std::runtime_error("AST JSON namespace symbol key contradicts its "
                             "typed declaration basis");
  }
}

LabelSymbolBasisKind labelSymbolBasisKind(const SgLabelSymbol *symbol) {
  ASSERT_not_null(symbol);
  const bool declaration = symbol->get_declaration() != nullptr;
  const bool statement = symbol->get_fortran_statement() != nullptr;
  const bool parameter =
      symbol->get_fortran_alternate_return_parameter() != nullptr;
  if (static_cast<int>(declaration) + statement + parameter != 1)
    throw std::runtime_error(
        "AST JSON label symbol must have exactly one typed basis");
  if (declaration)
    return LabelSymbolBasisKind::Declaration;
  if (statement)
    return LabelSymbolBasisKind::FortranStatement;
  return LabelSymbolBasisKind::AlternateReturn;
}

VariantT symbolVariantFromJsonKind(const std::string &kind) {
  static const std::pair<const char *, VariantT> variants[] = {
      {"SgAliasSymbol", V_SgAliasSymbol},
      {"SgClassSymbol", V_SgClassSymbol},
      {"SgCommonSymbol", V_SgCommonSymbol},
      {"SgNamelistSymbol", V_SgNamelistSymbol},
      {"SgEnumFieldSymbol", V_SgEnumFieldSymbol},
      {"SgEnumSymbol", V_SgEnumSymbol},
      {"SgFunctionSymbol", V_SgFunctionSymbol},
      {"SgInterfaceSymbol", V_SgInterfaceSymbol},
      {"SgIntrinsicSymbol", V_SgIntrinsicSymbol},
      {"SgLabelSymbol", V_SgLabelSymbol},
      {"SgMemberFunctionSymbol", V_SgMemberFunctionSymbol},
      {"SgModuleSymbol", V_SgModuleSymbol},
      {"SgNamespaceSymbol", V_SgNamespaceSymbol},
      {"SgNonrealSymbol", V_SgNonrealSymbol},
      {"SgRenameSymbol", V_SgRenameSymbol},
      {"SgTemplateClassSymbol", V_SgTemplateClassSymbol},
      {"SgTemplateFunctionSymbol", V_SgTemplateFunctionSymbol},
      {"SgTemplateMemberFunctionSymbol", V_SgTemplateMemberFunctionSymbol},
      {"SgTemplateSymbol", V_SgTemplateSymbol},
      {"SgTemplateTypedefSymbol", V_SgTemplateTypedefSymbol},
      {"SgTemplateVariableSymbol", V_SgTemplateVariableSymbol},
      {"SgTypedefSymbol", V_SgTypedefSymbol},
      {"SgVariableSymbol", V_SgVariableSymbol},
  };
  for (const auto &[name, variant] : variants) {
    if (kind == name)
      return variant;
  }
  throw std::runtime_error("AST JSON symbol kind is unsupported: " + kind);
}

namespace {
template <class Symbol, class Basis>
std::function<SgSymbol *()> symbolWithBasis(Basis *basis) {
  if (basis == nullptr)
    throw std::runtime_error("AST JSON symbol has an incompatible basis node");
  return [basis] { return new Symbol(basis); };
}

} // namespace

LabelSymbolFields labelSymbolFieldsFromJson(const JsonValue &json) {
  const auto basis = requiredEnum<LabelSymbolBasisKind>(
      json, "label_basis_kind", "label symbol",
      {LabelSymbolBasisKind::Declaration,
       LabelSymbolBasisKind::FortranStatement,
       LabelSymbolBasisKind::AlternateReturn});
  const int64_t value = json.requiredInt("label_numeric_label_value");
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max())
    throw std::runtime_error("AST JSON label numeric value is out of range");
  const auto type = requiredEnum<SgLabelSymbol::label_type_enum>(
      json, "label_type", "label symbol",
      {SgLabelSymbol::e_unknown_label_type,
       SgLabelSymbol::e_non_numeric_label_type,
       SgLabelSymbol::e_start_label_type, SgLabelSymbol::e_else_label_type,
       SgLabelSymbol::e_end_label_type,
       SgLabelSymbol::e_alternative_return_type});
  if ((basis == LabelSymbolBasisKind::Declaration &&
       (value != -1 || type != SgLabelSymbol::e_unknown_label_type)) ||
      (basis == LabelSymbolBasisKind::FortranStatement &&
       (value < 1 || value > 99999 ||
        (type != SgLabelSymbol::e_start_label_type &&
         type != SgLabelSymbol::e_end_label_type &&
         type != SgLabelSymbol::e_else_label_type))) ||
      (basis == LabelSymbolBasisKind::AlternateReturn &&
       (value != -1 || type != SgLabelSymbol::e_alternative_return_type)))
    throw std::runtime_error(
        "AST JSON label symbol has invalid typed identity");
  return {basis, static_cast<int>(value), type};
}
void validateLabelSymbolFields(const SgLabelSymbol *symbol,
                               const LabelSymbolFields &fields) {
  if (labelSymbolBasisKind(symbol) != fields.basis ||
      symbol->get_numeric_label_value() != fields.value ||
      symbol->get_label_type() != fields.type ||
      (fields.type == SgLabelSymbol::e_else_label_type &&
       isSgIfStmt(symbol->get_fortran_statement()) == nullptr))
    throw std::runtime_error(
        "AST JSON label symbol differs from its constructed identity");
}

void validateLabelSymbolFields(const SgLabelSymbol *symbol,
                               const JsonValue &json) {
  validateLabelSymbolFields(symbol, labelSymbolFieldsFromJson(json));
}

SgScopeStatement *labelSymbolScopeForBasis(SgNode *basis) {
  auto *parameter = isSgInitializedName(basis);
  if (SgLabelStatement *label = isSgLabelStatement(basis)) {
    if (label->get_scope() != nullptr) {
      return label->get_scope();
    }
  }
  for (SgNode *current = basis; current != nullptr;
       current = current->get_parent()) {
    if (SgFunctionDefinition *function = isSgFunctionDefinition(current)) {
      return function;
    }
    if (auto *module = isSgModuleStatement(current);
        module != nullptr && parameter == nullptr)
      return module->get_definition();
  }
  return parameter != nullptr ? parameter->get_scope() : nullptr;
}

static SgLabelSymbol *
labelSymbolForExactBasis(SgNode *basis, const LabelSymbolFields &fields) {
  SgScopeStatement *scope = labelSymbolScopeForBasis(basis);
  if (scope == nullptr)
    throw std::runtime_error("AST JSON label reference has no exact scope");
  SgSymbolTable *table = scope->get_symbol_table();
  ASSERT_not_null(table);
  SgLabelSymbol *symbol = nullptr;
  for (SgSymbol *entry : table->find_symbols_with_exact_basis(basis)) {
    ROSE_ASSERT(entry != nullptr && entry->get_symbol_basis() == basis &&
                entry->get_parent() == table && table->exists(entry));
    auto *candidate = isSgLabelSymbol(entry);
    if (candidate == nullptr ||
        labelSymbolBasisKind(candidate) != fields.basis ||
        candidate->get_numeric_label_value() != fields.value ||
        candidate->get_label_type() != fields.type)
      continue;
    if (symbol != nullptr)
      throw std::runtime_error(
          "AST JSON label reference has duplicate typed bindings");
    symbol = candidate;
  }
  if (symbol == nullptr) {
    throw std::runtime_error(
        "AST JSON label reference basis differs from its symbol binding");
  }
  validateLabelSymbolFields(symbol, fields);
  return symbol;
}

SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes) {
  const uint64_t declaration_id =
      json.requiredNonnegativeNodeId("symbol_declaration");
  if (declaration_id == 0) {
    return createExternalSymbolFromJson(json, nodes, nullptr);
  }
  const VariantT expected_kind =
      symbolVariantFromJsonKind(json.requiredString("symbol_kind"));

  SgNode *basis = nodeById(nodes, declaration_id);
  SgSymbol *symbol = nullptr;
  if (expected_kind == V_SgLabelSymbol) {
    symbol = labelSymbolForExactBasis(basis, labelSymbolFieldsFromJson(json));
  } else if (auto *group = isSgNameGroup(basis)) {
    auto *owner = isSgNamelistStatement(group->get_parent());
    if (owner != nullptr && owner->get_parent() != nullptr) {
      auto *scope = owner->get_scope();
      for (auto *candidate :
           scope->get_symbol_table()->find_symbols_with_exact_basis(group)) {
        if (candidate->variantT() == expected_kind) {
          if (symbol != nullptr)
            throw std::runtime_error(
                "AST JSON duplicate NAMELIST symbol basis");
          symbol = candidate;
        }
      }
    }
  } else if (SgInitializedName *name = isSgInitializedName(basis)) {
    SgScopeStatement *scope = name->get_scope();
    if (scope != nullptr)
      symbol = scope->find_symbol_from_declaration(name);
  } else if (SgDeclarationStatement *decl = isSgDeclarationStatement(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr)
      symbol = scope->find_symbol_from_declaration(decl);
  }
  if (symbol != nullptr) {
    if (symbolBasis(symbol) != basis || symbol->variantT() != expected_kind ||
        symbolName(symbol) != json.requiredString("symbol_name")) {
      throw std::runtime_error(
          "AST JSON internal symbol record disagrees with its exact typed "
          "binding");
    }
    return symbol;
  }

  std::ostringstream message;
  message << "AST JSON internal symbol reference has no exact restored "
             "symbol-table binding";
  message << " kind=" << json.requiredString("symbol_kind");
  message << " name=" << json.requiredString("symbol_name");
  message << " basis=" << basis->sage_class_name();
  message << " basis_id=" << declaration_id;
  throw std::runtime_error(message.str());
}

static SgSymbol *
findConstructedExactSymbolBinding(const JsonValue &json, const NodeMap &nodes,
                                  SgSymbolTable *local_table = nullptr) {
  if (json.kind == JsonValue::Kind::Null) {
    return nullptr;
  }
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON exact symbol reference is neither null nor an object");
  }

  const JsonValue &symbol_json = json.at("symbol");
  const std::string expected_kind = symbol_json.requiredString("symbol_kind");
  const VariantT expected_variant = symbolVariantFromJsonKind(expected_kind);
  const std::string expected_basis_name =
      symbol_json.requiredString("symbol_name");
  const uint64_t basis_id =
      symbol_json.requiredNonnegativeNodeId("symbol_declaration");
  const auto *local = json.find("binding_local");
  const uint64_t scope_id =
      local != nullptr ? 0 : json.requiredNonnegativeNodeId("binding_scope");
  if (local != nullptr && (!local->asBool() || local_table == nullptr ||
                           json.find("binding_scope") != nullptr))
    throw std::runtime_error(
        "AST JSON alias/rename local binding has no exact receiver");
  if (local == nullptr && scope_id == 0) {
    throw std::runtime_error(
        "AST JSON exact symbol reference has a null binding scope");
  }
  auto basis = nodes.find(basis_id);
  SgNode *expected_basis = basis != nodes.end() ? basis->second : nullptr;
  const JsonValue *external_module = nullptr;
  std::optional<SgClassDeclaration::class_types> external_module_type;
  std::string external_module_source;
  if (basis_id == 0) {
    const JsonValue *external_function = symbol_json.find("external_function");
    external_module = symbol_json.find("external_module");
    const JsonValue *external_class = symbol_json.find("external_class");
    if ((external_function != nullptr) + (external_module != nullptr) +
            (external_class != nullptr) !=
        1) {
      throw std::runtime_error(
          "AST JSON exact external symbol requires one declaration basis");
    }
    if (external_function != nullptr) {
      SgFunctionDeclaration *function =
          restoredExternalFunctionFromJson(*external_function);
      expected_basis = function != nullptr
                           ? function->get_firstNondefiningDeclaration()
                           : nullptr;
    } else if (external_class != nullptr) {
      expected_basis = restoredExternalClassFromJson(*external_class);
    } else {
      external_module_type = requiredEnum<SgClassDeclaration::class_types>(
          *external_module, "class_type", "external_module",
          {SgClassDeclaration::e_fortran_module});
      external_module_source = external_module->requiredString("source_file");
      const std::string external_module_name =
          external_module->requiredString("name");
      if (!external_module->requiredBool("present") ||
          requiredTranslationUnitSourceOrder(*external_module,
                                             "external_module")
              .has_value() ||
          external_module_name.empty() ||
          external_module_name.find('\0') != std::string::npos ||
          external_module_name != expected_basis_name ||
          external_module_source.empty() ||
          external_module_source.find('\0') != std::string::npos) {
        throw std::runtime_error(
            "AST JSON exact external module has an invalid typed identity");
      }
    }
  }
  const SgName binding_name = json.requiredName("binding_name");
  if ((external_module == nullptr && expected_basis == nullptr) ||
      (local == nullptr && nodes.count(scope_id) == 0))
    return nullptr;
  SgScopeStatement *scope = local != nullptr
                                ? isSgScopeStatement(local_table->get_parent())
                                : nodeByIdAs<SgScopeStatement>(nodes, scope_id);
  if (scope == nullptr)
    throw std::runtime_error(
        "AST JSON exact symbol has no typed binding scope");
  SgSymbolTable *table =
      local != nullptr ? local_table : scope->get_symbol_table();
  if (table == nullptr || table->get_table() == nullptr) {
    throw std::runtime_error(
        "AST JSON exact symbol binding scope has no restored symbol table");
  }

  SgSymbol *result = nullptr;
  size_t matches = 0;
  for (const auto &entry : *table->get_table()) {
    SgSymbol *candidate = entry.second;
    if (entry.first != binding_name || candidate == nullptr ||
        candidate->variantT() != expected_variant) {
      continue;
    }
    const SgNode *candidate_basis = symbolBasis(candidate);
    bool basis_matches = candidate_basis == expected_basis;
    if (external_module != nullptr) {
      const SgModuleStatement *module = isSgModuleStatement(candidate_basis);
      basis_matches =
          module != nullptr &&
          module->get_ast_json_external_role() ==
              SgDeclarationStatement::e_ast_json_external_module &&
          module->get_name().getString() ==
              external_module->requiredString("name") &&
          module->get_class_type() == *external_module_type &&
          module->get_ast_json_external_source_file() == external_module_source;
    }
    if (basis_matches && symbolName(candidate) == expected_basis_name) {
      result = candidate;
      ++matches;
    }
  }
  if (matches > 1) {
    std::ostringstream message;
    message << "AST JSON failed to restore one exact symbol binding";
    message << " scope=" << scope->sage_class_name();
    message << " name=" << binding_name.getString();
    message << " kind=" << expected_kind;
    message << " basis_name=" << expected_basis_name;
    message << " matches=" << matches;
    throw std::runtime_error(message.str());
  }
  return result;
}

bool exactSymbolBindingDependenciesReady(const JsonValue &json,
                                         const NodeMap &nodes) {
  return json.kind == JsonValue::Kind::Null ||
         findConstructedExactSymbolBinding(json, nodes) != nullptr;
}

SgSymbol *exactBoundSymbolFromJson(const JsonValue &json, const NodeMap &nodes,
                                   SgSymbolTable *local_table) {
  SgSymbol *symbol =
      findConstructedExactSymbolBinding(json, nodes, local_table);
  if (symbol == nullptr && json.kind != JsonValue::Kind::Null)
    throw std::runtime_error(
        "AST JSON failed to restore one exact symbol binding");
  return symbol;
}

size_t exactSymbolBindingEntryIndex(const JsonValue &binding,
                                    const JsonValue &entries) {
  if (entries.kind != JsonValue::Kind::Array)
    throw std::runtime_error(
        "AST JSON alias/rename binding table is not an array");
  const auto name = binding.requiredName("binding_name");
  const auto &symbol = binding.at("symbol");
  const auto kind = symbol.requiredString("symbol_kind");
  std::optional<size_t> result;
  for (size_t index = 0; index < entries.array.size(); ++index) {
    const auto &candidate = entries.array[index];
    if (candidate.requiredName("entry_name") != name ||
        candidate.requiredString("symbol_kind") != kind ||
        !candidate.at("symbol").exactlyEquals(symbol))
      continue;
    if (result)
      throw std::runtime_error(
          "AST JSON alias/rename target binding is ambiguous");
    result = index;
  }
  if (!result)
    throw std::runtime_error(
        "AST JSON alias/rename target has no exact recorded binding");
  return *result;
}

std::vector<size_t>
symbolDependencyOrder(const std::vector<std::optional<size_t>> &dependencies) {
  enum State { Unseen, Visiting, Complete };
  std::vector<State> states(dependencies.size(), Unseen);
  std::vector<size_t> order;
  order.reserve(dependencies.size());
  for (size_t index = 0; index < dependencies.size(); ++index) {
    std::vector<size_t> path;
    for (auto current = index;; current = *dependencies[current]) {
      if (states.at(current) == Complete)
        break;
      if (states[current] == Visiting)
        throw std::runtime_error(
            "AST JSON alias/rename targets contain a cycle");
      states[current] = Visiting;
      path.push_back(current);
      if (!dependencies[current])
        break;
    }
    for (auto position = path.rbegin(); position != path.rend(); ++position) {
      states[*position] = Complete;
      order.push_back(*position);
    }
  }
  return order;
}

std::function<SgSymbol *()> prepareSymbolForKindAndBasis(
    VariantT kind, SgNode *basis,
    const std::optional<LabelSymbolFields> &label_fields) {
  ROSE_ASSERT((kind == V_SgLabelSymbol) == label_fields.has_value());
  switch (kind) {
  case V_SgVariableSymbol:
    return symbolWithBasis<SgVariableSymbol>(isSgInitializedName(basis));
  case V_SgTemplateVariableSymbol:
    return symbolWithBasis<SgTemplateVariableSymbol>(
        isSgInitializedName(basis));
  case V_SgEnumFieldSymbol:
    return symbolWithBasis<SgEnumFieldSymbol>(isSgInitializedName(basis));
  case V_SgIntrinsicSymbol:
    return symbolWithBasis<SgIntrinsicSymbol>(isSgInitializedName(basis));
  case V_SgCommonSymbol:
    return symbolWithBasis<SgCommonSymbol>(isSgInitializedName(basis));
  case V_SgFunctionSymbol:
    return symbolWithBasis<SgFunctionSymbol>(isSgFunctionDeclaration(basis));
  case V_SgMemberFunctionSymbol:
    return symbolWithBasis<SgMemberFunctionSymbol>(
        isSgFunctionDeclaration(basis));
  case V_SgTemplateFunctionSymbol:
    return symbolWithBasis<SgTemplateFunctionSymbol>(
        isSgFunctionDeclaration(basis));
  case V_SgTemplateMemberFunctionSymbol:
    return symbolWithBasis<SgTemplateMemberFunctionSymbol>(
        isSgFunctionDeclaration(basis));
  case V_SgClassSymbol:
    return symbolWithBasis<SgClassSymbol>(isSgClassDeclaration(basis));
  case V_SgTemplateClassSymbol:
    return symbolWithBasis<SgTemplateClassSymbol>(
        isSgTemplateClassDeclaration(basis));
  case V_SgEnumSymbol:
    return symbolWithBasis<SgEnumSymbol>(isSgEnumDeclaration(basis));
  case V_SgTypedefSymbol:
    return symbolWithBasis<SgTypedefSymbol>(isSgTypedefDeclaration(basis));
  case V_SgTemplateTypedefSymbol:
    if (auto *instantiation =
            isSgTemplateInstantiationTypedefDeclaration(basis))
      return symbolWithBasis<SgTemplateTypedefSymbol>(instantiation);
    return symbolWithBasis<SgTemplateTypedefSymbol>(
        isSgTemplateTypedefDeclaration(basis));
  case V_SgLabelSymbol: {
    const auto fields = label_fields.value();
    auto *declaration = isSgLabelStatement(basis);
    auto *statement = isSgStatement(basis);
    auto *parameter = isSgInitializedName(basis);
    if ((fields.basis == LabelSymbolBasisKind::Declaration &&
         declaration == nullptr) ||
        (fields.basis == LabelSymbolBasisKind::FortranStatement &&
         (statement == nullptr ||
          (fields.type == SgLabelSymbol::e_else_label_type &&
           isSgIfStmt(statement) == nullptr))) ||
        (fields.basis == LabelSymbolBasisKind::AlternateReturn &&
         (parameter == nullptr || parameter->get_name() != "*")))
      throw std::runtime_error(
          "AST JSON label basis kind disagrees with its node");
    return [fields, declaration, statement, parameter]() -> SgSymbol * {
      if (fields.basis == LabelSymbolBasisKind::AlternateReturn)
        return new SgLabelSymbol(parameter);
      if (fields.basis == LabelSymbolBasisKind::Declaration)
        return new SgLabelSymbol(declaration);
      return new SgLabelSymbol(statement, fields.value, fields.type);
    };
  }
  case V_SgNamespaceSymbol:
    if (auto *declaration = isSgNamespaceDeclarationStatement(basis)) {
      return [declaration]() -> SgSymbol * {
        return new SgNamespaceSymbol(declaration, nullptr);
      };
    }
    if (auto *alias = isSgNamespaceAliasDeclarationStatement(basis)) {
      return [alias] { return new SgNamespaceSymbol(nullptr, alias); };
    }
    throw std::runtime_error("AST JSON namespace symbol has invalid basis");
  case V_SgModuleSymbol:
    return symbolWithBasis<SgModuleSymbol>(isSgModuleStatement(basis));
  case V_SgInterfaceSymbol:
    return symbolWithBasis<SgInterfaceSymbol>(isSgInterfaceStatement(basis));
  case V_SgNonrealSymbol:
    return symbolWithBasis<SgNonrealSymbol>(isSgNonrealDecl(basis));
  case V_SgTemplateSymbol:
    return symbolWithBasis<SgTemplateSymbol>(isSgTemplateDeclaration(basis));
  default:
    throw std::runtime_error("AST JSON symbol kind cannot be constructed from "
                             "a declaration basis");
  }
}

std::function<SgSymbol *()>
prepareSymbolForKindAndBasis(VariantT kind, SgNode *basis,
                             const JsonValue &properties) {
  std::optional<LabelSymbolFields> fields;
  if (kind == V_SgLabelSymbol)
    fields = labelSymbolFieldsFromJson(properties);
  return prepareSymbolForKindAndBasis(kind, basis, fields);
}

SgSymbol *createSymbolForKindAndBasis(VariantT kind, SgNode *basis,
                                      const JsonValue &properties) {
  auto construct = prepareSymbolForKindAndBasis(kind, basis, properties);
  return construct();
}

std::function<SgSymbol *()>
prepareExternalSymbolFromJson(const JsonValue &json, const NodeMap &nodes,
                              SgSymbolTable *owner_table) {
  const std::string kind = json.requiredString("symbol_kind");
  const VariantT symbol_variant = symbolVariantFromJsonKind(kind);
  const std::string name = json.requiredString("symbol_name");
  const JsonValue *external_function = json.find("external_function");
  const JsonValue *external_module = json.find("external_module");
  const JsonValue *external_class = json.find("external_class");
  const int identity_count = (external_function != nullptr) +
                             (external_module != nullptr) +
                             (external_class != nullptr);
  if (identity_count != 1) {
    throw std::runtime_error(
        "AST JSON external symbol requires exactly one declaration basis");
  }
  if (external_function != nullptr &&
      (!external_function->requiredBool("present") ||
       symbol_variant != V_SgFunctionSymbol)) {
    throw std::runtime_error(
        "AST JSON external function symbol has an incompatible basis");
  }
  if (external_module != nullptr &&
      (!external_module->requiredBool("present") ||
       (symbol_variant != V_SgModuleSymbol &&
        symbol_variant != V_SgClassSymbol))) {
    throw std::runtime_error(
        "AST JSON external module symbol has an incompatible basis");
  }
  if (external_class != nullptr) {
    const std::string class_kind = external_class->requiredString("kind");
    const bool compatible_kind = symbol_variant == V_SgClassSymbol ||
                                 (class_kind == "SgModuleStatement" &&
                                  symbol_variant == V_SgModuleSymbol);
    if (!external_class->requiredBool("present") || !compatible_kind) {
      throw std::runtime_error(
          "AST JSON external class symbol has an incompatible basis");
    }
  }
  const JsonValue *identity;
  if (external_function != nullptr)
    identity = external_function;
  else if (external_module != nullptr)
    identity = external_module;
  else
    identity = external_class;
  if (identity->requiredString("name") != name)
    throw std::runtime_error(
        "AST JSON external symbol has a conflicting basis name");
  std::function<SgNode *()> construct_basis;
  if (external_function != nullptr) {
    construct_basis = decodeExternalFunction(*external_function, nodes);
  } else if (external_module != nullptr) {
    construct_basis = decodeExternalModule(*external_module);
  } else {
    construct_basis = decodeExternalClassDeclaration(*external_class);
  }
  return [construct_basis = std::move(construct_basis), symbol_variant, name,
          kind, owner_table]() -> SgSymbol * {
    SgNode *basis = construct_basis();
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(basis)) {
      SgFunctionDeclaration *canonical_declaration =
          isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
      SgScopeStatement *scope = canonical_declaration != nullptr
                                    ? canonical_declaration->get_scope()
                                    : nullptr;
      if (owner_table != nullptr) {
        if (scope == nullptr || scope->get_symbol_table() != owner_table ||
            canonical_declaration->get_firstNondefiningDeclaration() !=
                canonical_declaration)
          throw std::runtime_error("AST JSON external function symbol has a "
                                   "conflicting table owner");
        auto symbol = std::make_unique<SgFunctionSymbol>(canonical_declaration);
        if (symbolName(symbol.get()) != name)
          throw std::runtime_error(
              "AST JSON external function symbol has a conflicting name");
        return symbol.release();
      }
      SgSymbol *canonical_symbol =
          scope != nullptr
              ? scope->find_symbol_from_declaration(canonical_declaration)
              : nullptr;
      if (canonical_symbol == nullptr ||
          canonical_symbol->variantT() != V_SgFunctionSymbol ||
          symbolName(canonical_symbol) != name ||
          canonical_symbol->get_symbol_basis() != canonical_declaration ||
          canonical_symbol->get_parent() != scope->get_symbol_table() ||
          !scope->get_symbol_table()->exists(canonical_symbol)) {
        throw std::runtime_error(
            "AST JSON external function symbol has no exact canonical "
            "declaration-owned binding: " +
            kind);
      }
      return canonical_symbol;
    }
    std::unique_ptr<SgSymbol> symbol(
        prepareSymbolForKindAndBasis(symbol_variant, basis, std::nullopt)());
    if (symbol == nullptr || symbol->get_symbol_basis() == nullptr ||
        symbolName(symbol.get()) != name) {
      throw std::runtime_error(
          "AST JSON cannot reconstruct external symbol reference: " + kind);
    }
    return symbol.release();
  };
}

SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes,
                                       SgSymbolTable *owner_table) {
  return prepareExternalSymbolFromJson(json, nodes, owner_table)();
}

void validateExternalSymbolBasisOwnership(SgSymbol *symbol) {
  if (symbol == nullptr) {
    throw std::runtime_error("AST JSON cannot validate a null external symbol");
  }
  SgNode *basis = symbol->get_symbol_basis();
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(basis)) {
    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(decl->get_parent());
    SgScopeStatement *scope = decl->get_scope();
    if (isAstJsonExternalFunction(decl)) {
      auto *canonical = scope != nullptr
                            ? scope->find_symbol_from_declaration(decl)
                            : nullptr;
      if (decl->get_parent() == nullptr ||
          decl->get_firstNondefiningDeclaration() != decl ||
          canonical == nullptr || canonical->get_symbol_basis() != decl ||
          canonical->variantT() != V_SgFunctionSymbol ||
          canonical->get_parent() != scope->get_symbol_table() ||
          !scope->get_symbol_table()->exists(canonical))
        throw std::runtime_error(
            "AST JSON external function has no exact canonical symbol owner");
      if (auxiliary != nullptr &&
          (auxiliary->get_parent() != scope ||
           scope->get_auxiliary_declarations() != auxiliary ||
           std::count(auxiliary->get_declarations().begin(),
                      auxiliary->get_declarations().end(), decl) != 1))
        throw std::runtime_error(
            "AST JSON external function has conflicting auxiliary ownership");
    }
  }
  if (SgModuleStatement *module = isSgModuleStatement(basis)) {
    if (isAstJsonExternalModule(module) &&
        (module->get_scope() != nullptr || module->get_parent() != nullptr)) {
      throw std::runtime_error(
          "AST JSON external module shell is not detached");
    }
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(basis)) {
    if (!isAstJsonExternalClassDeclaration(decl)) {
      return;
    }
    SgScopeStatement *scope = decl->get_scope();
    if (scope == nullptr || decl->get_parent() != scope) {
      throw std::runtime_error(
          "AST JSON external class shell does not belong to its isolated "
          "semantic scope");
    }
    SgGlobal *external_global = isSgGlobal(scope);
    if (SgClassDefinition *module_definition = isSgClassDefinition(scope)) {
      SgModuleStatement *module =
          isSgModuleStatement(module_definition->get_declaration());
      if (module == nullptr || !isAstJsonExternalModule(module) ||
          module_definition->get_parent() != module ||
          module->get_scope() != module->get_parent()) {
        throw std::runtime_error(
            "AST JSON external class shell has an invalid external module "
            "owner");
      }
      external_global = isSgGlobal(module->get_parent());
    }
    if (external_global == nullptr ||
        external_global->get_parent() != nullptr) {
      throw std::runtime_error(
          "AST JSON external class shell has no isolated semantic root");
    }
  }
}

bool symbolTablePreferencesMatch(
    const std::vector<const ExpectedSymbolTablePreference *> &entries) {
  for (const ExpectedSymbolTablePreference *entry : entries) {
    ROSE_ASSERT(entry != nullptr);
    ROSE_ASSERT(entry->table != nullptr);
    ROSE_ASSERT(entry->symbol != nullptr);
    if (symbolIsLookupPreferred(entry->table, entry->entry_name,
                                entry->symbol) != entry->lookup_preferred) {
      return false;
    }
  }
  return true;
}

void removeSymbolTablePreferenceGroup(
    const std::vector<const ExpectedSymbolTablePreference *> &entries) {
  for (const ExpectedSymbolTablePreference *entry : entries) {
    ROSE_ASSERT(entry != nullptr);
    ROSE_ASSERT(entry->table != nullptr);
    ROSE_ASSERT(entry->symbol != nullptr);
    if (!entry->table->exists(entry->entry_name, entry->symbol)) {
      std::ostringstream message;
      message << "AST JSON cannot reorder missing symbol-table entry";
      message << " entry=" << entry->entry_name.getString();
      message << " symbol=" << entry->symbol->class_name();
      message << " symbol_name=" << entry->symbol->get_name().getString();
      throw std::runtime_error(message.str());
    }
    entry->table->remove(entry->symbol);
  }
}

void insertSymbolTablePreferenceGroup(
    const std::vector<const ExpectedSymbolTablePreference *> &entries) {
  for (const ExpectedSymbolTablePreference *entry : entries) {
    ROSE_ASSERT(entry != nullptr);
    ROSE_ASSERT(entry->table != nullptr);
    ROSE_ASSERT(entry->symbol != nullptr);
    entry->table->insert(entry->entry_name, entry->symbol);
  }
}

SymbolTablePreferenceGroups groupSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries,
    std::optional<bool> case_insensitive) {
  SymbolTablePreferenceGroups groups;
  rose_hash_multimap name_semantics;
  if (case_insensitive)
    name_semantics.set_case_insensitive_semantics(*case_insensitive);
  using NameGroups = std::unordered_map<SgName, size_t, hash_Name, eqstr>;
  std::map<std::pair<SgSymbolTable *, VariantT>, NameGroups> indexes;
  for (const ExpectedSymbolTablePreference &entry : entries) {
    const VariantT category = entry.category;
    if (category == V_SgSymbol) {
      if (entry.lookup_preferred)
        throw std::runtime_error(
            "AST JSON unsupported symbol lookup preference for " +
            entry.entry_name.getString());
      continue;
    }
    ROSE_ASSERT(entry.table != nullptr || case_insensitive.has_value());
    if (entry.table != nullptr)
      ROSE_ASSERT(entry.table->get_table() != nullptr);
    const auto *semantics =
        case_insensitive ? &name_semantics : entry.table->get_table();
    auto names = indexes.try_emplace(std::make_pair(entry.table, category), 0,
                                     hash_Name(semantics), eqstr(semantics));
    auto group =
        names.first->second.try_emplace(entry.entry_name, groups.size());
    if (group.second)
      groups.emplace_back();
    groups[group.first->second].push_back(&entry);
  }
  return groups;
}

void validateSymbolTablePreferenceGroups(
    const std::vector<ExpectedSymbolTablePreference> &entries,
    std::optional<bool> case_insensitive) {
  for (const auto &group :
       groupSymbolTablePreferences(entries, case_insensitive)) {
    const auto preferred_count =
        std::count_if(group.begin(), group.end(),
                      [](const ExpectedSymbolTablePreference *entry) {
                        return entry->lookup_preferred;
                      });
    if (preferred_count != 1)
      throw std::runtime_error("AST JSON symbol preference group " +
                               group.front()->entry_name.getString() + "/" +
                               std::to_string(group.front()->category) +
                               " does not identify one exact preferred entry");
  }
}

void enforceSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries) {
  for (const auto &group : groupSymbolTablePreferences(entries)) {
    if (group.size() < 2 || symbolTablePreferencesMatch(group)) {
      continue;
    }

    auto ordered = group;
    const auto preferred =
        std::stable_partition(ordered.begin(), ordered.end(),
                              [](const ExpectedSymbolTablePreference *entry) {
                                return !entry->lookup_preferred;
                              });
    const auto preferred_count = std::distance(preferred, ordered.end());
    if (preferred_count != 1) {
      std::ostringstream message;
      message << "AST JSON cannot enforce symbol lookup preference";
      message << " entry=" << group.front()->entry_name;
      message << " category="
              << symbolLookupPreferenceCategory(group.front()->symbol);
      message << " preferred_count=" << preferred_count;
      throw std::runtime_error(message.str());
    }

    removeSymbolTablePreferenceGroup(group);
    insertSymbolTablePreferenceGroup(ordered);
    if (symbolTablePreferencesMatch(group)) {
      continue;
    }

    std::ostringstream message;
    message << "AST JSON failed to enforce symbol lookup preference";
    message << " entry=" << group.front()->entry_name;
    message << " category="
            << symbolLookupPreferenceCategory(group.front()->symbol);
    throw std::runtime_error(message.str());
  }
}

void validateSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries) {
  for (const ExpectedSymbolTablePreference &entry : entries) {
    ROSE_ASSERT(entry.table != nullptr);
    ROSE_ASSERT(entry.symbol != nullptr);
    const bool actual =
        symbolIsLookupPreferred(entry.table, entry.entry_name, entry.symbol);
    if (actual != entry.lookup_preferred) {
      std::ostringstream message;
      message << "AST JSON failed to restore symbol lookup preference";
      message << " entry=" << entry.entry_name.getString();
      message << " symbol=" << entry.symbol->class_name();
      message << " symbol_name=" << entry.symbol->get_name().getString();
      message << " expected="
              << (entry.lookup_preferred ? "preferred" : "non-preferred");
      message << " actual=" << (actual ? "preferred" : "non-preferred");
      throw std::runtime_error(message.str());
    }
  }
}

SerializedSymbolTableEntry::SerializedSymbolTableEntry(SgSymbolTable *owner,
                                                       const JsonValue &entry)
    : SerializedSymbolTableEntry(entry) {
  ASSERT_not_null(owner);
  table = owner;
}

SerializedSymbolTableEntry::SerializedSymbolTableEntry(const JsonValue &entry)
    : symbol_kind(
          symbolVariantFromJsonKind(entry.requiredString("symbol_kind"))),
      lookup_preferred(entry.requiredBool("lookup_preferred")),
      basis_id(symbol_kind == V_SgAliasSymbol || symbol_kind == V_SgRenameSymbol
                   ? std::nullopt
                   : std::optional<uint64_t>(
                         entry.at("symbol").requiredNonnegativeNodeId(
                             "symbol_declaration"))),
      entry_name(entry.requiredName("entry_name")) {
  if (const JsonValue *symbol = entry.find("symbol")) {
    if (symbolVariantFromJsonKind(symbol->requiredString("symbol_kind")) !=
        symbol_kind)
      throw std::runtime_error(
          "AST JSON symbol table entry kind disagrees with its symbol");
  }
  if (symbol_kind == V_SgLabelSymbol)
    label_fields = labelSymbolFieldsFromJson(entry.at("symbol"));
  if (symbol_kind == V_SgNamelistSymbol) {
    const auto &groups = entry.at("namelist_declarations");
    if (groups.kind != JsonValue::Kind::Array || groups.array.empty())
      throw std::runtime_error("AST JSON NAMELIST symbol has no declarations");
    std::set<uint64_t> seen;
    for (const auto &group : groups.array) {
      const auto id = requiredPositiveId(group, "NAMELIST declaration");
      if (!seen.insert(id).second)
        throw std::runtime_error(
            "AST JSON NAMELIST has duplicate declarations");
      namelist_declarations.push_back(id);
    }
    if (namelist_declarations.front() != basis_id.value())
      throw std::runtime_error(
          "AST JSON NAMELIST symbol has a different first declaration");
  }
  if (symbol_kind == V_SgAliasSymbol || symbol_kind == V_SgRenameSymbol) {
    const JsonValue &causes = entry.at("alias_causal_nodes");
    if (causes.kind != JsonValue::Kind::Array ||
        (symbol_kind == V_SgAliasSymbol && causes.array.empty()))
      throw std::runtime_error(
          "AST JSON alias_causal_nodes field is not a non-empty array");
    for (const JsonValue &cause : causes.array) {
      const int64_t id = cause.asInt();
      if (id <= 0)
        throw std::runtime_error("AST JSON " +
                                 entry.requiredString("symbol_kind") +
                                 " has an invalid causal node ID");
      causal_ids.push_back(static_cast<uint64_t>(id));
    }
    visibility.emplace(entry);
  }
}

VisibilitySymbolFields::VisibilitySymbolFields(const JsonValue &entry,
                                               SgSymbolTable *receiver_table)
    : kind(symbolVariantFromJsonKind(entry.requiredString("symbol_kind"))),
      target_binding(kind == V_SgAliasSymbol
                         ? entry.find("alias_target_binding")
                         : entry.find("original_symbol_binding")),
      target(target_binding != nullptr
                 ? &target_binding->at("symbol")
                 : &entry.at(kind == V_SgAliasSymbol ? "alias_target"
                                                     : "original_symbol")),
      target_kind(
          symbolVariantFromJsonKind(target->requiredString("symbol_kind"))),
      basis_id(target->requiredNonnegativeNodeId("symbol_declaration")),
      new_name(entry.requiredName(kind == V_SgAliasSymbol ? "alias_new_name"
                                                          : "rename_new_name")),
      is_renamed(kind == V_SgAliasSymbol
                     ? entry.requiredBool("alias_is_renamed")
                     : false),
      local_table(receiver_table) {
  ROSE_ASSERT(kind == V_SgAliasSymbol || kind == V_SgRenameSymbol);
  const bool visibility_target =
      target_kind == V_SgAliasSymbol || target_kind == V_SgRenameSymbol;
  if (visibility_target != (target_binding != nullptr) ||
      (target_binding != nullptr &&
       entry.find(kind == V_SgAliasSymbol ? "alias_target"
                                          : "original_symbol") != nullptr))
    throw std::runtime_error(
        "AST JSON alias target requires one exact binding representation");
  if (target_binding != nullptr) {
    if (const auto *local = target_binding->find("binding_local")) {
      if (!local->asBool() || local_table == nullptr ||
          target_binding->find("binding_scope") != nullptr)
        throw std::runtime_error(
            "AST JSON alias/rename local binding has no exact receiver");
    } else
      requiredPositiveId(target_binding->at("binding_scope"),
                         "alias target scope");
    if (target_binding->requiredName("binding_name").is_null())
      throw std::runtime_error(
          "AST JSON alias target has an empty binding name");
  }
  const std::string name = target->requiredString("symbol_name");
  const JsonValue &serializedSymbol = entry.at("symbol");
  if (basis_id !=
          serializedSymbol.requiredNonnegativeNodeId("symbol_declaration") ||
      (kind == V_SgRenameSymbol &&
       (serializedSymbol.requiredString("symbol_kind") != "SgRenameSymbol" ||
        serializedSymbol.requiredString("symbol_name") != name))) {
    throw std::runtime_error("AST JSON " + std::string(getVariantName(kind)) +
                             " basis diverges from its original symbol");
  }
  if (kind == V_SgRenameSymbol && basis_id == 0) {
    const JsonValue *external = serializedSymbol.find("external_function");
    const JsonValue *originalExternal = target->find("external_function");
    if (external == nullptr || originalExternal == nullptr ||
        !external->requiredBool("present") ||
        !external->exactlyEquals(*originalExternal)) {
      throw std::runtime_error(
          "AST JSON external SgRenameSymbol payload diverges from its "
          "original function");
    }
  }
}

static SgSymbol *visibilitySymbolTarget(const VisibilitySymbolFields &fields,
                                        const NodeMap &nodes) {
  SgSymbol *target;
  if (fields.target_binding != nullptr) {
    target = exactBoundSymbolFromJson(*fields.target_binding, nodes,
                                      fields.local_table);
  } else if (fields.basis_id == 0) {
    ROSE_ASSERT(fields.construct_external_target);
    target = fields.construct_external_target();
  } else {
    target = symbolFromJson(*fields.target, nodes);
  }
  if (target == nullptr || target->get_symbol_basis() == nullptr)
    throw std::runtime_error("AST JSON visibility symbol has no exact target");
  validateExternalSymbolBasisOwnership(target);
  return target;
}

static std::unique_ptr<SgSymbol>
constructVisibilitySymbol(const VisibilitySymbolFields &fields,
                          SgSymbol *target, SgNodePtrList causes) {
  if (fields.kind == V_SgAliasSymbol) {
    auto alias = std::make_unique<SgAliasSymbol>(target, fields.is_renamed,
                                                 fields.new_name);
    alias->get_causal_nodes().swap(causes);
    return alias;
  }
  ROSE_ASSERT(fields.kind == V_SgRenameSymbol);
  auto *basis = isSgFunctionDeclaration(target->get_symbol_basis());
  if (basis == nullptr)
    throw std::runtime_error(
        "AST JSON SgRenameSymbol has no exact function basis");
  auto rename =
      std::make_unique<SgRenameSymbol>(basis, target, fields.new_name);
  rename->get_causal_nodes().swap(causes);
  return rename;
}

std::unique_ptr<SgSymbol>
constructVisibilitySymbol(const VisibilitySymbolFields &fields,
                          const NodeMap &nodes, SgNodePtrList causes) {
  return constructVisibilitySymbol(
      fields, visibilitySymbolTarget(fields, nodes), std::move(causes));
}

static SgSymbol *
existingOrdinarySymbolTableEntry(const SerializedSymbolTableEntry &record,
                                 SgNode *basis) {
  SgSymbolTable *table = record.table;
  const SgName &entry_name = record.entry_name;
  const VariantT expected_variant = record.symbol_kind;
  SgSymbol *symbol = nullptr;
  for (SgSymbol *candidate : table->find_symbols_with_exact_basis(basis)) {
    if (candidate->variantT() == expected_variant &&
        table->stored_name_of(candidate) == entry_name) {
      if (symbol != nullptr) {
        throw std::runtime_error(
            "AST JSON scope symbol table holds duplicate entries for " +
            entry_name.getString());
      }
      symbol = candidate;
    }
  }
  if (expected_variant == V_SgNamespaceSymbol)
    validateNamespaceSymbolKey(entry_name, basis);
  if (auto *label = isSgLabelSymbol(symbol))
    validateLabelSymbolFields(label, record.label_fields.value());
  if (symbol != nullptr)
    validateExternalSymbolBasisOwnership(symbol);
  return symbol;
}

std::function<SgSymbol *()>
SerializedSymbolTableEntry::prepare(const NodeMap &nodes) const {
  if (symbol_kind != V_SgNamelistSymbol)
    return prepareSymbolForKindAndBasis(
        symbol_kind, nodeById(nodes, basis_id.value()), label_fields);
  SgNameGroupPtrList groups;
  for (auto id : namelist_declarations) {
    auto *group = nodeByIdAs<SgNameGroup>(nodes, id);
    auto *owner = isSgNamelistStatement(group->get_parent());
    if (owner == nullptr || owner->get_parent() != table->get_parent() ||
        !table->get_table()->key_eq()(entry_name,
                                      SgName(group->get_group_name())) ||
        !SgNameGroup::is_valid_group(group->get_group_name(),
                                     group->get_objects()))
      throw std::runtime_error(
          "AST JSON NAMELIST symbol has inconsistent declarations");
    for (auto *object : group->get_objects())
      if (object->get_parent() != group)
        throw std::runtime_error(
            "AST JSON NAMELIST symbol has unowned objects");
    groups.push_back(group);
  }
  return [groups] { return new SgNamelistSymbol(groups); };
}

static SgSymbol *
constructOrdinarySymbolTableEntry(const SerializedSymbolTableEntry &record,
                                  const NodeMap &nodes) {
  SgSymbolTable *table = record.table;
  const SgName &entry_name = record.entry_name;
  const VariantT expected_variant = record.symbol_kind;
  SgNode *basis = nodeById(nodes, record.basis_id.value());
  SgSymbol *symbol = existingOrdinarySymbolTableEntry(record, basis);
  if (auto *namelist = isSgNamelistSymbol(symbol)) {
    SgNameGroupPtrList groups;
    for (auto id : record.namelist_declarations)
      groups.push_back(nodeByIdAs<SgNameGroup>(nodes, id));
    if (namelist->get_declarations() != groups)
      throw std::runtime_error("AST JSON NAMELIST declaration family changed");
  }
  std::unique_ptr<SgSymbol> owned_symbol;
  if (symbol == nullptr) {
    auto construct = record.construct;
    if (!construct)
      construct = record.prepare(nodes);
    owned_symbol.reset(construct());
    symbol = owned_symbol.get();
  }
  if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
    std::ostringstream message;
    message << "AST JSON cannot reconstruct symbol table entry"
            << " kind=" << getVariantName(expected_variant)
            << " basis=" << basis->sage_class_name();
    throw std::runtime_error(message.str());
  }
  if (owned_symbol != nullptr) {
    validateExternalSymbolBasisOwnership(symbol);
    table->insert(entry_name, symbol);
    owned_symbol.release();
  }
  return symbol;
}

static bool
deferredSymbolDependenciesReady(const SerializedSymbolTableEntry &record,
                                const NodeMap &nodes) {
  if (!record.visibility) {
    const uint64_t basis_id = record.basis_id.value();
    if (basis_id == 0)
      throw std::runtime_error(
          "AST JSON pending ordinary symbol has no declaration basis");
    if (record.symbol_kind == V_SgNamelistSymbol) {
      for (auto id : record.namelist_declarations) {
        const auto found = nodes.find(id);
        if (found == nodes.end())
          return false;
        auto *group = isSgNameGroup(found->second);
        if (group == nullptr)
          throw std::runtime_error(
              "AST JSON NAMELIST symbol has a non-group declaration");
        auto *owner = isSgNamelistStatement(group->get_parent());
        if (owner == nullptr || owner->get_parent() == nullptr)
          return false;
      }
    }
    return nodes.count(basis_id) != 0;
  }
  bool ready = true;
  for (uint64_t id : record.causal_ids) {
    if (nodes.count(id) == 0)
      ready = false;
  }
  if (record.visibility->target_binding != nullptr)
    return ready && exactSymbolBindingDependenciesReady(
                        *record.visibility->target_binding, nodes);
  const uint64_t basis_id = record.visibility->basis_id;
  if (basis_id == 0)
    return ready;
  const auto basis = nodes.find(basis_id);
  if (basis == nodes.end())
    return false;
  SgScopeStatement *scope = nullptr;
  if (auto *name = isSgInitializedName(basis->second))
    scope = name->get_scope();
  else if (auto *declaration = isSgDeclarationStatement(basis->second))
    scope = declaration->get_scope();
  else if (record.visibility->target_kind == V_SgLabelSymbol)
    scope = labelSymbolScopeForBasis(basis->second);
  else if (auto *group = isSgNameGroup(basis->second)) {
    auto *owner = isSgNamelistStatement(group->get_parent());
    if (owner != nullptr && owner->get_parent() != nullptr)
      scope = owner->get_scope();
  }
  if (!ready || scope == nullptr)
    return false;
  ROSE_ASSERT(scope->get_symbol_table() != nullptr);
  return !scope->get_symbol_table()
              ->find_symbols_with_exact_basis(basis->second)
              .empty();
}

bool constructPendingSerializedSymbols(
    SerializedSymbolTableConstruction &construction, const NodeMap &nodes,
    bool require_complete) {
  bool constructed = false;
  bool progress;
  do {
    progress = false;
    for (size_t index = 0; index < construction.pending.size();) {
      const auto &entry = construction.pending[index];
      ROSE_ASSERT(entry.table != nullptr);
      if (!deferredSymbolDependenciesReady(entry, nodes)) {
        ++index;
        continue;
      }
      SgSymbol *symbol = nullptr;
      if (entry.visibility) {
        SgNodePtrList causes;
        for (uint64_t id : entry.causal_ids)
          causes.push_back(nodeById(nodes, id));
        const auto &fields = *entry.visibility;
        auto *target = visibilitySymbolTarget(fields, nodes);
        symbol =
            existingOrdinarySymbolTableEntry(entry, target->get_symbol_basis());
        if (symbol != nullptr) {
          auto *alias = isSgAliasSymbol(symbol);
          auto *rename = isSgRenameSymbol(symbol);
          const bool matches =
              alias != nullptr
                  ? alias->get_alias() == target &&
                        alias->get_isRenamed() == fields.is_renamed &&
                        alias->get_new_name() == fields.new_name &&
                        alias->get_causal_nodes() == causes
                  : rename != nullptr &&
                        rename->get_original_symbol() == target &&
                        rename->get_new_name() == fields.new_name &&
                        rename->get_causal_nodes() == causes;
          if (!matches)
            throw std::runtime_error("AST JSON visibility symbol differs from "
                                     "its existing exact binding");
        } else {
          auto owned_symbol =
              constructVisibilitySymbol(fields, target, std::move(causes));
          entry.table->insert(entry.entry_name, owned_symbol.get());
          symbol = owned_symbol.release();
        }
      } else {
        symbol = constructOrdinarySymbolTableEntry(entry, nodes);
      }
      construction.preferences.push_back(
          {entry.table, entry.entry_name, symbol, entry.lookup_preferred});
      construction.preferences_finalized = false;
      construction.pending.erase(construction.pending.begin() + index);
      progress = constructed = true;
    }
  } while (progress);
  if (require_complete && !construction.pending.empty())
    throw std::runtime_error("AST JSON symbol entries have missing or cyclic "
                             "construction dependencies");
  if (construction.pending.empty() && !construction.preferences_finalized) {
    enforceSymbolTablePreferences(construction.preferences);
    validateSymbolTablePreferences(construction.preferences);
    construction.preferences_finalized = true;
  }
  return constructed;
}

std::vector<SerializedSymbolTableEntry>
symbolTableEntriesFromRecord(const NodeRecord &record) {
  static const VariantVector scopes(V_SgScopeStatement);
  if (!kindInVariants(record.kind, scopes))
    throw std::runtime_error("AST JSON symbol table owner is not a scope: " +
                             record.kind);
  const JsonValue &entries = record.properties.at("symbol_table");
  if (entries.kind != JsonValue::Kind::Array)
    throw std::runtime_error(
        "AST JSON symbol_table field is not an array for " + record.kind);
  std::vector<SerializedSymbolTableEntry> decoded;
  decoded.reserve(entries.array.size());
  for (const JsonValue &entry : entries.array)
    decoded.emplace_back(entry);
  std::vector<ExpectedSymbolTablePreference> preferences;
  preferences.reserve(decoded.size());
  for (const auto &entry : decoded)
    preferences.emplace_back(nullptr, entry.entry_name, entry.symbol_kind,
                             entry.lookup_preferred);
  validateSymbolTablePreferenceGroups(
      preferences, record.properties.requiredBool("case_insensitive"));
  return decoded;
}

void restoreSerializedSymbolTables(
    const AstFileRecord &ast, const NodeMap &nodes,
    const std::unordered_set<uint64_t> &scope_ids,
    SerializedSymbolTableConstruction *construction) {
  SerializedSymbolTableRecords records;
  for (const NodeRecord &record : ast.nodes)
    if (scope_ids.count(record.id) != 0)
      records.emplace(record.id, symbolTableEntriesFromRecord(record));
  restoreSerializedSymbolTables(ast, nodes, scope_ids, records, construction);
}

void restoreSerializedSymbolTables(
    const AstFileRecord &ast, const NodeMap &nodes,
    const std::unordered_set<uint64_t> &scope_ids,
    const SerializedSymbolTableRecords &records,
    SerializedSymbolTableConstruction *construction) {
  SerializedSymbolTableConstruction local_construction;
  auto &state = construction != nullptr ? *construction : local_construction;
  auto &deferred_entries = state.pending;
  auto &expected_preferences = state.preferences;
  state.preferences_finalized = false;
  std::unordered_set<SgScopeStatement *> restored_scopes;
  struct ScopeSymbols {
    SgSymbolTable *table;
    std::vector<SerializedSymbolTableEntry> entries;
  };
  std::vector<ScopeSymbols> prepared_scopes;

  for (const NodeRecord &record : ast.nodes) {
    if (scope_ids.count(record.id) == 0)
      continue;
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      throw std::runtime_error("AST JSON scope was not built before its "
                               "symbol-table reconstruction: " +
                               record.kind);
    }
    SgScopeStatement *scope = isSgScopeStatement(restored_node->second);
    if (scope == nullptr) {
      throw std::runtime_error("AST JSON symbol table owner is not a scope: " +
                               record.kind);
    }
    restored_scopes.insert(scope);

    SgSymbolTable *table = scope->get_symbol_table();
    ROSE_ASSERT(table != nullptr && table->get_parent() == scope);
    if (scope->isCaseInsensitive() !=
        record.properties.requiredBool("case_insensitive")) {
      throw std::runtime_error(
          "AST JSON scope case sensitivity differs from construction");
    }
    auto decoded_entries = records.at(record.id);
    for (std::size_t index = 0; index < decoded_entries.size(); ++index) {
      auto &decoded = decoded_entries[index];
      ROSE_ASSERT(decoded.table == nullptr);
      decoded.table = table;
      if (decoded.visibility) {
        for (uint64_t id : decoded.causal_ids)
          if (nodes.count(id) == 0)
            ast.node(id);
        const uint64_t target_id = decoded.visibility->basis_id;
        if (decoded.visibility->target_binding != nullptr) {
          ast.node(
              decoded.visibility->target_binding->requiredNonnegativeNodeId(
                  "binding_scope"));
        } else if (target_id == 0)
          decoded.visibility->construct_external_target =
              prepareExternalSymbolFromJson(*decoded.visibility->target, nodes,
                                            nullptr);
        else if (nodes.count(target_id) == 0)
          ast.node(target_id);
        continue;
      }
      if (*decoded.basis_id == 0) {
        decoded.construct = prepareExternalSymbolFromJson(
            record.properties.at("symbol_table").array.at(index).at("symbol"),
            nodes, table);
        continue;
      }
      auto basis = nodes.find(*decoded.basis_id);
      if (basis == nodes.end()) {
        const NodeRecord &basis_record = ast.node(*decoded.basis_id);
        if (basis_record.kind != "SgInitializedName" &&
            basis_record.kind != "SgNameGroup" &&
            basis_record.kind != "SgEnumDeclaration" &&
            basis_record.kind != "SgNamespaceAliasDeclarationStatement" &&
            basis_record.kind != "SgTypedefDeclaration" &&
            basis_record.kind != "SgTemplateTypedefDeclaration" &&
            basis_record.kind != "SgNonrealDecl" &&
            basis_record.kind != "SgTemplateInstantiationTypedefDeclaration" &&
            !(decoded.symbol_kind == V_SgLabelSymbol &&
              requiresDelayedRebuild(basis_record))) {
          throw std::runtime_error(
              "AST JSON symbol table entry references unbuilt node " +
              std::to_string(*decoded.basis_id));
        }
        continue;
      }
      if (decoded.symbol_kind == V_SgNamelistSymbol)
        continue;
      decoded.construct = decoded.prepare(nodes);
      existingOrdinarySymbolTableEntry(decoded, basis->second);
    }
    prepared_scopes.push_back({table, std::move(decoded_entries)});
  }
  for (const ScopeSymbols &prepared : prepared_scopes) {
    SgSymbolTable *table = prepared.table;
    const auto &decoded_entries = prepared.entries;
    for (SgNode *existing_node : table->get_symbols()) {
      SgSymbol *existing = isSgSymbol(existing_node);
      if (existing == nullptr) {
        throw std::runtime_error(
            "AST JSON scope symbol table holds a non-symbol entry for " +
            table->get_parent()->class_name());
      }
      bool described = false;
      for (const auto &candidate : decoded_entries) {
        if (candidate.entry_name != table->stored_name_of(existing) ||
            candidate.symbol_kind != existing->variantT()) {
          continue;
        }
        const uint64_t basis_id = candidate.basis_id
                                      ? *candidate.basis_id
                                      : candidate.visibility->basis_id;
        const auto basis = nodes.find(basis_id);
        const bool basis_matches =
            basis_id > 0 && basis != nodes.end() &&
            basis->second == existing->get_symbol_basis();
        if (basis_matches) {
          described = true;
          break;
        }
      }
      if (!described) {
        throw std::runtime_error(
            "AST JSON scope symbol table holds an entry not described by "
            "its serialized entries for " +
            table->get_parent()->class_name());
      }
    }
  }
  for (const ScopeSymbols &prepared : prepared_scopes) {
    SgSymbolTable *table = prepared.table;
    for (const auto &decoded : prepared.entries) {
      const SgName &entry_name = decoded.entry_name;
      const bool lookup_preferred = decoded.lookup_preferred;
      if (decoded.visibility) {
        deferred_entries.push_back(decoded);
        continue;
      }

      const uint64_t basis_id = decoded.basis_id.value();
      if (basis_id == 0) {
        SgSymbol *raw_symbol = decoded.construct();
        if (raw_symbol != nullptr && raw_symbol->get_parent() != nullptr) {
          throw std::runtime_error(
              "AST JSON external symbol table entry is already owned");
        }
        std::unique_ptr<SgSymbol> symbol(raw_symbol);
        if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON failed to reconstruct external symbol table entry");
        }
        table->insert(entry_name, symbol.get());
        SgSymbol *published = symbol.release();
        validateExternalSymbolBasisOwnership(published);
        expected_preferences.push_back(
            {table, entry_name, published, lookup_preferred});
        continue;
      }
      if (nodes.count(basis_id) == 0) {
        deferred_entries.push_back(decoded);
        continue;
      }
      SgSymbol *symbol = constructOrdinarySymbolTableEntry(decoded, nodes);
      expected_preferences.push_back(
          {table, entry_name, symbol, lookup_preferred});
    }
  }

  struct AnonymousFortranProgramUnitSymbol {
    SgScopeStatement *scope = nullptr;
    SgName internal_key;
    SgFunctionSymbol *symbol = nullptr;
  };
  std::unordered_map<SgFunctionDeclaration *, AnonymousFortranProgramUnitSymbol>
      anonymous_fortran_program_unit_symbols;
  for (const auto &entry : nodes) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(entry.second);
    if (declaration == nullptr ||
        !SageInterface::isFortranProgramUnitWithoutSourceName(declaration)) {
      continue;
    }
    SgScopeStatement *scope = declaration->get_scope();
    if (scope == nullptr || scope->get_symbol_table() == nullptr) {
      throw std::runtime_error(
          "AST JSON anonymous Fortran program unit has no symbol scope");
    }
    if (restored_scopes.count(scope) == 0)
      continue;
    SgFunctionDeclaration *basis =
        isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
    if (basis == nullptr) {
      basis = declaration;
    }
    const SgName internalKey =
        SageInterface::getFortranProgramUnitSymbolTableKey(declaration);
    auto reconstructed = anonymous_fortran_program_unit_symbols.find(basis);
    if (reconstructed != anonymous_fortran_program_unit_symbols.end()) {
      const AnonymousFortranProgramUnitSymbol &record = reconstructed->second;
      if (record.scope != scope || record.internal_key != internalKey ||
          record.symbol == nullptr ||
          record.symbol->get_declaration() != basis ||
          scope->get_symbol_table()->find_function(internalKey) !=
              record.symbol ||
          scope->find_symbol_from_declaration(basis) != record.symbol) {
        throw std::runtime_error(
            "AST JSON anonymous Fortran program-unit declaration chain does "
            "not resolve to one exact reconstructed symbol");
      }
      continue;
    }
    if (scope->get_symbol_table()->find_function(internalKey) != nullptr ||
        scope->find_symbol_from_declaration(basis) != nullptr) {
      throw std::runtime_error(
          "AST JSON anonymous Fortran program-unit internal symbol already "
          "exists before reconstruction");
    }
    auto owned_symbol = std::make_unique<SgFunctionSymbol>(basis);
    SgFunctionSymbol *symbol = owned_symbol.get();
    scope->get_symbol_table()->insert(internalKey, symbol);
    owned_symbol.release();
    if (scope->get_symbol_table()->find_function(internalKey) != symbol ||
        scope->find_symbol_from_declaration(basis) != symbol ||
        symbol->get_declaration() != basis) {
      throw std::runtime_error(
          "AST JSON anonymous Fortran program-unit symbol was not published "
          "with its exact declaration-chain identity");
    }
    anonymous_fortran_program_unit_symbols.emplace(
        basis, AnonymousFortranProgramUnitSymbol{scope, internalKey, symbol});
  }

  constructPendingSerializedSymbols(state, nodes, construction == nullptr);
}

SgClassSymbol *classSymbolForDeclaration(SgClassDeclaration *decl) {
  SgScopeStatement *scope = decl != nullptr ? decl->get_scope() : nullptr;
  SgClassSymbol *symbol =
      scope != nullptr
          ? isSgClassSymbol(scope->find_symbol_from_declaration(decl))
          : nullptr;
  if (symbol == nullptr || symbol->get_declaration() != decl) {
    throw std::runtime_error(
        "AST JSON class symbol reference has no exact restored "
        "symbol-table binding");
  }
  return symbol;
}

SgNonrealSymbol *nonrealSymbolForDeclaration(SgNonrealDecl *decl) {
  SgScopeStatement *scope = decl != nullptr ? decl->get_scope() : nullptr;
  SgNonrealSymbol *symbol =
      scope != nullptr
          ? isSgNonrealSymbol(scope->find_symbol_from_declaration(decl))
          : nullptr;
  if (symbol == nullptr || symbol->get_declaration() != decl) {
    throw std::runtime_error(
        "AST JSON nonreal symbol reference has no exact restored "
        "symbol-table binding");
  }
  return symbol;
}

SgTemplateArgumentPtrList templateArgumentListFromJson(const JsonValue &json,
                                                       const NodeMap &nodes,
                                                       SgNode *parent) {
  SgTemplateArgumentPtrList result;
  if (json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON template argument list is not an array");
  }
  if (parent == nullptr) {
    throw std::runtime_error("AST JSON template argument list has no owner");
  }
  std::unordered_set<SgTemplateArgument *> seen;
  for (const JsonValue &arg_json : json.array) {
    const int64_t raw_id = arg_json.asInt();
    if (raw_id <= 0) {
      throw std::runtime_error(
          "AST JSON template argument ID must be positive");
    }
    const uint64_t id = static_cast<uint64_t>(raw_id);
    SgTemplateArgument *argument = nodeByIdAs<SgTemplateArgument>(nodes, id);
    if (!seen.insert(argument).second) {
      throw std::runtime_error(
          "AST JSON template argument list repeats a node");
    }
    if (argument->get_parent() != nullptr && argument->get_parent() != parent) {
      throw std::runtime_error(
          "AST JSON template argument has a different owner");
    }
    result.push_back(argument);
  }
  for (SgTemplateArgument *argument : result)
    argument->set_parent(parent);
  return result;
}

} // namespace AstJson
} // namespace Rose
