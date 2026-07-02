#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

SgSourceFile *collectionBoundaryFile = nullptr;

uint64_t varRefSymbolDeclarationId(
    SgVarRefExp *ref, const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (ref == nullptr) {
    return 0;
  }
  SgVariableSymbol *symbol = ref->get_symbol();
  if (symbol == nullptr) {
    throw std::runtime_error("AST JSON SgVarRefExp has no variable symbol");
  }
  const uint64_t id = idFor(ids, symbol->get_declaration());
  if (id == 0) {
    std::ostringstream message;
    message << "AST JSON SgVarRefExp symbol declaration was not collected: "
            << symbol->get_name().getString();
    auto append_source_file = [&](const char *label, SgNode *node) {
      SgSourceFile *source = SageInterface::getEnclosingSourceFile(node, true);
      message << ' ' << label << '=';
      if (source == nullptr) {
        message << "<null>";
      } else {
        message << source->getFileName() << '@' << source;
        if (SgNode *parent = source->get_parent()) {
          message << " parent=" << parent->sage_class_name() << '@' << parent;
        } else {
          message << " parent=<null>";
        }
      }
    };
    append_source_file("boundary_file", collectionBoundaryFile);
    if (SgInitializedName *declaration = symbol->get_declaration()) {
      message << " declaration=" << declaration->sage_class_name();
      append_source_file("declaration_file", declaration);
      if (SgNode *decl_parent = declaration->get_parent()) {
        message << " declaration_parent=" << decl_parent->sage_class_name();
      }
      if (SgScopeStatement *scope = declaration->get_scope()) {
        message << " declaration_scope=" << scope->sage_class_name();
      }
      message << " declaration_inside_boundary="
              << (insideCollectionBoundary(declaration) ? "true" : "false");
      message << " declaration_ancestors=";
      for (SgNode *ancestor = declaration; ancestor != nullptr;
           ancestor = ancestor->get_parent()) {
        if (ancestor != declaration) {
          message << "/";
        }
        message << ancestor->sage_class_name();
      }
    }
    if (SgNode *parent = ref->get_parent()) {
      message << " ref_parent=" << parent->sage_class_name();
    }
    append_source_file("ref_file", ref);
    message << " ref_inside_boundary="
            << (insideCollectionBoundary(ref) ? "true" : "false");
    message << " ref_text=" << ref->unparseToString();
    throw std::runtime_error(message.str());
  }
  return id;
}

