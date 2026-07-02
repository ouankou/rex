#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

void restoreLabelSymbolFields(SgLabelSymbol *symbol, const JsonValue &json) {
  if (symbol == nullptr) {
    return;
  }
  symbol->set_numeric_label_value(json.intOr(
      "label_numeric_label_value", symbol->get_numeric_label_value()));
  symbol->set_label_type(static_cast<SgLabelSymbol::label_type_enum>(
      json.intOr("label_type", static_cast<int>(symbol->get_label_type()))));
  if (symbol->get_declaration() == nullptr &&
      symbol->get_numeric_label_value() <= 0 &&
      json.stringOr("symbol_name").empty()) {
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
  const std::string name = json.stringOr("symbol_name");
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
      static_cast<uint64_t>(json.intOr("symbol_declaration", 0));
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
  } else if (isSgLabelStatement(basis) != nullptr ||
             isSgStatement(basis) != nullptr) {
    if (SgLabelSymbol *label = existingLabelSymbolFromJson(json, basis)) {
      restoreLabelSymbolFields(label, json);
      return label;
    }
  }

  std::string kind = json.stringOr("symbol_kind");
  if (kind.empty()) {
    kind = inferredSymbolKindForBasis(basis);
  }
  if (kind.empty()) {
    throw std::runtime_error("AST JSON cannot infer symbol kind for basis: " +
                             std::string(basis->sage_class_name()));
  }
  symbol = createSymbolForKindAndBasis(kind, basis);
  if (SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
    restoreLabelSymbolFields(label_symbol, json);
  }
  if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot reconstruct detached symbol reference: " + kind);
  }
  return symbol;
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
  SgSymbol *symbol = createSymbolForKindAndBasis(kind, basis);
  if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
    throw std::runtime_error(
        "AST JSON cannot reconstruct external symbol reference: " + kind);
  }
  return symbol;
}

void attachExternalSymbolBasisToScope(SgSymbol *symbol,
                                      SgScopeStatement *scope) {
  if (symbol == nullptr || scope == nullptr) {
    return;
  }
  SgNode *basis = symbol->get_symbol_basis();
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(basis)) {
    if (isAstJsonExternalFunction(decl)) {
      decl->set_scope(scope);
    }
  } else if (SgModuleStatement *module = isSgModuleStatement(basis)) {
    if (isAstJsonExternalModule(module)) {
      module->set_scope(scope);
      if (module->get_parent() == nullptr) {
        module->set_parent(scope);
      }
    }
  } else if (SgClassDeclaration *decl = isSgClassDeclaration(basis)) {
    if (isAstJsonExternalClassDeclaration(decl)) {
      decl->set_scope(scope);
      if (decl->get_parent() == nullptr) {
        decl->set_parent(scope);
      }
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
        json.stringOr("symbol_name"));
  }
  return symbol;
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
                          entry.boolOr("alias_is_renamed", false),
                          SgName(entry.stringOr("alias_new_name")));
    if (const JsonValue *causal_nodes = entry.find("alias_causal_nodes")) {
      if (causal_nodes->kind != JsonValue::Kind::Array) {
        throw std::runtime_error(
            "AST JSON alias_causal_nodes field is not an array");
      }
      for (const JsonValue &node_id : causal_nodes->array) {
        const uint64_t id = static_cast<uint64_t>(node_id.asInt());
        if (id != 0) {
          symbol->get_causal_nodes().push_back(nodeById(nodes, id));
        }
      }
    }
    return symbol;
  }
  if (kind == "SgRenameSymbol") {
    const JsonValue *original = entry.find("original_symbol");
    if (original == nullptr) {
      throw std::runtime_error(
          "AST JSON SgRenameSymbol table entry has no original_symbol");
    }
    SgNode *basis = nodeById(
        nodes, static_cast<uint64_t>(
                   entry.at("symbol").at("symbol_declaration").asInt()));
    return new SgRenameSymbol(isSgFunctionDeclaration(basis),
                              resolveExistingSymbolFromJson(*original, nodes),
                              SgName(entry.stringOr("rename_new_name")));
  }
  throw std::runtime_error("AST JSON deferred symbol kind is unsupported: " +
                           kind);
}

