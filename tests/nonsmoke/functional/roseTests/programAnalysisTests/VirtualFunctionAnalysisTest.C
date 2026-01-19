#include "sage3basic.h"

#include "AstDOTGeneration.h"

#include "CallGraph.h"

#include "CommandOptions.h"

#include "VirtualFunctionAnalysis.h"

#include <iostream>

#include <filesystem>

#include <string>

#include "rose_path_resolver.h"

using namespace std;

namespace {
std::filesystem::path resolveDotOutputDir(const RosePathRoots &roots) {
  std::filesystem::path output_dir;
  if (!roots.build_root.empty()) {
    output_dir =
        std::filesystem::path(roots.build_root) / "tests" / "dot-output";
  } else {
    std::error_code ec;
    output_dir = std::filesystem::temp_directory_path(ec);
  }
  std::error_code ec;
  std::filesystem::create_directories(output_dir, ec);
  if (ec) {
    output_dir = std::filesystem::current_path(ec);
  }
  return output_dir;
}
} // namespace

void PrintUsage(char *name) {
  cerr << name << " <options> " << "<program name>" << "\n";
  cerr << "-dot :generate DOT output \n";
}

int main(int argc, char *argv[]) {

  if (argc <= 1) {
    PrintUsage(argv[0]);
    return -1;
  }
  SgProject *project = frontend(argc, argv);

  printf("Build the call graph \n");

  CallGraphBuilder builder(project);
  builder.buildCallGraph();

  printf("DONE: Build the call graph \n");

  RosePathRoots roots = resolveRosePaths(argv[0]);
  std::filesystem::path dot_output_dir = resolveDotOutputDir(roots);

  // Generate call graph in dot format
  AstDOTGeneration dotgen;
  dotgen.writeIncidenceGraphToDOTFile(
      builder.getGraph(),
      (dot_output_dir / "full_call_graph.dot").string().c_str());

  printf("Calling SageInterface::changeAllBodiesToBlocks() \n");

  SageInterface::changeAllBodiesToBlocks(project);

  printf("DONE: Calling SageInterface::changeAllBodiesToBlocks() \n");

  SgFunctionDeclaration *mainDecl = SageInterface::findMain(project);
  if (mainDecl == NULL) {
    std::cerr
        << "Can't execute Virtual Function Analysis without main function\n";
    return 0;
  }

  printf("Calling VirtualFunctionAnalysis() \n");

  VirtualFunctionAnalysis *anal = new VirtualFunctionAnalysis(project);

  printf("Calling VirtualFunctionAnalysis(): run \n");

  anal->run();

  printf("Calling VirtualFunctionAnalysis(): pruneCallGraph \n");

  anal->pruneCallGraph(builder);

  printf("Calling VirtualFunctionAnalysis(): writeIncidenceGraphToDOTFile \n");

  AstDOTGeneration dotgen2;
  dotgen2.writeIncidenceGraphToDOTFile(
      builder.getGraph(), (dot_output_dir / "call_graph.dot").string().c_str());

  printf("Calling VirtualFunctionAnalysis(): delete \n");

  delete anal;

  printf("DONE: Calling VirtualFunctionAnalysis(): delete \n");

  return 0;
}
