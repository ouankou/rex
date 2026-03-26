// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.

// DQ (8/26/2016): Added support for the call graph generation for template
// functions in classes that are moved by legacy frontend outside of the class
// and for which we previously only captured the function prototype and for
// which the new fix adds the function definitions to the AST. Note that this
// cased the results to change and for many of the save correct answer to be
// updated to reflect the improved AST.  As a result of studing the results, the
// current call graph computation has a number of observed limitations:
//    1) SgConstructorInitializers are not included as function calls
//    2) implicit base class constructor calls are not handled.
// Note that these were acceptable limitations for the time when the original
// call graph support was written.  However, we will be replacing this call
// graph with a future on based on a subset of the interprocedural control flow
// graph.

#include "rose_config.h"
#undef CONFIG_ROSE /* prevent error about including both private and public    \
                      headers; must be between rose_config.h and rose.h */

#include "CallGraph.h"

#include "rose.h"

#include "rose_path_resolver.h"

#include <filesystem>

#include <fstream>

#include <iostream>

#include <map>

#include <stdlib.h>

#include <string>

#include <vector>

using namespace std;

std::string stripGlobalModifer(std::string str) {
  if (str.substr(0, 2) == "::")
    str = str.substr(2);

  return str;
};

static void normalizeTemplateNamesForDump(SgFunctionDeclaration *funcDecl) {
  if (funcDecl == NULL) {
    return;
  }

  if (SgTemplateInstantiationFunctionDecl *inst_func =
          isSgTemplateInstantiationFunctionDecl(funcDecl)) {
    inst_func->resetTemplateName();
  } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                 isSgTemplateInstantiationMemberFunctionDecl(funcDecl)) {
    inst_member->resetTemplateName();
    if (SgTemplateInstantiationDecl *inst_class = isSgTemplateInstantiationDecl(
            inst_member->get_associatedClassDeclaration())) {
      inst_class->resetTemplateName();
    }
  }
}

static std::string templateArgumentNameForDump(SgTemplateArgument *arg);

static std::string templateArgumentListForDump(
    const SgTemplateArgumentPtrList &templateArguments) {
  bool first_argument = true;
  std::string result;

  for (SgTemplateArgument *arg : templateArguments) {
    if (arg == NULL ||
        arg->get_argumentType() ==
            SgTemplateArgument::start_of_pack_expansion_argument) {
      continue;
    }

    if (first_argument) {
      result += " < ";
      first_argument = false;
    } else {
      result += " , ";
    }

    result += templateArgumentNameForDump(arg);
  }

  if (!first_argument) {
    result += " > ";
  }

  return result;
}

static std::string qualifiedClassNameForDump(SgClassDeclaration *classDecl) {
  ROSE_ASSERT(classDecl != NULL);

  if (SgClassDeclaration *first =
          isSgClassDeclaration(classDecl->get_firstNondefiningDeclaration())) {
    classDecl = first;
  }

  if (SgTemplateInstantiationDecl *inst_decl =
          isSgTemplateInstantiationDecl(classDecl)) {
    SgName base_name = inst_decl->get_templateName();
    if (base_name.is_null() && inst_decl->get_templateDeclaration() != NULL) {
      base_name = inst_decl->get_templateDeclaration()->get_name();
    }
    if (base_name.is_null()) {
      base_name = inst_decl->get_name();
    }

    std::string scope_name = stripGlobalModifer(
        inst_decl->get_scope()->get_qualified_name().getString());
    std::string class_name =
        base_name.getString() +
        templateArgumentListForDump(inst_decl->get_templateArguments());
    if (!scope_name.empty()) {
      return scope_name + "::" + class_name;
    }
    return class_name;
  }

  return classDecl->get_qualified_name().getString();
}