std::string
rawTypeJson(SgType *type,
            const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  if (type == nullptr) {
    fields.push_back(rawBoolField("present", false));
  } else {
    fields.push_back(rawBoolField("present", true));
    fields.push_back(rawStringField("kind", type->sage_class_name()));
    fields.push_back(rawStringField("text", jsonTypeText(type)));

    if (SgPointerMemberType *member_pointer = isSgPointerMemberType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(member_pointer->get_base_type(), ids));
      fields.push_back(jsonString("class_type") + ": " +
                       rawTypeJson(member_pointer->get_class_type(), ids));
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(pointer->get_base_type(), ids));
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(reference->get_base_type(), ids));
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(reference->get_base_type(), ids));
    } else if (SgTypeString *string_type = isSgTypeString(type)) {
      fields.push_back(
          jsonString("length_expression") + ": " +
          rawTypeOwnedExpressionRef(string_type->get_lengthExpression(), ids));
      fields.push_back(
          jsonString("type_kind") + ": " +
          rawTypeOwnedExpressionRef(string_type->get_type_kind(), ids));
    } else if (SgTypeComplex *complex_type = isSgTypeComplex(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(complex_type->get_base_type(), ids));
      fields.push_back(
          jsonString("type_kind") + ": " +
          rawTypeOwnedExpressionRef(complex_type->get_type_kind(), ids));
    } else if (SgArrayType *array = isSgArrayType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(array->get_base_type(), ids));
      fields.push_back(jsonString("index") + ": " +
                       rawTypeOwnedExpressionRef(array->get_index(), ids));
      fields.push_back(jsonString("dim_info") + ": " +
                       rawTypeOwnedExprListExpJson(array->get_dim_info(), ids));
      fields.push_back(rawIntegerField("rank", array->get_rank()));
      fields.push_back(rawIntegerField("number_of_elements",
                                       array->get_number_of_elements()));
      fields.push_back(rawBoolField("is_coarray", array->get_isCoArray()));
      fields.push_back(rawBoolField("is_variable_length_array",
                                    array->get_is_variable_length_array()));
    } else if (SgModifierType *modifier = isSgModifierType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(modifier->get_base_type(), ids));
      SgTypeModifier &type_modifier = modifier->get_typeModifier();
      SgConstVolatileModifier &cv = type_modifier.get_constVolatileModifier();
      fields.push_back(rawBoolField("modifier_const", cv.isConst()));
      fields.push_back(rawBoolField("modifier_volatile", cv.isVolatile()));
      fields.push_back(
          rawBoolField("modifier_restrict", type_modifier.isRestrict()));
    } else if (SgTypeLabel *label_type = isSgTypeLabel(type)) {
      fields.push_back(
          rawStringField("name", label_type->get_name().getString()));
    } else if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      SgTypedefDeclaration *typedef_declaration =
          isSgTypedefDeclaration(typedef_type->get_declaration());
      const uint64_t declaration_id =
          canonicalTypedefDeclarationId(ids, typedef_declaration);
      if (declaration_id == 0) {
        std::ostringstream message;
        message << "AST JSON SgTypedefType declaration was not collected: "
                << jsonTypeText(type);
        if (typedef_declaration != nullptr) {
          message << " declaration=" << typedef_declaration->get_name();
          if (SgNode *parent = typedef_declaration->get_parent()) {
            message << " declaration_parent=" << parent->sage_class_name();
          }
        }
        if (currentTypeSerializationNode != nullptr) {
          message << " while serializing "
                  << currentTypeSerializationNode->sage_class_name();
          const auto current_id = ids.find(currentTypeSerializationNode);
          if (current_id != ids.end()) {
            message << " id=" << current_id->second;
          }
          if (const SgVarRefExp *ref =
                  isSgVarRefExp(currentTypeSerializationNode)) {
            const SgVariableSymbol *symbol = ref->get_symbol();
            const SgInitializedName *declaration =
                symbol != nullptr ? symbol->get_declaration() : nullptr;
            message << " var_symbol="
                    << (symbol != nullptr ? symbol->get_name().getString()
                                          : "<null>");
            if (declaration != nullptr) {
              const auto declaration_id = ids.find(declaration);
              if (declaration_id != ids.end()) {
                message << " var_declaration_id=" << declaration_id->second;
              } else {
                message << " var_declaration_uncollected";
              }
              message << " var_declaration=" << declaration->get_name();
              if (SgType *declaration_type = declaration->get_type()) {
                message << " var_declaration_type="
                        << declaration_type->sage_class_name();
              }
            }
          }
          const SgNode *parent = currentTypeSerializationNode->get_parent();
          int depth = 0;
          while (parent != nullptr && depth < 8) {
            message << " parent" << depth << "=" << parent->sage_class_name();
            const auto parent_id = ids.find(parent);
            if (parent_id != ids.end()) {
              message << "#" << parent_id->second;
            }
            parent = parent->get_parent();
            ++depth;
          }
        }
        throw std::runtime_error(message.str());
      }
      fields.push_back(rawIntegerField("declaration", declaration_id));
      fields.push_back(
          rawBoolField("autonomous_declaration",
                       typedef_type->get_autonomous_declaration()));
    } else if (SgClassType *class_type = isSgClassType(type)) {
      SgClassDeclaration *decl =
          isSgClassDeclaration(class_type->get_declaration());
      uint64_t declaration_id = idFor(ids, decl);
      if (declaration_id == 0 && decl != nullptr) {
        declaration_id =
            idFor(ids, isSgClassDeclaration(decl->get_definingDeclaration()));
      }
      if (declaration_id == 0 && decl != nullptr) {
        declaration_id = idFor(
            ids, isSgClassDeclaration(decl->get_firstNondefiningDeclaration()));
      }
      const bool external_declaration =
          declaration_id == 0 && decl != nullptr &&
          (isAstJsonExternalClassDeclaration(decl) ||
           !insideCollectionBoundary(decl));
      if (declaration_id == 0) {
        if (external_declaration) {
          fields.push_back(jsonString("external_declaration") + ": " +
                           rawExternalClassDeclarationJson(decl));
        } else {
          std::ostringstream message;
          message << "AST JSON SgClassType declaration was not collected: "
                  << jsonTypeText(type);
          if (decl != nullptr) {
            message << " declaration=" << decl->get_name();
            if (SgNode *parent = decl->get_parent()) {
              message << " declaration_parent=" << parent->sage_class_name();
            } else {
              message << " declaration_parent=<null>";
            }
            if (SgScopeStatement *scope = decl->get_scope()) {
              message << " declaration_scope=" << scope->sage_class_name();
            } else {
              message << " declaration_scope=<null>";
            }
            const SgNode *decl_parent = decl->get_parent();
            int decl_depth = 0;
            while (decl_parent != nullptr && decl_depth < 8) {
              message << " declaration_parent" << decl_depth << "="
                      << decl_parent->sage_class_name();
              const auto parent_id = ids.find(decl_parent);
              if (parent_id != ids.end()) {
                message << "#" << parent_id->second;
              }
              decl_parent = decl_parent->get_parent();
              ++decl_depth;
            }
          }
          if (currentTypeSerializationNode != nullptr) {
            message << " while serializing "
                    << currentTypeSerializationNode->sage_class_name();
            const auto current_id = ids.find(currentTypeSerializationNode);
            if (current_id != ids.end()) {
              message << " id=" << current_id->second;
            }
            message << " text="
                    << safeNodeText(
                           const_cast<SgNode *>(currentTypeSerializationNode));
            const SgNode *parent = currentTypeSerializationNode->get_parent();
            int depth = 0;
            while (parent != nullptr && depth < 8) {
              message << " parent" << depth << "=" << parent->sage_class_name();
              const auto parent_id = ids.find(parent);
              if (parent_id != ids.end()) {
                message << "#" << parent_id->second;
              }
              parent = parent->get_parent();
              ++depth;
            }
          }
          throw std::runtime_error(message.str());
        }
      } else {
        fields.push_back(rawIntegerField("declaration", declaration_id));
      }
      fields.push_back(rawBoolField("autonomous_declaration",
                                    class_type->get_autonomous_declaration()));
    } else if (SgEnumType *enum_type = isSgEnumType(type)) {
      SgEnumDeclaration *decl =
          isSgEnumDeclaration(enum_type->get_declaration());
      uint64_t declaration_id = idFor(ids, decl);
      if (decl != nullptr) {
        if (SgEnumDeclaration *first_nondef =
                isSgEnumDeclaration(decl->get_firstNondefiningDeclaration())) {
          if (uint64_t first_nondef_id = idFor(ids, first_nondef)) {
            declaration_id = first_nondef_id;
          }
        }
      }
      if (declaration_id == 0) {
        throw std::runtime_error(
            "AST JSON SgEnumType declaration was not collected: " +
            jsonTypeText(type));
      }
      fields.push_back(rawIntegerField("declaration", declaration_id));
      fields.push_back(rawBoolField("autonomous_declaration",
                                    enum_type->get_autonomous_declaration()));
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
      const uint64_t declaration_id =
          idFor(ids, nonreal_type->get_declaration());
      if (declaration_id == 0) {
        throw std::runtime_error(
            "AST JSON SgNonrealType declaration was not collected: " +
            jsonTypeText(type));
      }
      fields.push_back(rawIntegerField("declaration", declaration_id));
      fields.push_back(
          rawBoolField("autonomous_declaration",
                       nonreal_type->get_autonomous_declaration()));
    } else if (SgDeclType *decl_type = isSgDeclType(type)) {
      fields.push_back(
          jsonString("base_expression") + ": " +
          rawTypeOwnedExpressionRef(decl_type->get_base_expression(), ids));
      fields.push_back(
          rawBoolField("is_gnu_decltype", decl_type->get_is_gnu_decltype()));
      if (SgExpression *base_expression = decl_type->get_base_expression()) {
        fields.push_back(jsonString("base_type") + ": " +
                         rawTypeJson(base_expression->get_type(), ids));
      } else {
        fields.push_back(jsonString("base_type") + ": " +
                         rawTypeJson(nullptr, ids));
      }
    } else if (SgTemplateType *template_type = isSgTemplateType(type)) {
      fields.push_back(
          rawStringField("name", template_type->get_name().getString()));
      fields.push_back(
          rawIntegerField("template_parameter_position",
                          template_type->get_template_parameter_position()));
      fields.push_back(
          rawIntegerField("template_parameter_depth",
                          template_type->get_template_parameter_depth()));
      fields.push_back(jsonString("class_type") + ": " +
                       rawTypeJson(template_type->get_class_type(), ids));
      fields.push_back(
          jsonString("parent_class_type") + ": " +
          rawTypeJson(template_type->get_parent_class_type(), ids));
    } else if (SgMemberFunctionType *member_type =
                   isSgMemberFunctionType(type)) {
      fields.push_back(jsonString("return_type") + ": " +
                       rawTypeJson(member_type->get_return_type(), ids));
      fields.push_back(jsonString("class_type") + ": " +
                       rawTypeJson(member_type->get_class_type(), ids));
      fields.push_back(rawIntegerField("mfunc_specifier",
                                       member_type->get_mfunc_specifier()));
      fields.push_back(
          rawBoolField("has_ellipses", member_type->get_has_ellipses()));
      std::ostringstream args;
      args << jsonString("arguments") << ": [";
      const SgTypePtrList &argument_types =
          member_type->get_argument_list() != nullptr
              ? member_type->get_argument_list()->get_arguments()
              : member_type->get_arguments();
      if (!argument_types.empty()) {
        args << '\n';
        for (size_t i = 0; i < argument_types.size(); ++i) {
          indent(args, 8);
          args << rawTypeJson(argument_types[i], ids);
          if (i + 1 != argument_types.size()) {
            args << ',';
          }
          args << '\n';
        }
        indent(args, 6);
      }
      args << "]";
      fields.push_back(args.str());
    } else if (SgFunctionType *function_type = isSgFunctionType(type)) {
      fields.push_back(jsonString("return_type") + ": " +
                       rawTypeJson(function_type->get_return_type(), ids));
      fields.push_back(
          rawBoolField("has_ellipses", function_type->get_has_ellipses()));
      std::ostringstream args;
      args << jsonString("arguments") << ": [";
      const SgTypePtrList &argument_types =
          function_type->get_argument_list() != nullptr
              ? function_type->get_argument_list()->get_arguments()
              : function_type->get_arguments();
      if (!argument_types.empty()) {
        args << '\n';
        for (size_t i = 0; i < argument_types.size(); ++i) {
          indent(args, 8);
          args << rawTypeJson(argument_types[i], ids);
          if (i + 1 != argument_types.size()) {
            args << ',';
          }
          args << '\n';
        }
        indent(args, 6);
      }
      args << "]";
      fields.push_back(args.str());
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

std::string rawOmpArrayDimensionsJson(
    const std::map<SgSymbol *,
                   std::vector<std::pair<SgExpression *, SgExpression *>>>
        &array_dimensions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::pair<std::string, std::string>> entries;
  for (const auto &entry : array_dimensions) {
    std::vector<std::string> fields;
    const uint64_t declaration_id = idFor(ids, symbolBasis(entry.first));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(entry.first, ids));
    std::ostringstream bounds;
    bounds << jsonString("bounds") << ": [";
    if (!entry.second.empty()) {
      bounds << '\n';
      for (size_t i = 0; i < entry.second.size(); ++i) {
        std::vector<std::string> bound_fields;
        bound_fields.push_back(jsonString("lower") + ": " +
                               rawExpressionRef(entry.second[i].first, ids));
        bound_fields.push_back(jsonString("upper") + ": " +
                               rawExpressionRef(entry.second[i].second, ids));
        writeRawObject(bounds, 10, bound_fields, i + 1 != entry.second.size());
      }
      indent(bounds, 8);
    }
    bounds << "]";
    fields.push_back(bounds.str());

    std::ostringstream item;
    writeRawObject(item, 0, fields, false);
    std::ostringstream key;
    key << std::setw(20) << std::setfill('0') << declaration_id << ':'
        << symbolName(entry.first);
    entries.emplace_back(key.str(), item.str());
  }
  std::sort(
      entries.begin(), entries.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });

  std::ostringstream out;
  out << "[";
  if (!entries.empty()) {
    out << '\n';
    for (size_t i = 0; i < entries.size(); ++i) {
      indent(out, 6);
      out << entries[i].second;
      if (i + 1 != entries.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawOmpDistDataPoliciesJson(
    const std::map<SgSymbol *,
                   std::vector<std::pair<SgOmpClause::omp_map_dist_data_enum,
                                         SgExpression *>>> &policies,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::pair<std::string, std::string>> entries;
  for (const auto &entry : policies) {
    std::vector<std::string> fields;
    const uint64_t declaration_id = idFor(ids, symbolBasis(entry.first));
    fields.push_back(jsonString("symbol") + ": " +
                     rawSymbolRef(entry.first, ids));
    std::ostringstream policy_json;
    policy_json << jsonString("policies") << ": [";
    if (!entry.second.empty()) {
      policy_json << '\n';
      for (size_t i = 0; i < entry.second.size(); ++i) {
        std::vector<std::string> policy_fields;
        policy_fields.push_back(
            rawIntegerField("policy", static_cast<int>(entry.second[i].first)));
        policy_fields.push_back(jsonString("expression") + ": " +
                                rawExpressionRef(entry.second[i].second, ids));
        writeRawObject(policy_json, 10, policy_fields,
                       i + 1 != entry.second.size());
      }
      indent(policy_json, 8);
    }
    policy_json << "]";
    fields.push_back(policy_json.str());

    std::ostringstream item;
    writeRawObject(item, 0, fields, false);
    std::ostringstream key;
    key << std::setw(20) << std::setfill('0') << declaration_id << ':'
        << symbolName(entry.first);
    entries.emplace_back(key.str(), item.str());
  }
  std::sort(
      entries.begin(), entries.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });

  std::ostringstream out;
  out << "[";
  if (!entries.empty()) {
    out << '\n';
    for (size_t i = 0; i < entries.size(); ++i) {
      indent(out, 6);
      out << entries[i].second;
      if (i + 1 != entries.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string
rawOmpIteratorJson(const std::list<std::list<SgExpression *>> &iterators,
                   const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!iterators.empty()) {
    out << '\n';
    size_t outer_index = 0;
    for (const std::list<SgExpression *> &iterator : iterators) {
      indent(out, 6);
      out << "[";
      if (!iterator.empty()) {
        out << '\n';
        size_t inner_index = 0;
        for (SgExpression *expr : iterator) {
          indent(out, 8);
          out << rawExpressionRef(expr, ids);
          if (++inner_index != iterator.size()) {
            out << ',';
          }
          out << '\n';
        }
        indent(out, 6);
      }
      out << "]";
      if (++outer_index != iterators.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawOmpExpressionListJson(
    const std::list<SgExpression *> &expressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!expressions.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgExpression *expr : expressions) {
      indent(out, 6);
      out << rawExpressionRef(expr, ids);
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

std::string rawTemplateArgumentListJson(
    const SgTemplateArgumentPtrList &arguments,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!arguments.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgTemplateArgument *argument : arguments) {
      indent(out, 6);
      out << idFor(ids, argument);
      if (++index != arguments.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string
rawTypeTraitArgsJson(const SgNodePtrList &args,
                     const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!args.empty()) {
    out << '\n';
    for (size_t i = 0; i < args.size(); ++i) {
      indent(out, 6);
      out << "{\n";
      if (SgType *type = isSgType(args[i])) {
        writeStringField(out, 8, "kind", "type");
        indent(out, 8);
        out << jsonString("type") << ": " << rawTypeJson(type, ids) << '\n';
      } else {
        const uint64_t id = idFor(ids, args[i]);
        if (id == 0) {
          throw std::runtime_error(
              std::string("AST JSON type trait argument was not collected: ") +
              (args[i] != nullptr ? args[i]->sage_class_name() : "<null>"));
        }
        writeStringField(out, 8, "kind", "node");
        writeIntegerField(out, 8, "node", id, false);
      }
      indent(out, 6);
      out << '}';
      if (i + 1 != args.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawOmpUsesAllocatorsDefinitionsJson(
    const std::list<SgOmpUsesAllocatorsDefination *> &definitions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!definitions.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgOmpUsesAllocatorsDefination *definition : definitions) {
      const uint64_t id = idFor(ids, definition);
      if (definition != nullptr && id == 0) {
        throw std::runtime_error(
            "AST JSON OMP uses_allocators definition target was not "
            "collected");
      }
      indent(out, 6);
      out << id;
      if (++index != definitions.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawAstAttributesJson(SgNode *node) {
  std::ostringstream out;
  out << "[";
  AstAttributeMechanism *attributes =
      node != nullptr ? node->get_attributeMechanism() : nullptr;
  if (attributes != nullptr && attributes->size() != 0) {
    std::vector<std::string> entries;
    for (const std::string &name : attributes->getAttributeIdentifiers()) {
      AstAttribute *attribute = node->getAttribute(name);
      if (AstIntAttribute *int_attribute =
              dynamic_cast<AstIntAttribute *>(attribute)) {
        std::vector<std::string> fields;
        fields.push_back(rawStringField("name", name));
        fields.push_back(rawStringField("type", "AstIntAttribute"));
        fields.push_back(rawIntegerField("value", int_attribute->getValue()));
        std::ostringstream item;
        writeRawObject(item, 0, fields, false);
        entries.push_back(item.str());
      } else if (AstValueAttribute<std::string> *string_attribute =
                     dynamic_cast<AstValueAttribute<std::string> *>(
                         attribute)) {
        std::vector<std::string> fields;
        fields.push_back(rawStringField("name", name));
        fields.push_back(rawStringField("type", "AstStringAttribute"));
        fields.push_back(rawStringField("value", string_attribute->get()));
        std::ostringstream item;
        writeRawObject(item, 0, fields, false);
        entries.push_back(item.str());
      } else if (name == "acc_fortran_end" || name == "omp_fortran_end" ||
                 name == "fortran_keep_openmp_pragma" ||
                 name == "omp_declare_target_extended_list") {
        std::vector<std::string> fields;
        fields.push_back(rawStringField("name", name));
        fields.push_back(rawStringField("type", "AstMarkerAttribute"));
        std::ostringstream item;
        writeRawObject(item, 0, fields, false);
        entries.push_back(item.str());
      }
    }
    if (!entries.empty()) {
      std::sort(entries.begin(), entries.end());
      out << '\n';
      for (size_t i = 0; i < entries.size(); ++i) {
        indent(out, 6);
        out << entries[i];
        if (i + 1 != entries.size()) {
          out << ',';
        }
        out << '\n';
      }
      indent(out, 4);
    }
  }
  out << "]";
  return out.str();
}

void addExpressionType(
    std::vector<std::string> &fields, SgExpression *expr,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (expr != nullptr) {
    fields.push_back(jsonString("type") + ": " +
                     rawTypeJson(expr->get_type(), ids));
  }
}

template <typename T>
void addReferenceQualificationFields(std::vector<std::string> &fields, T *ref) {
  fields.push_back(rawIntegerField("name_qualification_length",
                                   ref->get_name_qualification_length()));
  fields.push_back(rawBoolField("type_elaboration_required",
                                ref->get_type_elaboration_required()));
  fields.push_back(rawBoolField("global_qualification_required",
                                ref->get_global_qualification_required()));
  fields.push_back(
      rawIntegerField("explicit_name_qualification_length",
                      ref->get_explicit_name_qualification_length()));
  fields.push_back(rawBoolField("explicit_global_qualification",
                                ref->get_explicit_global_qualification()));
  fields.push_back(
      jsonString("explicit_name_qualification_tokens") + ": " +
      rawStringListJson(ref->get_explicit_name_qualification_tokens()));
}

void addExpressionQualificationFields(std::vector<std::string> &fields,
                                      SgExpression *expr) {
  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    if (isAnonymousDataMemberReference(ref)) {
      fields.push_back(rawIntegerField("name_qualification_length", 0));
      fields.push_back(rawBoolField("type_elaboration_required", false));
      fields.push_back(rawBoolField("global_qualification_required", false));
      fields.push_back(
          rawIntegerField("explicit_name_qualification_length", -1));
      fields.push_back(rawBoolField("explicit_global_qualification", false));
      fields.push_back(jsonString("explicit_name_qualification_tokens") + ": " +
                       rawStringListJson(SgStringList()));
    } else {
      addReferenceQualificationFields(fields, ref);
    }
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgEnumVal *ref = isSgEnumVal(expr)) {
    addReferenceQualificationFields(fields, ref);
  }
}

template <typename T>
void restoreReferenceQualificationFields(T *ref, const JsonValue &properties) {
  ref->set_name_qualification_length(
      static_cast<int>(properties.at("name_qualification_length").asInt()));
  ref->set_type_elaboration_required(
      properties.at("type_elaboration_required").asBool());
  ref->set_global_qualification_required(
      properties.at("global_qualification_required").asBool());
  ref->set_explicit_name_qualification_length(static_cast<int>(
      properties.at("explicit_name_qualification_length").asInt()));
  ref->set_explicit_global_qualification(
      properties.at("explicit_global_qualification").asBool());
  ref->set_explicit_name_qualification_tokens(
      stringListFromJson(properties.at("explicit_name_qualification_tokens"),
                         "explicit_name_qualification_tokens"));
}

void restoreExpressionQualificationFields(SgExpression *expr,
                                          const JsonValue &properties) {
  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgEnumVal *ref = isSgEnumVal(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  }
}

void addLocatedPreprocessing(std::vector<std::string> &fields,
                             const SgLocatedNode *node) {
  const AttachedPreprocessingInfoType *infos =
      node != nullptr
          ? const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo()
          : nullptr;
  std::ostringstream out;
  out << jsonString("preprocessing") << ": [";
  if (infos != nullptr && !infos->empty()) {
    std::vector<std::vector<std::string>> preprocessing_entries;
    for (size_t i = 0; i < infos->size(); ++i) {
      const PreprocessingInfo *info = (*infos)[i];
      if (info == nullptr) {
        std::ostringstream message;
        message << "AST JSON cannot serialize null PreprocessingInfo attached "
                << "to " << node->class_name();
        throw std::runtime_error(message.str());
      }
      if (isSgInitializedName(const_cast<SgLocatedNode *>(node)) != nullptr &&
          (info->getTypeOfDirective() ==
               PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           info->getTypeOfDirective() ==
               PreprocessingInfo::CpreprocessorIncludeNextDeclaration)) {
        SgNode *parent = const_cast<SgLocatedNode *>(node)->get_parent();
        SgLocatedNode *parent_located = isSgLocatedNode(parent);
        AttachedPreprocessingInfoType *parent_infos =
            parent_located != nullptr
                ? parent_located->getAttachedPreprocessingInfo()
                : nullptr;
        bool parent_has_same_include = false;
        if (parent_infos != nullptr) {
          for (const PreprocessingInfo *parent_info : *parent_infos) {
            if (parent_info != nullptr &&
                parent_info->getTypeOfDirective() ==
                    info->getTypeOfDirective() &&
                parent_info->getString() == info->getString()) {
              parent_has_same_include = true;
              break;
            }
          }
        }
        if (parent_has_same_include) {
          continue;
        }
      }
      std::vector<std::string> entry;
      entry.push_back(rawIntegerField(
          "directive", static_cast<int>(info->getTypeOfDirective())));
      entry.push_back(rawIntegerField(
          "relative", static_cast<int>(info->getRelativePosition())));
      entry.push_back(rawStringField("text", info->getString()));
      entry.push_back(jsonString("file_info") + ": " +
                      rawFileInfoJson(info->get_file_info()));
      entry.push_back(rawIntegerField("lines", info->getNumberOfLines()));
      entry.push_back(rawBoolField("transformation", info->isTransformation()));
      preprocessing_entries.push_back(std::move(entry));
    }
    if (!preprocessing_entries.empty()) {
      out << '\n';
      for (size_t i = 0; i < preprocessing_entries.size(); ++i) {
        writeRawObject(out, 8, preprocessing_entries[i],
                       i + 1 != preprocessing_entries.size());
      }
      indent(out, 6);
    }
  }
  out << "]";
  fields.push_back(out.str());
}

void writeFileInfoJson(std::ostream &out, int level, const Sg_File_Info *info,
                       bool comma) {
  indent(out, level);
  out << "{\n";
  if (info == nullptr) {
    writeBoolField(out, level + 2, "present", false, false);
  } else {
    writeBoolField(out, level + 2, "present", true);
    writeStringField(out, level + 2, "filename", info->get_filenameString());
    writeStringField(out, level + 2, "raw_filename", info->get_raw_filename());
    writeStringField(out, level + 2, "physical_filename",
                     info->get_physical_filename());
    writeIntegerField(out, level + 2, "file_id", info->get_file_id());
    const int physical_file_id = info->get_physical_file_id();
    std::string physical_raw_filename = info->get_physical_filename();
    if (physical_file_id >= 0) {
      physical_raw_filename = Sg_File_Info::getFilenameFromID(physical_file_id);
    }
    writeStringField(out, level + 2, "physical_raw_filename",
                     physical_raw_filename);
    writeIntegerField(out, level + 2, "physical_file_id", physical_file_id);
    const int *physical_internal_file_id =
        const_cast<Sg_File_Info *>(info)->get_physical_file_id_reference();
    writeIntegerField(out, level + 2, "physical_internal_file_id",
                      physical_internal_file_id != nullptr
                          ? *physical_internal_file_id
                          : physical_file_id);
    writeIntegerField(out, level + 2, "line", info->get_line());
    writeIntegerField(out, level + 2, "column", info->get_col());
    writeIntegerField(out, level + 2, "raw_line", info->get_raw_line());
    writeIntegerField(out, level + 2, "raw_column", info->get_raw_col());
    writeIntegerField(out, level + 2, "physical_line",
                      info->get_physical_line());
    writeIntegerField(out, level + 2, "source_sequence",
                      info->get_source_sequence_number());
    writeBoolField(out, level + 2, "compiler_generated",
                   info->isCompilerGenerated());
    writeBoolField(out, level + 2, "transformation", info->isTransformation());
    writeBoolField(out, level + 2, "frontend_specific",
                   info->isFrontendSpecific());
    writeBoolField(out, level + 2, "output_in_code_generation",
                   info->isOutputInCodeGeneration());
    writeBoolField(out, level + 2, "shared", info->isShared());
    writeBoolField(out, level + 2, "source_position_unavailable_in_frontend",
                   info->isSourcePositionUnavailableInFrontend());
    writeBoolField(out, level + 2, "comment_or_directive",
                   info->isCommentOrDirective());
    writeBoolField(out, level + 2, "token", info->isToken());
    writeBoolField(out, level + 2, "default_argument",
                   info->isDefaultArgument());
    writeBoolField(out, level + 2, "implicit_cast", info->isImplicitCast(),
                   false);
  }
  indent(out, level);
  out << '}';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

SgProject *currentDeserializationProject = nullptr;
SgNode *currentAuxiliaryOwner = nullptr;
std::vector<std::string> currentAuxiliaryTypeStack;
std::unordered_map<SgSourceFile *, std::vector<SgClassDeclaration *>>
    currentDeserializationClassDeclarationCache;

std::string safeNodeText(SgNode *node);

bool insideCollectionBoundary(SgNode *node) {
  if (node == nullptr || collectionBoundaryFile == nullptr) {
    return true;
  }
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (current == collectionBoundaryFile) {
      return true;
    }
    if (isSgType(current) != nullptr) {
      return true;
    }
    if (isSgSourceFile(current) != nullptr ||
        isSgFileList(current) != nullptr || isSgProject(current) != nullptr) {
      return false;
    }
  }
  return true;
}

CollectionBoundaryGuard::CollectionBoundaryGuard(SgSourceFile *file)
    : previous_(collectionBoundaryFile) {
  collectionBoundaryFile = file;
}

CollectionBoundaryGuard::~CollectionBoundaryGuard() {
  collectionBoundaryFile = previous_;
}

DeserializationProjectGuard::DeserializationProjectGuard(SgProject *project)
    : previous_(currentDeserializationProject) {
  currentDeserializationProject = project;
  currentDeserializationClassDeclarationCache.clear();
}

DeserializationProjectGuard::~DeserializationProjectGuard() {
  currentDeserializationClassDeclarationCache.clear();
  currentDeserializationProject = previous_;
}

class AuxiliaryOwnerGuard {
public:
  explicit AuxiliaryOwnerGuard(SgNode *node)
      : previous_(currentAuxiliaryOwner) {
    currentAuxiliaryOwner = node;
  }
  ~AuxiliaryOwnerGuard() { currentAuxiliaryOwner = previous_; }

private:
  SgNode *previous_;
};

class AuxiliaryTypeGuard {
public:
  explicit AuxiliaryTypeGuard(SgType *type) {
    currentAuxiliaryTypeStack.push_back(
        type != nullptr ? type->sage_class_name() : "<null>");
  }
  ~AuxiliaryTypeGuard() { currentAuxiliaryTypeStack.pop_back(); }
};

bool isAstJsonExternalMarker(SgNode *node) {
  return isAstJsonExternalFunction(isSgFunctionDeclaration(node)) ||
         isAstJsonExternalModule(isSgModuleStatement(node)) ||
         isAstJsonExternalClassDeclaration(isSgClassDeclaration(node));
}

bool isStructuralAstChildOfParent(SgNode *node) {
  SgNode *parent = node != nullptr ? node->get_parent() : nullptr;
  if (parent == nullptr || isAstJsonExternalMarker(parent)) {
    return false;
  }
  for (const std::pair<SgNode *, std::string> &entry :
       parent->returnDataMemberPointers()) {
    if (entry.first != node) {
      continue;
    }
    if (entry.second == "parent" || entry.second == "scope" ||
        entry.second == "symbol_table" || entry.second == "type_table" ||
        entry.second == "firstNondefiningDeclaration" ||
        entry.second == "definingDeclaration") {
      continue;
    }
    return true;
  }
  return false;
}

bool hasNonStructuralExternalMarkerAncestor(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (isAstJsonExternalMarker(current) &&
        !isStructuralAstChildOfParent(current)) {
      return true;
    }
  }
  return false;
}

void addSingleNode(SgNode *node, std::vector<SgNode *> &nodes,
                   std::unordered_set<SgNode *> &seen) {
  if (hasNonStructuralExternalMarkerAncestor(node)) {
    return;
  }
  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    if (collectionBoundaryFile != nullptr &&
        isSgType(ref->get_parent()) != nullptr) {
      const SgNode *basis = symbolBasis(ref->get_symbol());
      if (basis != nullptr &&
          !insideCollectionBoundary(const_cast<SgNode *>(basis))) {
        std::ostringstream message;
        message << "AST JSON type-owned SgVarRefExp references a symbol "
                   "outside the collection boundary: "
                << symbolName(ref->get_symbol());
        if (currentAuxiliaryOwner != nullptr) {
          message << " owner=" << currentAuxiliaryOwner->sage_class_name()
                  << " owner_text=" << safeNodeText(currentAuxiliaryOwner);
        }
        if (!currentAuxiliaryTypeStack.empty()) {
          message << " type_stack=";
          for (size_t i = 0; i < currentAuxiliaryTypeStack.size(); ++i) {
            if (i != 0) {
              message << "/";
            }
            message << currentAuxiliaryTypeStack[i];
          }
        }
        if (const SgNode *parent = ref->get_parent()) {
          message << " parent=" << parent->sage_class_name();
        }
        throw std::runtime_error(message.str());
      }
    }
  }
  if (node != nullptr && insideCollectionBoundary(node) &&
      seen.insert(node).second) {
    nodes.push_back(node);
  }
}

void addSubtreeNodes(SgNode *root, std::vector<SgNode *> &nodes,
                     std::unordered_set<SgNode *> &seen) {
  if (root == nullptr || !insideCollectionBoundary(root)) {
    return;
  }
  addSingleNode(root, nodes, seen);
  if (isSgBaseClass(root) != nullptr) {
    return;
  }
  RoseAst ast(root);
  for (RoseAst::iterator it = ast.begin().withoutNullValues(); it != ast.end();
       ++it) {
    if (insideCollectionBoundary(*it)) {
      addSingleNode(*it, nodes, seen);
    }
    if (isSgBaseClass(*it) != nullptr) {
      it.skipChildrenOnForward();
    }
  }
}

void addNodeAncestors(SgNode *node, std::vector<SgNode *> &nodes,
                      std::unordered_set<SgNode *> &seen) {
  for (SgNode *current = node != nullptr ? node->get_parent() : nullptr;
       current != nullptr; current = current->get_parent()) {
    if (!insideCollectionBoundary(current) ||
        isSgSourceFile(current) != nullptr ||
        isSgFileList(current) != nullptr || isSgProject(current) != nullptr) {
      return;
    }
    addSingleNode(current, nodes, seen);
  }
}

void addReferencedSymbolBasis(const SgSymbol *symbol,
                              std::vector<SgNode *> &nodes,
                              std::unordered_set<SgNode *> &seen) {
  SgNode *basis = const_cast<SgNode *>(symbolBasis(symbol));
  if (hasNonStructuralExternalMarkerAncestor(basis)) {
    return;
  }
  addSubtreeNodes(basis, nodes, seen);
  addNodeAncestors(basis, nodes, seen);
}

void addExpressionSymbolDependencies(SgNode *node, std::vector<SgNode *> &nodes,
                                     std::unordered_set<SgNode *> &seen);

void addExpressionSubtreeSymbolDependencies(
    SgExpression *root, std::vector<SgNode *> &nodes,
    std::unordered_set<SgNode *> &seen) {
  if (root == nullptr || !insideCollectionBoundary(root)) {
    return;
  }
  addExpressionSymbolDependencies(root, nodes, seen);
  RoseAst ast(root);
  for (RoseAst::iterator it = ast.begin().withoutNullValues(); it != ast.end();
       ++it) {
    if (!insideCollectionBoundary(*it)) {
      continue;
    }
    addExpressionSymbolDependencies(*it, nodes, seen);
    if (isSgBaseClass(*it) != nullptr) {
      it.skipChildrenOnForward();
    }
  }
}

void addOmpAuxiliaryNodes(SgNode *node, std::vector<SgNode *> &nodes,
                          std::unordered_set<SgNode *> &seen) {
  AuxiliaryOwnerGuard owner_guard(node);
  if (SgNode *parent = node != nullptr ? node->get_parent() : nullptr) {
    if (isSgType(parent) == nullptr && isSgFileList(parent) == nullptr &&
        isSgProject(parent) == nullptr) {
      addSubtreeNodes(parent, nodes, seen);
    }
  }

  auto add_expression_with_owner = [&](SgExpression *expr) {
    if (expr == nullptr) {
      return;
    }
    SgExpression *owner = expr;
    while (SgExpression *parent = isSgExpression(owner->get_parent())) {
      owner = parent;
    }
    SgNode *container = owner->get_parent();
    if (isSgExprStatement(container) != nullptr ||
        isSgInitializer(container) != nullptr) {
      addSubtreeNodes(container, nodes, seen);
      addExpressionSubtreeSymbolDependencies(owner, nodes, seen);
    } else {
      addSubtreeNodes(owner, nodes, seen);
      addExpressionSubtreeSymbolDependencies(owner, nodes, seen);
    }
  };

  std::function<void(SgType *)> add_type;
  auto add_type_owned_expression_dependencies = [&](SgExpression *expr) {
    if (expr == nullptr) {
      return;
    }
    addExpressionSubtreeSymbolDependencies(expr, nodes, seen);

    auto add_expression_type = [&](SgExpression *current) {
      if (current == nullptr) {
        return;
      }
      if (SgNewExp *new_expr = isSgNewExp(current)) {
        add_type(new_expr->get_specified_type());
      } else {
        add_type(current->get_type());
      }
      if (SgTypeExpression *type_expr = isSgTypeExpression(current)) {
        add_type(type_expr->get_type());
      }
    };

    add_expression_type(expr);
    RoseAst ast(expr);
    for (RoseAst::iterator it = ast.begin().withoutNullValues();
         it != ast.end(); ++it) {
      if (SgExpression *child = isSgExpression(*it)) {
        add_expression_type(child);
      }
      if (isSgBaseClass(*it) != nullptr) {
        it.skipChildrenOnForward();
      }
    }
  };

  add_type = [&](SgType *type) -> void {
    if (type == nullptr) {
      return;
    }
    AuxiliaryTypeGuard type_guard(type);
    if (SgPointerMemberType *member_pointer = isSgPointerMemberType(type)) {
      add_type(member_pointer->get_base_type());
      add_type(member_pointer->get_class_type());
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      add_type(pointer->get_base_type());
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      add_type(reference->get_base_type());
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      add_type(reference->get_base_type());
    } else if (SgTypeString *string_type = isSgTypeString(type)) {
      add_type_owned_expression_dependencies(
          string_type->get_lengthExpression());
      add_type_owned_expression_dependencies(string_type->get_type_kind());
    } else if (SgTypeComplex *complex_type = isSgTypeComplex(type)) {
      add_type(complex_type->get_base_type());
      add_type_owned_expression_dependencies(complex_type->get_type_kind());
    } else if (SgArrayType *array = isSgArrayType(type)) {
      add_type(array->get_base_type());
      add_type_owned_expression_dependencies(array->get_index());
      add_type_owned_expression_dependencies(array->get_dim_info());
    } else if (SgModifierType *modifier = isSgModifierType(type)) {
      add_type(modifier->get_base_type());
    } else if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      addSubtreeNodes(typedef_type->get_declaration(), nodes, seen);
    } else if (SgClassType *class_type = isSgClassType(type)) {
      addSubtreeNodes(class_type->get_declaration(), nodes, seen);
    } else if (SgEnumType *enum_type = isSgEnumType(type)) {
      addSubtreeNodes(enum_type->get_declaration(), nodes, seen);
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
      addSubtreeNodes(nonreal_type->get_declaration(), nodes, seen);
    } else if (SgTemplateType *template_type = isSgTemplateType(type)) {
      add_type(template_type->get_class_type());
      add_type(template_type->get_parent_class_type());
    } else if (SgMemberFunctionType *member_type =
                   isSgMemberFunctionType(type)) {
      add_type(member_type->get_return_type());
      add_type(member_type->get_class_type());
      for (SgType *arg_type : member_type->get_arguments()) {
        add_type(arg_type);
      }
    } else if (SgFunctionType *function_type = isSgFunctionType(type)) {
      add_type(function_type->get_return_type());
      for (SgType *arg_type : function_type->get_arguments()) {
        add_type(arg_type);
      }
    } else if (SgDeclType *decl_type = isSgDeclType(type)) {
      add_type_owned_expression_dependencies(decl_type->get_base_expression());
    }
  };
  auto add_template_argument = [&](auto &self,
                                   SgTemplateArgument *argument) -> void {
    if (argument == nullptr) {
      return;
    }
    addSubtreeNodes(argument, nodes, seen);
    add_type(argument->get_type());
    add_expression_with_owner(argument->get_expression());
    addSubtreeNodes(argument->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(argument->get_initializedName(), nodes, seen);
  };
  auto add_template_arguments =
      [&](const SgTemplateArgumentPtrList &arguments) {
        for (SgTemplateArgument *argument : arguments) {
          add_template_argument(add_template_argument, argument);
        }
      };
  auto add_template_parameter = [&](auto &self,
                                    SgTemplateParameter *parameter) -> void {
    if (parameter == nullptr) {
      return;
    }
    addSubtreeNodes(parameter, nodes, seen);
    add_type(parameter->get_type());
    add_type(parameter->get_defaultTypeParameter());
    add_expression_with_owner(parameter->get_expression());
    add_expression_with_owner(parameter->get_typeConstraint());
    add_expression_with_owner(parameter->get_defaultExpressionParameter());
    addSubtreeNodes(parameter->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(parameter->get_defaultTemplateDeclarationParameter(), nodes,
                    seen);
    addSubtreeNodes(parameter->get_initializedName(), nodes, seen);
  };
  auto add_template_parameters =
      [&](const SgTemplateParameterPtrList &parameters) {
        for (SgTemplateParameter *parameter : parameters) {
          add_template_parameter(add_template_parameter, parameter);
        }
      };

  if (SgExpression *expr = isSgExpression(node)) {
    addSubtreeNodes(expr->get_originalExpressionTree(), nodes, seen);
    if (SgNewExp *new_expr = isSgNewExp(expr)) {
      add_type(new_expr->get_specified_type());
    } else {
      add_type(expr->get_type());
    }
    if (SgTypeExpression *type_expr = isSgTypeExpression(expr)) {
      add_type(type_expr->get_type());
    }
    if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    } else if (SgTemplateFunctionRefExp *ref =
                   isSgTemplateFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol_i(), nodes, seen);
    } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
      add_template_arguments(ref->get_templateArguments());
    } else if (SgThisExp *this_expr = isSgThisExp(expr)) {
      addReferencedSymbolBasis(this_expr->get_class_symbol(), nodes, seen);
      addReferencedSymbolBasis(this_expr->get_nonreal_symbol(), nodes, seen);
    } else if (SgConstructorInitializer *init =
                   isSgConstructorInitializer(expr)) {
      addSubtreeNodes(init->get_declaration(), nodes, seen);
    }
    if (SgTypeTraitBuiltinOperator *op = isSgTypeTraitBuiltinOperator(expr)) {
      for (SgNode *arg : op->get_args()) {
        if (SgType *type_arg = isSgType(arg)) {
          add_type(type_arg);
        } else {
          addSubtreeNodes(arg, nodes, seen);
        }
      }
    }
  }
  if (SgInitializedName *name = isSgInitializedName(node)) {
    add_type(name->get_typeptr());
    addSubtreeNodes(name->get_declptr(), nodes, seen);
    addSubtreeNodes(name->get_definition(), nodes, seen);
    addSubtreeNodes(name->get_prev_decl_item(), nodes, seen);
  }
  if (SgVariableDefinition *def = isSgVariableDefinition(node)) {
    addSubtreeNodes(def->get_vardefn(), nodes, seen);
    addSubtreeNodes(def->get_bitfield(), nodes, seen);
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    add_type(decl->get_base_type());
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    add_type(decl->get_type());
    addSubtreeNodes(decl->get_functionParameterScope(), nodes, seen);
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    addSubtreeNodes(decl->get_scope(), nodes, seen);
    addSubtreeNodes(decl->get_firstNondefiningDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_definingDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_declarationScope(), nodes, seen);
    addSubtreeNodes(decl->get_nonreal_decl_scope(), nodes, seen);
  }
  if (SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(node)) {
    addSubtreeNodes(def->get_namespaceDeclaration(), nodes, seen);
    addSingleNode(def->get_previousNamespaceDefinition(), nodes, seen);
    addSingleNode(def->get_nextNamespaceDefinition(), nodes, seen);
    addSingleNode(def->get_global_definition(), nodes, seen);
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    add_type(decl->get_type());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    add_template_arguments(decl->get_tpl_args());
    add_expression_with_owner(decl->get_conceptConstraint());
  }
  if (SgClassDefinition *def = isSgClassDefinition(node)) {
    for (SgBaseClass *base : def->get_inheritances()) {
      addSingleNode(base, nodes, seen);
    }
  }
  if (SgBaseClass *base = isSgBaseClass(node)) {
    addSubtreeNodes(base->get_base_class(), nodes, seen);
    if (SgExpBaseClass *expr_base = isSgExpBaseClass(base)) {
      add_expression_with_owner(expr_base->get_base_class_exp());
    }
    if (SgNonrealBaseClass *nonreal_base = isSgNonrealBaseClass(base)) {
      addSubtreeNodes(nonreal_base->get_base_class_nonreal(), nodes, seen);
    }
  }
  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateInstantiationTypedefDeclaration *decl =
          isSgTemplateInstantiationTypedefDeclaration(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateInstantiationFunctionDecl *decl =
          isSgTemplateInstantiationFunctionDecl(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateInstantiationMemberFunctionDecl *decl =
          isSgTemplateInstantiationMemberFunctionDecl(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateVariableDeclaration *decl =
          isSgTemplateVariableDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateTypedefDeclaration *decl =
          isSgTemplateTypedefDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgTemplateFunctionDeclaration *decl =
          isSgTemplateFunctionDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgTemplateMemberFunctionDeclaration *decl =
          isSgTemplateMemberFunctionDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgOmpExecStatement *stmt = isSgOmpExecStatement(node)) {
    addSubtreeNodes(stmt->get_omp_parent(), nodes, seen);
    for (SgStatement *child : stmt->get_omp_children()) {
      addSubtreeNodes(child, nodes, seen);
    }
  }
  if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    add_template_argument(add_template_argument, argument);
  }
  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    add_template_parameter(add_template_parameter, parameter);
  }
  auto add_dimension_map = [&](const auto &dimension_map) {
    for (const auto &entry : dimension_map) {
      addReferencedSymbolBasis(entry.first, nodes, seen);
      for (const auto &bound : entry.second) {
        add_expression_with_owner(bound.first);
        add_expression_with_owner(bound.second);
      }
    }
  };
  auto add_iterators = [&](const std::list<std::list<SgExpression *>> &lists) {
    for (const std::list<SgExpression *> &list : lists) {
      for (SgExpression *expr : list) {
        add_expression_with_owner(expr);
      }
    }
  };
  auto add_clause_list = [&](const SgOmpClausePtrList &clauses) {
    for (SgOmpClause *clause : clauses) {
      addSubtreeNodes(clause, nodes, seen);
    }
  };

  if (SgOmpClauseStatement *stmt = isSgOmpClauseStatement(node)) {
    add_clause_list(stmt->get_clauses());
  } else if (SgOmpClauseBodyStatement *stmt =
                 isSgOmpClauseBodyStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpRequiresStatement *stmt = isSgOmpRequiresStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpTaskwaitStatement *stmt = isSgOmpTaskwaitStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }

  if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
    addSubtreeNodes(clause->get_variables(), nodes, seen);
    addSubtreeNodes(clause->get_mapper_identifier(), nodes, seen);
    add_dimension_map(clause->get_array_dimensions());
    for (const auto &entry : clause->get_dist_data_policies()) {
      addReferencedSymbolBasis(entry.first, nodes, seen);
      for (const auto &policy : entry.second) {
        addSubtreeNodes(policy.second, nodes, seen);
      }
    }
    add_iterators(clause->get_iterator());
  } else if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
    add_dimension_map(clause->get_array_dimensions());
    for (SgExpression *expr : clause->get_vec()) {
      add_expression_with_owner(expr);
    }
    add_iterators(clause->get_iterator());
  } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(node)) {
    add_dimension_map(clause->get_array_dimensions());
    add_iterators(clause->get_iterator());
  } else if (SgOmpToClause *clause = isSgOmpToClause(node)) {
    addSubtreeNodes(clause->get_mapper_identifier(), nodes, seen);
    add_dimension_map(clause->get_array_dimensions());
    add_iterators(clause->get_iterator());
  } else if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
    addSubtreeNodes(clause->get_mapper_identifier(), nodes, seen);
    add_dimension_map(clause->get_array_dimensions());
    add_iterators(clause->get_iterator());
  }

  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    addSubtreeNodes(clause->get_variables(), nodes, seen);
  }
  if (SgOmpExclusiveClause *clause = isSgOmpExclusiveClause(node)) {
    addSubtreeNodes(clause->get_variables(), nodes, seen);
  }
  if (SgOmpExpressionClause *clause = isSgOmpExpressionClause(node)) {
    add_expression_with_owner(clause->get_expression());
  }
  if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
    add_expression_with_owner(clause->get_user_defined_identifier());
  }
  if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    add_expression_with_owner(clause->get_user_defined_modifier());
  }
  if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    add_expression_with_owner(clause->get_user_defined_modifier());
  }
  if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    add_expression_with_owner(clause->get_user_defined_modifier());
  }
  if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
    add_expression_with_owner(clause->get_user_defined_identifier());
  }
  if (SgOmpTaskReductionClause *clause = isSgOmpTaskReductionClause(node)) {
    add_expression_with_owner(clause->get_user_defined_identifier());
  }
  if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
    add_expression_with_owner(clause->get_user_condition());
    add_expression_with_owner(clause->get_user_condition_score());
    add_expression_with_owner(clause->get_device_arch());
    add_expression_with_owner(clause->get_device_isa());
    add_expression_with_owner(clause->get_device_num());
    add_expression_with_owner(clause->get_implementation_user_defined());
    add_expression_with_owner(clause->get_implementation_extension());
    addSubtreeNodes(clause->get_variant_directive(), nodes, seen);
    for (SgStatement *directive : clause->get_construct_directives()) {
      addSubtreeNodes(directive, nodes, seen);
    }
  }
  if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
    add_expression_with_owner(clause->get_user_condition());
    add_expression_with_owner(clause->get_user_condition_score());
    add_expression_with_owner(clause->get_device_arch());
    add_expression_with_owner(clause->get_device_isa());
    add_expression_with_owner(clause->get_device_num());
    add_expression_with_owner(clause->get_implementation_user_defined());
    add_expression_with_owner(clause->get_implementation_extension());
    for (SgStatement *directive : clause->get_construct_directives()) {
      addSubtreeNodes(directive, nodes, seen);
    }
  }
  if (SgOmpUsesAllocatorsDefination *definition =
          isSgOmpUsesAllocatorsDefination(node)) {
    add_expression_with_owner(definition->get_user_defined_allocator());
    add_expression_with_owner(definition->get_allocator_traits_array());
  }
  if (SgOmpUsesAllocatorsClause *clause = isSgOmpUsesAllocatorsClause(node)) {
    for (SgOmpUsesAllocatorsDefination *definition :
         clause->get_uses_allocators_defination()) {
      if (definition == nullptr) {
        continue;
      }
      addSingleNode(definition, nodes, seen);
      add_expression_with_owner(definition->get_user_defined_allocator());
      add_expression_with_owner(definition->get_allocator_traits_array());
    }
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    add_expression_with_owner(stmt->get_user_defined_identifier());
    add_expression_with_owner(stmt->get_mapper_type());
    add_expression_with_owner(stmt->get_mapper_variable());
  }
  if (SgAttributeSpecificationStatement *stmt =
          isSgAttributeSpecificationStatement(node)) {
    addSubtreeNodes(stmt->get_parameter_list(), nodes, seen);
    addSubtreeNodes(stmt->get_bind_list(), nodes, seen);
  }
  if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
    for (SgInterfaceBody *body : stmt->get_interface_body_list()) {
      addSubtreeNodes(body, nodes, seen);
    }
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    addSubtreeNodes(body->get_functionDeclaration(), nodes, seen);
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    addSubtreeNodes(stmt->get_else_numeric_label(), nodes, seen);
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgStatement *stmt = isSgStatement(node)) {
    addSubtreeNodes(stmt->get_numeric_label(), nodes, seen);
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgGotoStatement *stmt = isSgGotoStatement(node)) {
    addSubtreeNodes(stmt->get_label(), nodes, seen);
    addSubtreeNodes(stmt->get_label_expression(), nodes, seen);
    addSubtreeNodes(stmt->get_selector_expression(), nodes, seen);
  }
  if (SgFortranNonblockedDo *stmt = isSgFortranNonblockedDo(node)) {
    addSubtreeNodes(stmt->get_end_statement(), nodes, seen);
  }
  if (SgDerivedTypeStatement *stmt = isSgDerivedTypeStatement(node)) {
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgUsingDeclarationStatement *decl = isSgUsingDeclarationStatement(node)) {
    addSubtreeNodes(decl->get_declaration(), nodes, seen);
    addSubtreeNodes(decl->get_initializedName(), nodes, seen);
  }
}

