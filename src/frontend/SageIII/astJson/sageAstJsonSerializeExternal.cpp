#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl) {
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("present", decl != nullptr));
  if (decl != nullptr) {
    fields.push_back(rawStringField("kind", decl->sage_class_name()));
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawIntegerField("class_type", decl->get_class_type()));
    fields.push_back(
        rawStringField("source_file", sourceFileNameForNode(decl)));
    fields.push_back(rawStringField("module_name", moduleNameForNode(decl)));
    fields.push_back(
        rawBoolField("has_definition", classDeclarationHasDefinition(decl)));
    fields.push_back(rawBoolField("is_first_nondefining",
                                  classDeclarationIsFirstNondefining(decl)));
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
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  const bool external = module != nullptr && (isAstJsonExternalModule(module) ||
                                              idFor(ids, module) == 0);
  fields.push_back(rawBoolField("present", external));
  if (external) {
    fields.push_back(rawStringField("name", module->get_name().getString()));
    fields.push_back(rawIntegerField("class_type", module->get_class_type()));
    fields.push_back(
        rawStringField("source_file", sourceFileNameForNode(module)));
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalFunctionParameterScopeJson(
    SgFunctionParameterScope *scope,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);

std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external) {
  std::vector<std::string> fields;
  const bool external =
      decl != nullptr &&
      (force_external || isAstJsonExternalFunction(decl) ||
       (idFor(ids, decl) == 0 && !insideCollectionBoundary(decl)));
  fields.push_back(rawBoolField("present", external));
  if (external) {
    fields.push_back(rawStringField("kind", decl->sage_class_name()));
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(
        rawStringField("source_file", sourceFileNameForExternalFunction(decl)));
    fields.push_back(
        jsonString("location") + ": " +
        rawRequiredLocationJson(decl, "external_function " +
                                          decl->get_name().getString()));
    SgFunctionParameterList *parameter_list = decl->get_parameterList();
    if (parameter_list == nullptr) {
      throw std::runtime_error("AST JSON external_function " +
                               decl->get_name().getString() +
                               " requires a parameterList");
    }
    fields.push_back(jsonString("parameter_list_location") + ": " +
                     rawRequiredLocationJson(
                         parameter_list, "external_function parameterList " +
                                             decl->get_name().getString()));
    SgFunctionParameterList *syntax_list = decl->get_parameterList_syntax();
    if (syntax_list != nullptr && syntax_list != parameter_list) {
      throw std::runtime_error(
          "AST JSON external_function " + decl->get_name().getString() +
          " has a distinct parameterList_syntax that is not yet serialized");
    }
    fields.push_back(
        rawBoolField("parameter_list_syntax_aliases_parameter_list",
                     syntax_list == parameter_list));
    fields.push_back(jsonString("function_parameter_scope") + ": " +
                     rawExternalFunctionParameterScopeJson(
                         decl->get_functionParameterScope(), ids,
                         decl->get_name().getString()));
    fields.push_back(jsonString("function_type") + ": " +
                     rawTypeJson(decl->get_type(), ids));
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(decl)) {
      fields.push_back(
          rawIntegerField("subprogram_kind", procedure->get_subprogram_kind()));
    }
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool declarationNeedsExternalReferenceRecord(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return decl != nullptr && idFor(ids, decl) == 0 &&
         (hasNonStructuralExternalMarkerAncestor(decl) ||
          ((isSgFunctionDeclaration(decl) != nullptr ||
            isSgModuleStatement(decl) != nullptr ||
            isSgClassDeclaration(decl) != nullptr) &&
           !isStructuralAstChildOfParent(decl)));
}

std::string externalFunctionParameterScopeSource(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (decl == nullptr || decl->get_functionParameterScope() == nullptr ||
      idFor(ids, decl->get_functionParameterScope()) != 0) {
    return "";
  }

  auto has_external_scope = [&](SgDeclarationStatement *candidate) -> bool {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(candidate);
    return function != nullptr &&
           function->get_functionParameterScope() ==
               decl->get_functionParameterScope() &&
           declarationNeedsExternalReferenceRecord(function, ids);
  };

  if (has_external_scope(decl->get_firstNondefiningDeclaration())) {
    return "firstNondefiningDeclaration";
  }
  if (has_external_scope(decl->get_definingDeclaration())) {
    return "definingDeclaration";
  }
  return "";
}

bool functionParameterScopeNeedsExternalReferenceRecord(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return !externalFunctionParameterScopeSource(decl, ids).empty();
}

std::string rawExternalDeclarationReferenceJson(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  const bool present = declarationNeedsExternalReferenceRecord(decl, ids);
  fields.push_back(rawBoolField("present", present));
  if (present) {
    fields.push_back(rawStringField("kind", decl->sage_class_name()));
    if (SgFunctionDeclaration *function_decl = isSgFunctionDeclaration(decl)) {
      fields.push_back(jsonString("external_function") + ": " +
                       rawExternalFunctionJson(function_decl, ids, true));
    } else if (SgModuleStatement *module = isSgModuleStatement(decl)) {
      fields.push_back(jsonString("external_module") + ": " +
                       rawExternalModuleJson(module, ids));
    } else if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      fields.push_back(jsonString("external_class") + ": " +
                       rawExternalClassDeclarationJson(class_decl));
    } else {
      throw std::runtime_error(
          std::string("AST JSON external declaration reference has unsupported "
                      "kind: ") +
          decl->sage_class_name());
    }
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalInitializedNameJson(
    SgInitializedName *name,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (name == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null initialized name");
  }
  if (name->get_initptr() != nullptr) {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " has an initialized name with an initializer that is not yet "
        "serialized: " +
        name->get_name().getString());
  }
  std::vector<std::string> fields;
  fields.push_back(rawStringField("name", name->get_name().getString()));
  fields.push_back(
      jsonString("location") + ": " +
      rawRequiredLocationJson(name, "external_function initializedName " +
                                        function_name +
                                        "::" + name->get_name().getString()));
  fields.push_back(jsonString("type") + ": " +
                   rawTypeJson(name->get_typeptr(), ids));
  fields.push_back(rawIntegerField(
      "storage_modifier",
      static_cast<int>(name->get_storageModifier().get_modifier())));
  fields.push_back(rawStringField("gnu_attribute_section_name",
                                  name->get_gnu_attribute_section_name()));
  fields.push_back(rawIntegerField("name_qualification_length",
                                   name->get_name_qualification_length()));
  fields.push_back(rawBoolField("type_elaboration_required",
                                name->get_type_elaboration_required()));
  fields.push_back(rawBoolField("global_qualification_required",
                                name->get_global_qualification_required()));
  fields.push_back(
      rawIntegerField("name_qualification_length_for_type",
                      name->get_name_qualification_length_for_type()));
  fields.push_back(
      rawBoolField("type_elaboration_required_for_type",
                   name->get_type_elaboration_required_for_type()));
  fields.push_back(
      rawBoolField("global_qualification_required_for_type",
                   name->get_global_qualification_required_for_type()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

void appendRawExternalDeclarationStatementFields(
    std::vector<std::string> &fields, SgDeclarationStatement *decl,
    const std::string &context) {
  if (decl == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a declaration statement");
  }
  SgDeclarationStatement *first_nondefining =
      decl->get_firstNondefiningDeclaration();
  SgDeclarationStatement *defining = decl->get_definingDeclaration();
  if (first_nondefining != nullptr && first_nondefining != decl) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has a firstNondefiningDeclaration that is not self or null");
  }
  if (defining != nullptr && defining != decl) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has a definingDeclaration that is not self or null");
  }
  if (decl->get_declarationScope() != nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " has a declarationScope that is not yet "
                             "serialized for nested external declarations");
  }
  if (decl->get_nonreal_decl_scope() != nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " has a nonreal_decl_scope that is not yet "
                             "serialized for nested external declarations");
  }

  const SgStorageModifier &storage =
      decl->get_declarationModifier().get_storageModifier();
  fields.push_back(
      rawIntegerField("decl_attributes", decl->get_decl_attributes()));
  fields.push_back(rawStringField("linkage", decl->get_linkage()));
  fields.push_back(
      jsonString("declaration_modifier_vector") + ": " +
      rawBitVectorJson(decl->get_declarationModifier().get_modifierVector()));
  fields.push_back(jsonString("declaration_type_modifier_vector") + ": " +
                   rawBitVectorJson(decl->get_declarationModifier()
                                        .get_typeModifier()
                                        .get_modifierVector()));
  fields.push_back(rawIntegerField("declaration_storage_modifier",
                                   static_cast<int>(storage.get_modifier())));
  fields.push_back(
      rawIntegerField("declaration_access_modifier",
                      static_cast<int>(decl->get_declarationModifier()
                                           .get_accessModifier()
                                           .get_modifier())));
  fields.push_back(rawBoolField(
      "declaration_access_is_explicit",
      decl->get_declarationModifier().get_accessModifier().get_is_explicit()));
  fields.push_back(rawBoolField("name_only", decl->get_nameOnly()));
  fields.push_back(rawBoolField("forward", decl->get_forward()));
  fields.push_back(rawBoolField("extern_brace", decl->get_externBrace()));
  fields.push_back(
      rawBoolField("skip_elaborate_type", decl->get_skipElaborateType()));
  fields.push_back(rawStringField("binding_label", decl->get_binding_label()));
  fields.push_back(
      rawBoolField("unparse_template_ast", decl->get_unparse_template_ast()));
  fields.push_back(rawStringField(
      "declaration_gnu_attribute_section_name",
      decl->get_declarationModifier().get_gnu_attribute_section_name()));
  fields.push_back(rawIntegerField(
      "declaration_gnu_attribute_visability",
      static_cast<int>(
          decl->get_declarationModifier().get_gnu_attribute_visability())));
  fields.push_back(
      rawBoolField("first_nondefining_is_self", first_nondefining == decl));
  fields.push_back(
      rawBoolField("first_nondefining_is_null", first_nondefining == nullptr));
  fields.push_back(
      rawBoolField("defining_declaration_is_self", defining == decl));
  fields.push_back(
      rawBoolField("defining_declaration_is_null", defining == nullptr));
}

