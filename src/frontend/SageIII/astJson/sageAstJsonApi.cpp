#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

namespace {

void normalizeSubtreeComparison(JsonValue &value) {
  if (value.kind == JsonValue::Kind::Array) {
    for (JsonValue &element : value.array) {
      normalizeSubtreeComparison(element);
    }
    return;
  }
  if (value.kind != JsonValue::Kind::Object) {
    return;
  }

  // These are translation-unit placement identities.  Repeated physical
  // header occurrences necessarily receive different values even when their
  // complete typed surface is equivalent.
  value.object.erase("source_sequence");
  value.object.erase("physical_file_occurrence_id");
  value.object.erase("translation_unit_source_order");
  for (auto &entry : value.object) {
    normalizeSubtreeComparison(entry.second);
  }
}

void retainLexicalSubtreeGraph(JsonValue &json) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON subtree signature root is not an object");
  }
  auto nodesField = json.object.find("nodes");
  if (nodesField == json.object.end() ||
      nodesField->second.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON subtree signature has no node array");
  }

  std::map<int64_t, JsonValue *> nodesById;
  for (JsonValue &node : nodesField->second.array) {
    nodesById.emplace(node.requiredInt("id"), &node);
  }
  const int64_t rootId = json.at("root_id").asInt();
  std::set<int64_t> reachable{rootId};
  std::vector<int64_t> worklist{rootId};
  while (!worklist.empty()) {
    const int64_t id = worklist.back();
    worklist.pop_back();
    const auto node = nodesById.find(id);
    if (node == nodesById.end()) {
      throw std::runtime_error(
          "AST JSON subtree signature references a missing node");
    }
    JsonValue &edges = node->second->object.at("edges");
    if (edges.kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON subtree signature node edges are not an array");
    }
    for (const JsonValue &edge : edges.array) {
      const std::string field = edge.requiredString("field");
      if (field == "parent" || field == "scope") {
        continue;
      }
      const int64_t target = edge.requiredInt("target");
      if (reachable.insert(target).second) {
        worklist.push_back(target);
      }
    }
  }

  std::vector<JsonValue> lexicalNodes;
  lexicalNodes.reserve(reachable.size());
  for (JsonValue &node : nodesField->second.array) {
    if (reachable.count(node.requiredInt("id")) == 0) {
      continue;
    }
    JsonValue &edges = node.object.at("edges");
    edges.array.erase(
        std::remove_if(edges.array.begin(), edges.array.end(),
                       [](const JsonValue &edge) {
                         const std::string field = edge.requiredString("field");
                         return field == "parent" || field == "scope";
                       }),
        edges.array.end());
    lexicalNodes.push_back(std::move(node));
  }
  nodesField->second.array = std::move(lexicalNodes);
  json.object.erase("node_count");
}

} // namespace

const char *checkpointName(Checkpoint checkpoint) {
  switch (checkpoint) {
  case Checkpoint::PreOmpConstruction:
    return "pre-omp-construction";
  case Checkpoint::PostOmpConstruction:
    return "post-omp-construction";
  case Checkpoint::PostOmpLowering:
    return "post-omp-lowering";
  }
  ROSE_ABORT();
}

bool checkpointEnabled(const SgSourceFile *file, Checkpoint checkpoint) {
  return hasCheckpointArgument(file, checkpoint);
}

Options optionsFromCommandLine(const SgSourceFile *file) {
  Options options;
  options.outputDirectory = defaultOutputDirectory(file);
  return options;
}

SgSourceFile *roundTripSourceFile(SgSourceFile *file, Checkpoint checkpoint) {
  return roundTripSourceFile(file, checkpoint, optionsFromCommandLine(file));
}