static std::string templateArgumentNameForDump(SgTemplateArgument *arg) {
  ROSE_ASSERT(arg != NULL);

  switch (arg->get_argumentType()) {
  case SgTemplateArgument::type_argument: {
    SgType *type = arg->get_type();
    if (SgNamedType *named_type = isSgNamedType(type)) {
      if (SgClassDeclaration *class_decl =
              isSgClassDeclaration(named_type->get_declaration())) {
        return stripGlobalModifer(qualifiedClassNameForDump(class_decl));
      }
      if (SgTypedefDeclaration *typedef_decl =
              isSgTypedefDeclaration(named_type->get_declaration())) {
        return stripGlobalModifer(
            typedef_decl->get_qualified_name().getString());
      }
      if (SgEnumDeclaration *enum_decl =
              isSgEnumDeclaration(named_type->get_declaration())) {
        return stripGlobalModifer(enum_decl->get_qualified_name().getString());
      }
    }
    return stripGlobalModifer(arg->unparseToString());
  }

  case SgTemplateArgument::nontype_argument:
  case SgTemplateArgument::template_template_argument:
  case SgTemplateArgument::argument_undefined:
  case SgTemplateArgument::start_of_pack_expansion_argument:
  default:
    return stripGlobalModifer(arg->unparseToString());
  }
}

static std::string qualifiedDumpName(SgFunctionDeclaration *funcDecl) {
  ROSE_ASSERT(funcDecl != NULL);
  normalizeTemplateNamesForDump(funcDecl);

  if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
          isSgTemplateInstantiationMemberFunctionDecl(funcDecl)) {
    SgClassDeclaration *assoc_class =
        isSgClassDeclaration(inst_member->get_associatedClassDeclaration());
    ROSE_ASSERT(assoc_class != NULL);
    return stripGlobalModifer(qualifiedClassNameForDump(assoc_class) +
                              "::" + inst_member->get_name().getString());
  }

  if (SgTemplateInstantiationFunctionDecl *inst_func =
          isSgTemplateInstantiationFunctionDecl(funcDecl)) {
    SgName base_name = inst_func->get_templateName();
    if (base_name.is_null()) {
      base_name = inst_func->get_name();
    }
    std::string function_name =
        base_name.getString() +
        templateArgumentListForDump(inst_func->get_templateArguments());
    return stripGlobalModifer(
        SgName::assembleQualifiedName(
            inst_func->get_scope()->get_qualified_name(), function_name)
            .getString());
  }

  return stripGlobalModifer(funcDecl->get_qualified_name().getString());
}

bool nodeCompareGraph(const SgGraphNode *a, const SgGraphNode *b) {
  SgFunctionDeclaration *funcDecl1 = isSgFunctionDeclaration(a->get_SgNode());
  ROSE_ASSERT(funcDecl1 != NULL);

  SgFunctionDeclaration *funcDecl2 = isSgFunctionDeclaration(b->get_SgNode());
  ROSE_ASSERT(funcDecl2 != NULL);

  return qualifiedDumpName(funcDecl1) < qualifiedDumpName(funcDecl2);
}

bool nodeCompareGraphPair(const std::pair<SgGraphNode *, int> &a,
                          const std::pair<SgGraphNode *, int> &b) {
  ROSE_ASSERT((a.first != b.first) || (a.second == b.second));
  return nodeCompareGraph(a.first, b.first);
}

