#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

void writeNodeJson(std::ostream &out, SgNode *node,
                   const std::unordered_map<const SgNode *, uint64_t> &ids,
                   bool comma) {
  if (SgAuxiliaryDeclarationList *container =
          isSgAuxiliaryDeclarationList(node)) {
    SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
    if (owner == nullptr || owner->get_auxiliary_declarations() != container ||
        container->get_declarations().empty()) {
      fprintf(stderr,
              "REX_AST_JSON_INVARIANT[auxiliary-ownership]: container=%p "
              "owner=%p/%s published=%p declarations=%zu\n",
              static_cast<void *>(container), static_cast<void *>(owner),
              owner != nullptr ? owner->sage_class_name() : "<null>",
              static_cast<void *>(owner != nullptr
                                      ? owner->get_auxiliary_declarations()
                                      : nullptr),
              container->get_declarations().size());
      if (owner != nullptr) {
        size_t depth = 0;
        for (SgNode *cursor = owner; cursor != nullptr && depth != 12;
             cursor = cursor->get_parent(), ++depth) {
          SgFunctionDeclaration *function = isSgFunctionDeclaration(cursor);
          fprintf(stderr,
                  "REX_AST_JSON_INVARIANT[auxiliary-ownership-owner]: "
                  "depth=%zu node=%p/%s parent=%p function=%s\n",
                  depth, static_cast<void *>(cursor), cursor->sage_class_name(),
                  static_cast<void *>(cursor->get_parent()),
                  function != nullptr ? function->get_name().getString().c_str()
                                      : "<none>");
          if (SgBasicBlock *block = isSgBasicBlock(cursor)) {
            fprintf(stderr,
                    "REX_AST_JSON_INVARIANT[auxiliary-ownership-block]: "
                    "depth=%zu statements=%zu",
                    depth, block->get_statements().size());
            for (SgStatement *statement : block->get_statements()) {
              fprintf(stderr, " %p/%s", static_cast<void *>(statement),
                      statement != nullptr ? statement->sage_class_name()
                                           : "<null>");
            }
            fprintf(stderr, "\n");
          }
        }
      }
      throw std::runtime_error(
          "AST JSON cannot serialize malformed auxiliary declaration "
          "ownership");
    }
    for (SgDeclarationStatement *declaration : container->get_declarations()) {
      if (declaration == nullptr || declaration->get_parent() != container ||
          declaration->get_scope() != owner ||
          std::count(container->get_declarations().begin(),
                     container->get_declarations().end(), declaration) != 1 ||
          owner->statementExistsInScope(declaration)) {
        throw std::runtime_error(
            "AST JSON cannot serialize an auxiliary declaration with "
            "inconsistent semantic or source-emission ownership");
      }
    }
  }

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
                         ->get_containsTransformationToSurroundingWhitespace());
  writeBoolField(
      out, 8, "source_range_ends_in_macro_expansion",
      located_for_flags != nullptr &&
          located_for_flags->get_source_range_ends_in_macro_expansion());
  writeBoolField(
      out, 8, "source_range_is_macro_expansion_fragment",
      located_for_flags != nullptr &&
          located_for_flags->get_source_range_is_macro_expansion_fragment(),
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
  SgExpression *expression_for_location = isSgExpression(node);
  writeFileInfoJson(out, 8, node->get_endOfConstruct(),
                    expression_for_location != nullptr);
  if (expression_for_location != nullptr) {
    indent(out, 8);
    out << jsonString("operator") << ": ";
    writeFileInfoJson(out, 8, expression_for_location->get_operatorPosition(),
                      false);
  }
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
  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    if (stmt->get_function_ref() == nullptr ||
        stmt->get_function_ref()->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON declare simd statement has no required exact target");
    }
    append_manual_edge(stmt->get_function_ref(), "function_ref");
    append_clause_edges(stmt->get_clauses());
  }
  if (SgOmpDeclareVariantStatement *stmt =
          isSgOmpDeclareVariantStatement(node)) {
    if (stmt->get_base_function_ref() == nullptr ||
        stmt->get_base_function_ref()->get_parent() != stmt ||
        stmt->get_variant_function_ref() == nullptr ||
        stmt->get_variant_function_ref()->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON declare variant statement has no required exact target");
    }
    append_manual_edge(stmt->get_base_function_ref(), "base_function_ref");
    append_manual_edge(stmt->get_variant_function_ref(),
                       "variant_function_ref");
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
  if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
    for (SgTemplateParameterList *parameter_list :
         decl->get_sourceSpelledTemplateHeaders()) {
      if (parameter_list == nullptr || parameter_list->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON variable source template header is not owned by its "
            "exact SgVariableDeclaration");
      }
      append_manual_edge(parameter_list, "source_spelled_template_headers");
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    for (SgTemplateParameterList *parameter_list :
         decl->get_sourceSpelledTemplateHeaders()) {
      if (parameter_list == nullptr || parameter_list->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON function source template header is not owned by its "
            "exact SgFunctionDeclaration");
      }
      append_manual_edge(parameter_list, "source_spelled_template_headers");
    }
    if (SgFunctionDeclaration *pattern =
            decl->get_templateInstantiationPattern()) {
      if (pattern == decl ||
          isSgTemplateInstantiationFunctionDecl(decl) != nullptr ||
          isSgTemplateInstantiationMemberFunctionDecl(decl) != nullptr ||
          pattern->get_templateInstantiationPattern() != nullptr) {
        throw std::runtime_error(
            "AST JSON function has a malformed exact instantiation-pattern "
            "edge");
      }
    }
    if (decl->get_template_instantiation_pattern_is_unpublished() ==
        (decl->get_templateInstantiationPattern() != nullptr)) {
      if (decl->get_template_instantiation_pattern_is_unpublished()) {
        throw std::runtime_error(
            "AST JSON function has both exact and unpublished instantiation "
            "patterns");
      }
    }
    append_manual_edge(decl->get_templateInstantiationPattern(),
                       "template_instantiation_pattern");
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    for (SgTemplateParameterList *parameter_list :
         decl->get_sourceSpelledTemplateHeaders()) {
      if (parameter_list == nullptr || parameter_list->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON class source template header is not owned by its exact "
            "SgClassDeclaration");
      }
      append_manual_edge(parameter_list, "source_spelled_template_headers");
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
  if (SgBasicBlock *stmt = isSgBasicBlock(node)) {
    append_manual_edge(stmt->get_fortran_block_end_numeric_label(),
                       "end_numeric_label");
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
    if (node == collectionBoundaryRoot && entry.second == "parent") {
      continue;
    }
    if (isSgSourceFile(node) != nullptr &&
        entry.second == "associated_include_file") {
      // SgIncludeFile is parser planning metadata outside the Sage AST
      // ownership graph.  Physical include syntax is serialized on its exact
      // located owner as preprocessing information.
      continue;
    }
    if (collectionBoundaryRoot != nullptr && isSgSourceFile(node) != nullptr &&
        entry.second == "token_list") {
      // Token ownership is part of a complete source-file checkpoint, not of
      // the typed lexical subtree used to compare repeated header instances.
      continue;
    }
    if (entry.second == "frontendExternalFileList") {
      SgSourceFile *source_file = isSgSourceFile(node);
      SgFileList *external_files = isSgFileList(entry.first);
      SgFileList *project_files = source_file != nullptr
                                      ? isSgFileList(source_file->get_parent())
                                      : nullptr;
      if (source_file == nullptr || external_files == nullptr ||
          source_file->get_frontendExternalFileList() != external_files ||
          external_files->get_parent() != source_file ||
          project_files == nullptr ||
          external_files->get_listOfFiles().empty()) {
        throw std::runtime_error(
            "AST JSON found malformed frontend-external file ownership");
      }
      std::unordered_set<SgFile *> unique_external_files;
      for (SgFile *external_file : external_files->get_listOfFiles()) {
        if (external_file == nullptr ||
            external_file->get_parent() != external_files ||
            !external_file->get_skip_unparse() ||
            !external_file->get_skipfinalCompileStep() ||
            std::find(project_files->get_listOfFiles().begin(),
                      project_files->get_listOfFiles().end(), external_file) !=
                project_files->get_listOfFiles().end() ||
            !unique_external_files.insert(external_file).second) {
          throw std::runtime_error(
              "AST JSON found malformed frontend-external source file");
        }
      }
      // Imported module source files are frontend-only semantic context, not
      // part of this source file's checkpoint boundary.  The external
      // declaration records and ownership-path metadata carry every semantic
      // dependency used by the checkpoint.
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
    if (entry.second == "scope") {
      SgInitializedName *name = isSgInitializedName(node);
      SgFunctionParameterList *parameters =
          name != nullptr ? isSgFunctionParameterList(name->get_parent())
                          : nullptr;
      SgFunctionDeclaration *function =
          parameters != nullptr
              ? isSgFunctionDeclaration(parameters->get_parent())
              : nullptr;
      if (function != nullptr && function->get_parameterList() == parameters &&
          function->get_functionParameterScope() == entry.first &&
          functionParameterScopeNeedsExternalReferenceRecord(function, ids)) {
        continue;
      }
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
    writeStringArrayField(out, 4, "frontend_include_ownership_paths", {});
    writeStringArrayField(out, 4, "frontend_system_include_ownership_paths",
                          {});
    writeStringArrayField(out, 4, "frontend_external_ownership_paths", {});
    writeFileIdMapJson(out, 4, false);
  } else {
    writeStringField(out, 4, "source_file", file->get_sourceFileNameWithPath());
    writeStringField(out, 4, "output_file",
                     file->get_unparse_output_filename());
    writeStringArrayField(out, 4, "command_line", commandLine(file));
    const SgStringList &include_ownership_paths =
        file->get_frontendIncludeOwnershipPathList();
    writeStringArrayField(
        out, 4, "frontend_include_ownership_paths",
        std::vector<std::string>(include_ownership_paths.begin(),
                                 include_ownership_paths.end()));
    const SgStringList &system_include_ownership_paths =
        file->get_frontendSystemIncludeOwnershipPathList();
    writeStringArrayField(
        out, 4, "frontend_system_include_ownership_paths",
        std::vector<std::string>(system_include_ownership_paths.begin(),
                                 system_include_ownership_paths.end()));
    const SgStringList &external_ownership_paths =
        file->get_frontendExternalOwnershipPathList();
    writeStringArrayField(
        out, 4, "frontend_external_ownership_paths",
        std::vector<std::string>(external_ownership_paths.begin(),
                                 external_ownership_paths.end()));
    writeBoolField(out, 4, "openmp", file->get_openmp());
    writeBoolField(out, 4, "openmp_parse_only", file->get_openmp_parse_only());
    writeBoolField(out, 4, "openmp_ast_only", file->get_openmp_ast_only());
    writeBoolField(out, 4, "openmp_analyzing", file->get_openmp_analyzing());
    writeBoolField(out, 4, "openmp_lowering", file->get_openmp_lowering());
    writeBoolField(out, 4, "openmp_processed", file->get_openmp_processed());
    writeBoolField(out, 4, "openacc", file->get_openacc());
    writeBoolField(out, 4, "skipfinalCompileStep",
                   file->get_skipfinalCompileStep());
    writeBoolField(out, 4, "unparse_tokens", file->get_unparse_tokens());
    writeFileIdMapJson(out, 4, false);
  }
  indent(out, 2);
  out << "},\n";
}

std::string buildJson(SgNode *root, Checkpoint checkpoint, SgSourceFile *file) {
  ROSE_ASSERT(root != nullptr);

  CollectionBoundaryGuard boundary(file != nullptr ? file
                                                   : isSgSourceFile(root));
  SubtreeBoundaryGuard subtreeBoundary(root != file ? root : nullptr);
  ArrayTypeSerializationIdentityGuard array_type_identity_guard;
  PointerMemberTypeSerializationIdentityGuard type_identity_guard;
  std::vector<SgNode *> nodes = collectNodes(root, file);
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
