#include "ompAstConstruction.h"
#include "sageAstJsonPrivate.h"
#ifdef ROSE_FLANG_FRONTEND
#include "FlangModuleInfo.h"
#endif

namespace Rose {
namespace AstJson {

std::filesystem::path temporaryWritePath(const std::filesystem::path &path) {
  static uint64_t counter = 0;
  std::ostringstream name;
  name << "." << path.filename().string() << ".tmp." << processId() << "."
       << ++counter;
  return path.parent_path() / name.str();
}

void writeFile(const std::filesystem::path &path, const std::string &content) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  const std::filesystem::path temporary_path = temporaryWritePath(path);
  std::ofstream out(temporary_path);
  if (!out) {
    throw std::runtime_error("failed to open AST JSON file for writing: " +
                             temporary_path.string());
  }
  out << content;
  if (!out) {
    throw std::runtime_error("failed to write AST JSON file: " +
                             temporary_path.string());
  }
  out.close();
  if (!out) {
    throw std::runtime_error("failed to close AST JSON file after writing: " +
                             temporary_path.string());
  }
  std::error_code ec;
  std::filesystem::rename(temporary_path, path, ec);
  if (ec) {
    std::filesystem::remove(temporary_path);
    throw std::runtime_error("failed to publish AST JSON file " +
                             path.string() + ": " + ec.message());
  }
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to read AST JSON file: " + path.string());
  }
  std::ostringstream out;
  out << in.rdbuf();
  if (!in.eof() && in.fail()) {
    throw std::runtime_error("failed while reading AST JSON file: " +
                             path.string());
  }
  return out.str();
}

