#include "sage3basic.h"

#include "DependenceGraph.h"

using namespace std;
const char *DependenceNode::typeNames[DependenceNode::NUM_NODE_TYPES] = {
    "CONTROL",  "SGNODE",    "CALLSITE", "ACTUALIN",     "ACTUALOUT",
    "FORMALIN", "FORMALOUT", "ENTRY",    "ACTUALRETURN", "FORMALRETURN"};

const char *DependenceGraph::getEdgeName(EdgeType type) {
  switch (type) {
  case SYNTACTIC:
    return "SYNTACTIC";
  case CONTROL:
    return "CONTROL";
  case DATA:
    return "DATA";
  case SUMMARY:
    return "SUMMARY";
  case CALL:
    return "CALL";
  case PARAMETER_IN:
    return "PARAMETER_IN";
  case PARAMETER_OUT:
    return "PARAMETER_OUT";
  case DATA_HELPER:
    return "DATA_HELPER";
  case CONTROL_HELPER:
    return "CONTROL_HELPER";
  case GLOBALVAR_HELPER:
    return "GLOBALVAR_HELPER";
  case COMPLETENESS_HELPER:
    return "COMPLETENESS_HELPER";
  case BELONGS_TO:
    return "BELONGS_TO";
  default:
    return "UNKNOWN";
  }
}

DependenceGraph::~DependenceGraph() {
  edgeTypeMap.clear();
  edgeMap.clear();
  nodeTypeToDepNodeMapMap.clear();

  std::set<SimpleDirectedGraphNode *> nodes;
  nodes.swap(_nodes);
  for (SimpleDirectedGraphNode *node : nodes) {
    delete node;
  }
}

DependenceNode *DependenceGraph::createNode(SgNode *node) {
  return createNode(DependenceNode::SGNODE, node);
}

void DependenceGraph::deleteNode(DependenceNode *node) {
  for (SimpleDirectedGraphNode *successor : node->getSuccessors()) {
    auto *to = static_cast<DependenceNode *>(successor);
    const auto types = edgeType(node, to);
    ROSE_ASSERT(!types.empty());
    for (EdgeType type : types)
      removeEdge(node, to, type);
  }
  for (SimpleDirectedGraphNode *predecessor : node->getPredecessors()) {
    auto *from = static_cast<DependenceNode *>(predecessor);
    const auto types = edgeType(from, node);
    ROSE_ASSERT(!types.empty());
    for (EdgeType type : types)
      removeEdge(from, node, type);
  }
  DependenceNode::NodeType type = node->getType();
  SgNode *identifyingNode = node->getSgNode();
  auto typeIt = nodeTypeToDepNodeMapMap.find(type);
  ROSE_ASSERT(typeIt != nodeTypeToDepNodeMapMap.end());
  auto nodeIt = typeIt->second.find(identifyingNode);
  ROSE_ASSERT(nodeIt != typeIt->second.end() && nodeIt->second == node);
  typeIt->second.erase(nodeIt);
  if (typeIt->second.empty())
    nodeTypeToDepNodeMapMap.erase(typeIt);
  removeNode(node);
  delete node;
}

DependenceNode *DependenceGraph::createNode(DependenceNode::NodeType type,
                                            SgNode *identifyingNode) {
  ROSE_ASSERT(identifyingNode != nullptr);
  auto &nodes = nodeTypeToDepNodeMapMap[type];
  auto found = nodes.find(identifyingNode);
  if (found != nodes.end())
    return found->second;

  DependenceNode *depNode = new DependenceNode(type, identifyingNode);
  nodes.emplace(identifyingNode, depNode);
  addNode(depNode);
  return depNode;
}
DependenceNode *DependenceGraph::getNode(DependenceNode::NodeType type,
                                         SgNode *identifyingNode) {
  return createNode(type, identifyingNode);
}

DependenceNode *DependenceGraph::getExistingNode(DependenceNode::NodeType type,
                                                 SgNode *identifyingNode) {
  if (identifyingNode == nullptr)
    return nullptr;
  auto typeIt = nodeTypeToDepNodeMapMap.find(type);
  if (typeIt == nodeTypeToDepNodeMapMap.end())
    return nullptr;
  auto nodeIt = typeIt->second.find(identifyingNode);
  return nodeIt == typeIt->second.end() ? nullptr : nodeIt->second;
}

DependenceNode *DependenceGraph::getExistingNode(SgNode *identifyingNode) {
  return getExistingNode(DependenceNode::SGNODE, identifyingNode);
}

DependenceNode *DependenceGraph::getNode(SgNode *node) {
  if (node == nullptr)
    ROSE_ABORT();
  return getNode(DependenceNode::SGNODE, node);
}

void DependenceGraph::removeEdge(DependenceNode *from, DependenceNode *to,
                                 EdgeType e) {
  const Edge edge(from, to);
  auto typeIt = edgeTypeMap.find(e);
  auto edgeIt = edgeMap.find(edge);
  ROSE_ASSERT(typeIt != edgeTypeMap.end() && typeIt->second.count(edge) == 1);
  ROSE_ASSERT(edgeIt != edgeMap.end() && edgeIt->second.count(e) == 1);

  typeIt->second.erase(edge);
  if (typeIt->second.empty())
    edgeTypeMap.erase(typeIt);

  edgeIt->second.erase(e);
  if (edgeIt->second.empty()) {
    edgeMap.erase(edgeIt);
    removeLink(from, to);
  }
}
void DependenceGraph::establishEdge(DependenceNode *from, DependenceNode *to,
                                    EdgeType e) {
  // internal Book-keeping
  edgeTypeMap[e].insert(Edge(from, to));
  edgeMap[Edge(from, to)].insert(e);
  // link those two nodes using the SimpleGraphInterface
  addLink(from, to);
}

