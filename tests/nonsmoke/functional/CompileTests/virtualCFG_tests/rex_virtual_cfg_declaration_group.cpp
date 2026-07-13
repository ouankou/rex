#include "rose.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace {
using VirtualCFG::CFGEdge;
using VirtualCFG::CFGNode;

bool isReachable(CFGNode start, CFGNode target) {
  std::set<CFGNode> visited;
  std::vector<CFGNode> worklist{start};
  while (!worklist.empty()) {
    const CFGNode current = worklist.back();
    worklist.pop_back();
    if (!visited.insert(current).second) {
      continue;
    }
    if (current == target) {
      return true;
    }
    for (const CFGEdge &edge : current.outEdges()) {
      worklist.push_back(edge.target());
    }
  }
  return false;
}

void requireOnlyEdge(CFGNode source, CFGNode target) {
  const std::vector<CFGEdge> edges = source.outEdges();
  ROSE_ASSERT(edges.size() == 1);
  ROSE_ASSERT(edges.front().source() == source);
  ROSE_ASSERT(edges.front().target() == target);

  const std::vector<CFGEdge> incoming = target.inEdges();
  ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), edges.front()) !=
              incoming.end());
}

std::string declarationName(SgDeclarationStatement *declaration) {
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(declaration)) {
    ROSE_ASSERT(variable->get_variables().size() == 1);
    ROSE_ASSERT(variable->get_variables().front() != nullptr);
    return variable->get_variables().front()->get_name().getString();
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration)) {
    return function->get_name().getString();
  }
  return "";
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgDeclarationGroupStatement *group = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgDeclarationGroupStatement)) {
    SgDeclarationGroupStatement *candidate =
        isSgDeclarationGroupStatement(node);
    ROSE_ASSERT(candidate != nullptr);
    for (SgDeclarationStatement *member : candidate->get_declarations()) {
      if (declarationName(member) == "rex_cfg_first_value") {
        ROSE_ASSERT(group == nullptr);
        group = candidate;
      }
    }
  }
  ROSE_ASSERT(group != nullptr);
  group->validate();

  const SgDeclarationStatementPtrList &members = group->get_declarations();
  ROSE_ASSERT(members.size() == 3);
  ROSE_ASSERT(declarationName(members[0]) == "rex_cfg_first_value");
  ROSE_ASSERT(declarationName(members[1]) == "rex_cfg_middle_function");
  ROSE_ASSERT(declarationName(members[2]) == "rex_cfg_second_value");

  SgVariableDeclaration *first = isSgVariableDeclaration(members[0]);
  SgFunctionDeclaration *middle = isSgFunctionDeclaration(members[1]);
  SgVariableDeclaration *second = isSgVariableDeclaration(members[2]);
  ROSE_ASSERT(first != nullptr);
  ROSE_ASSERT(middle != nullptr);
  ROSE_ASSERT(second != nullptr);

  ROSE_ASSERT(group->cfgIndexForEnd() == members.size());
  for (unsigned int index = 0; index <= group->cfgIndexForEnd(); ++index) {
    ROSE_ASSERT(!group->cfgIsIndexInteresting(index));
  }

  requireOnlyEdge(CFGNode(group, 0), first->cfgForBeginning());
  requireOnlyEdge(first->cfgForEnd(), CFGNode(group, 1));
  requireOnlyEdge(CFGNode(group, 1), middle->cfgForBeginning());
  requireOnlyEdge(middle->cfgForEnd(), CFGNode(group, 2));
  requireOnlyEdge(CFGNode(group, 2), second->cfgForBeginning());
  requireOnlyEdge(second->cfgForEnd(), CFGNode(group, 3));

  SgFunctionCallExp *firstCall = nullptr;
  SgFunctionCallExp *secondCall = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(group, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgFunctionDeclaration *callee = call->getAssociatedFunctionDeclaration();
    ROSE_ASSERT(callee != nullptr);
    if (callee->get_name().getString() == "rex_cfg_first") {
      ROSE_ASSERT(firstCall == nullptr);
      firstCall = call;
    } else if (callee->get_name().getString() == "rex_cfg_second") {
      ROSE_ASSERT(secondCall == nullptr);
      secondCall = call;
    }
  }
  ROSE_ASSERT(firstCall != nullptr);
  ROSE_ASSERT(secondCall != nullptr);

  ROSE_ASSERT(
      isReachable(group->cfgForBeginning(), firstCall->cfgForBeginning()));
  ROSE_ASSERT(
      isReachable(firstCall->cfgForBeginning(), secondCall->cfgForBeginning()));
  ROSE_ASSERT(isReachable(secondCall->cfgForBeginning(), group->cfgForEnd()));
  ROSE_ASSERT(!isReachable(secondCall->cfgForBeginning(),
                           firstCall->cfgForBeginning()));

  AstTests::runAllTests(project);
  return 0;
}