AstFileRecord parseAstFileJson(const std::string &json,
                               const std::string &expected_checkpoint) {
  JsonValue root = parseJson(json);
  if (root.at("format").asString() != kFormat) {
    throw std::runtime_error("AST JSON format marker is invalid");
  }
  if (root.at("schema_version").asInt() != kSchemaVersion) {
    throw std::runtime_error("AST JSON schema_version is unsupported");
  }

  AstFileRecord result;
  const int64_t root_id = root.requiredInt("root_id");
  if (root_id <= 0) {
    throw std::runtime_error("AST JSON root_id is not positive");
  }
  result.root_id = static_cast<uint64_t>(root_id);
  const int64_t project_files_id = root.requiredInt("project_files_id");
  if (project_files_id < 0) {
    throw std::runtime_error("AST JSON project_files_id is negative");
  }
  result.project_files_id = static_cast<uint64_t>(project_files_id);
  result.root_kind = root.at("root_kind").asString();
  result.metadata = root.at("metadata");
  result.checkpoint = result.metadata.at("checkpoint").asString();
  if (result.checkpoint != expected_checkpoint) {
    throw std::runtime_error("AST JSON checkpoint name does not match request");
  }

  const JsonValue &nodes = root.at("nodes");
  if (nodes.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON nodes field is not an array");
  }
  result.nodes.reserve(nodes.array.size());
  for (const JsonValue &node_value : nodes.array) {
    NodeRecord record;
    const int64_t node_id = node_value.requiredInt("id");
    if (node_id <= 0) {
      throw std::runtime_error("AST JSON node id is not positive");
    }
    record.id = static_cast<uint64_t>(node_id);
    record.kind = node_value.at("kind").asString();
    const int64_t variant = node_value.requiredInt("variant");
    if (variant < 0 || variant >= V_SgNumVariants)
      throw std::runtime_error("AST JSON node variant is out of range");
    record.variant = static_cast<VariantT>(variant);
    if (roseGlobalVariantNameList[record.variant] != record.kind)
      throw std::runtime_error("AST JSON node kind and variant disagree");
    record.flags = node_value.at("flags");
    // Validate every record, including delayed nodes, before AST allocation.
    validateConstructionFlags(record);
    record.location = node_value.at("location");
    record.properties = node_value.at("properties");
    record.preprocessing = record.properties.at("preprocessing");
    (void)record.location.at("start").requiredBool("present");
    (void)record.location.at("end").requiredBool("present");

    const JsonValue &edges = node_value.at("edges");
    if (edges.kind != JsonValue::Kind::Array) {
      throw std::runtime_error("AST JSON node edges field is not an array");
    }
    for (const JsonValue &edge_value : edges.array) {
      EdgeRecord edge;
      edge.field = edge_value.at("field").asString();
      const int64_t index = edge_value.requiredInt("index");
      const int64_t target = edge_value.requiredInt("target");
      if (index < 0 || target <= 0) {
        throw std::runtime_error("AST JSON edge index or target is invalid");
      }
      edge.index = static_cast<uint64_t>(index);
      edge.target = static_cast<uint64_t>(target);
      record.edges.push_back(std::move(edge));
    }

    if (!result.index_by_id.emplace(record.id, result.nodes.size()).second) {
      throw std::runtime_error("AST JSON contains duplicate node id " +
                               std::to_string(record.id));
    }
    result.nodes.push_back(std::move(record));
  }

  if (root.at("node_count").asInt() !=
      static_cast<int64_t>(result.nodes.size())) {
    throw std::runtime_error("AST JSON node_count does not match nodes array");
  }
  // Validate the record graph before reconstruction allocates any Sage nodes.
  // Each serialized field/index names one edge, including auxiliary records
  // that are not reached from the checkpoint root's structural traversal.
  for (const NodeRecord &record : result.nodes) {
    std::unordered_map<std::string, std::unordered_set<uint64_t>> indices;
    for (const EdgeRecord &edge : record.edges) {
      if (edge.field.empty())
        throw std::runtime_error("AST JSON edge field is empty");
      if (!indices[edge.field].insert(edge.index).second)
        throw std::runtime_error(
            "AST JSON contains a duplicate edge field/index");
      (void)result.node(edge.target);
    }
  }
  if (result.root_kind != "SgSourceFile" ||
      result.node(result.root_id).kind != result.root_kind) {
    throw std::runtime_error("AST JSON checkpoint root is not SgSourceFile");
  }
  if (result.project_files_id != 0) {
    const NodeRecord &files = result.node(result.project_files_id);
    if (files.kind != "SgFileList") {
      throw std::runtime_error("AST JSON project_files_id is not SgFileList");
    }
    std::unordered_set<uint64_t> members;
    std::unordered_set<uint64_t> indices;
    for (const EdgeRecord &edge : files.edges) {
      if (edge.field != "listOfFiles" || edge.index >= files.edges.size() ||
          !indices.insert(edge.index).second ||
          !members.insert(edge.target).second) {
        throw std::runtime_error("AST JSON project file list is malformed");
      }
      const NodeRecord &file = result.node(edge.target);
      if (file.kind != "SgSourceFile" ||
          singleEdgeTarget(file, "parent") != result.project_files_id) {
        throw std::runtime_error(
            "AST JSON project file has a conflicting owner");
      }
    }
    if (members.count(result.root_id) != 1) {
      throw std::runtime_error(
          "AST JSON checkpoint root is not a project file");
    }
  }
  validateParentChains(result);
  return result;
}

void validateParentChains(const AstFileRecord &ast) {
  enum class ParentState : unsigned char { Unvisited, Visiting, Complete };
  std::vector<ParentState> parent_states(ast.nodes.size(),
                                         ParentState::Unvisited);
  std::vector<size_t> parent_path;
  for (size_t start = 0; start < ast.nodes.size(); ++start) {
    parent_path.clear();
    size_t current = start;
    while (parent_states[current] != ParentState::Complete) {
      if (parent_states[current] == ParentState::Visiting)
        throw std::runtime_error("AST JSON contains a parent cycle at node " +
                                 std::to_string(ast.nodes[current].id));
      parent_states[current] = ParentState::Visiting;
      parent_path.push_back(current);
      const uint64_t parent = singleEdgeTarget(ast.nodes[current], "parent");
      if (parent == 0)
        break;
      (void)ast.node(parent);
      current = ast.index_by_id.at(parent);
    }
    for (size_t index : parent_path)
      parent_states[index] = ParentState::Complete;
  }
}

void appendComparableObject(
    std::ostream &out, const JsonValue &value,
    const char *error = "AST JSON comparable value is not an object") {
  if (value.kind != JsonValue::Kind::Object)
    throw std::runtime_error(error);
  appendCanonicalJson(out, value);
}

