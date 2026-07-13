#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

void restoreLabelSymbolFields(SgLabelSymbol *symbol, const JsonValue &json) {
  if (symbol == nullptr) {
    return;
  }
  symbol->set_numeric_label_value(
      json.requiredInt("label_numeric_label_value"));
  symbol->set_label_type(static_cast<SgLabelSymbol::label_type_enum>(
      json.requiredInt("label_type")));
  if (symbol->get_declaration() == nullptr &&
      symbol->get_numeric_label_value() <= 0 &&
      json.requiredString("symbol_name").empty()) {
    if (SgLabelStatement *label =
            isSgLabelStatement(symbol->get_fortran_statement())) {
      symbol->set_declaration(label);
      symbol->set_fortran_statement(nullptr);
    }
  }
}

SgScopeStatement *labelSymbolScopeForBasis(SgNode *basis) {
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
  }
  return nearestScope(basis);
}

SgLabelSymbol *existingLabelSymbolFromJson(const JsonValue &json,
                                           SgNode *basis) {
  SgScopeStatement *scope = labelSymbolScopeForBasis(basis);
  if (scope == nullptr) {
    return nullptr;
  }
  const std::string name = json.requiredString("symbol_name");
  if (name.empty()) {
    return nullptr;
  }
  return scope->lookup_label_symbol(SgName(name));
}

std::string inferredSymbolKindForBasis(SgNode *basis) {
  if (isSgInitializedName(basis) != nullptr) {
    return "SgVariableSymbol";
  }
  if (isSgMemberFunctionDeclaration(basis) != nullptr) {
    return "SgMemberFunctionSymbol";
  }
  if (isSgFunctionDeclaration(basis) != nullptr) {
    return "SgFunctionSymbol";
  }
  if (isSgClassDeclaration(basis) != nullptr) {
    return "SgClassSymbol";
  }
  if (isSgEnumDeclaration(basis) != nullptr) {
    return "SgEnumSymbol";
  }
  if (isSgTypedefDeclaration(basis) != nullptr) {
    return "SgTypedefSymbol";
  }
  if (isSgLabelStatement(basis) != nullptr) {
    return "SgLabelSymbol";
  }
  if (isSgNonrealDecl(basis) != nullptr) {
    return "SgNonrealSymbol";
  }
  if (isSgTemplateDeclaration(basis) != nullptr) {
    return "SgTemplateSymbol";
  }
  return "";
}

SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes) {
  const uint64_t declaration_id =
      static_cast<uint64_t>(json.requiredInt("symbol_declaration"));
  if (declaration_id == 0) {
    SgSymbol *external_symbol = createExternalSymbolFromJson(json, nodes);
    if (external_symbol != nullptr) {
      return external_symbol;
    }
    return nullptr;
  }

  SgNode *basis = nodeById(nodes, declaration_id);
  SgSymbol *symbol = nullptr;
  if (SgInitializedName *name = isSgInitializedName(basis)) {
    SgScopeStatement *scope = name->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(name)) != nullptr) {
      return symbol;
    }
  } else if (SgMemberFunctionDeclaration *decl =
                 isSgMemberFunctionDeclaration(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(decl)) != nullptr) {
      return symbol;
    }
  } else if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(decl)) != nullptr) {
      return symbol;
    }
  } else if (SgClassDeclaration *decl = isSgClassDeclaration(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(decl)) != nullptr) {
      return symbol;
    }
  } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(decl)) != nullptr) {
      return symbol;
    }
  } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(decl)) != nullptr) {
      return symbol;
    }
  } else if (SgDeclarationStatement *decl = isSgDeclarationStatement(basis)) {
    SgScopeStatement *scope = decl->get_scope();
    if (scope != nullptr &&
        (symbol = scope->find_symbol_from_declaration(decl)) != nullptr) {
      return symbol;
    }
  } else if (isSgLabelStatement(basis) != nullptr ||
             isSgStatement(basis) != nullptr) {
    if (SgLabelSymbol *label = existingLabelSymbolFromJson(json, basis)) {
      restoreLabelSymbolFields(label, json);
      return label;
    }
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

SgSymbol *exactBoundSymbolFromJson(const JsonValue &json,
                                   const NodeMap &nodes) {
  if (json.kind == JsonValue::Kind::Null) {
    return nullptr;
  }
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON exact symbol reference is neither null nor an object");
  }

  const JsonValue &symbol_json = json.at("symbol");
  const std::string expected_kind = symbol_json.requiredString("symbol_kind");
  const std::string expected_basis_name =
      symbol_json.requiredString("symbol_name");
  const uint64_t basis_id =
      static_cast<uint64_t>(symbol_json.requiredInt("symbol_declaration"));
  SgNode *expected_basis = basis_id != 0 ? nodeById(nodes, basis_id) : nullptr;

  const uint64_t scope_id =
      static_cast<uint64_t>(json.requiredInt("binding_scope"));
  if (scope_id == 0) {
    throw std::runtime_error(
        "AST JSON exact symbol reference has a null binding scope");
  }
  SgScopeStatement *scope = nodeByIdAs<SgScopeStatement>(nodes, scope_id);
  SgSymbolTable *table = scope->get_symbol_table();
  if (table == nullptr || table->get_table() == nullptr) {
    throw std::runtime_error(
        "AST JSON exact symbol binding scope has no restored symbol table");
  }

  const SgName binding_name(json.requiredString("binding_name"));
  SgSymbol *result = nullptr;
  size_t matches = 0;
  for (const auto &entry : *table->get_table()) {
    SgSymbol *candidate = entry.second;
    if (entry.first != binding_name || candidate == nullptr ||
        candidate->class_name() != expected_kind) {
      continue;
    }
    const SgNode *candidate_basis = symbolBasis(candidate);
    const bool basis_matches =
        expected_basis != nullptr
            ? candidate_basis == expected_basis
            : candidate_basis != nullptr &&
                  symbolName(candidate) == expected_basis_name;
    if (basis_matches) {
      result = candidate;
      ++matches;
    }
  }
  if (result == nullptr || matches != 1) {
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

SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis) {
  if (kind == "SgVariableSymbol") {
    return new SgVariableSymbol(isSgInitializedName(basis));
  }
  if (kind == "SgTemplateVariableSymbol") {
    return new SgTemplateVariableSymbol(isSgInitializedName(basis));
  }
  if (kind == "SgEnumFieldSymbol") {
    return new SgEnumFieldSymbol(isSgInitializedName(basis));
  }
  if (kind == "SgIntrinsicSymbol") {
    return new SgIntrinsicSymbol(isSgInitializedName(basis));
  }
  if (kind == "SgCommonSymbol") {
    return new SgCommonSymbol(isSgInitializedName(basis));
  }
  if (kind == "SgFunctionSymbol") {
    return new SgFunctionSymbol(isSgFunctionDeclaration(basis));
  }
  if (kind == "SgMemberFunctionSymbol") {
    return new SgMemberFunctionSymbol(isSgFunctionDeclaration(basis));
  }
  if (kind == "SgTemplateFunctionSymbol") {
    return new SgTemplateFunctionSymbol(isSgFunctionDeclaration(basis));
  }
  if (kind == "SgTemplateMemberFunctionSymbol") {
    return new SgTemplateMemberFunctionSymbol(isSgFunctionDeclaration(basis));
  }
  if (kind == "SgClassSymbol") {
    return new SgClassSymbol(isSgClassDeclaration(basis));
  }
  if (kind == "SgTemplateClassSymbol") {
    return new SgTemplateClassSymbol(isSgClassDeclaration(basis));
  }
  if (kind == "SgEnumSymbol") {
    return new SgEnumSymbol(isSgEnumDeclaration(basis));
  }
  if (kind == "SgTypedefSymbol") {
    return new SgTypedefSymbol(isSgTypedefDeclaration(basis));
  }
  if (kind == "SgTemplateTypedefSymbol") {
    return new SgTemplateTypedefSymbol(isSgTypedefDeclaration(basis));
  }
  if (kind == "SgLabelSymbol") {
    if (SgLabelStatement *label = isSgLabelStatement(basis)) {
      if (label->get_label().getString().empty()) {
        SgLabelSymbol *symbol =
            new SgLabelSymbol(static_cast<SgLabelStatement *>(nullptr));
        symbol->set_fortran_statement(label);
        return symbol;
      }
      return new SgLabelSymbol(label);
    }
    if (SgInitializedName *name = isSgInitializedName(basis)) {
      return new SgLabelSymbol(name);
    }
    if (SgStatement *stmt = isSgStatement(basis)) {
      SgLabelSymbol *symbol =
          new SgLabelSymbol(static_cast<SgLabelStatement *>(nullptr));
      symbol->set_fortran_statement(stmt);
      return symbol;
    }
    return nullptr;
  }
  if (kind == "SgNamespaceSymbol") {
    return new SgNamespaceSymbol(
        isSgNamespaceDeclarationStatement(basis) != nullptr
            ? isSgNamespaceDeclarationStatement(basis)->get_name()
            : SgName(""),
        isSgNamespaceDeclarationStatement(basis), nullptr, false);
  }
  if (kind == "SgModuleSymbol") {
    return new SgModuleSymbol(isSgModuleStatement(basis));
  }
  if (kind == "SgInterfaceSymbol") {
    return new SgInterfaceSymbol(isSgInterfaceStatement(basis));
  }
  if (kind == "SgNonrealSymbol") {
    return new SgNonrealSymbol(isSgNonrealDecl(basis));
  }
  if (kind == "SgTemplateSymbol") {
    return new SgTemplateSymbol(isSgTemplateDeclaration(basis));
  }
  return nullptr;
}

SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes) {
  const std::string kind = json.at("symbol_kind").asString();
  SgNode *basis = nullptr;
  if (const JsonValue *external = json.find("external_function")) {
    basis = externalFunctionFromJson(*external, nodes);
  } else if (const JsonValue *external = json.find("external_module")) {
    basis = externalModuleFromJson(*external);
  } else if (const JsonValue *external = json.find("external_class")) {
    basis = externalClassDeclarationFromJson(*external);
  }
  if (basis == nullptr) {
    return nullptr;
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(basis)) {
    SgFunctionDeclaration *canonical_declaration =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    SgScopeStatement *scope = canonical_declaration != nullptr
                                  ? canonical_declaration->get_scope()
                                  : nullptr;
    SgSymbol *canonical_symbol =
        scope != nullptr
            ? scope->find_symbol_from_declaration(canonical_declaration)
            : nullptr;
    if (canonical_symbol == nullptr || canonical_symbol->class_name() != kind ||
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
  SgSymbol *symbol = createSymbolForKindAndBasis(kind, basis);
  if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot reconstruct external symbol reference: " + kind);
  }
  return symbol;
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
    if (isAstJsonExternalFunction(decl) &&
        (auxiliary == nullptr || scope == nullptr ||
         auxiliary->get_parent() != scope ||
         scope->get_auxiliary_declarations() != auxiliary ||
         isSgGlobal(scope) == nullptr || scope->get_parent() != nullptr ||
         std::count(auxiliary->get_declarations().begin(),
                    auxiliary->get_declarations().end(), decl) != 1)) {
      throw std::runtime_error(
          "AST JSON external function shell has no isolated semantic root");
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

SgSymbol *resolveExistingSymbolFromJson(const JsonValue &json,
                                        const NodeMap &nodes) {
  const uint64_t declaration_id =
      static_cast<uint64_t>(json.at("symbol_declaration").asInt());
  if (declaration_id == 0) {
    SgSymbol *external_symbol = createExternalSymbolFromJson(json, nodes);
    if (external_symbol != nullptr) {
      return external_symbol;
    }
    throw std::runtime_error(
        "AST JSON symbol reference has no symbol_declaration");
  }

  SgNode *basis = nodeById(nodes, declaration_id);
  SgScopeStatement *scope = nullptr;
  if (SgInitializedName *name = isSgInitializedName(basis)) {
    scope = name->get_scope();
  } else if (SgDeclarationStatement *decl = isSgDeclarationStatement(basis)) {
    scope = decl->get_scope();
  } else if (SgLabelStatement *label = isSgLabelStatement(basis)) {
    scope = label->get_scope();
  }
  if (scope == nullptr) {
    throw std::runtime_error(
        "AST JSON symbol reference basis has no owning scope: " +
        std::string(basis->sage_class_name()));
  }

  SgSymbol *symbol = nullptr;
  if (SgInitializedName *name = isSgInitializedName(basis)) {
    symbol = scope->find_symbol_from_declaration(name);
  } else if (SgDeclarationStatement *decl = isSgDeclarationStatement(basis)) {
    symbol = scope->find_symbol_from_declaration(decl);
  } else if (SgLabelStatement *label = isSgLabelStatement(basis)) {
    symbol = scope->find_symbol_from_declaration(label);
  }
  if (symbol == nullptr) {
    throw std::runtime_error(
        "AST JSON failed to resolve serialized symbol reference: " +
        json.requiredString("symbol_name"));
  }
  return symbol;
}

SgFunctionSymbol *functionReferenceSemanticSymbolFromJson(
    const JsonValue &semantic_symbol,
    const JsonValue &source_visible_symbol_record,
    SgFunctionSymbol *source_visible_symbol, const NodeMap &nodes) {
  if (source_visible_symbol != nullptr) {
    const JsonValue *source_visible_symbol_json =
        source_visible_symbol_record.find("symbol");
    if (source_visible_symbol_json == nullptr) {
      throw std::runtime_error(
          "AST JSON Fortran source-visible function symbol has no symbol "
          "record");
    }
    if (source_visible_symbol_json->exactlyEquals(semantic_symbol)) {
      return source_visible_symbol;
    }
  }

  SgSymbol *resolved = resolveExistingSymbolFromJson(semantic_symbol, nodes);
  SgFunctionSymbol *function_symbol = isSgFunctionSymbol(resolved);
  if (function_symbol == nullptr ||
      function_symbol->class_name() !=
          semantic_symbol.requiredString("symbol_kind") ||
      function_symbol->get_name().getString() !=
          semantic_symbol.requiredString("symbol_name")) {
    throw std::runtime_error(
        "AST JSON SgFunctionRefExp semantic symbol did not resolve to its "
        "exact serialized kind and name");
  }
  return function_symbol;
}

struct DeferredSymbolTableEntry {
  SgSymbolTable *table = nullptr;
  SgName entry_name;
  const JsonValue *entry_json = nullptr;
};

struct ExpectedSymbolTablePreference {
  SgSymbolTable *table = nullptr;
  SgName entry_name;
  SgSymbol *symbol = nullptr;
  bool lookup_preferred = false;
};

std::string symbolLookupPreferenceCategory(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return "";
  }
  if (isSgVariableSymbol(symbol) != nullptr) {
    return "variable";
  }
  if (isSgClassSymbol(symbol) != nullptr) {
    return "class";
  }
  if (isSgEnumSymbol(symbol) != nullptr) {
    return "enum";
  }
  if (isSgEnumFieldSymbol(symbol) != nullptr) {
    return "enum_field";
  }
  if (isSgTypedefSymbol(symbol) != nullptr) {
    return "typedef";
  }
  if (isSgLabelSymbol(symbol) != nullptr) {
    return "label";
  }
  if (isSgNamespaceSymbol(symbol) != nullptr) {
    return "namespace";
  }
  if (isSgFunctionSymbol(symbol) != nullptr) {
    return "function";
  }
  return "";
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
    if (!entry->table->find(entry->entry_name, entry->symbol)) {
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

void enforceSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries) {
  struct PreferenceGroupKey {
    SgSymbolTable *table = nullptr;
    std::string entry_name;
    std::string category;

    bool operator<(const PreferenceGroupKey &rhs) const {
      if (table != rhs.table) {
        return std::less<SgSymbolTable *>()(table, rhs.table);
      }
      if (entry_name != rhs.entry_name) {
        return entry_name < rhs.entry_name;
      }
      return category < rhs.category;
    }
  };

  std::map<PreferenceGroupKey,
           std::vector<const ExpectedSymbolTablePreference *>>
      groups;
  for (const ExpectedSymbolTablePreference &entry : entries) {
    const std::string category = symbolLookupPreferenceCategory(entry.symbol);
    if (category.empty()) {
      continue;
    }
    PreferenceGroupKey key;
    key.table = entry.table;
    key.entry_name = entry.entry_name.getString();
    key.category = category;
    groups[key].push_back(&entry);
  }

  for (const auto &group_entry : groups) {
    const std::vector<const ExpectedSymbolTablePreference *> &group =
        group_entry.second;
    if (group.size() < 2 || symbolTablePreferencesMatch(group)) {
      continue;
    }

    std::vector<const ExpectedSymbolTablePreference *> preferred;
    std::vector<const ExpectedSymbolTablePreference *> nonpreferred;
    for (const ExpectedSymbolTablePreference *entry : group) {
      if (entry->lookup_preferred) {
        preferred.push_back(entry);
      } else {
        nonpreferred.push_back(entry);
      }
    }
    if (preferred.size() != 1) {
      std::ostringstream message;
      message << "AST JSON cannot enforce symbol lookup preference";
      message << " entry=" << group_entry.first.entry_name;
      message << " category=" << group_entry.first.category;
      message << " preferred_count=" << preferred.size();
      throw std::runtime_error(message.str());
    }

    std::vector<const ExpectedSymbolTablePreference *> ordered;
    ordered.reserve(group.size());
    ordered.insert(ordered.end(), nonpreferred.begin(), nonpreferred.end());
    ordered.insert(ordered.end(), preferred.begin(), preferred.end());
    removeSymbolTablePreferenceGroup(group);
    insertSymbolTablePreferenceGroup(ordered);
    if (symbolTablePreferencesMatch(group)) {
      continue;
    }

    ordered.clear();
    ordered.insert(ordered.end(), preferred.begin(), preferred.end());
    ordered.insert(ordered.end(), nonpreferred.begin(), nonpreferred.end());
    removeSymbolTablePreferenceGroup(group);
    insertSymbolTablePreferenceGroup(ordered);
    if (symbolTablePreferencesMatch(group)) {
      continue;
    }

    std::ostringstream message;
    message << "AST JSON failed to enforce symbol lookup preference";
    message << " entry=" << group_entry.first.entry_name;
    message << " category=" << group_entry.first.category;
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

SgSymbol *createDeferredSymbolTableSymbol(const JsonValue &entry,
                                          const NodeMap &nodes) {
  const std::string kind = entry.at("symbol_kind").asString();
  if (kind == "SgAliasSymbol") {
    const JsonValue *target = entry.find("alias_target");
    if (target == nullptr) {
      throw std::runtime_error(
          "AST JSON SgAliasSymbol table entry has no alias_target");
    }
    SgAliasSymbol *symbol =
        new SgAliasSymbol(resolveExistingSymbolFromJson(*target, nodes),
                          entry.requiredBool("alias_is_renamed"),
                          SgName(entry.requiredString("alias_new_name")));
    const JsonValue &causal_nodes = entry.at("alias_causal_nodes");
    if (causal_nodes.kind != JsonValue::Kind::Array ||
        causal_nodes.array.empty()) {
      throw std::runtime_error(
          "AST JSON alias_causal_nodes field is not a non-empty array");
    }
    for (const JsonValue &node_id : causal_nodes.array) {
      const uint64_t id = static_cast<uint64_t>(node_id.asInt());
      if (id == 0) {
        throw std::runtime_error(
            "AST JSON SgAliasSymbol has a null causal node");
      }
      symbol->get_causal_nodes().push_back(nodeById(nodes, id));
    }
    return symbol;
  }
  if (kind == "SgRenameSymbol") {
    const JsonValue *original = entry.find("original_symbol");
    if (original == nullptr) {
      throw std::runtime_error(
          "AST JSON SgRenameSymbol table entry has no original_symbol");
    }
    SgSymbol *originalSymbol = resolveExistingSymbolFromJson(*original, nodes);
    SgFunctionDeclaration *originalBasis = isSgFunctionDeclaration(
        originalSymbol != nullptr ? originalSymbol->get_symbol_basis()
                                  : nullptr);
    const JsonValue &serializedSymbol = entry.at("symbol");
    const int64_t rawBasisId =
        serializedSymbol.at("symbol_declaration").asInt();
    if (rawBasisId < 0 || originalBasis == nullptr) {
      throw std::runtime_error(
          "AST JSON SgRenameSymbol has no exact function basis");
    }

    SgFunctionDeclaration *basis = originalBasis;
    if (rawBasisId != 0) {
      basis = nodeByIdAs<SgFunctionDeclaration>(
          nodes, static_cast<uint64_t>(rawBasisId));
      if (basis != originalBasis) {
        throw std::runtime_error(
            "AST JSON SgRenameSymbol basis diverges from its original "
            "symbol");
      }
    } else {
      const JsonValue *external = serializedSymbol.find("external_function");
      if (external == nullptr ||
          external->requiredString("name") !=
              originalBasis->get_name().getString() ||
          external->requiredString("kind") != originalBasis->class_name()) {
        throw std::runtime_error(
            "AST JSON external SgRenameSymbol payload diverges from its "
            "original function");
      }
    }
    return new SgRenameSymbol(basis, originalSymbol,
                              SgName(entry.requiredString("rename_new_name")));
  }
  throw std::runtime_error("AST JSON deferred symbol kind is unsupported: " +
                           kind);
}

void restoreSerializedSymbolTables(const AstFileRecord &ast,
                                   const NodeMap &nodes) {
  std::vector<DeferredSymbolTableEntry> deferred_entries;
  std::vector<ExpectedSymbolTablePreference> expected_preferences;

  for (const NodeRecord &record : ast.nodes) {
    auto restored_node = nodes.find(record.id);
    if (restored_node == nodes.end()) {
      if (record.properties.find("symbol_table") != nullptr) {
        throw std::runtime_error("AST JSON scope was not built before its "
                                 "symbol-table reconstruction: " +
                                 record.kind);
      }
      continue;
    }
    SgScopeStatement *scope = isSgScopeStatement(restored_node->second);
    if (scope == nullptr) {
      continue;
    }

    const JsonValue &entries = record.properties.at("symbol_table");
    if (entries.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON symbol_table field is not an array for " + record.kind);
    }

    const int table_size =
        std::max<int>(17, static_cast<int>(entries.array.size() * 2 + 1));
    SgSymbolTable *table = new SgSymbolTable(table_size);
    table->set_parent(scope);
    table->setCaseInsensitive(
        record.properties.requiredBool("case_insensitive"));
    scope->set_symbol_table(table);

    std::vector<const JsonValue *> insertion_entries;
    insertion_entries.reserve(entries.array.size());
    for (const JsonValue &entry : entries.array) {
      insertion_entries.push_back(&entry);
    }
    std::stable_sort(
        insertion_entries.begin(), insertion_entries.end(),
        [](const JsonValue *lhs, const JsonValue *rhs) {
          const std::string lhs_name = lhs->at("entry_name").asString();
          const std::string rhs_name = rhs->at("entry_name").asString();
          const std::string lhs_kind = lhs->at("symbol_kind").asString();
          const std::string rhs_kind = rhs->at("symbol_kind").asString();
          const bool lhs_preferred = lhs->requiredBool("lookup_preferred");
          const bool rhs_preferred = rhs->requiredBool("lookup_preferred");
          if (lhs_name != rhs_name) {
            return lhs_name < rhs_name;
          }
          if (lhs_kind != rhs_kind) {
            return lhs_kind < rhs_kind;
          }
          if (lhs_preferred != rhs_preferred) {
            return !lhs_preferred && rhs_preferred;
          }
          const uint64_t lhs_basis = static_cast<uint64_t>(
              lhs->at("symbol").at("symbol_declaration").asInt());
          const uint64_t rhs_basis = static_cast<uint64_t>(
              rhs->at("symbol").at("symbol_declaration").asInt());
          if (lhs_basis != rhs_basis) {
            if (lhs_basis == 0) {
              return false;
            }
            if (rhs_basis == 0) {
              return true;
            }
            return lhs_basis < rhs_basis;
          }
          return false;
        });
    for (const JsonValue *entry_ptr : insertion_entries) {
      const JsonValue &entry = *entry_ptr;
      const std::string kind = entry.at("symbol_kind").asString();
      const SgName entry_name(entry.at("entry_name").asString());
      if (kind == "SgAliasSymbol" || kind == "SgRenameSymbol") {
        DeferredSymbolTableEntry deferred;
        deferred.table = table;
        deferred.entry_name = entry_name;
        deferred.entry_json = &entry;
        deferred_entries.push_back(deferred);
        continue;
      }

      const JsonValue &symbol_json = entry.at("symbol");
      const uint64_t basis_id =
          static_cast<uint64_t>(symbol_json.at("symbol_declaration").asInt());
      if (basis_id == 0) {
        SgSymbol *symbol = createExternalSymbolFromJson(symbol_json, nodes);
        if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON failed to reconstruct external symbol table entry");
        }
        validateExternalSymbolBasisOwnership(symbol);
        table->insert(entry_name, symbol);
        expected_preferences.push_back(
            {table, entry_name, symbol,
             entry.requiredBool("lookup_preferred")});
        continue;
      }
      if (basis_id == 0) {
        throw std::runtime_error(
            "AST JSON symbol table entry has no symbol_declaration");
      }
      SgNode *basis = nodeById(nodes, basis_id);
      SgSymbol *symbol = createSymbolForKindAndBasis(kind, basis);
      if (SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
        restoreLabelSymbolFields(label_symbol, symbol_json);
      }
      if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
        std::ostringstream message;
        message << "AST JSON cannot reconstruct symbol table entry"
                << " kind=" << kind << " basis=" << basis->sage_class_name();
        throw std::runtime_error(message.str());
      }
      if (SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
        namespace_symbol->set_isAlias(entry.requiredBool("namespace_is_alias"));
        namespace_symbol->set_declaration(
            isSgNamespaceDeclarationStatement(basis));
        const uint64_t alias_id = static_cast<uint64_t>(
            entry.requiredInt("namespace_alias_declaration"));
        if (alias_id != 0) {
          namespace_symbol->set_aliasDeclaration(
              nodeByIdAs<SgNamespaceAliasDeclarationStatement>(nodes,
                                                               alias_id));
        }
      }
      validateExternalSymbolBasisOwnership(symbol);
      table->insert(entry_name, symbol);
      expected_preferences.push_back(
          {table, entry_name, symbol, entry.requiredBool("lookup_preferred")});
    }
  }

  for (const DeferredSymbolTableEntry &deferred : deferred_entries) {
    ROSE_ASSERT(deferred.table != nullptr);
    ROSE_ASSERT(deferred.entry_json != nullptr);
    SgSymbol *symbol =
        createDeferredSymbolTableSymbol(*deferred.entry_json, nodes);
    if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to reconstruct deferred symbol table entry");
    }
    validateExternalSymbolBasisOwnership(symbol);
    deferred.table->insert(deferred.entry_name, symbol);
    expected_preferences.push_back(
        {deferred.table, deferred.entry_name, symbol,
         deferred.entry_json->requiredBool("lookup_preferred")});
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
    SgFunctionSymbol *symbol = new SgFunctionSymbol(basis);
    scope->insert_symbol(internalKey, symbol);
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

  enforceSymbolTablePreferences(expected_preferences);
  validateSymbolTablePreferences(expected_preferences);
}

SgClassSymbol *classSymbolForDeclaration(SgClassDeclaration *decl) {
  if (decl == nullptr) {
    return nullptr;
  }
  SgScopeStatement *scope = decl->get_scope();
  if (scope != nullptr) {
    if (SgClassSymbol *symbol =
            isSgClassSymbol(scope->find_symbol_from_declaration(decl))) {
      return symbol;
    }
  }
  ensureClassTypeForDeclaration(decl);
  SgClassSymbol *symbol = nullptr;
  if (isSgTemplateClassDeclaration(decl) != nullptr ||
      isSgTemplateInstantiationDecl(decl) != nullptr) {
    symbol = new SgTemplateClassSymbol(decl);
  } else {
    symbol = new SgClassSymbol(decl);
  }
  return symbol;
}

bool declarationHasDefinition(SgDeclarationStatement *decl) {
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(decl)) {
    return function->get_definition() != nullptr;
  }
  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    return class_decl->get_definition() != nullptr;
  }
  return false;
}

SgNonrealSymbol *nonrealSymbolForDeclaration(SgNonrealDecl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }
  SgScopeStatement *scope = decl->get_scope();
  if (scope != nullptr) {
    if (SgNonrealSymbol *symbol =
            isSgNonrealSymbol(scope->find_symbol_from_declaration(decl))) {
      return symbol;
    }
  }
  if (decl->get_type() == nullptr) {
    decl->set_type(new SgNonrealType(decl));
  }
  return new SgNonrealSymbol(decl);
}