void addExpressionSymbolDependencies(SgNode *node, std::vector<SgNode *> &nodes,
                                     std::unordered_set<SgNode *> &seen) {
  SgExpression *expr = isSgExpression(node);
  if (expr == nullptr) {
    return;
  }
  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
  } else if (SgLabelRefExp *ref = isSgLabelRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    SgFunctionSymbol *symbol = ref->get_symbol();
    addReferencedSymbolBasis(symbol, nodes, seen);
    if (symbol != nullptr &&
        !isAstJsonExternalFunction(symbol->get_declaration())) {
      addSubtreeNodes(symbol->get_declaration(), nodes, seen);
      addNodeAncestors(symbol->get_declaration(), nodes, seen);
    }
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol_i(), nodes, seen);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
  } else if (SgThisExp *this_expr = isSgThisExp(expr)) {
    addReferencedSymbolBasis(this_expr->get_class_symbol(), nodes, seen);
    addReferencedSymbolBasis(this_expr->get_nonreal_symbol(), nodes, seen);
  } else if (SgEnumVal *value = isSgEnumVal(expr)) {
    addSubtreeNodes(value->get_declaration(), nodes, seen);
    addNodeAncestors(value->get_declaration(), nodes, seen);
  }
}

void addScopeSymbolTableDependencies(SgNode *node, std::vector<SgNode *> &nodes,
                                     std::unordered_set<SgNode *> &seen) {
  SgScopeStatement *scope = isSgScopeStatement(node);
  SgSymbolTable *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  if (table == nullptr || table->get_table() == nullptr) {
    return;
  }
  for (const std::pair<const SgName, SgSymbol *> &entry : *table->get_table()) {
    SgSymbol *symbol = entry.second;
    addReferencedSymbolBasis(symbol, nodes, seen);
    if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      addReferencedSymbolBasis(alias->get_alias(), nodes, seen);
      for (SgNode *causal_node : alias->get_causal_nodes()) {
        addSubtreeNodes(causal_node, nodes, seen);
        addNodeAncestors(causal_node, nodes, seen);
      }
    }
    if (SgRenameSymbol *rename = isSgRenameSymbol(symbol)) {
      addReferencedSymbolBasis(rename->get_original_symbol(), nodes, seen);
    }
    if (SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
      addSubtreeNodes(namespace_symbol->get_aliasDeclaration(), nodes, seen);
      addNodeAncestors(namespace_symbol->get_aliasDeclaration(), nodes, seen);
    }
  }
}