std::string semanticSignature(const AstFileRecord &ast) {
  std::ostringstream out;
  out << "format=" << kFormat << '\n';
  out << "schema=" << kSchemaVersion << '\n';
  out << "checkpoint=" << ast.checkpoint << '\n';
  out << "root=" << ast.root_id << ":" << ast.root_kind << '\n';
  out << "project_files=" << ast.project_files_id << '\n';
  out << "metadata=";
  appendComparableObject(out, ast.metadata);
  out << '\n';

  for (const NodeRecord &record : ast.nodes) {
    out << "node " << record.id << ' ' << record.kind << ' ' << record.variant
        << '\n';
    out << "flags=";
    appendComparableObject(out, record.flags);
    out << '\n';
    out << "location=";
    appendComparableObject(out, record.location);
    out << '\n';
    out << "properties=";
    appendComparableObject(out, record.properties,
                           "AST JSON properties value is not an object");
    out << '\n';

    std::vector<EdgeRecord> edges = record.edges;
    std::sort(edges.begin(), edges.end(),
              [](const EdgeRecord &lhs, const EdgeRecord &rhs) {
                if (lhs.field != rhs.field) {
                  return lhs.field < rhs.field;
                }
                if (lhs.index != rhs.index) {
                  return lhs.index < rhs.index;
                }
                return lhs.target < rhs.target;
              });
    out << "edges=[";
    for (size_t i = 0; i < edges.size(); ++i) {
      if (i != 0) {
        out << ',';
      }
      out << edges[i].field << ':' << edges[i].index << "->" << edges[i].target;
    }
    out << "]\n";
  }
  return out.str();
}

std::filesystem::path checkpointPath(SgSourceFile *file, Checkpoint checkpoint,
                                     const Options &options) {
  static uint64_t counter = 0;
  const std::string source_path = file->get_sourceFileNameWithPath();
  const std::string base = sanitizePathComponent(
      StringUtility::stripPathFromFileName(file->get_sourceFileNameWithPath()));
  const std::string source_id = hex64(fnv1a64(source_path));
  std::ostringstream name;
  name << "rex_ast_" << checkpointName(checkpoint) << "_" << base << "_"
       << source_id << "_p" << processId() << "_" << ++counter << ".json";
  return std::filesystem::path(options.outputDirectory) / name.str();
}

SgProject *owningProject(SgSourceFile *file) {
  if (file == nullptr || file->get_parent() == nullptr) {
    return nullptr;
  }
  SgFileList *file_list = isSgFileList(file->get_parent());
  if (file_list == nullptr) {
    return nullptr;
  }
  return isSgProject(file_list->get_parent());
}