void restoreSerializedSymbolTables(const AstFileRecord &ast,
                                   const NodeMap &nodes) {
  std::vector<DeferredSymbolTableEntry> deferred_entries;
  std::vector<ExpectedSymbolTablePreference> expected_preferences;

  for (const NodeRecord &record : ast.nodes) {
    SgScopeStatement *scope = isSgScopeStatement(nodeById(nodes, record.id));
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
    table->setCaseInsensitive(record.properties.boolOr(
        "case_insensitive", table->isCaseInsensitive()));
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
          const bool lhs_preferred = lhs->boolOr("lookup_preferred", false);
          const bool rhs_preferred = rhs->boolOr("lookup_preferred", false);
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
        attachExternalSymbolBasisToScope(symbol, scope);
        table->insert(entry_name, symbol);
        expected_preferences.push_back(
            {table, entry_name, symbol,
             entry.boolOr("lookup_preferred", false)});
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
        namespace_symbol->set_isAlias(entry.boolOr(
            "namespace_is_alias", namespace_symbol->get_isAlias()));
        namespace_symbol->set_declaration(
            isSgNamespaceDeclarationStatement(basis));
        const uint64_t alias_id = static_cast<uint64_t>(
            entry.intOr("namespace_alias_declaration", 0));
        if (alias_id != 0) {
          namespace_symbol->set_aliasDeclaration(
              nodeByIdAs<SgNamespaceAliasDeclarationStatement>(nodes,
                                                               alias_id));
        }
      }
      attachExternalSymbolBasisToScope(symbol, scope);
      table->insert(entry_name, symbol);
      expected_preferences.push_back(
          {table, entry_name, symbol, entry.boolOr("lookup_preferred", false)});
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
    attachExternalSymbolBasisToScope(
        symbol, isSgScopeStatement(deferred.table->get_parent()));
    deferred.table->insert(deferred.entry_name, symbol);
    expected_preferences.push_back(
        {deferred.table, deferred.entry_name, symbol,
         deferred.entry_json->boolOr("lookup_preferred", false)});
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

std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
arrayDimensionsFromJson(const JsonValue &json, const NodeMap &nodes) {
  std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
      result;
  if (json.kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &entry : json.array) {
    const JsonValue *symbol_json = entry.find("symbol");
    if (symbol_json == nullptr) {
      continue;
    }
    SgSymbol *symbol = symbolFromJson(*symbol_json, nodes);
    if (symbol == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to resolve OMP array-section symbol");
    }
    std::vector<std::pair<SgExpression *, SgExpression *>> bounds;
    const JsonValue *bounds_json = entry.find("bounds");
    if (bounds_json != nullptr && bounds_json->kind == JsonValue::Kind::Array) {
      for (const JsonValue &bound : bounds_json->array) {
        const JsonValue *lower_json = bound.find("lower");
        const JsonValue *upper_json = bound.find("upper");
        bounds.emplace_back(
            lower_json != nullptr ? expressionFromRef(*lower_json, nodes)
                                  : nullptr,
            upper_json != nullptr ? expressionFromRef(*upper_json, nodes)
                                  : nullptr);
      }
    }
    result[symbol] = std::move(bounds);
  }
  return result;
}

std::map<
    SgSymbol *,
    std::vector<std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>>
distDataPoliciesFromJson(const JsonValue &json, const NodeMap &nodes) {
  std::map<SgSymbol *,
           std::vector<
               std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>>
      result;
  if (json.kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &entry : json.array) {
    const JsonValue *symbol_json = entry.find("symbol");
    if (symbol_json == nullptr) {
      continue;
    }
    SgSymbol *symbol = symbolFromJson(*symbol_json, nodes);
    if (symbol == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to resolve OMP dist-data symbol");
    }
    std::vector<std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>
        policies;
    const JsonValue *policies_json = entry.find("policies");
    if (policies_json != nullptr &&
        policies_json->kind == JsonValue::Kind::Array) {
      for (const JsonValue &policy : policies_json->array) {
        const JsonValue *expr_json = policy.find("expression");
        policies.emplace_back(static_cast<SgOmpClause::omp_map_dist_data_enum>(
                                  policy.intOr("policy", 0)),
                              expr_json != nullptr
                                  ? expressionFromRef(*expr_json, nodes)
                                  : nullptr);
      }
    }
    result[symbol] = std::move(policies);
  }
  return result;
}

std::list<std::list<SgExpression *>> iteratorFromJson(const JsonValue &json,
                                                      const NodeMap &nodes) {
  std::list<std::list<SgExpression *>> result;
  if (json.kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &iterator_json : json.array) {
    std::list<SgExpression *> iterator;
    if (iterator_json.kind == JsonValue::Kind::Array) {
      for (const JsonValue &expr_json : iterator_json.array) {
        iterator.push_back(expressionFromRef(expr_json, nodes));
      }
    }
    result.push_back(std::move(iterator));
  }
  return result;
}

std::list<SgExpression *> expressionListFromJson(const JsonValue &json,
                                                 const NodeMap &nodes) {
  std::list<SgExpression *> result;
  if (json.kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &expr_json : json.array) {
    result.push_back(expressionFromRef(expr_json, nodes));
  }
  return result;
}

SgTemplateArgumentPtrList templateArgumentListFromJson(const JsonValue &json,
                                                       const NodeMap &nodes,
                                                       SgNode *parent) {
  SgTemplateArgumentPtrList result;
  if (json.kind != JsonValue::Kind::Array) {
    return result;
  }
  for (const JsonValue &arg_json : json.array) {
    const uint64_t id = static_cast<uint64_t>(arg_json.asInt());
    if (id == 0) {
      continue;
    }
    SgTemplateArgument *argument = nodeByIdAs<SgTemplateArgument>(nodes, id);
    argument->set_parent(parent);
    result.push_back(argument);
  }
  return result;
}

std::list<SgOmpUsesAllocatorsDefination *>
usesAllocatorsDefinitionsFromJson(const JsonValue &json, const NodeMap &nodes,
                                  SgOmpUsesAllocatorsClause *parent) {
  std::list<SgOmpUsesAllocatorsDefination *> result;
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
              p.intOr("directive_name_modifier", 0)));
    }

    if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
      clause->set_operation(static_cast<SgOmpClause::omp_map_operator_enum>(
          p.intOr("operation", SgOmpClause::e_omp_map_unknown)));
      clause->set_modifier1(static_cast<SgOmpClause::omp_map_modifier_enum>(
          p.intOr("modifier1", SgOmpClause::e_omp_map_modifier_unspecified)));
      clause->set_modifier2(static_cast<SgOmpClause::omp_map_modifier_enum>(
          p.intOr("modifier2", SgOmpClause::e_omp_map_modifier_unspecified)));
      clause->set_modifier3(static_cast<SgOmpClause::omp_map_modifier_enum>(
          p.intOr("modifier3", SgOmpClause::e_omp_map_modifier_unspecified)));
      if (const JsonValue *mapper = p.find("mapper_identifier")) {
        clause->set_mapper_identifier(expressionFromRef(*mapper, nodes));
      }
      if (const JsonValue *dims = p.find("array_dimensions")) {
        clause->set_array_dimensions(arrayDimensionsFromJson(*dims, nodes));
      }
      if (const JsonValue *policies = p.find("dist_data_policies")) {
        clause->set_dist_data_policies(
            distDataPoliciesFromJson(*policies, nodes));
      }
      if (const JsonValue *iterator = p.find("iterator")) {
        clause->set_iterator(iteratorFromJson(*iterator, nodes));
      }
    } else if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
      clause->set_depend_modifier(
          static_cast<SgOmpClause::omp_depend_modifier_enum>(
              p.intOr("depend_modifier",
                      SgOmpClause::e_omp_depend_modifier_unspecified)));
      clause->set_dependence_type(
          static_cast<SgOmpClause::omp_dependence_type_enum>(p.intOr(
              "dependence_type", SgOmpClause::e_omp_depend_unspecified)));
      if (const JsonValue *dims = p.find("array_dimensions")) {
        clause->set_array_dimensions(arrayDimensionsFromJson(*dims, nodes));
      }
      if (const JsonValue *iterator = p.find("iterator")) {
        clause->set_iterator(iteratorFromJson(*iterator, nodes));
      }
      if (const JsonValue *vec = p.find("vec")) {
        clause->set_vec(expressionListFromJson(*vec, nodes));
      }
    } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
      clause->set_affinity_modifier(
          static_cast<SgOmpClause::omp_affinity_modifier_enum>(
              p.intOr("affinity_modifier",
                      SgOmpClause::e_omp_affinity_modifier_unspecified)));
      if (const JsonValue *dims = p.find("array_dimensions")) {
        clause->set_array_dimensions(arrayDimensionsFromJson(*dims, nodes));
      }
      if (const JsonValue *iterator = p.find("iterator")) {
        clause->set_iterator(iteratorFromJson(*iterator, nodes));
      }
    } else if (SgOmpToClause *clause = isSgOmpToClause(node)) {
      clause->set_kind(static_cast<SgOmpClause::omp_to_kind_enum>(
          p.intOr("kind", SgOmpClause::e_omp_to_kind_unknown)));
      if (const JsonValue *mapper = p.find("mapper_identifier")) {
        clause->set_mapper_identifier(expressionFromRef(*mapper, nodes));
      }
      if (const JsonValue *dims = p.find("array_dimensions")) {
        clause->set_array_dimensions(arrayDimensionsFromJson(*dims, nodes));
      }
      if (const JsonValue *iterator = p.find("iterator")) {
        clause->set_iterator(iteratorFromJson(*iterator, nodes));
      }
    } else if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
      clause->set_kind(static_cast<SgOmpClause::omp_from_kind_enum>(
          p.intOr("kind", SgOmpClause::e_omp_from_kind_unknown)));
      if (const JsonValue *mapper = p.find("mapper_identifier")) {
        clause->set_mapper_identifier(expressionFromRef(*mapper, nodes));
      }
      if (const JsonValue *dims = p.find("array_dimensions")) {
        clause->set_array_dimensions(arrayDimensionsFromJson(*dims, nodes));
      }
      if (const JsonValue *iterator = p.find("iterator")) {
        clause->set_iterator(iteratorFromJson(*iterator, nodes));
      }
    } else if (SgOmpOrderClause *clause = isSgOmpOrderClause(node)) {
      clause->set_kind(static_cast<SgOmpClause::omp_order_kind_enum>(
          p.intOr("kind", SgOmpClause::e_omp_order_kind_unspecified)));
      clause->set_modifier(static_cast<SgOmpClause::omp_order_modifier_enum>(
          p.intOr("modifier", SgOmpClause::e_omp_order_modifier_unspecified)));
    } else if (SgOmpAtomicDefaultMemOrderClause *clause =
                   isSgOmpAtomicDefaultMemOrderClause(node)) {
      clause->set_kind(
          static_cast<
              SgOmpClause::omp_atomic_default_mem_order_kind_enum>(p.intOr(
              "kind",
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified)));
    } else if (SgOmpDefaultmapClause *clause = isSgOmpDefaultmapClause(node)) {
      clause->set_behavior(
          static_cast<SgOmpClause::omp_defaultmap_behavior_enum>(p.intOr(
              "behavior", SgOmpClause::e_omp_defaultmap_behavior_unspecified)));
      clause->set_category(
          static_cast<SgOmpClause::omp_defaultmap_category_enum>(p.intOr(
              "category", SgOmpClause::e_omp_defaultmap_category_unspecified)));
    } else if (SgOmpBindClause *clause = isSgOmpBindClause(node)) {
      clause->set_binding(static_cast<SgOmpClause::omp_bind_binding_enum>(
          p.intOr("binding", SgOmpClause::e_omp_bind_binding_unspecified)));
    } else if (SgOmpFailClause *clause = isSgOmpFailClause(node)) {
      clause->set_memory_order(
          static_cast<SgOmpClause::omp_fail_memory_order_kind_enum>(
              p.intOr("memory_order",
                      SgOmpClause::e_omp_fail_memory_order_kind_unspecified)));
    } else if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
      clause->set_modifier(static_cast<SgOmpClause::omp_linear_modifier_enum>(
          p.intOr("modifier", SgOmpClause::e_omp_linear_modifier_unspecified)));
    } else if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_reduction_modifier_enum>(p.intOr(
              "modifier", SgOmpClause::e_omp_reduction_modifier_unknown)));
      clause->set_identifier(
          static_cast<SgOmpClause::omp_reduction_identifier_enum>(
              p.intOr("identifier", SgOmpClause::e_omp_reduction_unknown)));
    } else if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
      clause->set_modifier(static_cast<SgOmpClause::omp_allocate_modifier_enum>(
          p.intOr("modifier", SgOmpClause::e_omp_allocate_modifier_unknown)));
    } else if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_allocator_modifier_enum>(p.intOr(
              "modifier", SgOmpClause::e_omp_allocator_modifier_unknown)));
    } else if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_adjust_args_modifier_enum>(p.intOr(
              "modifier", SgOmpClause::e_omp_adjust_args_modifier_unknown)));
    } else if (SgOmpInReductionClause *clause =
                   isSgOmpInReductionClause(node)) {
      clause->set_identifier(
          static_cast<SgOmpClause::omp_in_reduction_identifier_enum>(
              p.intOr("identifier",
                      SgOmpClause::e_omp_in_reduction_identifier_unspecified)));
    } else if (SgOmpTaskReductionClause *clause =
                   isSgOmpTaskReductionClause(node)) {
      clause->set_identifier(
          static_cast<SgOmpClause::omp_task_reduction_identifier_enum>(p.intOr(
              "identifier",
              SgOmpClause::e_omp_task_reduction_identifier_unspecified)));
    } else if (SgOmpDepobjUpdateClause *clause =
                   isSgOmpDepobjUpdateClause(node)) {
      clause->set_modifier(static_cast<SgOmpClause::omp_depobj_modifier_enum>(
          p.intOr("modifier", SgOmpClause::e_omp_depobj_modifier_unknown)));
    } else if (SgOmpDistScheduleClause *clause =
                   isSgOmpDistScheduleClause(node)) {
      clause->set_kind(static_cast<SgOmpClause::omp_dist_schedule_kind_enum>(
          p.intOr("kind", SgOmpClause::e_omp_dist_schedule_kind_unspecified)));
    } else if (SgOmpNumTasksClause *clause = isSgOmpNumTasksClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_num_tasks_modifier_enum>(p.intOr(
              "modifier", SgOmpClause::e_omp_num_tasks_modifier_unspecified)));
    } else if (SgOmpGrainsizeClause *clause = isSgOmpGrainsizeClause(node)) {
      clause->set_modifier(
          static_cast<SgOmpClause::omp_grainsize_modifier_enum>(p.intOr(
              "modifier", SgOmpClause::e_omp_grainsize_modifier_unspecified)));
    } else if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
      clause->set_target_device_selector(p.boolOr(
          "target_device_selector", clause->get_target_device_selector()));
      clause->set_device_kind(
          static_cast<SgOmpClause::omp_when_context_kind_enum>(p.intOr(
              "device_kind", SgOmpClause::e_omp_when_context_kind_unknown)));
      clause->set_implementation_vendor(
          static_cast<SgOmpClause::omp_when_context_vendor_enum>(
              p.intOr("implementation_vendor",
                      SgOmpClause::e_omp_when_context_vendor_unspecified)));
    } else if (SgOmpUsesAllocatorsDefination *definition =
                   isSgOmpUsesAllocatorsDefination(node)) {
      definition->set_allocator(
          static_cast<SgOmpClause::omp_uses_allocators_allocator_enum>(
              p.intOr("allocator",
                      SgOmpClause::e_omp_uses_allocators_allocator_unknown)));
    } else if (SgOmpUsesAllocatorsClause *clause =
                   isSgOmpUsesAllocatorsClause(node)) {
      if (const JsonValue *definitions =
              p.find("uses_allocators_definitions")) {
        clause->set_uses_allocators_defination(
            usesAllocatorsDefinitionsFromJson(*definitions, nodes, clause));
      }
    } else if (SgOmpDeclareMapperStatement *stmt =
                   isSgOmpDeclareMapperStatement(node)) {
      stmt->set_identifier(
          static_cast<SgOmpClause::omp_declare_mapper_identifier_enum>(p.intOr(
              "identifier",
              SgOmpClause::e_omp_declare_mapper_identifier_unspecified)));
    }
  }
}

} // namespace AstJson
} // namespace Rose
