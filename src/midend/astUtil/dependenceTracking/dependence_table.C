#include "AstInterface_ROSE.h"

#include "AstUtilInterface.h"

#include "OperatorAnnotation.h"

#include "OperatorDescriptors.h"

#include "dependence_table.h"

#include <list>

namespace {
std::string wrap_string(const std::string &s) {
  std::string new_string;
  unsigned wrap = 10;
  unsigned maxwrap = 20;
  unsigned index = 0;
  for (auto c : s) {
    ++index;
    if (index > maxwrap || (index > wrap && c == ':')) {
      new_string.push_back('\\');
      new_string.push_back('n');
      index = 0;
    }
    new_string.push_back(c);
  }
  return new_string;
}

bool requiresQuotedDependenceToken(const std::string &token) {
  if (token.empty()) {
    return true;
  }
  for (std::string::size_type i = 0; i < token.size(); ++i) {
    switch (token[i]) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\\':
    case '"':
    case '=':
    case '[':
    case ']':
    case '{':
    case '}':
    case ';':
      return true;
    case ':':
      if (i + 1 < token.size() && token[i + 1] == ':') {
        ++i;
      } else {
        return true;
      }
      break;
    default:
      break;
    }
  }
  return false;
}

void writeDependenceToken(std::ostream &output, const std::string &token) {
  // The table grammar uses whitespace and punctuation as structural tokens.
  // Quote opaque payloads whenever their spelling overlaps that grammar.
  if (!requiresQuotedDependenceToken(token)) {
    output << token;
    return;
  }

  output << '"';
  for (char c : token) {
    switch (c) {
    case '\\':
      output << "\\\\";
      break;
    case '"':
      output << "\\\"";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << c;
      break;
    }
  }
  output << '"';
}

std::pair<std::string, std::string>
dependenceLabel(AstUtilInterface::OperatorSideEffect relation) {
  using AstUtilInterface::OperatorSideEffect;

  switch (relation) {
  case OperatorSideEffect::ModifyUnknown:
    return {"modify", "unknown"};
  case OperatorSideEffect::ReadUnknown:
    return {"read", "unknown"};
  case OperatorSideEffect::CallUnknown:
    return {"call", "unknown"};
  case OperatorSideEffect::Init:
    return {"read", "init"};
  default:
    return {AstUtilInterface::OperatorSideEffectName(relation), ""};
  }
}
} // namespace