std::string rawExternalVariableDeclarationJson(
    SgVariableDeclaration *variable,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (variable == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null parameter-scope variable "
                             "declaration");
  }
  if (variable->get_baseTypeDefiningDeclaration() != nullptr) {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " parameter-scope variable declaration has a base type defining "
        "declaration that is not yet serialized");
  }
  std::vector<std::string> fields;
  fields.push_back(rawStringField("kind", variable->sage_class_name()));
  fields.push_back(
      jsonString("location") + ": " +
      rawRequiredLocationJson(
          variable, "external_function variableDeclaration " + function_name));
  appendRawExternalDeclarationStatementFields(
      fields, variable,
      "external_function " + function_name + " variableDeclaration");
  fields.push_back(
      rawBoolField("requires_global_name_qualification_on_type",
                   variable->get_requiresGlobalNameQualificationOnType()));
  fields.push_back(rawIntegerField("name_qualification_length",
                                   variable->get_name_qualification_length()));
  fields.push_back(rawBoolField("type_elaboration_required",
                                variable->get_type_elaboration_required()));
  fields.push_back(rawBoolField("global_qualification_required",
                                variable->get_global_qualification_required()));
  std::ostringstream variables;
  variables << jsonString("variables") << ": [";
  const SgInitializedNamePtrList &names = variable->get_variables();
  if (!names.empty()) {
    variables << '\n';
    for (size_t i = 0; i < names.size(); ++i) {
      indent(variables, 4);
      variables << rawExternalInitializedNameJson(names[i], ids, function_name);
      if (i + 1 != names.size()) {
        variables << ',';
      }
      variables << '\n';
    }
    indent(variables, 2);
  }
  variables << "]";
  fields.push_back(variables.str());
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalRenamePairJson(SgRenamePair *rename,
                                      const std::string &function_name) {
  if (rename == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null SgUseStatement rename pair");
  }
  std::vector<std::string> fields;
  fields.push_back(rawStringField("kind", rename->sage_class_name()));
  fields.push_back(jsonString("location") + ": " + rawLocationJson(rename));
  fields.push_back(
      rawStringField("local_name", rename->get_local_name().getString()));
  fields.push_back(
      rawStringField("use_name", rename->get_use_name().getString()));
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalUseStatementJson(
    SgUseStatement *stmt,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (stmt == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " has a null parameter-scope use statement");
  }

  const uint64_t module_id = idFor(ids, stmt->get_module());
  std::vector<std::string> fields;
  fields.push_back(rawStringField("kind", stmt->sage_class_name()));
  fields.push_back(
      jsonString("location") + ": " +
      rawRequiredLocationJson(stmt, "external_function useStatement " +
                                        function_name));
  appendRawExternalDeclarationStatementFields(
      fields, stmt, "external_function " + function_name + " useStatement");
  fields.push_back(rawStringField("name", stmt->get_name().getString()));
  fields.push_back(rawBoolField("only_option", stmt->get_only_option()));
  fields.push_back(rawStringField("module_nature", stmt->get_module_nature()));
  fields.push_back(rawIntegerField("module", module_id));
  fields.push_back(jsonString("external_module") + ": " +
                   rawExternalModuleJson(stmt->get_module(), ids));
  std::ostringstream rename_list;
  rename_list << jsonString("rename_list") << ": [";
  const SgRenamePairPtrList &renames = stmt->get_rename_list();
  if (!renames.empty()) {
    rename_list << '\n';
    for (size_t i = 0; i < renames.size(); ++i) {
      indent(rename_list, 4);
      rename_list << rawExternalRenamePairJson(renames[i], function_name);
      if (i + 1 != renames.size()) {
        rename_list << ',';
      }
      rename_list << '\n';
    }
    indent(rename_list, 2);
  }
  rename_list << "]";
  fields.push_back(rename_list.str());

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawExternalParameterScopeDeclarationJson(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
    return rawExternalVariableDeclarationJson(variable, ids, function_name);
  }
  if (SgUseStatement *stmt = isSgUseStatement(decl)) {
    return rawExternalUseStatementJson(stmt, ids, function_name);
  }
  throw std::runtime_error("AST JSON external_function " + function_name +
                           " functionParameterScope declaration kind is not "
                           "supported: " +
                           (decl != nullptr
                                ? std::string(decl->sage_class_name())
                                : std::string("<null>")));
}

std::string rawExternalFunctionParameterScopeJson(
    SgFunctionParameterScope *scope,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name) {
  std::vector<std::string> fields;
  fields.push_back(rawBoolField("present", scope != nullptr));
  if (scope != nullptr) {
    std::map<SgInitializedName *, std::pair<size_t, size_t>> variable_indices;
    std::map<SgDeclarationStatement *, size_t> declaration_indices;
    std::ostringstream declarations;
    declarations << jsonString("declarations") << ": [";
    const SgDeclarationStatementPtrList &decls = scope->get_declarations();
    if (!decls.empty()) {
      declarations << '\n';
      for (size_t i = 0; i < decls.size(); ++i) {
        if (decls[i] != nullptr) {
          declaration_indices[decls[i]] = i;
        }
        SgVariableDeclaration *variable = isSgVariableDeclaration(decls[i]);
        if (variable != nullptr) {
          const SgInitializedNamePtrList &names = variable->get_variables();
          for (size_t j = 0; j < names.size(); ++j) {
            if (names[j] != nullptr) {
              variable_indices[names[j]] = std::make_pair(i, j);
            }
          }
        }
        indent(declarations, 4);
        declarations << rawExternalParameterScopeDeclarationJson(decls[i], ids,
                                                                 function_name);
        if (i + 1 != decls.size()) {
          declarations << ',';
        }
        declarations << '\n';
      }
      indent(declarations, 2);
    }
    declarations << "]";
    SgSymbolTable *table = scope->get_symbol_table();
    const size_t table_size = table != nullptr && table->get_table() != nullptr
                                  ? table->get_table()->size()
                                  : 0;
    fields.push_back(jsonString("location") + ": " +
                     rawRequiredLocationJson(
                         scope, "external_function functionParameterScope " +
                                    function_name));
    fields.push_back(declarations.str());
    fields.push_back(
        rawIntegerField("symbol_table_size", static_cast<int64_t>(table_size)));
    fields.push_back(rawBoolField("symbol_table_present", table != nullptr));
    if (table != nullptr) {
      fields.push_back(rawBoolField("symbol_table_case_insensitive",
                                    table->isCaseInsensitive()));
      std::ostringstream entries;
      entries << jsonString("symbol_table") << ": [";
      if (table->get_table() != nullptr && !table->get_table()->empty()) {
        std::vector<SymbolTableEntryJson> serialized_entries;
        for (const std::pair<const SgName, SgSymbol *> &entry :
             *table->get_table()) {
          SgSymbol *symbol = entry.second;
          if (symbol == nullptr) {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope symbol table has a null entry: " +
                entry.first.getString());
          }
          SgVariableSymbol *variable_symbol = isSgVariableSymbol(symbol);
          SgInitializedName *declaration =
              variable_symbol != nullptr ? variable_symbol->get_declaration()
                                         : nullptr;
          auto index = variable_indices.find(declaration);
          std::vector<std::string> entry_fields;
          entry_fields.push_back(
              rawStringField("entry_name", entry.first.getString()));
          entry_fields.push_back(
              rawStringField("symbol_kind", symbol->class_name()));
          entry_fields.push_back(rawBoolField(
              "lookup_preferred",
              symbolIsLookupPreferred(table, entry.first, symbol)));
          if (variable_symbol != nullptr && index != variable_indices.end()) {
            entry_fields.push_back(
                rawIntegerField("declaration_index",
                                static_cast<int64_t>(index->second.first)));
            entry_fields.push_back(rawIntegerField(
                "variable_index", static_cast<int64_t>(index->second.second)));
          } else {
            entry_fields.push_back(jsonString("symbol") + ": " +
                                   rawSymbolRef(symbol, ids));
          }
          if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
            if (alias->get_alias() == nullptr) {
              throw std::runtime_error("AST JSON external_function " +
                                       function_name +
                                       " functionParameterScope SgAliasSymbol "
                                       "has no alias target: " +
                                       entry.first.getString());
            }
            entry_fields.push_back(jsonString("alias_target") + ": " +
                                   rawSymbolRef(alias->get_alias(), ids));
            entry_fields.push_back(
                rawBoolField("alias_is_renamed", alias->get_isRenamed()));
            entry_fields.push_back(rawStringField(
                "alias_new_name", alias->get_new_name().getString()));
            std::ostringstream causal_nodes;
            causal_nodes << jsonString("alias_causal_nodes") << ": [";
            const SgNodePtrList &nodes = alias->get_causal_nodes();
            if (!nodes.empty()) {
              causal_nodes << '\n';
              for (size_t i = 0; i < nodes.size(); ++i) {
                SgNode *causal_node = nodes[i];
                const uint64_t causal_id = idFor(ids, causal_node);
                auto declaration_index = declaration_indices.find(
                    isSgDeclarationStatement(causal_node));
                if (causal_node != nullptr && causal_id == 0 &&
                    declaration_index == declaration_indices.end()) {
                  throw std::runtime_error(
                      "AST JSON external_function " + function_name +
                      " functionParameterScope SgAliasSymbol causal node is "
                      "neither collected nor a nested declaration: " +
                      causal_node->sage_class_name());
                }
                std::vector<std::string> causal_fields;
                causal_fields.push_back(
                    rawIntegerField("node", static_cast<int64_t>(causal_id)));
                causal_fields.push_back(rawIntegerField(
                    "declaration_index",
                    declaration_index != declaration_indices.end()
                        ? static_cast<int64_t>(declaration_index->second)
                        : static_cast<int64_t>(-1)));
                indent(causal_nodes, 6);
                writeRawObject(causal_nodes, 6, causal_fields, false);
                if (i + 1 != nodes.size()) {
                  causal_nodes << ',';
                }
                causal_nodes << '\n';
              }
              indent(causal_nodes, 4);
            }
            causal_nodes << "]";
            entry_fields.push_back(causal_nodes.str());
          }
          if (SgRenameSymbol *rename = isSgRenameSymbol(symbol)) {
            if (rename->get_original_symbol() == nullptr) {
              throw std::runtime_error(
                  "AST JSON external_function " + function_name +
                  " functionParameterScope "
                  "SgRenameSymbol has no original symbol: " +
                  entry.first.getString());
            }
            entry_fields.push_back(
                jsonString("original_symbol") + ": " +
                rawSymbolRef(rename->get_original_symbol(), ids));
            entry_fields.push_back(rawStringField(
                "rename_new_name", rename->get_new_name().getString()));
          }
          std::ostringstream entry_out;
          writeRawObject(entry_out, 0, entry_fields, false);
          std::string entry_json = entry_out.str();
          if (!entry_json.empty() && entry_json.back() == '\n') {
            entry_json.pop_back();
          }

          SymbolTableEntryJson serialized;
          serialized.entry_name = entry.first.getString();
          serialized.symbol_kind = symbol->class_name();
          serialized.basis_id = idFor(ids, symbolBasis(symbol));
          serialized.lookup_preferred =
              symbolIsLookupPreferred(table, entry.first, symbol);
          serialized.json = std::move(entry_json);
          serialized_entries.push_back(std::move(serialized));
        }

        std::stable_sort(serialized_entries.begin(), serialized_entries.end(),
                         [](const SymbolTableEntryJson &lhs,
                            const SymbolTableEntryJson &rhs) {
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
                             return lhs.lookup_preferred &&
                                    !rhs.lookup_preferred;
                           }
                           if (lhs.entry_name != rhs.entry_name) {
                             return lhs.entry_name < rhs.entry_name;
                           }
                           return lhs.symbol_kind < rhs.symbol_kind;
                         });

        entries << '\n';
        for (size_t i = 0; i < serialized_entries.size(); ++i) {
          indent(entries, 4);
          entries << serialized_entries[i].json;
          if (i + 1 != serialized_entries.size()) {
            entries << ',';
          }
          entries << '\n';
        }
        indent(entries, 2);
      }
      entries << "]";
      fields.push_back(entries.str());
    }
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool isExternalUseModuleEdge(
    SgNode *source, SgNode *target, const std::string &field,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  return field == "module" && isSgUseStatement(source) != nullptr &&
         isSgModuleStatement(target) != nullptr && idFor(ids, target) == 0;
}