bool DependenceGraph::edgeExists(DependenceNode *from, DependenceNode *to,
                                 EdgeType e) {
  auto typeIt = edgeTypeMap.find(e);
  return typeIt != edgeTypeMap.end() && typeIt->second.count(Edge(from, to));
}
bool DependenceGraph::hasOutgingEdge(DependenceNode *src, EdgeType compare) {
  set<SimpleDirectedGraphNode *> successors = src->getSuccessors();
  for (set<SimpleDirectedGraphNode *>::iterator i = successors.begin();
       i != successors.end(); i++) {
    DependenceNode *dst = dynamic_cast<DependenceNode *>(*i);
    if (edgeType(src, dst).count(compare) > 0) {
      return true;
    }
  }
  return false;
}

std::set<DependenceGraph::EdgeType>
DependenceGraph::edgeType(DependenceNode *from, DependenceNode *to) {
  auto edgeIt = edgeMap.find(Edge(from, to));
  return edgeIt == edgeMap.end() ? std::set<EdgeType>() : edgeIt->second;
}

void DependenceGraph::writeDot(const char *filename) {
  std::vector<std::string> styles;
  std::ofstream f(filename);
  f.exceptions(std::ios::failbit | std::ios::badbit);

  f << "digraph \"G" << filename << "\" {" << std::endl;
  // output all of the nodes
  std::set<SimpleDirectedGraphNode *>::iterator i;
  for (i = _nodes.begin(); i != _nodes.end(); i++) {
    styles.clear();
    DependenceNode *node = dynamic_cast<DependenceNode *>(*i);

    SimpleDirectedGraphNode *d = *i;
    char buf[sizeof(SimpleDirectedGraphNode *) * 2 + 3];

    snprintf(buf, sizeof(buf), "%p", d);
    f << "\"" << buf << "\" [";
    // label
    f << "label = \"";
    _displayData(d, f);
    f << "\"";
    if (node->isHighlighted()) {
      f << ",color=red";
      styles.push_back(string("bold"));
    }

    // append a shape
    switch (node->getType()) {
    case DependenceNode::ENTRY:
      f << ",shape=hexagon";
      f << ",fillcolor=deepskyblue";
      styles.push_back(string("filled"));
      break;
    case DependenceNode::ACTUALIN:
    case DependenceNode::ACTUALOUT:
    case DependenceNode::FORMALIN:
    case DependenceNode::FORMALOUT:
    case DependenceNode::FORMALRETURN:
    case DependenceNode::ACTUALRETURN:
      f << ",shape=box";
      break;
    default:
      break;
    }
    f << ",style=\"";
    for (unsigned int st = 0; st < styles.size(); st++) {
      if (st != 0)
        f << ",";
      f << styles[st];
    }
    f << "\"";
    // end the properties for that node
    f << "];" << std::endl;
  }

  // output all of the edges (we'll just use successor edges
  for (i = _nodes.begin(); i != _nodes.end(); i++) {
    DependenceNode *d1 = dynamic_cast<DependenceNode *>(*i);

    std::set<SimpleDirectedGraphNode *> succs = d1->getSuccessors();
    std::set<SimpleDirectedGraphNode *>::iterator j;
    for (j = succs.begin(); j != succs.end(); j++) {
      DependenceNode *d2 = dynamic_cast<DependenceNode *>(*j);

      char buf1[sizeof(DependenceNode *) * 2 + 3];
      char buf2[sizeof(DependenceNode *) * 2 + 3];

      snprintf(buf1, sizeof(buf1), "%p", d1);
      snprintf(buf2, sizeof(buf2), "%p", d2);

      const std::set<EdgeType> ets = edgeType(d1, d2);
      ROSE_ASSERT(!ets.empty());

      const bool highlightEdge = d1->isHighlighted() && d2->isHighlighted();

      for (EdgeType et : ets) {
        f << "\"" << buf1 << "\" -> \"" << buf2 << "\"";
        f << "[";
        f << "label=\"" << getEdgeName(et) << "\"";
        if (!highlightEdge) {
          switch (et) {
          case SUMMARY:
            f << ",color=cyan";
            break;
          case CONTROL:
            f << ",color=blue";
            break;
          case DATA:
            f << ",color=green";
            break;
          case CALL:
            f << ",color=gold1";
            break;
          case SYNTACTIC:
            f << ",color=gray";
            break;
          default:
            break;
          }
        } else {
          f << ",style=bold";
          f << ",color=red";
        }
        f << "];" << std::endl;
      }
    }
  }

  f << "}" << std::endl;
  f.close();
}

bool IsImportantForSliceSgFilter(SgNode *n) {
  if (isSgExprStatement(n)) {
    return false;
  }
  if (isSgFunctionCallExp(n)) {
    return true;
  }

  if (isSgStatement(n))
    return true;
  if (isSgExpression(n) && !isSgExpression(n->get_parent()))
    return true;
  return false;
}

NodeQuerySynthesizedAttributeType
queryIsImportantForSliceType(SgNode *astNode) {
  NodeQuerySynthesizedAttributeType retVal;
  if (IsImportantForSliceSgFilter(astNode))
    retVal.push_back(astNode);
  return retVal;
}

NodeQuerySynthesizedAttributeType
queryIsImportantForSliceTypeWithCalls(SgNode *astNode) {
  NodeQuerySynthesizedAttributeType retVal;
  if (IsImportantForSliceSgFilter(astNode))
    retVal.push_back(astNode);
  return retVal;
}
