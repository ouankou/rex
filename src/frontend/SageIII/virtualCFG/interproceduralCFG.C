#include "CallGraph.h"

#include "interproceduralCFG.h"

#include "sage3basic.h"

#include <vector>

namespace StaticCFG {

void InterproceduralCFG::buildFullCFG() {
  clearNodesAndEdges();
  std::set<VirtualCFG::CFGNode> explored;
  graph_ = new SgIncidenceDirectedGraph;
  ClassHierarchyWrapper classHierarchy(SageInterface::getProject());

  // If the start node is an SgProject, build the CFG from main.
  CFGNode start;
  if (isSgProject(start_)) {
    SgFunctionDeclaration *mainDefDecl = SageInterface::findMain(start_);
    if (mainDefDecl == NULL)
      ROSE_ASSERT(!"Cannot build CFG for project with no main function");

    SgFunctionDefinition *mainDef = mainDefDecl->get_definition();
    if (mainDef == NULL)
      ROSE_ASSERT(!"Cannot build CFG for project with no main function");

    start = mainDef->cfgForBeginning();
  } else {
    start = start_->cfgForBeginning();
  }

  buildCFG(start, explored, &classHierarchy);
}

void InterproceduralCFG::buildFilteredCFG() {
  ROSE_ASSERT(!"InterproceduralCFG:buildFilteredCFG() is unimplemented");
}

void InterproceduralCFG::buildCFG(CFGNode start, std::set<CFGNode> &explored,
                                  ClassHierarchyWrapper *classHierarchy) {
  // Nodes are visited depth first, but the pending targets live on an
  // explicit stack: a whole-program path through the CFG can be far longer
  // than the call stack can hold.
  std::vector<std::pair<std::vector<CFGNode>, size_t>> pending;
  auto visit = [&](const CFGNode &n) {
    SgNode *sgnode = n.getNode();
    ROSE_ASSERT(sgnode);
    if (!explored.insert(n).second)
      return;

    getOrCreateGraphNode(n);

    std::vector<CFGEdge> outEdges;
    unsigned int idx = n.getIndex();
    auto *constructor = isSgConstructorInitializer(sgnode);

    const bool is_call =
        isSgFunctionCallExp(sgnode) || isSgCxxSubobjectDestruction(sgnode) ||
        (constructor != nullptr && constructor->get_declaration() != nullptr);
    if (is_call && idx == sgnode->cfgIndexForEnd() - 1) {
      ROSE_ASSERT(isSgExpression(sgnode));
      Rose_STL_Container<SgFunctionDefinition *> defs;
      CallTargetSet::getDefinitionsForExpression(isSgExpression(sgnode),
                                                 classHierarchy, defs);
      if (defs.size() == 0) {
        std::cerr << sgnode->get_file_info()->get_filenameString() << ":"
                  << sgnode->get_file_info()->get_line()
                  << " warning: CallGraph found no definition(s) for "
                  << sgnode->class_name()
                  << ". Skipping interprocedural behavior." << std::endl;
        outEdges = n.outEdges();
      } else {
        for (SgFunctionDefinition *def : defs) {
          outEdges.emplace_back(n, def->cfgForBeginning());
          outEdges.emplace_back(def->cfgForEnd(), CFGNode(sgnode, idx + 1));
        }
      }
    } else {
      outEdges = n.outEdges();
    }

    std::set<CFGNode> targets;
    for (const CFGEdge &edge : outEdges) {
      CFGNode tar = edge.target();
      ROSE_ASSERT(tar.getNode() != nullptr);
      targets.insert(tar);

      addEdge(CFGPath(edge));
    }
    pending.emplace_back(std::vector<CFGNode>(targets.begin(), targets.end()),
                         0);
  };

  visit(start);
  while (!pending.empty()) {
    auto &[targets, next] = pending.back();
    if (next == targets.size()) {
      pending.pop_back();
      continue;
    }
    const CFGNode target = targets[next++];
    visit(target);
  }
}

} // end of namespace StaticCFG