void replaceProjectFiles(SgProject *project, const AstFileRecord &ast,
                         const NodeMap &original, const NodeMap &restored) {
  ROSE_ASSERT(project != nullptr && ast.project_files_id != 0);
  auto *old_list = nodeByIdAs<SgFileList>(original, ast.project_files_id);
  auto *new_list = nodeByIdAs<SgFileList>(restored, ast.project_files_id);
  ROSE_ASSERT(old_list != new_list && old_list->get_parent() == project);
  SgFilePtrList &files = project->get_fileList();
  SgFilePtrList &replacements = new_list->get_listOfFiles();
  ROSE_ASSERT(&files == &old_list->get_listOfFiles());
  ROSE_ASSERT(files.size() == replacements.size());

  std::unordered_map<SgNode *, SgNode *> identities;
  for (const auto &entry : original) {
    SgNode *replacement = nodeById(restored, entry.first);
    ROSE_ASSERT(entry.second->variantT() == replacement->variantT());
    ROSE_ASSERT(identities.emplace(entry.second, replacement).second);
  }
  // Inline external peers are absent from the indexed graph, but peers whose
  // semantic scope is replaced must retire in the same deletion transaction.
  std::vector<SgNode *> retired_roots{new_list};
  for (const auto &[id, node] : original) {
    auto *declaration = isSgDeclarationStatement(node);
    if (declaration == nullptr)
      continue;
    auto *replacement = nodeByIdAs<SgDeclarationStatement>(restored, id);
    const std::pair<SgDeclarationStatement *, SgDeclarationStatement *>
        peers[] = {{declaration->get_firstNondefiningDeclaration(),
                    replacement->get_firstNondefiningDeclaration()},
                   {declaration->get_definingDeclaration(),
                    replacement->get_definingDeclaration()}};
    for (const auto &[peer, restored_peer] : peers) {
      if (peer == nullptr || peer->get_ast_json_external_role() ==
                                 SgDeclarationStatement::e_ast_json_local)
        continue;
      ROSE_ASSERT(restored_peer != nullptr &&
                  peer->variantT() == restored_peer->variantT() &&
                  peer->get_ast_json_external_role() ==
                      restored_peer->get_ast_json_external_role());
      if (identities.count(peer) != 0) {
        ROSE_ASSERT(identities.at(peer) == restored_peer);
        continue;
      }
      if (identities.count(peer->get_scope()) == 0)
        continue;
      ROSE_ASSERT(peer != restored_peer);
      ROSE_ASSERT(peer->get_parent() == project ||
                  identities.count(peer->get_parent()) != 0);
      ROSE_ASSERT(restored_peer->get_scope() ==
                  identities.at(peer->get_scope()));
      identities.emplace(peer, restored_peer);
      retired_roots.push_back(peer);
    }
  }
  for (const auto &[id, node] : original) {
    auto *scope = isSgScopeStatement(node);
    if (scope == nullptr)
      continue;
    auto *replacement_scope = nodeByIdAs<SgScopeStatement>(restored, id);
    for (const auto &entry : *scope->get_symbol_table()->get_table()) {
      SgSymbol *symbol = entry.second;
      const auto basis = identities.find(symbol->get_symbol_basis());
      if (basis == identities.end())
        continue;
      SgSymbol *replacement = nullptr;
      const auto matches =
          replacement_scope->get_symbol_table()->get_table()->equal_range(
              entry.first);
      for (auto candidate = matches.first; candidate != matches.second;
           ++candidate) {
        if (candidate->second->get_symbol_basis() == basis->second &&
            candidate->second->variantT() == symbol->variantT()) {
          ROSE_ASSERT(replacement == nullptr ||
                      replacement == candidate->second);
          replacement = candidate->second;
        }
      }
      ROSE_ASSERT(replacement != nullptr);
      identities.emplace(symbol, replacement);
    }
  }
  for (size_t index = 0; index < files.size(); ++index) {
    ROSE_ASSERT(identities.at(files[index]) == replacements[index]);
    ROSE_ASSERT(replacements[index]->get_parent() == new_list);
  }
  ROSE_ASSERT(isSgSourceFile(nodeById(restored, ast.root_id)) != nullptr);
  class ProjectReferences : public SgSimpleProcessing,
                            public SimpleReferenceToPointerHandler {
    const std::unordered_map<SgNode *, SgNode *> &identities_;

  public:
    explicit ProjectReferences(
        const std::unordered_map<SgNode *, SgNode *> &identities)
        : identities_(identities) {}
    void operator()(SgNode *&edge, const SgName &, bool) override {
      if (const auto replacement = identities_.find(edge);
          replacement != identities_.end())
        edge = replacement->second;
    }
    void visit(SgNode *node) override {
      node->processDataMemberReferenceToPointers(this);
      if (auto *scope = isSgScopeStatement(node)) {
        auto *table = scope->get_symbol_table();
        std::vector<std::pair<SgSymbol *, const SgNode *>> symbols;
        // An alias can acquire its new basis when another table changes its
        // target.
        for (const auto &[basis, entries] : table->get_symbolBasisExactIndex())
          for (SgSymbol *symbol : entries)
            symbols.emplace_back(symbol, basis);
        for (const auto &entry : *table->get_table())
          entry.second->processDataMemberReferenceToPointers(this);
        for (const auto &[symbol, basis] : symbols)
          if (symbol->get_symbol_basis() != basis)
            table->reindex_symbol_basis(symbol, basis);
      }
    }
  } references(identities);
  references.traverse(project->get_globalScopeAcrossFiles(), preorder);
#ifdef ROSE_FLANG_FRONTEND
  FlangModuleInfo::replaceAstIdentities(project, identities);
#endif
  for (const auto &entry : original) {
    if (auto *source = isSgSourceFile(entry.second))
      OmpSupport::discardOpenMPProducerSemanticRecords(source);
  }
  // Keep the project's list and its iterators stable for the frontend's active
  // per-file traversal. All replacement nodes are complete before publication.
  for (size_t index = 0; index < files.size(); ++index) {
    SgFile *original_file = files[index];
    files[index] = replacements[index];
    files[index]->set_parent(old_list);
    replacements[index] = original_file;
    original_file->set_parent(new_list);
  }
  attachJsonNodeId(old_list, ast.project_files_id);
  SageInterface::deleteAST(retired_roots,
                           SageInterface::DeleteAstMode::kRequireIsolated);
}