void sortedCallGraphDump(string fileName, SgIncidenceDirectedGraph *cg) {
  // Opening output file
  ofstream file;
  file.open(fileName.c_str());

  // Get all nodes of the current CallGraph
  list<pair<SgGraphNode *, int>> cgNodes;

  rose_graph_integer_node_hash_map &nodes = cg->get_node_index_to_node_map();

  for (rose_graph_integer_node_hash_map::iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    cgNodes.push_back(pair<SgGraphNode *, int>(it->second, it->first));
  }

  cgNodes.sort(nodeCompareGraphPair);
  cgNodes.unique();

  auto formatDumpName = [](SgFunctionDeclaration *func_decl) -> std::string {
    return qualifiedDumpName(func_decl);
  };

  for (list<pair<SgGraphNode *, int>>::iterator it = cgNodes.begin();
       it != cgNodes.end(); it++) {
    // get list over the end-points for which this node points to
    list<SgGraphNode *> calledNodes;
    rose_graph_integer_edge_hash_multimap &outEdges =
        cg->get_node_index_to_edge_multimap_edgesOut();
    for (rose_graph_integer_edge_hash_multimap::const_iterator outEdgeIt =
             outEdges.begin();
         outEdgeIt != outEdges.end(); ++outEdgeIt) {

      if (outEdgeIt->first == it->second) {
        SgDirectedGraphEdge *graphEdge =
            isSgDirectedGraphEdge(outEdgeIt->second);
        ROSE_ASSERT(graphEdge != NULL);
        calledNodes.push_back(graphEdge->get_to());
      }
    }

    calledNodes.sort(nodeCompareGraph);
    calledNodes.unique();

    // Output the unique graph
    SgFunctionDeclaration *cur_function =
        isSgFunctionDeclaration((it->first)->get_SgNode());

    if (SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL)
      std::cout << "Node " << cur_function << " has " << calledNodes.size()
                << " calls to it." << std::endl;

    ROSE_ASSERT(cur_function != NULL);
    std::string output_line = formatDumpName(cur_function) + " ->";

    if (!calledNodes.empty()) {
      for (list<SgGraphNode *>::iterator j = calledNodes.begin();
           j != calledNodes.end(); j++) {
        SgFunctionDeclaration *j_function =
            isSgFunctionDeclaration((*j)->get_SgNode());

        output_line += " " + formatDumpName(j_function);
      }
    }

    while (!output_line.empty() && output_line[output_line.size() - 1] == ' ') {
      output_line.resize(output_line.size() - 1);
    }

    file << output_line << endl;
  }

  file.close();
};

struct OnlyCurrentDirectory {
  static size_t nselected;

#define DEBUG_SELECTOR 0

