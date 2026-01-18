// DQ (10/5/2014): This is more strict now that we include rose_config.h in the
// sage3basic.h. #include "rose.h"

#include "sage3basic.h"

#include "frontierDetection.h"

#include "previousAndNextNode.h"

using namespace std;

// This is the data structure used to store the previous and next IR node.
// One of these data structors is used for each IR node in a map of all
// the IR nodes in the AST.
PreviousAndNextNodeData::PreviousAndNextNodeData(SgNode *previous, SgNode *next)
    : previous(previous), next(next) {}

PreviousAndNextAttribute::PreviousAndNextAttribute(SgNode *from, SgNode *to,
                                                   std::string name,
                                                   std::string options)
    : from(from), to(to), name(name), options(options) {}

PreviousAndNextAttribute::PreviousAndNextAttribute(
    const PreviousAndNextAttribute &X) {
  // Copy constructor.

  from = X.from;
  to = X.to;
  name = X.name;
  options = X.options;
}

std::string PreviousAndNextAttribute::additionalNodeOptions() {
  string s;
  return s;
}

std::vector<AstAttribute::AttributeEdgeInfo>
PreviousAndNextAttribute::additionalEdgeInfo() {
  std::vector<AstAttribute::AttributeEdgeInfo> edgeList;
  AstAttribute::AttributeEdgeInfo edge(from, to, name, options);
  edgeList.push_back(edge);

  return edgeList;
}

std::vector<AstAttribute::AttributeNodeInfo>
PreviousAndNextAttribute::additionalNodeInfo() {
  std::vector<AstAttribute::AttributeNodeInfo> nodeList;

  return nodeList;
}

AstAttribute *PreviousAndNextAttribute::copy() const { return NULL; }

// DQ (11/14/2017): This addition is not portable, should not be specified
// outside of the class definition, and fails for C++11 mode on the GNU 4.8.5
// compiler and the llvm (some version that Craig used). DQ (6/11/2017): Added
// virtual function now required to eliminate warning at runtime.
AstAttribute::OwnershipPolicy
PreviousAndNextAttribute::getOwnershipPolicy() const // override
{
  return CONTAINER_OWNERSHIP;
}

std::map<SgNode *, PreviousAndNextNodeData *>
computePreviousAndNextNodes(SgGlobal *globalScope,
                            std::vector<FrontierNode *> frontierNodes) {
  // This is an alternative way to compute the previous/next node map using the
  // token/AST unparsing frontier list directly.

  std::map<SgNode *, PreviousAndNextNodeData *> previousAndNextNodeMap;

  SgStatement *previousNode = globalScope;
  SgStatement *previousPreviousNode = globalScope;
  for (size_t j = 0; j < frontierNodes.size(); j++) {
    // SgStatement* statement = topAttribute.frontierNodes[j];
    // ROSE_ASSERT(statement != NULL);
    FrontierNode *frontierNode = frontierNodes[j];
    ROSE_ASSERT(frontierNode != NULL);
    SgStatement *statement = frontierNode->node;
    ROSE_ASSERT(statement != NULL);
    PreviousAndNextNodeData *previousAndNextNodeData =
        new PreviousAndNextNodeData(previousPreviousNode, statement);
    // Insert a element into the map for the current IR node being traversed.
    if (previousNode != NULL) {
      previousAndNextNodeMap.insert(
          std::pair<SgNode *, PreviousAndNextNodeData *>(
              previousNode, previousAndNextNodeData));
    } else {
      printf("WARNING: previousNode == NULL: j = %" PRIuPTR
             " can't insert entry into previousAndNextNodeMap: statement = %p "
             "= %s \n",
             j, statement, statement->class_name().c_str());
    }

    previousPreviousNode = previousNode;
    previousNode = statement;
  }

  // Handle the last frontier IR node
  if (frontierNodes.empty() == false) {
    PreviousAndNextNodeData *previousAndNextNodeData =
        new PreviousAndNextNodeData(previousPreviousNode, globalScope);

    // Insert a element into the map for the current IR node being traversed.
    previousAndNextNodeMap.insert(
        std::pair<SgNode *, PreviousAndNextNodeData *>(
            previousNode, previousAndNextNodeData));
  }

  return previousAndNextNodeMap;
}