namespace AstUtilInterface {

std::string CollectDependences::local_read_string(std::istream &input_file) {
  std::string next_string;
  char c;
  while ((input_file >> c).good()) {
    switch (c) {
    case '\\':
      next_string.push_back(c);
      if (!(input_file >> c).good()) {
        return next_string;
      }
      break;
    case '"': {
      if (!next_string.empty()) {
        Log.fatal("Quoted dependence token must start at a token boundary");
      }
      // Quoted payloads are decoded with unformatted reads so whitespace and
      // grammar punctuation remain part of the exact dependence signature.
      bool escaped = false;
      bool closed = false;
      while (input_file.get(c)) {
        if (escaped) {
          switch (c) {
          case '\\':
          case '"':
            next_string.push_back(c);
            break;
          case 'n':
            next_string.push_back('\n');
            break;
          case 'r':
            next_string.push_back('\r');
            break;
          case 't':
            next_string.push_back('\t');
            break;
          default:
            Log.fatal(std::string("Unsupported dependence token escape \\") +
                      c);
          }
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == '"') {
          closed = true;
          break;
        } else {
          next_string.push_back(c);
        }
      }
      if (!closed) {
        Log.fatal("Unterminated quoted dependence token");
      }
      Log.push("reading quoted string " + next_string);
      return next_string;
    }
    case ' ':
    case '\n':
    case '\r':
      if (!next_string.empty()) {
        // This starts a new token. Return the current one.
        Log.push("Seeing separator. Finished reading token " + next_string);
        return next_string;
      }
      // Skip empty space.
      break;
    case ':': {
      // Make sure read double "::" as part of a name.
      char c1 = input_file.peek();
      if (c1 == ':') {
        input_file >> c1;
        next_string += "::";
        Log.push("Seeing `::'. continue reading token " + next_string);
        break;
      }
      if (!next_string.empty()) {
        // This starts a new token. Return the current one.
        input_file.putback(c);
        Log.push("Seeing separator. Finished reading token " + next_string);
      } else {
        // Found a token. Return it.
        next_string.push_back(c);
        Log.push("reading separator token " + next_string);
      }
      return next_string;
    }
    case '=':
    case '[':
    case ']':
    case '{':
    case '}':
    case ';':
      if (!next_string.empty()) {
        // This starts a new token. Return the current one.
        input_file.putback(c);
        Log.push("Seeing separator. Finished reading token " + next_string);
      } else {
        // Found a token. Return it.
        next_string.push_back(c);
        Log.push("reading separator token " + next_string);
      }
      return next_string;
    default:
      next_string.push_back(c);
      break;
    }
  }
  Log.push("Return Next token  " + next_string);
  return next_string;
}

void CollectDependences::CollectFromFile(std::istream &input_file) {
  Log.push("Constructing DependenceTable");
  while (input_file.good()) {
    // Each line starts with the name of a component in the software.
    std::string dest = local_read_string(input_file);
    if (dest.empty()) {
      break;
    }
    if (dest == "}" || dest == ";") {
      continue;
    }
    Log.push("Destination name: " + dest);
    std::string next_string;
    std::string source;
    std::string dep_type;
    std::vector<std::string> dep_types;
    std::string attr;
    // Read and process all the components that `dest' depends on immediately.
    while (!(next_string = local_read_string(input_file)).empty()) {
      if (next_string == ";") {
        if (!source.empty()) {
          save_dependence(DependenceEntry(dest, source, dep_type, attr));
          Log.push("Saving " + source + "->" + dest + "[" + dep_type + "]");
        }
        Log.push("Done reading line\n");
        break;
      } else if (next_string == "->") {
        source = dest;
        Log.push("Setting source = " + source);
        dest = local_read_string(input_file);
        Log.push("Setting dest = " + dest);
      } else if (next_string == ":") {
        source = local_read_string(input_file);
        dep_type.clear();
        dep_types.clear();
        while (source == "[") {
          std::string current_dep_type;
          while ((next_string = local_read_string(input_file)) != "]") {
            if (next_string.empty()) {
              Log.fatal("Expecting \"]\" but get " + next_string);
            }
            current_dep_type += next_string;
          }
          dep_types.push_back(current_dep_type);
          source = local_read_string(input_file);
        }
        if (!dep_types.empty()) {
          dep_type = dep_types.front();
        }
        if (source == ";") {
          Log.push("Creating node with attributes for " + dest);
          save_node_attributes(dest, dep_types);
          next_string = ";";
          source.clear();
          break;
        }
        Log.push("Successfully setting source = " + source);
      } else if (next_string == "=") {
        attr.clear();
        while (!(next_string = local_read_string(input_file)).empty()) {
          if (next_string == ";" || next_string == "\n") {
            break;
          }
          attr += next_string;
        }
        if (next_string == ";") {
          input_file.putback(';');
        }
        Log.push("Setting attr:  " + attr);
      } else if (next_string == "{") {
        Log.push("Skipping graph configuration: " + dest + " " + next_string);
        break;
      } else {
        Log.fatal("Unexpected token " + next_string);
      }
    }
    if (next_string != ";" && next_string != "}" && next_string != "{") {
      Log.fatal("Expecting `;' or `}' but getting " + next_string);
    } else if (input_file.peek() == EOF) {
      break;
    }
  }
  Log.push("Done Constructing DependenceTable");
}

void CollectTransitiveDependences::save_dependence(const DependenceEntry &e) {
  // Here we revert the dependence direction for downstream/backward dependences
  if (dependence_map_.find(current_start(e)) == dependence_map_.end()) {
    saved_sources_.push_back(current_start(e));
  }
  if (already_saved_.find(e) == already_saved_.end()) {
    already_saved_.insert(e);
    dependence_map_[current_start(e)].push_back(e);
    if (e.attr_entry() != "") {
      dependence_map_[e.attr_entry()].push_back(e);
    }
    if (e.type_entry() != "") {
      dependence_map_[e.type_entry()].push_back(e);
    }
  }
}

void CollectTransitiveDependences::Compute(
    const std::string &input, std::set<std::string> &result,
    const std::function<bool(const DependenceEntry &)> *what_to_do) {
  DebugLog Log("-debug-dep-table");
  if (result.find(input) != result.end()) {
    // Transitively collect more results only if it hasn't yet been done.
    Log.push("Skip collecting transitive dependence for " + input);
    return;
  }
  Log.push("Collect transitive dependence for " + input);
  result.insert(input);
  const auto &dependences = dependence_map_[input];
  // Terminates if dependences are empty, with the loop below skipped.
  for (auto dependence : dependences) {
    if (what_to_do == 0 || (*what_to_do)(dependence)) {
      save_dependence(dependence);
      if (current_start(dependence) == input) {
        Compute(next_start(dependence), result, what_to_do);
      }
    }
  }
}

void CollectTransitiveDependences ::Compute(
    const std::vector<std::string> &input, std::set<std::string> *result,
    const std::function<bool(const DependenceEntry &)> *what_to_do) {
  DebugLog Log("-debug-dep-table");
  Log.push("Output results of transitive dependence analysis");
  auto from = input;
  for (std::string e : from) {
    std::set<std::string> destinations;
    Compute(e, destinations, what_to_do);
    if (result != 0) {
      for (auto d : destinations) {
        result->insert(d);
      }
    }
  }
}

void DependenceTable ::OutputDependences(std::ostream &output) {
  Log.push("Output results of dependence analysis");
  for (auto op : saved_dependences_sig_) {
    for (auto e : saved_dependences_relation_[op]) {
      output << e << std::endl;
    }
  }
}

void DependenceTable ::OutputDataDependences(std::ostream &output) {
  Log.push("Output data dependences only.");
  for (auto op : saved_dependences_sig_) {
    for (auto e : saved_dependences_relation_[op]) {
      e.output_data_dependence(output);
    }
  }
}

std::ostream &operator<<(std::ostream &output, const DependenceEntry &e) {
  writeDependenceToken(output, e.first_entry());
  output << " : ";
  if (e.type_entry() != "") {
    output << "[ ";
    writeDependenceToken(output, e.type_entry());
    output << " ] ";
  }
  writeDependenceToken(output, e.second_entry());
  if (e.attr_entry() != "") {
    output << " = ";
    writeDependenceToken(output, e.attr_entry());
    output << " ;";
  } else {
    output << " ;";
  }
  return output;
}

bool DependenceEntry::output_data_dependence(std::ostream &output) const {
  if (attr_entry().empty()) {
    return false;
  }
  writeDependenceToken(output, second_entry());
  output << " : [ ";
  writeDependenceToken(output, type_entry());
  output << " ] ";
  writeDependenceToken(output, attr_entry());
  output << " ;\n";
  return true;
}

/************************************/
/* Class for supporting clustering of dependences in GUI */
/************************************/
class ClusterDependences {
  std::map<std::string, std::string> cluster_map;
  std::map<std::string, std::set<std::string>> clusters;
  std::map<std::string, std::set<DependenceEntry>> out_edge_map;
  std::map<std::string, std::set<DependenceEntry>> in_edge_map;
  std::set<std::string> functions;
  int cluster_index_ = 0;
  const DependenceTable &deptable_;

