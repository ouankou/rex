#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

void writeNodeJson(std::ostream &out, SgNode *node,
                   const std::unordered_map<const SgNode *, uint64_t> &ids,
                   bool comma) {
  indent(out, 4);
  out << "{\n";
  writeIntegerField(out, 6, "id", ids.at(node));
  writeStringField(out, 6, "kind", node->sage_class_name());
  writeIntegerField(out, 6, "variant", node->variantT());
  indent(out, 6);
  out << jsonString("flags") << ": {\n";
  writeBoolField(out, 8, "contains_transformation",
                 node->get_containsTransformation());
  const SgLocatedNode *located_for_flags = isSgLocatedNode(node);
  writeBoolField(out, 8, "contains_transformation_to_surrounding_whitespace",
                 located_for_flags != nullptr &&
                     located_for_flags
                         ->get_containsTransformationToSurroundingWhitespace(),
                 false);
  indent(out, 6);
  out << "},\n";

  indent(out, 6);
  out << jsonString("location") << ": {\n";
  indent(out, 8);
  out << jsonString("start") << ": ";
  writeFileInfoJson(out, 8, node->get_startOfConstruct(), true);
  indent(out, 8);
  out << jsonString("end") << ": ";
  writeFileInfoJson(out, 8, node->get_endOfConstruct(), false);
  indent(out, 6);
  out << "},\n";

  indent(out, 6);
  {
    TypeSerializationContext type_context(node);
    out << jsonString("properties") << ": " << rawNodeProperties(node, ids)
        << ",\n";
  }

  indent(out, 6);
  out << jsonString("edges") << ": [\n";
  std::vector<std::pair<SgNode *, std::string>> pointers =
      node->returnDataMemberPointers();
  auto append_manual_edge = [&](SgNode *target, const std::string &field) {
    if (target == nullptr) {
      return;
    }
    for (const std::pair<SgNode *, std::string> &entry : pointers) {
      if (entry.first == target && entry.second == field) {
        return;
      }
    }
    pointers.push_back(std::make_pair(target, field));
  };
  auto append_clause_edges = [&](const SgOmpClausePtrList &clauses) {
    for (SgOmpClause *clause : clauses) {
      append_manual_edge(clause, "clauses");
    }
  };
  if (SgOmpClauseStatement *stmt = isSgOmpClauseStatement(node)) {
    append_clause_edges(stmt->get_clauses());
  } else if (SgOmpClauseBodyStatement *stmt =
                 isSgOmpClauseBodyStatement(node)) {
    append_clause_edges(stmt->get_clauses());
  }
  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    append_clause_edges(stmt->get_clauses());
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    append_clause_edges(stmt->get_clauses());
  }
  if (SgOmpRequiresStatement *stmt = isSgOmpRequiresStatement(node)) {
    append_clause_edges(stmt->get_clauses());
  }
  if (SgOmpTaskwaitStatement *stmt = isSgOmpTaskwaitStatement(node)) {
    append_clause_edges(stmt->get_clauses());
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    append_manual_edge(decl->get_scope(), "scope");
    if (!declarationNeedsExternalReferenceRecord(
            decl->get_firstNondefiningDeclaration(), ids)) {
      append_manual_edge(decl->get_firstNondefiningDeclaration(),
                         "firstNondefiningDeclaration");
    }
    if (!declarationNeedsExternalReferenceRecord(
            decl->get_definingDeclaration(), ids)) {
      append_manual_edge(decl->get_definingDeclaration(),
                         "definingDeclaration");
    }
  }
  if (SgStatement *stmt = isSgStatement(node)) {
    append_manual_edge(stmt->get_numeric_label(), "numeric_label");
  }
  if (SgLabelStatement *stmt = isSgLabelStatement(node)) {
    append_manual_edge(stmt->get_scope(), "scope");
  }
  if (SgClassDefinition *def = isSgClassDefinition(node)) {
    append_manual_edge(def->get_declaration(), "declaration");
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    append_manual_edge(stmt->get_else_numeric_label(), "else_numeric_label");
    append_manual_edge(stmt->get_end_numeric_label(), "end_numeric_label");
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    append_manual_edge(stmt->get_end_numeric_label(), "end_numeric_label");
  }
  if (SgFortranNonblockedDo *stmt = isSgFortranNonblockedDo(node)) {
    append_manual_edge(stmt->get_end_statement(), "end_statement");
  }
  if (SgGotoStatement *stmt = isSgGotoStatement(node)) {
    append_manual_edge(stmt->get_label(), "label");
    append_manual_edge(stmt->get_label_expression(), "label_expression");
    append_manual_edge(stmt->get_selector_expression(), "selector_expression");
  }
  std::stable_sort(pointers.begin(), pointers.end(),
                   [](const std::pair<SgNode *, std::string> &lhs,
                      const std::pair<SgNode *, std::string> &rhs) {
                     return lhs.second < rhs.second;
                   });

  std::map<std::string, uint64_t> index_by_field;
  bool wrote_edge = false;
  for (const std::pair<SgNode *, std::string> &entry : pointers) {
    if (entry.first == nullptr) {
      continue;
    }
    if (entry.second == "scope") {
      if (const SgStatement *stmt = isSgStatement(node)) {
        if (!stmt->hasExplicitScope() &&
            edgeTargetIsInParentChain(node, entry.first)) {
          continue;
        }
      }
    }
    if (isSg_File_Info(entry.first) != nullptr) {
      continue;
    }
    if (entry.second == "symbol_table" &&
        isSgSymbolTable(entry.first) != nullptr) {
      continue;
    }
    if (entry.second == "type_table" && isSgTypeTable(entry.first) != nullptr) {
      continue;
    }
    if (isSgType(entry.first) != nullptr) {
      continue;
    }
    if (entry.second == "storageModifier" &&
        isSgStorageModifier(entry.first) != nullptr) {
      continue;
    }
    if (entry.second == "baseClassModifier" &&
        isSgBaseClassModifier(entry.first) != nullptr) {
      continue;
    }
    if (isSgSymbol(entry.first) != nullptr) {
      continue;
    }
    if ((entry.second == "firstNondefiningDeclaration" ||
         entry.second == "definingDeclaration") &&
        declarationNeedsExternalReferenceRecord(
            isSgDeclarationStatement(entry.first), ids)) {
      continue;
    }
    if (entry.second == "functionParameterScope" &&
        functionParameterScopeNeedsExternalReferenceRecord(
            isSgFunctionDeclaration(node), ids)) {
      continue;
    }
    auto target = ids.find(entry.first);
    if (target == ids.end()) {
      if (entry.second == "parent" && isSgSourceFile(node) != nullptr &&
          (isSgFileList(entry.first) != nullptr ||
           isSgProject(entry.first) != nullptr)) {
        continue;
      }
      if (isExternalUseModuleEdge(node, entry.first, entry.second, ids)) {
        continue;
      }
      auto describe_node = [](SgNode *candidate) {
        std::ostringstream description;
        if (candidate == nullptr) {
          return std::string("<null>");
        }
        description << candidate->sage_class_name();
        if (SgTemplateInstantiationDecl *decl =
                isSgTemplateInstantiationDecl(candidate)) {
          description << "(" << decl->get_name().getString() << ")";
        } else if (SgTemplateClassDeclaration *decl =
                       isSgTemplateClassDeclaration(candidate)) {
          description << "(" << decl->get_name().getString() << ")";
        } else if (SgClassDeclaration *decl = isSgClassDeclaration(candidate)) {
          description << "(" << decl->get_name().getString() << ")";
        } else if (SgFunctionDeclaration *decl =
                       isSgFunctionDeclaration(candidate)) {
          description << "(" << decl->get_name().getString() << ")";
        } else if (SgInitializedName *name = isSgInitializedName(candidate)) {
          description << "(" << name->get_name().getString() << ")";
        }
        return description.str();
      };
      std::ostringstream source_chain;
      for (SgNode *current = node; current != nullptr;
           current = current->get_parent()) {
        if (source_chain.tellp() > 0) {
          source_chain << " <- ";
        }
        source_chain << describe_node(current);
      }
      std::ostringstream chain;
      std::string target_file;
      for (SgNode *current = entry.first; current != nullptr;
           current = current->get_parent()) {
        if (chain.tellp() > 0) {
          chain << " <- ";
        }
        chain << describe_node(current);
        if (SgSourceFile *source_file = isSgSourceFile(current)) {
          target_file = source_file->getFileName();
        }
      }
      std::string boundary_file = collectionBoundaryFile != nullptr
                                      ? collectionBoundaryFile->getFileName()
                                      : std::string();
      const bool target_is_boundary_global =
          collectionBoundaryFile != nullptr &&
          collectionBoundaryFile->get_globalScope() == entry.first;
      throw std::runtime_error(
          std::string("AST JSON edge target was not collected: ") +
          node->sage_class_name() + "." + entry.second + " -> " +
          entry.first->sage_class_name() + " source_parent_chain=[" +
          source_chain.str() + "] target_parent_chain=[" + chain.str() +
          "] target_file=" + target_file + " boundary_file=" + boundary_file +
          " target_is_boundary_global=" +
          (target_is_boundary_global ? "true" : "false"));
    }
    if (wrote_edge) {
      out << ",\n";
    }
    indent(out, 8);
    out << "{\n";
    writeStringField(out, 10, "field", entry.second);
    writeIntegerField(out, 10, "index", index_by_field[entry.second]++);
    writeIntegerField(out, 10, "target", target->second, false);
    indent(out, 8);
    out << '}';
    wrote_edge = true;
  }
  if (wrote_edge) {
    out << '\n';
  }
  indent(out, 6);
  out << "]\n";
  indent(out, 4);
  out << '}';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeStringArrayField(std::ostream &out, int level, const char *name,
                           const std::vector<std::string> &values,
                           bool comma = true) {
  indent(out, level);
  out << jsonString(name) << ": [";
  if (!values.empty()) {
    out << '\n';
    for (size_t i = 0; i < values.size(); ++i) {
      indent(out, level + 2);
      out << jsonString(values[i]);
      if (i + 1 != values.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, level);
  }
  out << ']';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeMetadataJson(std::ostream &out, SgSourceFile *file,
                       Checkpoint checkpoint) {
  indent(out, 2);
  out << jsonString("metadata") << ": {\n";
  writeStringField(out, 4, "checkpoint", checkpointName(checkpoint));
  if (file == nullptr) {
    writeStringField(out, 4, "source_file", "");
    writeStringField(out, 4, "output_file", "");
    writeStringArrayField(out, 4, "command_line", {});
    writeFileIdMapJson(out, 4, false);
  } else {
    writeStringField(out, 4, "source_file", file->get_sourceFileNameWithPath());
    writeStringField(out, 4, "output_file",
                     file->get_unparse_output_filename());
    writeStringArrayField(out, 4, "command_line", commandLine(file));
    writeBoolField(out, 4, "openmp", file->get_openmp());
    writeBoolField(out, 4, "openmp_parse_only", file->get_openmp_parse_only());
    writeBoolField(out, 4, "openmp_ast_only", file->get_openmp_ast_only());
    writeBoolField(out, 4, "openmp_analyzing", file->get_openmp_analyzing());
    writeBoolField(out, 4, "openmp_lowering", file->get_openmp_lowering());
    writeBoolField(out, 4, "openmp_processed", file->get_openmp_processed());
    writeBoolField(out, 4, "openacc", file->get_openacc());
    writeBoolField(out, 4, "skipfinalCompileStep",
                   file->get_skipfinalCompileStep());
    writeBoolField(out, 4, "suppress_variable_declaration_normalization",
                   file->get_suppress_variable_declaration_normalization());
    writeBoolField(out, 4, "unparse_tokens", file->get_unparse_tokens());
    writeFileIdMapJson(out, 4, false);
  }
  indent(out, 2);
  out << "},\n";
}

std::string buildJson(SgNode *root, Checkpoint checkpoint, SgSourceFile *file) {
  ROSE_ASSERT(root != nullptr);

  struct StaticDataMemberQualificationTraversal : public AstSimpleProcessing {
    static size_t qualificationDepth(const std::string &prefix) {
      size_t depth = 0;
      size_t pos = 0;
      while ((pos = prefix.find("::", pos)) != std::string::npos) {
        ++depth;
        pos += 2;
      }
      return depth;
    }

    static SgClassDeclaration *
    classDeclarationForScope(SgScopeStatement *scope) {
      if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
        return class_def->get_declaration();
      }
      return nullptr;
    }

    static std::string qualifierForOwner(SgClassDeclaration *owner_decl) {
      if (owner_decl == nullptr ||
          isSgTemplateClassDeclaration(owner_decl) != nullptr ||
          isSgTemplateInstantiationDecl(owner_decl) != nullptr) {
        return "";
      }
      std::string prefix = owner_decl->get_qualified_name().getString();
      if (prefix.empty()) {
        return "";
      }
      while (prefix.rfind("::", 0) == 0) {
        prefix.erase(0, 2);
      }
      prefix += "::";
      return prefix;
    }

    static bool insideMemberFunctionOfOwner(SgNode *node,
                                            SgClassDeclaration *owner_decl) {
      for (SgNode *current = node; current != nullptr;
           current = current->get_parent()) {
        if (SgMemberFunctionDeclaration *member_decl =
                isSgMemberFunctionDeclaration(current)) {
          return member_decl->get_class_scope() == owner_decl->get_definition();
        }
      }
      return false;
    }

    static void setNameQualifier(SgNode *node, const std::string &prefix) {
      SgUnorderedMapNodeToString &name_map =
          SgNode::get_globalQualifiedNameMapForNames();
      auto found = name_map.find(node);
      if (found != name_map.end()) {
        std::string existing = found->second;
        while (existing.rfind("::", 0) == 0) {
          existing.erase(0, 2);
        }
        if (existing == prefix) {
          found->second = prefix;
          return;
        }
        throw std::runtime_error(
            "AST JSON qualifier materialization conflicts with existing Sage "
            "qualified-name map");
      }
      name_map[node] = prefix;
    }

    void visit(SgNode *node) override {
      if (SgVarRefExp *ref = isSgVarRefExp(node)) {
        if (isRightHandSideOfMemberAccess(ref)) {
          return;
        }
        SgVariableSymbol *symbol = ref->get_symbol();
        SgInitializedName *decl_name =
            symbol != nullptr ? symbol->get_declaration() : nullptr;
        SgClassDeclaration *owner_decl =
            decl_name != nullptr
                ? classDeclarationForScope(decl_name->get_scope())
                : nullptr;
        const std::string prefix = qualifierForOwner(owner_decl);
        if (!prefix.empty() && !insideMemberFunctionOfOwner(ref, owner_decl)) {
          setNameQualifier(ref, prefix);
          ref->set_name_qualification_length(
              static_cast<int>(qualificationDepth(prefix)));
          ref->set_global_qualification_required(false);
          ref->set_type_elaboration_required(false);
        }
        return;
      }

      SgVariableDeclaration *decl = isSgVariableDeclaration(node);
      if (decl == nullptr || decl->get_variables().empty()) {
        return;
      }

      SgInitializedName *name = decl->get_variables().front();
      if (name == nullptr || name->get_prev_decl_item() == nullptr ||
          name->get_scope() == nullptr || decl->get_scope() == nullptr ||
          name->get_scope() == decl->get_scope()) {
        return;
      }

      SgClassDeclaration *owner_decl =
          classDeclarationForScope(name->get_scope());
      const std::string prefix = qualifierForOwner(owner_decl);
      if (prefix.empty()) {
        return;
      }
      setNameQualifier(name, prefix);
      name->set_name_qualification_length(
          static_cast<int>(qualificationDepth(prefix)));
      name->set_global_qualification_required(prefix.rfind("::", 0) == 0);
      name->set_type_elaboration_required(false);
    }
  };

  StaticDataMemberQualificationTraversal static_data_member_qualification;
  static_data_member_qualification.traverse(root, preorder);

  CollectionBoundaryGuard boundary(isSgSourceFile(root));
  std::vector<SgNode *> nodes = collectNodes(root);
  std::unordered_map<const SgNode *, uint64_t> ids;
  ids.reserve(nodes.size());
  std::unordered_set<uint64_t> used_ids;
  bool has_preserved_ids = false;

  for (SgNode *node : nodes) {
    const uint64_t preserved_id = preservedJsonNodeId(node);
    if (preserved_id == 0) {
      continue;
    }
    if (!used_ids.insert(preserved_id).second) {
      throw std::runtime_error("AST JSON preserved node id is duplicated: " +
                               std::to_string(preserved_id));
    }
    ids.emplace(node, preserved_id);
    has_preserved_ids = true;
  }

  uint64_t next_id = 1;
  for (SgNode *node : nodes) {
    if (ids.find(node) != ids.end()) {
      continue;
    }
    while (used_ids.find(next_id) != used_ids.end()) {
      ++next_id;
    }
    ids.emplace(node, next_id);
    used_ids.insert(next_id);
    ++next_id;
  }

  if (has_preserved_ids) {
    std::stable_sort(nodes.begin(), nodes.end(), [&](SgNode *lhs, SgNode *rhs) {
      return ids.at(lhs) < ids.at(rhs);
    });
  }

  std::ostringstream out;
  out << "{\n";
  writeStringField(out, 2, "format", kFormat);
  writeIntegerField(out, 2, "schema_version", kSchemaVersion);
  writeIntegerField(out, 2, "root_id", ids.at(root));
  writeStringField(out, 2, "root_kind", root->sage_class_name());
  writeIntegerField(out, 2, "node_count", nodes.size());
  writeMetadataJson(out, file, checkpoint);
  indent(out, 2);
  out << jsonString("nodes") << ": [\n";
  for (size_t i = 0; i < nodes.size(); ++i) {
    writeNodeJson(out, nodes[i], ids, i + 1 != nodes.size());
  }
  indent(out, 2);
  out << "]\n";
  out << "}\n";
  return out.str();
}

} // namespace AstJson
} // namespace Rose