std::string
rawNodeProperties(SgNode *node,
                  const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  fields.push_back(rawStringField("unparse", safeNodeText(node)));
  fields.push_back(jsonString("attributes") + ": " +
                   rawAstAttributesJson(node));
  fields.push_back(jsonString("qualified_name_state") + ": " +
                   rawQualifiedNameStateJson(node, ids));
  if (SgScopeStatement *scope = isSgScopeStatement(node)) {
    fields.push_back(
        rawBoolField("case_insensitive", scope->isCaseInsensitive()));
    fields.push_back(jsonString("symbol_table") + ": " +
                     rawSymbolTableJson(scope, ids));
  }

  if (SgSourceFile *file = isSgSourceFile(node)) {
    fields.push_back(rawStringField("source_filename_with_path",
                                    file->get_sourceFileNameWithPath()));
    fields.push_back(rawStringField("source_filename_without_path",
                                    file->get_sourceFileNameWithoutPath()));
    fields.push_back(rawStringField("unparse_output_filename",
                                    file->get_unparse_output_filename()));
    fields.push_back(rawBoolField("C_only", file->get_C_only()));
    fields.push_back(rawBoolField("Cxx_only", file->get_Cxx_only()));
    fields.push_back(rawBoolField("Fortran_only", file->get_Fortran_only()));
    fields.push_back(
        rawBoolField("CoArrayFortran_only", file->get_CoArrayFortran_only()));
    fields.push_back(rawBoolField("Cuda_only", file->get_Cuda_only()));
    fields.push_back(rawBoolField("OpenCL_only", file->get_OpenCL_only()));
    fields.push_back(rawBoolField("requires_C_preprocessor",
                                  file->get_requires_C_preprocessor()));
    fields.push_back(rawIntegerField("input_format", file->get_inputFormat()));
    fields.push_back(
        rawIntegerField("output_format", file->get_outputFormat()));
    fields.push_back(rawIntegerField("backend_compile_format",
                                     file->get_backendCompileFormat()));
    fields.push_back(rawBoolField("fortran_implicit_none",
                                  file->get_fortran_implicit_none()));
    fields.push_back(
        rawIntegerField("input_language", file->get_inputLanguage()));
    fields.push_back(
        rawIntegerField("output_language", file->get_outputLanguage()));
    fields.push_back(rawBoolField("strict_language_handling",
                                  file->get_strict_language_handling()));
    fields.push_back(rawBoolField("source_uses_cpp_extension",
                                  file->get_sourceFileUsesCppFileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran_extension",
                     file->get_sourceFileUsesFortranFileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran77_extension",
                     file->get_sourceFileUsesFortran77FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran90_extension",
                     file->get_sourceFileUsesFortran90FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran95_extension",
                     file->get_sourceFileUsesFortran95FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran2003_extension",
                     file->get_sourceFileUsesFortran2003FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_fortran2008_extension",
                     file->get_sourceFileUsesFortran2008FileExtension()));
    fields.push_back(
        rawBoolField("source_uses_coarray_fortran_extension",
                     file->get_sourceFileUsesCoArrayFortranFileExtension()));
    fields.push_back(rawBoolField("source_file_type_is_unknown",
                                  file->get_sourceFileTypeIsUnknown()));
    fields.push_back(rawBoolField("experimental_flang_frontend",
                                  file->get_experimental_flang_frontend()));
  } else if (SgToken *token = isSgToken(node)) {
    fields.push_back(
        rawStringField("lexeme_string", token->get_lexeme_string()));
    fields.push_back(rawIntegerField(
        "classification_code",
        static_cast<int64_t>(token->get_classification_code())));
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    fields.push_back(rawStringField("name", name->get_name().getString()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(name->get_typeptr(), ids));
    fields.push_back(rawIntegerField(
        "storage_modifier",
        static_cast<int>(name->get_storageModifier().get_modifier())));
    fields.push_back(rawStringField("gnu_attribute_section_name",
                                    name->get_gnu_attribute_section_name()));
    fields.push_back(rawIntegerField("name_qualification_length",
                                     name->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  name->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  name->get_global_qualification_required()));
    fields.push_back(
        rawIntegerField("name_qualification_length_for_type",
                        name->get_name_qualification_length_for_type()));
    fields.push_back(
        rawBoolField("type_elaboration_required_for_type",
                     name->get_type_elaboration_required_for_type()));
    fields.push_back(
        rawBoolField("global_qualification_required_for_type",
                     name->get_global_qualification_required_for_type()));
  } else if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(jsonString("function_type") + ": " +
                     rawTypeJson(decl->get_type(), ids));
    if (functionParameterScopeNeedsExternalReferenceRecord(decl, ids)) {
      fields.push_back(
          rawStringField("external_function_parameter_scope_source",
                         externalFunctionParameterScopeSource(decl, ids)));
      fields.push_back(jsonString("external_function_parameter_scope") + ": " +
                       rawExternalFunctionParameterScopeJson(
                           decl->get_functionParameterScope(), ids,
                           decl->get_name().getString()));
    }
    fields.push_back(jsonString("return_type") + ": " +
                     rawTypeJson(decl->get_type() != nullptr
                                     ? decl->get_type()->get_return_type()
                                     : nullptr,
                                 ids));
    fields.push_back(
        jsonString("function_modifier_vector") + ": " +
        rawBitVectorJson(decl->get_functionModifier().get_modifierVector()));
    fields.push_back(
        jsonString("special_function_modifier_vector") + ": " +
        rawBitVectorJson(
            decl->get_specialFunctionModifier().get_modifierVector()));
    fields.push_back(rawBoolField("named_in_end_statement",
                                  decl->get_named_in_end_statement()));
    fields.push_back(rawStringField("asm_name", decl->get_asm_name()));
    fields.push_back(
        rawBoolField("old_style_definition", decl->get_oldStyleDefinition()));
    fields.push_back(rawIntegerField(
        "specialization", static_cast<int>(decl->get_specialization())));
    fields.push_back(
        rawBoolField("requires_name_qualification_on_return_type",
                     decl->get_requiresNameQualificationOnReturnType()));
    fields.push_back(rawStringField("gnu_extension_section",
                                    decl->get_gnu_extension_section()));
    fields.push_back(
        rawStringField("gnu_extension_alias", decl->get_gnu_extension_alias()));
    fields.push_back(rawIntegerField(
        "gnu_extension_visability",
        static_cast<int>(decl->get_gnu_extension_visability())));
    fields.push_back(rawIntegerField("name_qualification_length",
                                     decl->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  decl->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  decl->get_global_qualification_required()));
    fields.push_back(
        rawIntegerField("name_qualification_length_for_return_type",
                        decl->get_name_qualification_length_for_return_type()));
    fields.push_back(
        rawBoolField("type_elaboration_required_for_return_type",
                     decl->get_type_elaboration_required_for_return_type()));
    fields.push_back(rawBoolField(
        "global_qualification_required_for_return_type",
        decl->get_global_qualification_required_for_return_type()));
    fields.push_back(rawBoolField("prototype_is_without_parameters",
                                  decl->get_prototypeIsWithoutParameters()));
    fields.push_back(rawIntegerField("gnu_regparm_attribute",
                                     decl->get_gnu_regparm_attribute()));
    fields.push_back(rawBoolField("type_syntax_is_available",
                                  decl->get_type_syntax_is_available()));
    fields.push_back(rawBoolField("using_c11_noreturn_keyword",
                                  decl->get_using_C11_Noreturn_keyword()));
    fields.push_back(rawBoolField("is_constexpr", decl->get_is_constexpr()));
    fields.push_back(
        rawBoolField("using_new_function_return_type_syntax",
                     decl->get_using_new_function_return_type_syntax()));
    fields.push_back(
        rawBoolField("is_deduction_guide", decl->get_is_deduction_guide()));
    fields.push_back(
        rawBoolField("marked_as_frontend_normalization",
                     decl->get_marked_as_frontend_normalization()));
    fields.push_back(
        rawBoolField("is_implicit_function", decl->get_is_implicit_function()));
    if (SgProcedureHeaderStatement *procedure =
            isSgProcedureHeaderStatement(decl)) {
      fields.push_back(
          rawIntegerField("subprogram_kind", procedure->get_subprogram_kind()));
    }
    if (SgTemplateInstantiationFunctionDecl *tmpl =
            isSgTemplateInstantiationFunctionDecl(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(rawBoolField("name_reset_from_mangled_form",
                                    tmpl->get_nameResetFromMangledForm()));
      fields.push_back(
          rawBoolField("template_argument_list_is_explicit",
                       tmpl->get_template_argument_list_is_explicit()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
    if (SgTemplateInstantiationMemberFunctionDecl *tmpl =
            isSgTemplateInstantiationMemberFunctionDecl(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(rawBoolField("name_reset_from_mangled_form",
                                    tmpl->get_nameResetFromMangledForm()));
      fields.push_back(
          rawBoolField("template_argument_list_is_explicit",
                       tmpl->get_template_argument_list_is_explicit()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
  } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(jsonString("base_type") + ": " +
                     rawTypeJson(decl->get_base_type(), ids));
    fields.push_back(
        rawBoolField("typedef_base_type_contains_defining_declaration",
                     decl->get_typedefBaseTypeContainsDefiningDeclaration()));
    fields.push_back(rawBoolField("is_autonomous_declaration",
                                  decl->get_isAutonomousDeclaration()));
    if (SgTemplateInstantiationTypedefDeclaration *tmpl =
            isSgTemplateInstantiationTypedefDeclaration(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(rawStringField("template_header",
                                      tmpl->get_templateHeader().getString()));
      fields.push_back(rawBoolField("name_reset_from_mangled_form",
                                    tmpl->get_nameResetFromMangledForm()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
  } else if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawIntegerField("class_type", decl->get_class_type()));
    fields.push_back(rawBoolField("is_unnamed", decl->get_isUnNamed()));
    fields.push_back(rawBoolField("is_autonomous_declaration",
                                  decl->get_isAutonomousDeclaration()));
    if (SgTemplateClassDeclaration *tmpl = isSgTemplateClassDeclaration(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
    }
    if (SgTemplateInstantiationDecl *tmpl =
            isSgTemplateInstantiationDecl(decl)) {
      fields.push_back(rawStringField("template_name",
                                      tmpl->get_templateName().getString()));
      fields.push_back(rawStringField("template_header",
                                      tmpl->get_templateHeader().getString()));
      fields.push_back(rawBoolField("name_reset_from_mangled_form",
                                    tmpl->get_nameResetFromMangledForm()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_evaluated",
                       tmpl->get_constraintSatisfactionEvaluated()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_satisfied",
                       tmpl->get_constraintSatisfactionSatisfied()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_contains_errors",
                       tmpl->get_constraintSatisfactionContainsErrors()));
      fields.push_back(
          rawBoolField("constraint_satisfaction_substitution_failure",
                       tmpl->get_constraintSatisfactionSubstitutionFailure()));
      fields.push_back(
          rawStringField("constraint_satisfaction_summary",
                         tmpl->get_constraintSatisfactionSummary()));
      fields.push_back(
          rawBoolField("sfinae_evaluated", tmpl->get_sfinaeEvaluated()));
      fields.push_back(rawBoolField("sfinae_substitution_failure",
                                    tmpl->get_sfinaeSubstitutionFailure()));
      fields.push_back(
          rawStringField("sfinae_summary", tmpl->get_sfinaeSummary()));
      fields.push_back(
          jsonString("template_arguments") + ": " +
          rawTemplateArgumentListJson(tmpl->get_templateArguments(), ids));
      fields.push_back(jsonString("deduced_template_arguments") + ": " +
                       rawTemplateArgumentListJson(
                           tmpl->get_deducedTemplateArguments(), ids));
    }
  } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(rawBoolField("embedded", decl->get_embedded()));
    fields.push_back(rawBoolField("is_unnamed", decl->get_isUnNamed()));
    fields.push_back(rawBoolField("is_autonomous_declaration",
                                  decl->get_isAutonomousDeclaration()));
    fields.push_back(rawBoolField("is_scoped_enum", decl->get_isScopedEnum()));
    fields.push_back(jsonString("field_type") + ": " +
                     rawTypeJson(decl->get_field_type(), ids));
  } else if (SgNamespaceDeclarationStatement *decl =
                 isSgNamespaceDeclarationStatement(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(
        rawBoolField("is_unnamed_namespace", decl->get_isUnnamedNamespace()));
    fields.push_back(
        rawBoolField("is_inlined_namespace", decl->get_isInlinedNamespace()));
  } else if (SgNamespaceDefinitionStatement *def =
                 isSgNamespaceDefinitionStatement(node)) {
    fields.push_back(
        rawBoolField("is_union_of_reentrant_namespace_definitions",
                     def->get_isUnionOfReentrantNamespaceDefinitions()));
  } else if (SgUsingDirectiveStatement *stmt =
                 isSgUsingDirectiveStatement(node)) {
    SgNamespaceDeclarationStatement *decl = stmt->get_namespaceDeclaration();
    fields.push_back(
        rawIntegerField("namespace_declaration", idFor(ids, decl)));
    fields.push_back(rawStringField(
        "namespace_name", decl != nullptr ? decl->get_name().getString() : ""));
    fields.push_back(
        rawBoolField("namespace_is_unnamed",
                     decl != nullptr ? decl->get_isUnnamedNamespace() : false));
  } else if (SgUsingDeclarationStatement *stmt =
                 isSgUsingDeclarationStatement(node)) {
    SgDeclarationStatement *decl = stmt->get_declaration();
    SgInitializedName *name = stmt->get_initializedName();
    fields.push_back(rawIntegerField("declaration", idFor(ids, decl)));
    fields.push_back(
        rawStringField("declaration_name",
                       decl != nullptr ? SageInterface::get_name(decl) : ""));
    fields.push_back(rawIntegerField("initialized_name", idFor(ids, name)));
    fields.push_back(
        rawStringField("initialized_name_name",
                       name != nullptr ? name->get_name().getString() : ""));
    fields.push_back(
        jsonString("initialized_name_type") + ": " +
        rawTypeJson(name != nullptr ? name->get_typeptr() : nullptr, ids));
  } else if (SgUseStatement *stmt = isSgUseStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
    fields.push_back(rawBoolField("only_option", stmt->get_only_option()));
    fields.push_back(
        rawStringField("module_nature", stmt->get_module_nature()));
    fields.push_back(rawIntegerField("module", idFor(ids, stmt->get_module())));
    fields.push_back(jsonString("external_module") + ": " +
                     rawExternalModuleJson(stmt->get_module(), ids));
  } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    fields.push_back(rawStringField("name", decl->get_name().getString()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(decl->get_type(), ids));
    fields.push_back(rawIntegerField("template_parameter_position",
                                     decl->get_template_parameter_position()));
    fields.push_back(rawIntegerField("template_parameter_depth",
                                     decl->get_template_parameter_depth()));
    fields.push_back(
        rawBoolField("is_class_member", decl->get_is_class_member()));
    fields.push_back(
        rawBoolField("is_template_param", decl->get_is_template_param()));
    fields.push_back(rawBoolField("is_template_template_param",
                                  decl->get_is_template_template_param()));
    fields.push_back(
        rawBoolField("has_template_keyword", decl->get_has_template_keyword()));
    fields.push_back(
        rawBoolField("has_global_qualifier", decl->get_has_global_qualifier()));
    fields.push_back(
        rawBoolField("suppress_typename", decl->get_suppress_typename()));
    fields.push_back(
        rawBoolField("is_nonreal_template", decl->get_is_nonreal_template()));
    fields.push_back(rawBoolField("is_concept", decl->get_is_concept()));
    fields.push_back(
        rawBoolField("is_nonreal_function", decl->get_is_nonreal_function()));
    fields.push_back(jsonString("tpl_args") + ": " +
                     rawTemplateArgumentListJson(decl->get_tpl_args(), ids));
  } else if (SgBaseClass *base = isSgBaseClass(node)) {
    fields.push_back(
        rawIntegerField("base_class", idFor(ids, base->get_base_class())));
    fields.push_back(
        rawBoolField("is_direct_base_class", base->get_isDirectBaseClass()));
    fields.push_back(
        rawBoolField("pack_expansion", base->get_pack_expansion()));
    fields.push_back(rawIntegerField(
        "base_class_modifier",
        base->get_baseClassModifier() != nullptr
            ? static_cast<int>(base->get_baseClassModifier()->get_modifier())
            : static_cast<int>(SgBaseClassModifier::e_unknown)));
    fields.push_back(
        rawIntegerField("base_class_access_modifier",
                        base->get_baseClassModifier() != nullptr
                            ? static_cast<int>(base->get_baseClassModifier()
                                                   ->get_accessModifier()
                                                   .get_modifier())
                            : static_cast<int>(SgAccessModifier::e_unknown)));
    fields.push_back(rawBoolField("base_class_access_is_explicit",
                                  base->get_baseClassModifier() != nullptr
                                      ? base->get_baseClassModifier()
                                            ->get_accessModifier()
                                            .get_is_explicit()
                                      : false));
    fields.push_back(rawIntegerField("name_qualification_length",
                                     base->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  base->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  base->get_global_qualification_required()));
    if (SgExpBaseClass *expr_base = isSgExpBaseClass(base)) {
      fields.push_back(jsonString("base_class_exp") + ": " +
                       rawExpressionRef(expr_base->get_base_class_exp(), ids));
    }
    if (SgNonrealBaseClass *nonreal_base = isSgNonrealBaseClass(base)) {
      fields.push_back(
          rawIntegerField("base_class_nonreal",
                          idFor(ids, nonreal_base->get_base_class_nonreal())));
    }
  } else if (SgPragma *pragma = isSgPragma(node)) {
    fields.push_back(rawStringField("name", pragma->get_name()));
  } else if (SgTypeTraitBuiltinOperator *op =
                 isSgTypeTraitBuiltinOperator(node)) {
    fields.push_back(rawStringField("name", op->get_name().getString()));
    fields.push_back(jsonString("args") + ": " +
                     rawTypeTraitArgsJson(op->get_args(), ids));
  } else if (SgRenamePair *rename = isSgRenamePair(node)) {
    fields.push_back(
        rawStringField("local_name", rename->get_local_name().getString()));
    fields.push_back(
        rawStringField("use_name", rename->get_use_name().getString()));
  } else if (SgCommonBlockObject *object = isSgCommonBlockObject(node)) {
    fields.push_back(rawStringField("block_name", object->get_block_name()));
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    const SgStorageModifier &storage =
        decl->get_declarationModifier().get_storageModifier();
    fields.push_back(
        rawIntegerField("decl_attributes", decl->get_decl_attributes()));
    fields.push_back(rawStringField("linkage", decl->get_linkage()));
    fields.push_back(
        jsonString("declaration_modifier_vector") + ": " +
        rawBitVectorJson(decl->get_declarationModifier().get_modifierVector()));
    fields.push_back(jsonString("declaration_type_modifier_vector") + ": " +
                     rawBitVectorJson(decl->get_declarationModifier()
                                          .get_typeModifier()
                                          .get_modifierVector()));
    fields.push_back(rawIntegerField("declaration_storage_modifier",
                                     static_cast<int>(storage.get_modifier())));
    fields.push_back(
        rawIntegerField("declaration_access_modifier",
                        static_cast<int>(decl->get_declarationModifier()
                                             .get_accessModifier()
                                             .get_modifier())));
    fields.push_back(rawBoolField("declaration_access_is_explicit",
                                  decl->get_declarationModifier()
                                      .get_accessModifier()
                                      .get_is_explicit()));
    fields.push_back(rawBoolField("name_only", decl->get_nameOnly()));
    fields.push_back(rawBoolField("forward", decl->get_forward()));
    fields.push_back(rawBoolField("extern_brace", decl->get_externBrace()));
    fields.push_back(
        rawBoolField("skip_elaborate_type", decl->get_skipElaborateType()));
    fields.push_back(
        rawStringField("binding_label", decl->get_binding_label()));
    fields.push_back(
        rawBoolField("unparse_template_ast", decl->get_unparse_template_ast()));
    fields.push_back(rawStringField(
        "declaration_gnu_attribute_section_name",
        decl->get_declarationModifier().get_gnu_attribute_section_name()));
    fields.push_back(rawIntegerField(
        "declaration_gnu_attribute_visability",
        static_cast<int>(
            decl->get_declarationModifier().get_gnu_attribute_visability())));
    fields.push_back(jsonString("external_first_nondefining_declaration") +
                     ": " +
                     rawExternalDeclarationReferenceJson(
                         decl->get_firstNondefiningDeclaration(), ids));
    fields.push_back(jsonString("external_defining_declaration") + ": " +
                     rawExternalDeclarationReferenceJson(
                         decl->get_definingDeclaration(), ids));
  }

  if (SgExpression *expr = isSgExpression(node)) {
    addExpressionType(fields, expr, ids);
    fields.push_back(rawBoolField("lvalue", expr->get_lvalue()));
    fields.push_back(rawBoolField("need_paren", expr->get_need_paren()));
    fields.push_back(rawBoolField("global_qualified_name",
                                  expr->get_global_qualified_name()));
    addExpressionQualificationFields(fields, expr);
    if (SgFunctionCallExp *call = isSgFunctionCallExp(expr)) {
      fields.push_back(rawBoolField("uses_operator_syntax",
                                    call->get_uses_operator_syntax()));
    }
    if (SgTypeExpression *type_expr = isSgTypeExpression(expr)) {
      fields.push_back(
          rawStringField("type_spelling", type_expr->unparseToString()));
    } else if (SgSubscriptExpression *subscript =
                   isSgSubscriptExpression(expr)) {
      if (serializingTypeOwnedExpression) {
        fields.push_back(
            jsonString("lower_bound") + ": " +
            rawTypeOwnedExpressionRef(subscript->get_lowerBound(), ids));
        fields.push_back(
            jsonString("upper_bound") + ": " +
            rawTypeOwnedExpressionRef(subscript->get_upperBound(), ids));
        fields.push_back(
            jsonString("stride") + ": " +
            rawTypeOwnedExpressionRef(subscript->get_stride(), ids));
      }
    }
    if (serializingTypeOwnedExpression) {
      if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
        fields.push_back(
            jsonString("operand") + ": " +
            rawTypeOwnedExpressionRef(unary->get_operand_i(), ids));
      }
      if (SgBinaryOp *binary = isSgBinaryOp(expr)) {
        fields.push_back(
            jsonString("lhs_operand") + ": " +
            rawTypeOwnedExpressionRef(binary->get_lhs_operand_i(), ids));
        fields.push_back(
            jsonString("rhs_operand") + ": " +
            rawTypeOwnedExpressionRef(binary->get_rhs_operand_i(), ids));
      }
    }
  }

  if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    fields.push_back(
        rawIntegerField("argument_type", argument->get_argumentType()));
    fields.push_back(rawBoolField("is_array_bound_unknown_type",
                                  argument->get_isArrayBoundUnknownType()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(argument->get_type(), ids));
    fields.push_back(jsonString("expression") + ": " +
                     rawExpressionRef(argument->get_expression(), ids));
    fields.push_back(
        rawIntegerField("template_declaration",
                        idFor(ids, argument->get_templateDeclaration())));
    fields.push_back(rawIntegerField(
        "initialized_name", idFor(ids, argument->get_initializedName())));
    fields.push_back(rawBoolField("explicitly_specified",
                                  argument->get_explicitlySpecified()));
    fields.push_back(
        rawBoolField("is_pack_element", argument->get_is_pack_element()));
  }
  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    fields.push_back(
        rawIntegerField("parameter_type", parameter->get_parameterType()));
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(parameter->get_type(), ids));
    fields.push_back(jsonString("default_type_parameter") + ": " +
                     rawTypeJson(parameter->get_defaultTypeParameter(), ids));
    fields.push_back(jsonString("expression") + ": " +
                     rawExpressionRef(parameter->get_expression(), ids));
    fields.push_back(jsonString("type_constraint") + ": " +
                     rawExpressionRef(parameter->get_typeConstraint(), ids));
    fields.push_back(
        jsonString("default_expression_parameter") + ": " +
        rawExpressionRef(parameter->get_defaultExpressionParameter(), ids));
    fields.push_back(
        rawIntegerField("template_declaration",
                        idFor(ids, parameter->get_templateDeclaration())));
    fields.push_back(rawIntegerField(
        "default_template_declaration_parameter",
        idFor(ids, parameter->get_defaultTemplateDeclarationParameter())));
    fields.push_back(rawIntegerField(
        "initialized_name", idFor(ids, parameter->get_initializedName())));
    fields.push_back(
        rawIntegerField("template_parameter_keyword",
                        parameter->get_templateParameterKeyword()));
    fields.push_back(
        rawBoolField("is_abbreviated_function_template_parameter",
                     parameter->get_isAbbreviatedFunctionTemplateParameter()));
    fields.push_back(
        rawBoolField("is_parameter_pack", parameter->get_is_parameter_pack()));
  }

  if (SgActualArgumentExpression *actual = isSgActualArgumentExpression(node)) {
    fields.push_back(rawStringField("argument_name",
                                    actual->get_argument_name().getString()));
  }

  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    fields.push_back(rawIntegerField("symbol_declaration",
                                     varRefSymbolDeclarationId(ref, ids)));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
  } else if (SgLabelRefExp *ref = isSgLabelRefExp(node)) {
    if (ref->get_symbol() == nullptr) {
      throw std::runtime_error("AST JSON SgLabelRefExp has no symbol");
    }
    fields.push_back(rawIntegerField(
        "symbol_declaration", idFor(ids, symbolBasis(ref->get_symbol()))));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(node)) {
    SgFunctionSymbol *symbol = ref->get_symbol();
    SgFunctionDeclaration *decl =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    const uint64_t decl_id = idFor(ids, decl);
    const bool external_decl =
        decl != nullptr && (isAstJsonExternalFunction(decl) ||
                            (decl_id == 0 && !insideCollectionBoundary(decl)));
    if (symbol == nullptr || (decl_id == 0 && !external_decl)) {
      std::ostringstream message;
      message
          << "AST JSON SgFunctionRefExp symbol declaration was not collected";
      if (symbol != nullptr) {
        message << ": " << symbol->get_name().getString();
        if (decl != nullptr) {
          message << " declaration=" << decl->sage_class_name() << "("
                  << decl->get_name().getString() << ")";
          if (SgNode *parent = decl->get_parent()) {
            message << " declaration_parent=" << parent->sage_class_name();
          }
          std::ostringstream declaration_chain;
          for (SgNode *current = decl; current != nullptr;
               current = current->get_parent()) {
            if (declaration_chain.tellp() > 0) {
              declaration_chain << " <- ";
            }
            declaration_chain << current->sage_class_name();
          }
          message << " declaration_chain=[" << declaration_chain.str() << "]";
        }
      }
      std::ostringstream parent_chain;
      for (SgNode *current = ref; current != nullptr;
           current = current->get_parent()) {
        if (parent_chain.tellp() > 0) {
          parent_chain << " <- ";
        }
        parent_chain << current->sage_class_name();
      }
      message << " parent_chain=[" << parent_chain.str() << "]";
      throw std::runtime_error(message.str());
    }
    fields.push_back(
        rawIntegerField("symbol_declaration", external_decl ? 0 : decl_id));
    fields.push_back(
        rawStringField("symbol_name", symbol->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));
    fields.push_back(jsonString("external_function") + ": " +
                     rawExternalFunctionJson(decl, ids));
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(node)) {
    if (ref->get_symbol() == nullptr ||
        idFor(ids, ref->get_symbol()->get_declaration()) == 0) {
      throw std::runtime_error("AST JSON SgTemplateFunctionRefExp symbol "
                               "declaration was not collected");
    }
    fields.push_back(
        rawIntegerField("symbol_declaration",
                        idFor(ids, ref->get_symbol()->get_declaration())));
    fields.push_back(rawStringField("symbol_name",
                                    ref->get_symbol()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol(), ids));
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(node)) {
    if (ref->get_symbol_i() == nullptr ||
        idFor(ids, ref->get_symbol_i()->get_declaration()) == 0) {
      throw std::runtime_error("AST JSON SgMemberFunctionRefExp symbol "
                               "declaration was not collected");
    }
    fields.push_back(
        rawIntegerField("symbol_declaration",
                        idFor(ids, ref->get_symbol_i()->get_declaration())));
    fields.push_back(rawStringField(
        "symbol_name", ref->get_symbol_i()->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(ref->get_symbol_i(), ids));
    fields.push_back(rawIntegerField("virtual_call", ref->get_virtual_call()));
    fields.push_back(
        rawIntegerField("need_qualifier", ref->get_need_qualifier()));
  } else if (SgThisExp *expr = isSgThisExp(node)) {
    SgClassSymbol *class_symbol = expr->get_class_symbol();
    SgNonrealSymbol *nonreal_symbol = expr->get_nonreal_symbol();
    if (class_symbol == nullptr && nonreal_symbol == nullptr) {
      throw std::runtime_error(
          "AST JSON SgThisExp has neither class nor nonreal symbol");
    }
    if (class_symbol != nullptr &&
        idFor(ids, class_symbol->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgThisExp class symbol declaration was not collected");
    }
    if (nonreal_symbol != nullptr &&
        idFor(ids, nonreal_symbol->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgThisExp nonreal symbol declaration was not collected");
    }
    fields.push_back(rawIntegerField(
        "class_symbol_declaration",
        idFor(ids, class_symbol != nullptr ? class_symbol->get_declaration()
                                           : nullptr)));
    fields.push_back(rawStringField(
        "class_symbol_name",
        class_symbol != nullptr ? class_symbol->get_name().getString() : ""));
    fields.push_back(rawIntegerField(
        "nonreal_symbol_declaration",
        idFor(ids, nonreal_symbol != nullptr ? nonreal_symbol->get_declaration()
                                             : nullptr)));
    fields.push_back(rawStringField("nonreal_symbol_name",
                                    nonreal_symbol != nullptr
                                        ? nonreal_symbol->get_name().getString()
                                        : ""));
    fields.push_back(rawIntegerField("pobj_this", expr->get_pobj_this()));
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
    SgNonrealSymbol *symbol = ref->get_symbol();
    if (symbol == nullptr || idFor(ids, symbol->get_declaration()) == 0) {
      throw std::runtime_error(
          "AST JSON SgNonrealRefExp symbol declaration was not collected");
    }
    fields.push_back(rawIntegerField("symbol_declaration",
                                     idFor(ids, symbol->get_declaration())));
    fields.push_back(
        rawStringField("symbol_name", symbol->get_name().getString()));
    fields.push_back(jsonString("symbol") + ": " + rawSymbolRef(symbol, ids));
    fields.push_back(
        jsonString("template_arguments") + ": " +
        rawTemplateArgumentListJson(ref->get_templateArguments(), ids));
    fields.push_back(rawBoolField("constraint_satisfaction_evaluated",
                                  ref->get_constraintSatisfactionEvaluated()));
    fields.push_back(rawBoolField("constraint_satisfaction_satisfied",
                                  ref->get_constraintSatisfactionSatisfied()));
    fields.push_back(
        rawBoolField("constraint_satisfaction_contains_errors",
                     ref->get_constraintSatisfactionContainsErrors()));
    fields.push_back(
        rawBoolField("constraint_satisfaction_substitution_failure",
                     ref->get_constraintSatisfactionSubstitutionFailure()));
    fields.push_back(rawStringField("constraint_satisfaction_summary",
                                    ref->get_constraintSatisfactionSummary()));
    fields.push_back(
        rawBoolField("sfinae_evaluated", ref->get_sfinaeEvaluated()));
    fields.push_back(rawBoolField("sfinae_substitution_failure",
                                  ref->get_sfinaeSubstitutionFailure()));
    fields.push_back(
        rawStringField("sfinae_summary", ref->get_sfinaeSummary()));
  } else if (SgBoolValExp *value = isSgBoolValExp(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
  } else if (SgShortVal *value = isSgShortVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedShortVal *value = isSgUnsignedShortVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgIntVal *value = isSgIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedIntVal *value = isSgUnsignedIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgLongIntVal *value = isSgLongIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedLongVal *value = isSgUnsignedLongVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgLongLongIntVal *value = isSgLongLongIntVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedLongLongIntVal *value =
                 isSgUnsignedLongLongIntVal(node)) {
    fields.push_back(
        rawIntegerField("value", static_cast<int64_t>(value->get_value())));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgCharVal *value = isSgCharVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgUnsignedCharVal *value = isSgUnsignedCharVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgFloatVal *value = isSgFloatVal(node)) {
    fields.push_back(rawStringField("value", value->get_valueString()));
  } else if (SgDoubleVal *value = isSgDoubleVal(node)) {
    fields.push_back(rawStringField("value", value->get_valueString()));
  } else if (SgComplexVal *value = isSgComplexVal(node)) {
    fields.push_back(jsonString("precision_type") + ": " +
                     rawTypeJson(value->get_precisionType(), ids));
    fields.push_back(jsonString("real_value") + ": " +
                     rawTypeOwnedExpressionRef(value->get_real_value(), ids));
    fields.push_back(
        jsonString("imaginary_value") + ": " +
        rawTypeOwnedExpressionRef(value->get_imaginary_value(), ids));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgStringVal *value = isSgStringVal(node)) {
    fields.push_back(rawStringField("value", value->get_value()));
    fields.push_back(rawBoolField("wchar_string", value->get_wcharString()));
    fields.push_back(
        rawIntegerField("string_delimiter", value->get_stringDelimiter()));
    fields.push_back(
        rawBoolField("is_16bit_string", value->get_is16bitString()));
    fields.push_back(
        rawBoolField("is_32bit_string", value->get_is32bitString()));
    fields.push_back(rawBoolField("is_raw_string", value->get_isRawString()));
    fields.push_back(
        rawStringField("raw_string_value", value->get_raw_string_value()));
  } else if (SgEnumVal *value = isSgEnumVal(node)) {
    fields.push_back(rawIntegerField("value", value->get_value()));
    fields.push_back(rawStringField("name", value->get_name().getString()));
    fields.push_back(
        rawIntegerField("declaration", idFor(ids, value->get_declaration())));
    fields.push_back(rawBoolField("requires_name_qualification",
                                  value->get_requiresNameQualification()));
  } else if (SgTemplateParameterVal *value = isSgTemplateParameterVal(node)) {
    fields.push_back(rawIntegerField("template_parameter_position",
                                     value->get_template_parameter_position()));
    fields.push_back(rawStringField("value_string", value->get_valueString()));
  } else if (SgCastExp *cast = isSgCastExp(node)) {
    fields.push_back(rawIntegerField("cast_type", cast->cast_type()));
  } else if (SgConstructorInitializer *init =
                 isSgConstructorInitializer(node)) {
    fields.push_back(rawBoolField("need_name", init->get_need_name()));
    fields.push_back(
        rawBoolField("need_qualifier", init->get_need_qualifier()));
    fields.push_back(rawBoolField("need_parenthesis_after_name",
                                  init->get_need_parenthesis_after_name()));
    fields.push_back(rawBoolField("associated_class_unknown",
                                  init->get_associated_class_unknown()));
  } else if (SgStaticAssertionDeclaration *decl =
                 isSgStaticAssertionDeclaration(node)) {
    fields.push_back(rawStringField("string_literal",
                                    decl->get_string_literal().getString()));
  } else if (SgBreakStmt *stmt = isSgBreakStmt(node)) {
    fields.push_back(
        rawStringField("do_string_label", stmt->get_do_string_label()));
  } else if (SgContinueStmt *stmt = isSgContinueStmt(node)) {
    fields.push_back(
        rawStringField("do_string_label", stmt->get_do_string_label()));
  } else if (SgProcessControlStatement *stmt =
                 isSgProcessControlStatement(node)) {
    fields.push_back(rawIntegerField("control_kind", stmt->get_control_kind()));
  } else if (SgAttributeSpecificationStatement *stmt =
                 isSgAttributeSpecificationStatement(node)) {
    fields.push_back(
        rawIntegerField("attribute_kind", stmt->get_attribute_kind()));
    fields.push_back(rawIntegerField("intent", stmt->get_intent()));
    fields.push_back(jsonString("name_list") + ": " +
                     rawStringListJson(stmt->get_name_list()));
  } else if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
    fields.push_back(rawIntegerField("generic_spec", stmt->get_generic_spec()));
  } else if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    fields.push_back(
        rawStringField("function_name", body->get_function_name().getString()));
    fields.push_back(
        rawBoolField("use_function_name", body->get_use_function_name()));
    fields.push_back(rawIntegerField(
        "function_declaration", idFor(ids, body->get_functionDeclaration())));
  } else if (SgCaseOptionStmt *stmt = isSgCaseOptionStmt(node)) {
    fields.push_back(
        rawStringField("case_construct_name", stmt->get_case_construct_name()));
  } else if (SgDefaultOptionStmt *stmt = isSgDefaultOptionStmt(node)) {
    fields.push_back(rawStringField("default_construct_name",
                                    stmt->get_default_construct_name()));
  } else if (SgSizeOfOp *size_of = isSgSizeOfOp(node)) {
    fields.push_back(jsonString("operand_type") + ": " +
                     rawTypeJson(size_of->get_operand_type(), ids));
    fields.push_back(
        rawBoolField("size_of_contains_base_type_defining_declaration",
                     size_of->get_sizeOfContainsBaseTypeDefiningDeclaration()));
    fields.push_back(rawBoolField(
        "is_objectless_nonstatic_data_member_reference",
        size_of->get_is_objectless_nonstatic_data_member_reference()));
    fields.push_back(
        rawBoolField("is_sizeof_pack", size_of->get_is_sizeof_pack()));
  } else if (SgUnaryOp *unary = isSgUnaryOp(node)) {
    fields.push_back(rawIntegerField("mode", unary->get_mode()));
  } else if (SgOmpClause *clause = isSgOmpClause(node)) {
    fields.push_back(rawIntegerField("directive_name_modifier",
                                     clause->get_directive_name_modifier()));
  }

  if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
    fields.push_back(rawIntegerField("operation", clause->get_operation()));
    fields.push_back(rawIntegerField("modifier1", clause->get_modifier1()));
    fields.push_back(rawIntegerField("modifier2", clause->get_modifier2()));
    fields.push_back(rawIntegerField("modifier3", clause->get_modifier3()));
    fields.push_back(jsonString("mapper_identifier") + ": " +
                     rawExpressionRef(clause->get_mapper_identifier(), ids));
    fields.push_back(
        jsonString("array_dimensions") + ": " +
        rawOmpArrayDimensionsJson(clause->get_array_dimensions(), ids));
    fields.push_back(
        jsonString("dist_data_policies") + ": " +
        rawOmpDistDataPoliciesJson(clause->get_dist_data_policies(), ids));
    fields.push_back(jsonString("iterator") + ": " +
                     rawOmpIteratorJson(clause->get_iterator(), ids));
  } else if (SgOmpDeviceClause *clause = isSgOmpDeviceClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpDefaultClause *clause = isSgOmpDefaultClause(node)) {
    fields.push_back(
        rawIntegerField("data_sharing", clause->get_data_sharing()));
  } else if (SgOmpProcBindClause *clause = isSgOmpProcBindClause(node)) {
    fields.push_back(rawIntegerField("policy", clause->get_policy()));
  } else if (SgOmpIfClause *clause = isSgOmpIfClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpLastprivateClause *clause = isSgOmpLastprivateClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(rawIntegerField("identifier", clause->get_identifier()));
    fields.push_back(
        jsonString("user_defined_identifier") + ": " +
        rawExpressionRef(clause->get_user_defined_identifier(), ids));
  } else if (SgOmpLinearClause *clause = isSgOmpLinearClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpScheduleClause *clause = isSgOmpScheduleClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(rawIntegerField("modifier1", clause->get_modifier1()));
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  } else if (SgOmpDistScheduleClause *clause =
                 isSgOmpDistScheduleClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  } else if (SgOmpOrderClause *clause = isSgOmpOrderClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpAtomicDefaultMemOrderClause *clause =
                 isSgOmpAtomicDefaultMemOrderClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
  } else if (SgOmpDefaultmapClause *clause = isSgOmpDefaultmapClause(node)) {
    fields.push_back(rawIntegerField("behavior", clause->get_behavior()));
    fields.push_back(rawIntegerField("category", clause->get_category()));
  } else if (SgOmpBindClause *clause = isSgOmpBindClause(node)) {
    fields.push_back(rawIntegerField("binding", clause->get_binding()));
  } else if (SgOmpFailClause *clause = isSgOmpFailClause(node)) {
    fields.push_back(
        rawIntegerField("memory_order", clause->get_memory_order()));
  } else if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(
        jsonString("user_defined_modifier") + ": " +
        rawExpressionRef(clause->get_user_defined_modifier(), ids));
  } else if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(
        jsonString("user_defined_modifier") + ": " +
        rawExpressionRef(clause->get_user_defined_modifier(), ids));
  } else if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
    fields.push_back(
        jsonString("user_defined_modifier") + ": " +
        rawExpressionRef(clause->get_user_defined_modifier(), ids));
  } else if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
    fields.push_back(rawIntegerField("identifier", clause->get_identifier()));
  } else if (SgOmpTaskReductionClause *clause =
                 isSgOmpTaskReductionClause(node)) {
    fields.push_back(rawIntegerField("identifier", clause->get_identifier()));
    fields.push_back(
        jsonString("user_defined_identifier") + ": " +
        rawExpressionRef(clause->get_user_defined_identifier(), ids));
  } else if (SgOmpDepobjUpdateClause *clause =
                 isSgOmpDepobjUpdateClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpNumTasksClause *clause = isSgOmpNumTasksClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpGrainsizeClause *clause = isSgOmpGrainsizeClause(node)) {
    fields.push_back(rawIntegerField("modifier", clause->get_modifier()));
  } else if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
    fields.push_back(jsonString("user_condition") + ": " +
                     rawExpressionRef(clause->get_user_condition(), ids));
    fields.push_back(jsonString("user_condition_score") + ": " +
                     rawExpressionRef(clause->get_user_condition_score(), ids));
    fields.push_back(rawBoolField("target_device_selector",
                                  clause->get_target_device_selector()));
    fields.push_back(jsonString("device_arch") + ": " +
                     rawExpressionRef(clause->get_device_arch(), ids));
    fields.push_back(jsonString("device_isa") + ": " +
                     rawExpressionRef(clause->get_device_isa(), ids));
    fields.push_back(jsonString("device_num") + ": " +
                     rawExpressionRef(clause->get_device_num(), ids));
    fields.push_back(rawIntegerField("device_kind", clause->get_device_kind()));
    fields.push_back(rawIntegerField("implementation_vendor",
                                     clause->get_implementation_vendor()));
    fields.push_back(
        jsonString("implementation_user_defined") + ": " +
        rawExpressionRef(clause->get_implementation_user_defined(), ids));
    fields.push_back(
        jsonString("implementation_extension") + ": " +
        rawExpressionRef(clause->get_implementation_extension(), ids));
  } else if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
    fields.push_back(jsonString("user_condition") + ": " +
                     rawExpressionRef(clause->get_user_condition(), ids));
    fields.push_back(jsonString("user_condition_score") + ": " +
                     rawExpressionRef(clause->get_user_condition_score(), ids));
    fields.push_back(rawBoolField("target_device_selector",
                                  clause->get_target_device_selector()));
    fields.push_back(jsonString("device_arch") + ": " +
                     rawExpressionRef(clause->get_device_arch(), ids));
    fields.push_back(jsonString("device_isa") + ": " +
                     rawExpressionRef(clause->get_device_isa(), ids));
    fields.push_back(jsonString("device_num") + ": " +
                     rawExpressionRef(clause->get_device_num(), ids));
    fields.push_back(rawIntegerField("device_kind", clause->get_device_kind()));
    fields.push_back(rawIntegerField("implementation_vendor",
                                     clause->get_implementation_vendor()));
    fields.push_back(
        jsonString("implementation_user_defined") + ": " +
        rawExpressionRef(clause->get_implementation_user_defined(), ids));
    fields.push_back(
        jsonString("implementation_extension") + ": " +
        rawExpressionRef(clause->get_implementation_extension(), ids));
  } else if (SgOmpUsesAllocatorsDefination *definition =
                 isSgOmpUsesAllocatorsDefination(node)) {
    fields.push_back(rawIntegerField("allocator", definition->get_allocator()));
    fields.push_back(
        jsonString("user_defined_allocator") + ": " +
        rawExpressionRef(definition->get_user_defined_allocator(), ids));
    fields.push_back(
        jsonString("allocator_traits_array") + ": " +
        rawExpressionRef(definition->get_allocator_traits_array(), ids));
  } else if (SgOmpUsesAllocatorsClause *clause =
                 isSgOmpUsesAllocatorsClause(node)) {
    fields.push_back(jsonString("uses_allocators_definitions") + ": " +
                     rawOmpUsesAllocatorsDefinitionsJson(
                         clause->get_uses_allocators_defination(), ids));
  } else if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
    fields.push_back(
        rawIntegerField("depend_modifier", clause->get_depend_modifier()));
    fields.push_back(
        rawIntegerField("dependence_type", clause->get_dependence_type()));
    fields.push_back(
        jsonString("array_dimensions") + ": " +
        rawOmpArrayDimensionsJson(clause->get_array_dimensions(), ids));
    fields.push_back(jsonString("iterator") + ": " +
                     rawOmpIteratorJson(clause->get_iterator(), ids));
    fields.push_back(jsonString("vec") + ": " +
                     rawOmpExpressionListJson(clause->get_vec(), ids));
  } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
    fields.push_back(
        rawIntegerField("affinity_modifier", clause->get_affinity_modifier()));
    fields.push_back(
        jsonString("array_dimensions") + ": " +
        rawOmpArrayDimensionsJson(clause->get_array_dimensions(), ids));
    fields.push_back(jsonString("iterator") + ": " +
                     rawOmpIteratorJson(clause->get_iterator(), ids));
  } else if (SgOmpToClause *clause = isSgOmpToClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
    fields.push_back(jsonString("mapper_identifier") + ": " +
                     rawExpressionRef(clause->get_mapper_identifier(), ids));
    fields.push_back(
        jsonString("array_dimensions") + ": " +
        rawOmpArrayDimensionsJson(clause->get_array_dimensions(), ids));
    fields.push_back(jsonString("iterator") + ": " +
                     rawOmpIteratorJson(clause->get_iterator(), ids));
  } else if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
    fields.push_back(rawIntegerField("kind", clause->get_kind()));
    fields.push_back(jsonString("mapper_identifier") + ": " +
                     rawExpressionRef(clause->get_mapper_identifier(), ids));
    fields.push_back(
        jsonString("array_dimensions") + ": " +
        rawOmpArrayDimensionsJson(clause->get_array_dimensions(), ids));
    fields.push_back(jsonString("iterator") + ": " +
                     rawOmpIteratorJson(clause->get_iterator(), ids));
  }

  if (SgOmpCriticalStatement *stmt = isSgOmpCriticalStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
  }
  if (SgOmpDepobjStatement *stmt = isSgOmpDepobjStatement(node)) {
    fields.push_back(rawStringField("name", stmt->get_name().getString()));
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    fields.push_back(rawIntegerField("identifier", stmt->get_identifier()));
  }
  if (SgOmpDeclareTargetStatement *stmt = isSgOmpDeclareTargetStatement(node)) {
    fields.push_back(
        rawIntegerField("device_type_kind", stmt->get_device_type_kind()));
  }
  if (SgOmpBeginDeclareVariantStatement *stmt =
          isSgOmpBeginDeclareVariantStatement(node)) {
    fields.push_back(
        rawStringField("captured_region", stmt->get_captured_region()));
  }
  if (SgImplicitStatement *stmt = isSgImplicitStatement(node)) {
    fields.push_back(rawBoolField("implicit_none", stmt->get_implicit_none()));
    fields.push_back(
        rawIntegerField("implicit_spec", stmt->get_implicit_spec()));
  }
  if (SgFortranIncludeLine *stmt = isSgFortranIncludeLine(node)) {
    fields.push_back(rawStringField("filename", stmt->get_filename()));
  }
  if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
    fields.push_back(rawStringField("label", stmt->get_label().getString()));
    fields.push_back(
        rawBoolField("gnu_extension_unused", stmt->get_gnu_extension_unused()));
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    fields.push_back(rawStringField("string_label", stmt->get_string_label()));
    fields.push_back(
        rawBoolField("has_end_statement", stmt->get_has_end_statement()));
    fields.push_back(
        rawBoolField("use_then_keyword", stmt->get_use_then_keyword()));
    fields.push_back(
        rawBoolField("is_else_if_statement", stmt->get_is_else_if_statement()));
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    fields.push_back(rawStringField("string_label", stmt->get_string_label()));
    fields.push_back(
        rawBoolField("has_end_statement", stmt->get_has_end_statement()));
  }
  if (SgFortranDo *stmt = isSgFortranDo(node)) {
    fields.push_back(rawStringField("string_label", stmt->get_string_label()));
    fields.push_back(rawBoolField("old_style", stmt->get_old_style()));
    fields.push_back(
        rawBoolField("has_end_statement", stmt->get_has_end_statement()));
  }
  if (SgIOStatement *stmt = isSgIOStatement(node)) {
    fields.push_back(rawIntegerField("io_statement", stmt->get_io_statement()));
  }
  if (SgNewExp *expr = isSgNewExp(node)) {
    fields.push_back(jsonString("specified_type") + ": " +
                     rawTypeJson(expr->get_specified_type(), ids));
    fields.push_back(rawIntegerField(
        "need_global_specifier",
        static_cast<int64_t>(expr->get_need_global_specifier())));
    fields.push_back(rawBoolField("type_id_is_parenthesized",
                                  expr->get_type_id_is_parenthesized()));
  }
  if (SgDeleteExp *expr = isSgDeleteExp(node)) {
    fields.push_back(rawIntegerField(
        "is_array", static_cast<int64_t>(expr->get_is_array())));
    fields.push_back(rawIntegerField(
        "need_global_specifier",
        static_cast<int64_t>(expr->get_need_global_specifier())));
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    auto append_declaration_qualification = [&](auto *qualified_decl) {
      fields.push_back(
          rawIntegerField("name_qualification_length",
                          qualified_decl->get_name_qualification_length()));
      fields.push_back(
          rawBoolField("type_elaboration_required",
                       qualified_decl->get_type_elaboration_required()));
      fields.push_back(
          rawBoolField("global_qualification_required",
                       qualified_decl->get_global_qualification_required()));
    };
    if (isSgFunctionDeclaration(decl) == nullptr) {
      if (SgVariableDeclaration *qualified = isSgVariableDeclaration(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgEnumDeclaration *qualified = isSgEnumDeclaration(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgTypedefDeclaration *qualified =
                     isSgTypedefDeclaration(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgUsingDirectiveStatement *qualified =
                     isSgUsingDirectiveStatement(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgUsingDeclarationStatement *qualified =
                     isSgUsingDeclarationStatement(decl)) {
        append_declaration_qualification(qualified);
      } else if (SgClassDeclaration *qualified = isSgClassDeclaration(decl)) {
        append_declaration_qualification(qualified);
      }
    }
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
      fields.push_back(
          rawBoolField("requires_global_name_qualification_on_type",
                       variable->get_requiresGlobalNameQualificationOnType()));
    }
  }

  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    addLocatedPreprocessing(fields, located);
  } else {
    fields.push_back(jsonString("preprocessing") + ": []");
  }

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

} // namespace AstJson
} // namespace Rose