  bool setupNamespace(const std::string &s) {
    auto namespace_pos = s.rfind("::");
    if (namespace_pos > 0 && namespace_pos < s.size()) {
      std::string cluster_name = s.substr(0, namespace_pos);
      clusters[cluster_name].insert(s);
      return true;
    }
    return false;
  }

public:
  explicit ClusterDependences(const DependenceTable &deptable)
      : deptable_(deptable) {}

  void setupNode(const std::string &s) {
    assert(!s.empty());
    if (cluster_map.find(s) != cluster_map.end()) {
      return;
    }
    if (setupNamespace(s)) {
      return;
    }

    auto cluster_string = std::to_string(cluster_index_);
    cluster_index_++;
    clusters[cluster_string].insert(s);
    cluster_map[s] = cluster_string;
  }

  void setupEdge(const DependenceEntry &e) {
    assert(!e.first_entry().empty());
    assert(!e.second_entry().empty());
    functions.insert(e.first_entry());
    if (e.type_entry() == "call") {
      functions.insert(e.second_entry());
    }
    out_edge_map[e.first_entry()].insert(e);
    in_edge_map[e.second_entry()].insert(e);
  }

  void setupClusters(int threshold = 1) {
    std::list<std::string> work;
    for (const auto &m : cluster_map) {
      assert(!m.second.empty());
      assert(!m.first.empty());
      auto node = m.first;
      if (clusters[m.second].size() > 1) {
        continue;
      }
      work.push_back(node);
    }
    for (const auto &node : work) {
      std::map<std::string, int> connectivity;
      for (const auto &e : in_edge_map[node]) {
        auto c = cluster_map[e.first_entry()];
        if (c.empty()) {
          continue;
        }
        if (connectivity.find(c) == connectivity.end()) {
          connectivity[c] = 0;
        } else {
          connectivity[c]++;
        }
      }
      for (const auto &e : out_edge_map[node]) {
        auto c = cluster_map[e.second_entry()];
        if (c.empty()) {
          continue;
        }
        if (connectivity.find(c) == connectivity.end()) {
          connectivity[c] = 0;
        } else {
          connectivity[c]++;
        }
      }
      std::string cluster;
      if (connectivity.size() == 1) {
        cluster = connectivity.begin()->first;
        cluster_map[node] = cluster;
        clusters[cluster].insert(node);
      } else {
        for (const auto &con : connectivity) {
          if (con.second >= threshold) {
            if (cluster.empty()) {
              cluster = con.first;
              assert(!cluster.empty());
              cluster_map[node] = cluster;
              clusters[cluster].insert(node);
            } else {
              for (const auto &n : clusters[con.first]) {
                cluster_map[n] = cluster;
                clusters[cluster].insert(n);
              }
            }
          }
        }
      }
    }
  }