  bool operator()(SgFunctionDeclaration *node) const {
    ROSE_ASSERT(node != NULL);

    // build tree; use the resolved build root so symbolic links are normalized.
    static const RosePathRoots roots = resolveRosePaths(nullptr);
    std::string stringToFilter;
    if (!roots.build_root.empty()) {
      stringToFilter =
          (std::filesystem::path(roots.build_root) / "tests").string();
    }
    // Liao 6/20/2011, we have to use the same source path without symbolic
    // links to have the right match
    std::string srcDir =
        ROSE_SOURCE_TREE_PATH + std::string("/tests"); // source tree
    // std::string srcDir = ROSE_SOURCE_TREE + std::string("/tests");
    // //source tree
    //
    //  Hard code this for initial testing on target exercise.
    std::string secondaryTestSrcDir =
        ROSE_SOURCE_TREE + std::string("/developersScratchSpace");

    // Not all SgFunctionDeclaration's come from a file.  If ROSE only ever
    // encountered a defining declaration in the input then it will create a
    // compiler-generated non-defining declaration that is not associated with
    // any file.  The call graph layer always uses the first non-defining
    // declaration, thus we won't always see a valid file, in which case we need
    // to get the file name from the defining declaration.
    string sourceFilename = node->get_file_info()->get_filename();
    if (sourceFilename.empty() || 0 == sourceFilename.compare("NULL_FILE")) {
      SgFunctionDeclaration *defdecl =
          isSgFunctionDeclaration(node->get_definingDeclaration());
      if (defdecl)
        sourceFilename = defdecl->get_file_info()->get_filename();
    }
    if (sourceFilename.empty() || 0 == sourceFilename.compare("NULL_FILE")) {
      return false;
    }

    string sourceFilenameSubstring = sourceFilename.substr(
        0, stringToFilter.size()); // if the file is from the build tree?
    string sourceFilenameSrcdirSubstring = sourceFilename.substr(
        0, srcDir.size()); // or from the ROSE source tree?
    // or from the developer scratch space?
    string sourceFilenameSecondaryTestSrcdirSubstring =
        sourceFilename.substr(0, secondaryTestSrcDir.size());

#if DEBUG_SELECTOR
    printf("test 1 \n");
    printf("In select: sourceFilenameSubstring = %s \n",
           sourceFilenameSubstring.c_str());
    printf("   --- stringToFilter = %s \n", stringToFilter.c_str());
#endif

    bool retval = false;
    auto root_exists = [](const std::string &root_path) {
      if (root_path.empty()) {
        return false;
      }
      std::error_code ec;
      bool exists = std::filesystem::exists(root_path, ec);
      return exists && !ec;
    };

    if (root_exists(stringToFilter) &&
        rosePathIsWithinTree(stringToFilter, sourceFilename)) {
      retval = true;
    } else {
#if DEBUG_SELECTOR
      printf("test 2 \n");
      printf("   --- sourceFilenameSrcdirSubstring = %s \n",
             sourceFilenameSrcdirSubstring.c_str());
      printf("   --- srcDir = %s \n", srcDir.c_str());
#endif
      if (root_exists(srcDir) && rosePathIsWithinTree(srcDir, sourceFilename)) {
        retval = true;
      } else {
#if DEBUG_SELECTOR
        printf("test 3 \n");
        printf("   --- sourceFilenameSecondaryTestSrcdirSubstring = %s \n",
               sourceFilenameSecondaryTestSrcdirSubstring.c_str());
        printf("   --- secondaryTestSrcDir = %s \n",
               secondaryTestSrcDir.c_str());
#endif
        if (root_exists(secondaryTestSrcDir) &&
            rosePathIsWithinTree(secondaryTestSrcDir, sourceFilename)) {
          retval = true;
        } else {
          // DQ (9/1/2016): Test if this is a template instantiation from a
          // template that would be true using our filter.
          retval = SageInterface::
              isTemplateInstantiationFromTemplateDeclarationSatisfyingFilter(
                  node, this);
        }
      }
    }

    if (retval) {
#if DEBUG_SELECTOR
      std::cerr << "OnlyCurrentDirectory is selecting node " << node << " \""
                << node->get_qualified_name().getString() << "\"\n";
#endif
      ++nselected;
    }

    return retval;
  }
};

size_t OnlyCurrentDirectory::nselected = 0;

int main(int argc, char **argv) {
  /*
    std::cerr
    <<"*******************************************************************************************************\n"
              <<"*** BIG FAT WARNING:  This test, " <<argv[0] <<",\n"
              <<"*** only works if the input files are in the right place in the
    ROSE source tree!  If they are not\n"
              <<"*** in the correct location the test will pass without doing
    anything.  Look for output lines that\n"
              <<"*** begin with \"OnlyCurrentDirectory is selecting node\"
    (there should be at least one).\n"
              <<"*******************************************************************************************************\n";
  */
  std::vector<std::string> argvList(argv, argv + argc);

  // Read the comparison file
  std::string graphCompareOutput = "";
  CommandlineProcessing::isOptionWithParameter(argvList, "-compare:", "(graph)",
                                               graphCompareOutput, true);
  CommandlineProcessing::removeArgsWithParameters(argvList, "-compare:");

  // Run frontend
  SgProject *project = frontend(argvList);
  ROSE_ASSERT(project != NULL);

  // Build the callgraph
  CallGraphBuilder cgb(project);
  OnlyCurrentDirectory selector;
  cgb.buildCallGraph(selector);
  if (0 == selector.nselected) {
    std::cerr << "Error: Test did not detect any function call. All tests "
                 "contain at least one function call."
              << std::endl;
    exit(1);
  }

  if (graphCompareOutput == "")
    graphCompareOutput = ((project->get_outputFileName()) + ".cg.dmp");

  cout << "Writing custom compare to: " << graphCompareOutput << endl;

  SgIncidenceDirectedGraph *newGraph = cgb.getGraph();
  sortedCallGraphDump(graphCompareOutput, newGraph);

  return 0;
}