SgSourceFile *roundTripSourceFile(SgSourceFile *file, Checkpoint checkpoint,
                                  const Options &options) {
  ASSERT_not_null(file);

  const std::filesystem::path path = checkpointPath(file, checkpoint, options);
  SgProject *project = owningProject(file);
  ROSE_ASSERT(project != nullptr);
  SgFileList *project_files = isSgFileList(file->get_parent());
  ROSE_ASSERT(project_files != nullptr &&
              project_files->get_parent() == project);
  NodeMap original_nodes;
  // Source files can share imported declarations and canonical types. Snapshot
  // and replace their complete graph in one transaction so no surviving input
  // can retain a semantic edge into a detached checkpoint file.
  const std::string original_json =
      buildJson(file, checkpoint, file, project_files, &original_nodes);
  writeFile(path, original_json);

  const std::string parsed_json = readFile(path);
  AstFileRecord ast;
  try {
    ast = parseAstFileJson(parsed_json, checkpointName(checkpoint));
  } catch (const std::exception &e) {
    const std::filesystem::path read_error_path =
        path.parent_path() / (path.stem().string() + ".read-parse-error" +
                              path.extension().string());
    writeFile(read_error_path, parsed_json);
    throw std::runtime_error("AST JSON parse failed for checkpoint " +
                             std::string(checkpointName(checkpoint)) +
                             "; wrote parsed JSON to " +
                             read_error_path.string() + ": " + e.what());
  }

  NodeMap restored_nodes;
  SgNode::SourceStateConstruction construction;
  SgSourceFile *copy = reconstructSourceFile(ast, file, &restored_nodes);

  if (options.compareCanonicalRoundTrip) {
    const std::string copied_json =
        buildJson(copy, checkpoint, copy,
                  nodeByIdAs<SgFileList>(restored_nodes, ast.project_files_id));
    AstFileRecord copied_ast;
    try {
      copied_ast = parseAstFileJson(copied_json, checkpointName(checkpoint));
    } catch (const std::exception &e) {
      const std::filesystem::path copied_parse_error_path =
          path.parent_path() /
          (path.stem().string() + ".reconstructed-parse-error" +
           path.extension().string());
      writeFile(copied_parse_error_path, copied_json);
      throw std::runtime_error(
          "AST JSON reconstructed parse failed for checkpoint " +
          std::string(checkpointName(checkpoint)) +
          "; wrote reconstructed JSON to " + copied_parse_error_path.string() +
          ": " + e.what());
    }
    const std::string original_signature = semanticSignature(ast);
    const std::string copied_signature = semanticSignature(copied_ast);
    if (original_signature != copied_signature) {
      const std::filesystem::path copied_path =
          path.parent_path() /
          (path.stem().string() + ".reconstructed" + path.extension().string());
      const std::filesystem::path signature_path =
          path.parent_path() / (path.stem().string() + ".signature.txt");
      const std::filesystem::path copied_signature_path =
          path.parent_path() /
          (path.stem().string() + ".reconstructed.signature.txt");
      writeFile(copied_path, copied_json);
      writeFile(signature_path, original_signature);
      writeFile(copied_signature_path, copied_signature);
      throw std::runtime_error("AST JSON round-trip mismatch for checkpoint " +
                               std::string(checkpointName(checkpoint)) +
                               "; wrote reconstructed JSON to " +
                               copied_path.string());
    }
  }

  replaceProjectFiles(project, ast, original_nodes, restored_nodes);
  return copy;
}

void writeProjectJson(SgProject *project, const std::string &path,
                      const Options &options) {
  ROSE_ASSERT(project != nullptr);
  (void)options;
  writeFile(path, buildJson(project, Checkpoint::PreOmpConstruction, nullptr));
}

void writeSourceFileJson(SgSourceFile *file, Checkpoint checkpoint,
                         const std::string &path, const Options &options) {
  ROSE_ASSERT(file != nullptr);
  (void)options;
  writeFile(path, buildJson(file, checkpoint, file));
}

std::string canonicalSubtreeSignature(SgNode *root) {
  ROSE_ASSERT(root != nullptr);
  SgSourceFile *file = nullptr;
  for (SgNode *ancestor = root; ancestor != nullptr;
       ancestor = ancestor->get_parent()) {
    file = isSgSourceFile(ancestor);
    if (file != nullptr) {
      break;
    }
  }
  if (file == nullptr) {
    throw std::runtime_error(
        "AST JSON subtree signature has no owning source file");
  }
  const Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  JsonValue json = parseJson(buildJson(root, checkpoint, file));
  retainLexicalSubtreeGraph(json);
  normalizeSubtreeComparison(json);
  std::ostringstream signature;
  appendCanonicalJson(signature, json);
  return signature.str();
}

} // namespace AstJson
} // namespace Rose