SgTemplateArgumentPtrList templateArgumentListFromJson(const JsonValue &json,
                                                       const NodeMap &nodes,
                                                       SgNode *parent) {
  SgTemplateArgumentPtrList result;
  if (json.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON template argument list is not an array");
  }
  for (const JsonValue &arg_json : json.array) {
    const int64_t raw_id = arg_json.asInt();
    if (raw_id <= 0) {
      throw std::runtime_error(
          "AST JSON template argument ID must be positive");
    }
    const uint64_t id = static_cast<uint64_t>(raw_id);
    SgTemplateArgument *argument = nodeByIdAs<SgTemplateArgument>(nodes, id);
    argument->set_parent(parent);
    result.push_back(argument);
  }
  return result;
}

SgOmpUsesAllocatorsDefinationPtrList
usesAllocatorsDefinitionsFromJson(const JsonValue &json, const NodeMap &nodes,
                                  SgOmpUsesAllocatorsClause *parent) {
  SgOmpUsesAllocatorsDefinationPtrList result;
  if (json.kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &entry : json.array) {
    const uint64_t id = static_cast<uint64_t>(entry.asInt());
    if (id == 0) {
      continue;
    }
    SgOmpUsesAllocatorsDefination *definition =
        nodeByIdAs<SgOmpUsesAllocatorsDefination>(nodes, id);
    definition->set_parent(parent);
    result.push_back(definition);
  }
  return result;
}