std::string auxiliaryNodeSortKey(SgNode *node) {
  std::ostringstream key;
  key << (node != nullptr ? node->sage_class_name() : "") << '|';
  if (SgInitializedName *name = isSgInitializedName(node)) {
    key << name->get_name().getString();
  } else if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    key << decl->get_name().getString();
  } else if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    key << argument->get_argumentType() << ':'
        << jsonTypeText(argument->get_type());
    if (argument->get_expression() != nullptr) {
      key << ':' << argument->get_expression()->sage_class_name() << ':'
          << argument->get_expression()->unparseToString();
    }
  } else if (SgIntVal *value = isSgIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgUnsignedIntVal *value = isSgUnsignedIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgLongIntVal *value = isSgLongIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgUnsignedLongVal *value = isSgUnsignedLongVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgLongLongIntVal *value = isSgLongLongIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgUnsignedLongLongIntVal *value =
                 isSgUnsignedLongLongIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  }
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    if (Sg_File_Info *start = located->get_startOfConstruct()) {
      key << '|' << start->get_filenameString() << ':' << start->get_raw_line()
          << ':' << start->get_raw_col();
    }
  }
  return key.str();
}

std::vector<SgNode *> collectNodes(SgNode *root) {
  CollectionBoundaryGuard boundary(isSgSourceFile(root));
  std::vector<SgNode *> nodes;
  std::unordered_set<SgNode *> seen;
  addSubtreeNodes(root, nodes, seen);
  if (SgSourceFile *file = isSgSourceFile(root)) {
    for (SgToken *token : file->get_token_list()) {
      addSingleNode(token, nodes, seen);
    }
  }
  const size_t original_count = nodes.size();
  for (size_t i = 0; i < nodes.size(); ++i) {
    addOmpAuxiliaryNodes(nodes[i], nodes, seen);
    addExpressionSymbolDependencies(nodes[i], nodes, seen);
    addScopeSymbolTableDependencies(nodes[i], nodes, seen);
  }
  for (size_t i = original_count; i < nodes.size(); ++i) {
    addOmpAuxiliaryNodes(nodes[i], nodes, seen);
    addExpressionSymbolDependencies(nodes[i], nodes, seen);
    addScopeSymbolTableDependencies(nodes[i], nodes, seen);
  }
  std::stable_sort(nodes.begin() + original_count, nodes.end(),
                   [](SgNode *lhs, SgNode *rhs) {
                     return auxiliaryNodeSortKey(lhs) <
                            auxiliaryNodeSortKey(rhs);
                   });
  return nodes;
}