void replaceFileInProject(SgSourceFile *old_file, SgSourceFile *new_file) {
  ROSE_ASSERT(old_file != nullptr);
  ROSE_ASSERT(new_file != nullptr);
  SgProject *project = owningProject(old_file);
  ROSE_ASSERT(project != nullptr);

  SgFileList *file_list_node = isSgFileList(old_file->get_parent());
  ROSE_ASSERT(file_list_node != nullptr);
  // The replacement owns its reconstructed module graph. Moving the old
  // files here would disconnect its serialized component and type identities.
  SgFileList *old_external = old_file->get_frontendExternalFileList();
  SgFileList *new_external = new_file->get_frontendExternalFileList();
  if ((old_external == nullptr) != (new_external == nullptr) ||
      (old_external != nullptr &&
       (old_external == new_external ||
        old_external->get_listOfFiles().size() !=
            new_external->get_listOfFiles().size()))) {
    throw std::runtime_error(
        "AST JSON replacement did not preserve its owned module graph");
  }
  SgFilePtrList &files = project->get_fileList();
  bool replaced = false;
  for (SgFile *&entry : files) {
    if (entry == old_file) {
      entry = new_file;
      replaced = true;
      break;
    }
  }
  ROSE_ASSERT(replaced);
  new_file->set_parent(file_list_node);
  if (new_file->get_globalScope() != nullptr) {
    new_file->get_globalScope()->set_parent(new_file);
  }
  OmpSupport::discardOpenMPProducerSemanticRecords(old_file);
  old_file->set_parent(nullptr);
}

std::vector<std::string> commandLineFromJson(const JsonValue &properties) {
  const JsonValue &array = properties.at("command_line");
  if (array.kind != JsonValue::Kind::Array || array.array.empty()) {
    throw std::runtime_error(
        "AST JSON source file command_line must be a nonempty array");
  }
  std::vector<std::string> result;
  for (const JsonValue &value : array.array) {
    result.push_back(value.asString());
    if (result.back().find('\0') != std::string::npos)
      throw std::runtime_error(
          "AST JSON source file command_line contains a NUL byte");
  }
  if (result.front().empty()) {
    throw std::runtime_error(
        "AST JSON source file command_line has no driver name");
  }
  return result;
}

void restoreFileIdMapFromMetadata(const JsonValue &metadata) {
  const JsonValue &entries = metadata.at("file_id_map");
  if (entries.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON metadata file_id_map is not an array");
  }

  std::map<int, std::string> id_to_name;
  std::map<std::string, int> name_to_id;
  int64_t expected_id = 0;
  for (const JsonValue &entry : entries.array) {
    if (entry.kind != JsonValue::Kind::Object) {
      throw std::runtime_error(
          "AST JSON metadata file_id_map entry is not an object");
    }
    const int64_t id64 = entry.at("id").asInt();
    if (id64 != expected_id) {
      std::ostringstream message;
      message << "AST JSON metadata file_id_map must be dense and sorted: "
              << "expected id " << expected_id << ", found " << id64;
      throw std::runtime_error(message.str());
    }
    if (id64 > std::numeric_limits<int>::max()) {
      throw std::runtime_error(
          "AST JSON metadata file_id_map id overflows int");
    }
    const std::string filename = entry.at("filename").asString();
    if (filename.empty()) {
      std::ostringstream message;
      message << "AST JSON metadata file_id_map has empty filename for id "
              << id64;
      throw std::runtime_error(message.str());
    }
    if (!name_to_id.emplace(filename, static_cast<int>(id64)).second) {
      throw std::runtime_error(
          "AST JSON metadata file_id_map has duplicate filename: " + filename);
    }
    id_to_name[static_cast<int>(id64)] = filename;
    ++expected_id;
  }
  Sg_File_Info::get_fileidtoname_map().swap(id_to_name);
  Sg_File_Info::get_nametofileid_map().swap(name_to_id);
}

} // namespace AstJson
} // namespace Rose