void applyOmpAuxiliaryState(const AstFileRecord &ast, const NodeMap &nodes) {
  for (const NodeRecord &record : ast.nodes) {
    SgNode *node = nodeById(nodes, record.id);
    const JsonValue &p = record.properties;

    if (SgOmpClause *clause = isSgOmpClause(node)) {
      clause->set_directive_name_modifier(
          static_cast<SgOmpClause::omp_directive_name_modifier_enum>(
              p.requiredInt("directive_name_modifier")));
    }
    if (SgOmpDirectiveKindClause *clause = isSgOmpDirectiveKindClause(node)) {
      clause->validate_directive_kinds();
    }

    if (SgOmpOrderClause *clause = isSgOmpOrderClause(node)) {
      clause->set_kind(
          static_cast<SgOmpClause::omp_order_kind_enum>(p.requiredInt("kind")));
      clause->set_modifier(static_cast<SgOmpClause::omp_order_modifier_enum>(
          p.requiredInt("modifier")));
    } else if (SgOmpAtomicDefaultMemOrderClause *clause =
                   isSgOmpAtomicDefaultMemOrderClause(node)) {
      clause->set_kind(
          static_cast<SgOmpClause::omp_atomic_default_mem_order_kind_enum>(
              p.requiredInt("kind")));
    } else if (SgOmpDefaultmapClause *clause = isSgOmpDefaultmapClause(node)) {
      clause->set_behavior(
          static_cast<SgOmpClause::omp_defaultmap_behavior_enum>(
              p.requiredInt("behavior")));
      clause->set_category(
          static_cast<SgOmpClause::omp_defaultmap_category_enum>(
              p.requiredInt("category")));
    } else if (SgOmpBindClause *clause = isSgOmpBindClause(node)) {
      clause->set_binding(static_cast<SgOmpClause::omp_bind_binding_enum>(
          p.requiredInt("binding")));
    } else if (SgOmpFailClause *clause = isSgOmpFailClause(node)) {
      clause->set_memory_order(
          static_cast<SgOmpClause::omp_fail_memory_order_kind_enum>(
              p.requiredInt("memory_order")));
    } else if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
      clause->set_modifier(static_cast<SgOmpClause::omp_linear_modifier_enum>(
          p.requiredInt("modifier")));
    } else if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_reduction_modifier_enum>(
              p.requiredInt("modifier")));
      clause->set_identifier(
          static_cast<SgOmpClause::omp_reduction_identifier_enum>(
              p.requiredInt("identifier")));
    } else if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
      clause->set_modifier(static_cast<SgOmpClause::omp_allocate_modifier_enum>(
          p.requiredInt("modifier")));
    } else if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_allocator_modifier_enum>(
              p.requiredInt("modifier")));
    } else if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_adjust_args_modifier_enum>(
              p.requiredInt("modifier")));
    } else if (SgOmpInReductionClause *clause =
                   isSgOmpInReductionClause(node)) {
      clause->set_identifier(
          static_cast<SgOmpClause::omp_in_reduction_identifier_enum>(
              p.requiredInt("identifier")));
    } else if (SgOmpTaskReductionClause *clause =
                   isSgOmpTaskReductionClause(node)) {
      clause->set_identifier(
          static_cast<SgOmpClause::omp_task_reduction_identifier_enum>(
              p.requiredInt("identifier")));
    } else if (SgOmpDepobjUpdateClause *clause =
                   isSgOmpDepobjUpdateClause(node)) {
      clause->set_modifier(static_cast<SgOmpClause::omp_depobj_modifier_enum>(
          p.requiredInt("modifier")));
    } else if (SgOmpDistScheduleClause *clause =
                   isSgOmpDistScheduleClause(node)) {
      clause->set_kind(static_cast<SgOmpClause::omp_dist_schedule_kind_enum>(
          p.requiredInt("kind")));
    } else if (SgOmpNumTasksClause *clause = isSgOmpNumTasksClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_num_tasks_modifier_enum>(
              p.requiredInt("modifier")));
    } else if (SgOmpGrainsizeClause *clause = isSgOmpGrainsizeClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_grainsize_modifier_enum>(
              p.requiredInt("modifier")));
    } else if (SgOmpUsesAllocatorsDefination *definition =
                   isSgOmpUsesAllocatorsDefination(node)) {
      definition->set_allocator(
          static_cast<SgOmpClause::omp_uses_allocators_allocator_enum>(
              p.requiredInt("allocator")));
    } else if (SgOmpUsesAllocatorsClause *clause =
                   isSgOmpUsesAllocatorsClause(node)) {
      if (const JsonValue *definitions =
              p.find("uses_allocators_definitions")) {
        clause->get_uses_allocators_defination() =
            usesAllocatorsDefinitionsFromJson(*definitions, nodes, clause);
      }
    } else if (SgOmpDeclareMapperStatement *stmt =
                   isSgOmpDeclareMapperStatement(node)) {
      stmt->set_identifier(
          static_cast<SgOmpClause::omp_declare_mapper_identifier_enum>(
              p.requiredInt("identifier")));
    }
  }
}

} // namespace AstJson
} // namespace Rose