void clearGlobalQualificationState() {
  SgNode::get_globalQualifiedNameMapForNames().clear();
  SgNode::get_globalQualifiedNameMapForTypes().clear();
  SgNode::get_globalQualifiedNameMapForTemplateHeaders().clear();
  SgNode::get_globalTypeNameMap().clear();
  SgNode::get_globalQualifiedNameMapForMapsOfTypes().clear();
}

GlobalQualificationStateSnapshot::GlobalQualificationStateSnapshot()
    : names(SgNode::get_globalQualifiedNameMapForNames()),
      types(SgNode::get_globalQualifiedNameMapForTypes()),
      template_headers(SgNode::get_globalQualifiedNameMapForTemplateHeaders()),
      type_names(SgNode::get_globalTypeNameMap()),
      type_maps(SgNode::get_globalQualifiedNameMapForMapsOfTypes()) {}

GlobalQualificationStateSnapshot::~GlobalQualificationStateSnapshot() {
  restore();
}

void GlobalQualificationStateSnapshot::restore() {
  if (restored) {
    return;
  }
  SgNode::get_globalQualifiedNameMapForNames() = names;
  SgNode::get_globalQualifiedNameMapForTypes() = types;
  SgNode::get_globalQualifiedNameMapForTemplateHeaders() = template_headers;
  SgNode::get_globalTypeNameMap() = type_names;
  SgNode::get_globalQualifiedNameMapForMapsOfTypes() = type_maps;
  restored = true;
}