  std::string edge_to_string(const std::string &s) const {
    std::string color = "black";
    if (s == "modify") {
      color = "red";
    } else if (s == "read") {
      color = "green";
    }
    if (!s.empty()) {
      return "[ label=\"" + s + "\", color=" + color + "]";
    }
    return "[ color=" + color + "]";
  }

  void outputCluster(const std::string &cluster_name, std::ostream &output) {
    bool do_cluster = clusters[cluster_name].size() > 1;
    if (do_cluster) {
      output << "subgraph \"cluster_" << cluster_name << "\" {\n";
      output << "  style=filled;\n";
      output << "  color=lightgrey;\n";
      output << "  label=\"" << cluster_name << "\";\n";
    }
    for (const auto &cluster_member : clusters[cluster_name]) {
      if (functions.find(cluster_member) != functions.end()) {
        output << "\"" << wrap_string(cluster_member) << "\" [shape=box]";
      } else {
        output << "\"" << wrap_string(cluster_member) << "\" [shape=diamond]";
      }
      for (const auto &attr : deptable_.get_nodeInfo(cluster_member)) {
        output << " [ " << attr << " ]";
      }
      output << " ; \n";
    }
    if (do_cluster) {
      output << "}\n";
    }
  }

  void output(std::ostream &output) {
    output << "digraph {\n";
    for (const auto &cluster_entry : clusters) {
      outputCluster(cluster_entry.first, output);
    }
    for (const auto &m : out_edge_map) {
      for (const auto &e : m.second) {
        output << "\"" << wrap_string(e.first_entry()) << "\" -> \""
               << wrap_string(e.second_entry()) << "\""
               << edge_to_string(e.type_entry()) << " ;\n";
      }
    }
    output << "}";
  }
};
/************************************/

void DependenceTable ::OutputDependencesInGUI(std::ostream &output) {
  Log.push("Output dependence analysis GUI");

  ClusterDependences clusters(*this);
  CollectNodes(
      [&clusters](const std::string &node) { clusters.setupNode(node); });
  for (auto op : saved_dependences_sig_) {
    for (auto e : saved_dependences_relation_[op]) {
      clusters.setupNode(e.first_entry());
      clusters.setupNode(e.second_entry());
      clusters.setupEdge(e);
    }
  }
  clusters.setupClusters();
  clusters.output(output);
}

void DependenceTable::save_node_attributes(
    const std::string &sig, const std::vector<std::string> &attributes) {
  InsertNode(sig);
  get_nodeInfo(sig) = attributes;
}

void DependenceTable::save_dependence(const DependenceEntry &e) {
  // Save inside the dependence table (base class).
  DebugLog DebugSaveDep("-debugdep");
  DebugSaveDep([&e]() { return "processing " + e.to_string(); });

  if (update_annotations_) {
    // Save into annotation  if necessary.
    if (e.type_entry() == "parameter") {
      OperatorSideEffectAnnotation *funcAnnot =
          OperatorSideEffectAnnotation::get_inst();
      OperatorSideEffectDescriptor *desc1 =
          funcAnnot->get_modify_descriptor(e.first_entry(), true);
      assert(desc1 != 0);
      desc1->get_param_decl().add_param(/*param type*/ e.attr_entry(),
                                        /* param name*/ e.second_entry());
      OperatorSideEffectDescriptor *desc2 =
          funcAnnot->get_read_descriptor(e.first_entry(), true);
      assert(desc2 != 0);
      desc2->get_param_decl().add_param(/*param type*/ e.attr_entry(),
                                        /* param name*/ e.second_entry());
      DebugSaveDep([&e]() { return "Saving parameter " + e.second_entry(); });
    } else if (e.type_entry() == "modify") {
      OperatorSideEffectAnnotation *funcAnnot =
          OperatorSideEffectAnnotation::get_inst();
      OperatorSideEffectDescriptor *desc =
          funcAnnot->get_modify_descriptor(e.first_entry(), true);
      assert(desc != 0);
      DebugSaveDep([&e]() { return "processing " + e.second_entry(); });
      SymbolicVal var = SymbolicValGenerator::GetSymbolicVal(e.second_entry());
      desc->push_back(var);
      DebugSaveDep([&var]() { return "Saving modify " + var.toString(); });
    } else if (e.type_entry() == "read") {
      OperatorSideEffectAnnotation *funcAnnot =
          OperatorSideEffectAnnotation::get_inst();
      OperatorSideEffectDescriptor *desc =
          funcAnnot->get_read_descriptor(e.first_entry(), true);
      assert(desc != 0);
      SymbolicVal var = SymbolicValGenerator::GetSymbolicVal(e.second_entry());
      desc->push_back(var);
      DebugSaveDep([&var]() { return "Saving read " + var.toString(); });
    }
  }
  DependenceTable::SaveDependence(DependenceEntry(e));
}

void DependenceTable::ClearOperatorSideEffect(SgNode *op) {
  auto sig = GetVariableSignature(op);
  ClearDependence(sig);
}

bool DependenceTable::SaveOperatorSideEffect(
    SgNode *op, const AstNodePtr &varref,
    AstUtilInterface::OperatorSideEffect relation, SgNode *details) {
  std::string attr;
  if (details != 0) {
    Log.push("Skipping Side effect details: " +
             AstInterface::AstToString(AstNodePtrImpl(details)));
    // QY: Do not save side effect details in annotation as we currently don't
    // use it.
    // attr = AstUtilInterface::GetVariableSignature(details);
  }
  const std::string op_sig = AstUtilInterface::GetVariableSignature(op);
  const auto label = dependenceLabel(relation);
  DependenceEntry e(op_sig, AstUtilInterface::GetVariableSignature(varref),
                    label.first, label.second.empty() ? attr : label.second);
  Log.push("saving dependence: " + e.to_string());
  SaveDependence(e);
  return true;
}

}; // namespace AstUtilInterface
