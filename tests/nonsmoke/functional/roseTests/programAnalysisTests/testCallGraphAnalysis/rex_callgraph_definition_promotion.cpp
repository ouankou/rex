#include "newCallGraph.h"
#include "rose.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>

int main(int argc, char **argv) {
  auto *project = frontend(argc, argv);
  SgFunctionDeclaration *definition = nullptr;
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_definition() != nullptr) {
      definition = function;
      break;
    }
  }
  ASSERT_not_null(definition);
  const auto name = definition->get_mangled_name().getString();
  NewCallGraph::CallGraphFileStructure previous;
  previous.isNode = true;
  ROSE_ASSERT(name.size() < sizeof(previous.function_name));
  std::memcpy(previous.function_name, name.c_str(), name.size() + 1);
  std::strcpy(previous.file_name, "rex_previous_translation_unit.c");
  std::strcpy(previous.directoryToUseInCompilingSourceFile, "/rex/saved");
  std::strcpy(previous.commandlineToCompileSourceFile, "clang-22 -c saved.c");
  {
    std::ofstream output("binaryCallGraph", std::ios::binary);
    output.write(reinterpret_cast<const char *>(&previous), sizeof(previous));
    ROSE_ASSERT(output.good());
  }
  NewCallGraph::CallGraph graph;
  graph.traverse(project, preorder);
  std::set<std::string> identities;
  std::map<std::string, std::string> sources;
  for (SgFile *file : project->get_fileList()) {
    for (auto *node : NodeQuery::querySubTree(file, V_SgFunctionDeclaration)) {
      auto *function = isSgFunctionDeclaration(node);
      const std::string identity = function->get_mangled_name().getString();
      identities.insert(identity);
      if (function->get_definition() != nullptr || sources.count(identity) == 0)
        sources[identity] = file->get_sourceFileNameWithPath();
    }
  }
  ROSE_ASSERT(graph.nodeMap.size() == identities.size());
  for (const auto &identity : identities)
    ROSE_ASSERT(graph.nodeMap.count(identity) == 1);
  std::set<NewCallGraph::CallGraph::EdgeKey> edges;
  for (auto *node : NodeQuery::querySubTree(project, V_SgFunctionRefExp)) {
    auto *reference = isSgFunctionRefExp(node);
    auto *caller = SageInterface::getEnclosingFunctionDeclaration(reference);
    ASSERT_not_null(caller);
    edges.emplace(caller->get_mangled_name().getString(),
                  reference->get_symbol()
                      ->get_declaration()
                      ->get_mangled_name()
                      .getString());
  }
  ROSE_ASSERT(graph.edgeMap.size() == edges.size());
  for (const auto &edge : edges)
    ROSE_ASSERT(graph.edgeMap.count(edge) == 1);
  NewCallGraph::generateCallGraphFile(project, graph);
  NewCallGraph::generateCallGraphFile(project, graph);
  std::unique_ptr<std::vector<NewCallGraph::CallGraphFileStructure>> records(
      NewCallGraph::readCallGraphFile("binaryCallGraph"));
  ASSERT_not_null(records.get());
  ROSE_ASSERT(records->size() == identities.size() + edges.size());
  std::set<std::string> storedIdentities;
  std::set<NewCallGraph::CallGraph::EdgeKey> storedEdges;
  bool found = false;
  for (const auto &record : *records) {
    if (record.isNode) {
      ROSE_ASSERT(storedIdentities.insert(record.function_name).second);
      ROSE_ASSERT(record.file_name == sources.at(record.function_name));
    } else
      ROSE_ASSERT(
          storedEdges.emplace(record.function_name, record.function_call_name)
              .second);
    if (record.isNode && record.function_name == name) {
      ROSE_ASSERT(!found);
      found = true;
      ROSE_ASSERT(record.isDefiningDeclaration);
      ROSE_ASSERT(record.file_name ==
                  project->get_file(0).get_sourceFileNameWithPath());
      ROSE_ASSERT(std::strcmp(record.directoryToUseInCompilingSourceFile,
                              previous.directoryToUseInCompilingSourceFile) ==
                  0);
      ROSE_ASSERT(std::strcmp(record.commandlineToCompileSourceFile,
                              previous.commandlineToCompileSourceFile) == 0);
    }
  }
  ROSE_ASSERT(found);
  ROSE_ASSERT(storedIdentities == identities);
  ROSE_ASSERT(storedEdges == edges);

  ROSE_ASSERT(std::remove("binaryCallGraph") == 0);
  NewCallGraph::generateCallGraphFile(project, graph);
  records.reset(NewCallGraph::readCallGraphFile("binaryCallGraph"));
  ASSERT_not_null(records.get());
  ROSE_ASSERT(records->size() == identities.size() + edges.size());
  storedIdentities.clear();
  storedEdges.clear();
  for (const auto &record : *records) {
    if (record.isNode) {
      ROSE_ASSERT(storedIdentities.insert(record.function_name).second);
      ROSE_ASSERT(record.file_name == sources.at(record.function_name));
      auto *function =
          graph.nodeMap.at(record.function_name)->functionDeclaration;
      ROSE_ASSERT(record.isDefiningDeclaration ==
                  (function == function->get_definingDeclaration()));
    } else {
      ROSE_ASSERT(
          storedEdges.emplace(record.function_name, record.function_call_name)
              .second);
    }
  }
  ROSE_ASSERT(storedIdentities == identities);
  ROSE_ASSERT(storedEdges == edges);
}