std::string safeNodeText(SgNode *node) {
  if (node == nullptr) {
    return "";
  }
  if (SgInitializedName *name = isSgInitializedName(node)) {
    return name->get_name().getString();
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgNamespaceDeclarationStatement *decl =
          isSgNamespaceDeclarationStatement(node)) {
    return decl->get_name().getString();
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    return decl->get_name().getString();
  }
  if (SgPragma *pragma = isSgPragma(node)) {
    return pragma->get_name();
  }
  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  if (SgFunctionRefExp *ref = isSgFunctionRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(node)) {
    return ref->get_symbol_i() != nullptr
               ? ref->get_symbol_i()->get_name().getString()
               : "";
  }
  if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  return "";
}

std::string sourceFileNameForNode(SgNode *node) {
  const std::string external_source =
      astJsonStringAttribute(node, kAstJsonExternalSourceFileAttribute);
  if (!external_source.empty()) {
    return external_source;
  }
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgSourceFile *source_file = isSgSourceFile(current)) {
      return source_file->getFileName();
    }
  }
  return "";
}

std::string sourceFileNameForExternalFunction(SgFunctionDeclaration *decl) {
  std::string source_file = sourceFileNameForNode(decl);
  if (source_file.empty() && collectionBoundaryFile != nullptr) {
    source_file = collectionBoundaryFile->getFileName();
  }
  if (source_file.empty()) {
    std::ostringstream message;
    message << "AST JSON external_function ";
    if (decl == nullptr) {
      message << "<null>";
    } else {
      message << decl->get_name().getString();
      if (SgNode *parent = decl->get_parent()) {
        message << " parent=" << parent->sage_class_name();
      } else {
        message << " parent=<null>";
      }
    }
    message << " requires a non-empty source_file";
    throw std::runtime_error(message.str());
  }
  return source_file;
}

SgModuleStatement *enclosingModuleStatement(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgModuleStatement *module = isSgModuleStatement(current)) {
      return module;
    }
  }
  return nullptr;
}

std::string moduleNameForNode(SgNode *node) {
  SgModuleStatement *module = enclosingModuleStatement(node);
  return module != nullptr ? module->get_name().getString() : "";
}

bool classDeclarationHasDefinition(SgClassDeclaration *decl) {
  return decl != nullptr && decl->get_definition() != nullptr;
}

bool classDeclarationIsFirstNondefining(SgClassDeclaration *decl) {
  return decl != nullptr && decl->get_firstNondefiningDeclaration() == decl;
}

} // namespace AstJson
} // namespace Rose
