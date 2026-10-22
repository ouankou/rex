#ifndef _DEPENDENCEGRAPH_H_
#define _DEPENDENCEGRAPH_H_

#include "AstInterface.h"

#include "CallGraph.h"

#include "DefUseChain.h"

#include "ReachingDefinition.h"

#include "StmtInfoCollect.h"

#include <algorithm>

#include <map>

#include <ostream>

#include <set>

#include <string>

#include <utility>

#include "SimpleDirectedGraph.h"

#include "SDGLibraryExtender.h"

#include "DominanceFrontier.h"

#include "DominatorTree.h"

#include "filteredCFG.h"

#include "EDefUse.h"
#include "virtualCFG.h"

/* ! \class DependenceNode

   This class represents a node in a dependence graph. By inheriting from
   SimpleDirectedGraphNode, it keeps track of both its successors and its
   predecessors. The node can be one of several different types, indicating
   its role in a dependence graph. The node may also contain a pointer to an
   SgNode.

   The use of this class requires that the file SimpleDirectedGraph.h exist.

*/
ROSE_DLL_API bool IsImportantForSliceSgFilter(SgNode *n);
struct IsImportantForSliceCFGFilter;

inline std::string dependenceDiagnosticNodeText(const SgNode *node) {
  ASSERT_not_null(node);
  if (const SgNullExpression *absence = isSgNullExpression(node)) {
    SgNode *owner = absence->get_parent();
    if (absence->get_role() !=
            SgNullExpression::e_null_expression_syntactic_absence ||
        owner == nullptr) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[diagnostic-null-expression]: null=%p "
              "role=%d parent=%p/%s has no exact syntactic-absence owner\n",
              static_cast<const void *>(absence),
              static_cast<int>(absence->get_role()), static_cast<void *>(owner),
              owner != nullptr ? owner->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    const SgNodePtrList children = owner->get_traversalSuccessorContainer();
    if (std::count(children.begin(), children.end(), absence) != 1) {
      fprintf(stderr,
              "REX_SLICING_INVARIANT[diagnostic-null-expression]: null=%p "
              "has no unique owning traversal edge in parent=%p/%s\n",
              static_cast<const void *>(absence), static_cast<void *>(owner),
              owner->class_name().c_str());
      ROSE_ABORT();
    }
    if (const SgForStatement *for_statement = isSgForStatement(owner);
        for_statement != nullptr && for_statement->get_increment() == absence) {
      return "<SgNullExpression: absent for increment>";
    }
    return std::string("<SgNullExpression: syntactic absence owned by ") +
           owner->sage_class_name() + ">";
  }
  if (isSgFunctionParameterList(node) != nullptr ||
      isSgCtorInitializerList(node) != nullptr ||
      isSgCatchStatementSeq(node) != nullptr) {
    return std::string("<") + node->sage_class_name() +
           ": owned syntax container>";
  }
  return node->unparseToString();
}

NodeQuerySynthesizedAttributeType queryIsImportantForSliceType(SgNode *astNode);
NodeQuerySynthesizedAttributeType
queryIsImportantForSliceTypeWithCalls(SgNode *astNode);
struct IsImportantForSliceCFGFilter {
  bool operator()(VirtualCFG::CFGNode cfgn) const {
    SgNode *n = cfgn.getNode();
    // modify the is interesting for for,while,do-while and if construct
    // the exit-branch of the cfg needs to be on the controling expression, this
    // if take care of that
    // Jim Leek: 2/21/2009: Added to fix problem with dupicate nodes appearing
    // in Dominance Tree.
    if (isSgExprStatement(n)) {
      return false;
    }

    if (isSgForStatement(n) || isSgDoWhileStmt(n) || isSgWhileStmt(n) ||
        isSgIfStmt(n)) {
      return false;
    }
    // not a loop -> normal isInteresting
    else {
      if (!cfgn.isInteresting())
        return false;
    }
    return IsImportantForSliceSgFilter(cfgn.getNode());
  };
};
typedef VirtualCFG::FilteredCFGEdge<IsImportantForSliceCFGFilter> SliceCFGEdge;
typedef VirtualCFG::FilteredCFGNode<IsImportantForSliceCFGFilter> SliceCFGNode;
typedef DominatorTreesAndDominanceFrontiers::TemplatedDominatorTree<
    IsImportantForSliceCFGFilter>
    SliceDominatorTree;
typedef DominatorTreesAndDominanceFrontiers::TemplatedDominanceFrontier<
    IsImportantForSliceCFGFilter>
    SliceDominanceFrontier;

class ROSE_DLL_API DependenceNode : public SimpleDirectedGraphNode {
public:
  // ! This enum notes what type of node this is
  enum NodeType // NO_STRINGIFY
  {
    CONTROL = 0,   /* !< Used to indicate a dummy node for
                      control dependence */
    SGNODE = 1,    /* !< Used to indicate a node with an SgNode
                      in it */
    CALLSITE = 2,  /* !< Used to indicate a call-site node for
                      interprocedural slicing */
    ACTUALIN = 3,  /* !< Used to indicate the arguments at a call
                      site */
    ACTUALOUT = 4, /* !< Used to indicate returned values at a call site*/

    FORMALIN = 5,  /* !< Used to indicate the arguments into a
                      function */
    FORMALOUT = 6, /* !< Used to indicate the return values from
                      a function */
    ENTRY = 7,     /* !< Used to indicate the entry point of a
                      function */
    ACTUALRETURN =
        8, /* ! since it may happen that an actual out has the same identifiyer
              as the actual out from the return, an ew id was introduces*/
    FORMALRETURN = 9,
    NUM_NODE_TYPES /* !< Must be last enum in list to establish
                      number of types */
  };
  bool isDummyNode() {
    if (depType == ACTUALOUT || depType == FORMALOUT || depType == CONTROL ||
        depType == ENTRY)
      return true;
    else
      return false;
  }

  /* ! \brief Holds C-strings associated with the node types

     The array is initialized in DependenceGraph.C */
  static const char *typeNames[NUM_NODE_TYPES];

  /* !

     \brief Constructor for DependenceNode, defines type, SgNode it
     contains, and name

     Every DependenceNode has a type. Some types have an associated SgNode
     with them: - SGNODE: the SgNode that the DependenceNode represents -
     CALLSITE: the SgFunctionCallExp associated with the function call -
     ENTRY: the SgFunctionDeclaration for the function - ACTUALIN/OUT: the
     SgExpressions that are the arguments to the function call

     The FORMALIN/OUT nodes have a name associated with them (the name of
     the variable in the parameter list).

     CONTROL nodes have neither associated SgNodes nor names.

     If we build a node using this constructor, the _copiedfrom member is
     NULL. */

  DependenceNode(SgNode *node)
      : depType(SGNODE), sgNode(node),
        name(escapeString(dependenceDiagnosticNodeText(node))),
        highlight(false) {}

  DependenceNode(NodeType type, SgNode *node = NULL, std::string depName = "")
      : depType(type), sgNode(node), name(depName), highlight(false) {}
  // ! If only a name is provided, we assume that there is no associated
  // SgNode
  // Han: changed the order of initialization
  DependenceNode(NodeType type, std::string depName)
      : depType(type), sgNode(NULL), name(depName), highlight(false) {}

  // Han: added virtual destructor
  virtual ~DependenceNode() {};

  void highlightNode() { highlight = true; }
  void unHighlightNode() { highlight = false; }
  bool isHighlighted() { return highlight; }

  // ! returns the associated SgNode
  SgNode *getSgNode() { return sgNode; }

  // ! returns the type
  NodeType getType() { return depType; }

  // ! returns the associated name
  std::string getName() { return name; }

  void setName(std::string newName) { name = newName; }

  /* ! \brief returns whether a node has an interprocedural type

     Certain node types are only used during interprocedural slicing (i.e.
     when we use a SystemDependenceGraph. These are: - CALLSITE - ACTUALIN -
     ACTUALOUT - FORMALIN - FORMALOUT - ENTRY */
  bool isInterproc() {
    switch (depType) {
    case CALLSITE:
    case ACTUALIN:
    case ACTUALOUT:
    case FORMALIN:
    case FORMALOUT:
    case ENTRY:
      return true;
    default:
      return false;
    }
  }

  // ! returns whether a node is a formal argument
  bool isFormal() {
    switch (depType) {
    case FORMALIN:
    case FORMALOUT:
      return true;
    default:
      return false;
    }
  }

  // ! returns whether a node is an actual argument
  bool isActual() {
    switch (depType) {
    case ACTUALIN:
    case ACTUALOUT:
      return true;
    default:
      return false;
    }
  }
  bool isFormalReturn() {
    if (depType == FORMALRETURN && isSgFunctionDeclaration(sgNode))
      return true;
    else
      return false;
  }

  // ! Prints the contents of the node to os.
  virtual void writeOut(std::ostream &os) {

    switch (depType) {
    case SGNODE:
      // no output for normal nodes
      break;
      //              os << "SGNODE\\n";break;
    case CONTROL:
      os << "CONTROL\\n";
      break;
    case CALLSITE:
      os << "CALLSITE\\n";
      break;
    case ACTUALIN:
      os << "ACTUALIN\\n";
      break;
    case ACTUALOUT:
      os << "ACTUALOUT\\n";
      break;
    case FORMALIN:
      os << "FORMALIN\\n";
      break;
    case FORMALOUT:
      os << "FORMALOUT\\n";
      break;
    case FORMALRETURN:
      os << "FORMALRETURN\\n";
      break;
    case ACTUALRETURN:
      os << "ACTUALRETURN\\n";
      break;
    case ENTRY:
      os << "ENTRY\\n";
      break;
    default:
      os << "NULL!!!!";
      break;
    }
    if (sgNode != NULL) {
      if (isSgExpressionRoot(sgNode)) {
        os << escapeString(dependenceDiagnosticNodeText(
            isSgExpressionRoot(sgNode)->get_operand()));
      } else if (isSgFunctionDeclaration(sgNode)) {
        os << isSgFunctionDeclaration(sgNode)->get_name().str();
      } else if (isSgFunctionDefinition(sgNode)) {
        os << isSgFunctionDefinition(sgNode)
                  ->get_declaration()
                  ->get_name()
                  .getString();
      } else if (isSgInitializedName(sgNode)) {
        os << isSgInitializedName(sgNode)->get_qualified_name().getString();
      } else {
        os << escapeString(dependenceDiagnosticNodeText(sgNode));
      }
    }
  }

private:
  // Han: changed the order of declaration
  NodeType depType;
  SgNode *sgNode;

  std::string name;

protected:
  bool highlight;
};

/* ! \class InterproceduralInfo

   This class holds information necessary to perform interprocedural slicing.
   As specified in the paper by Horowitz et al, every function in a program
   requires: - An "entry" node - A "formal-in" node for each parameter - A
   "formal-out" node for each return parameter

   Similarly, every function call in a procedure requires: - A "call-site"
   node - An "actual-in" node for every argument to the function - An
   "actual-out" node for every return variable

   There should be one InterproceduralInfo object associated with each
   procedure in a program. It is initialized by passing in a newly created
   object to the constructor of a ControlDependenceGraph. It must then be
   passed to the constructor of a DataDependenceGraph. At this point, it
   contains all the appropriate intraprocedural edges and nodes as specified
   above.

*/
#include "InterproceduralInfo.h"

/* ! \class DependenceGraph

   This class is a SimpleDirectedGraph which uses DependenceNodes. However,
   unlike a standard SimpleDirectedGraph, the SimpleDependenceGraph may
   contain several different types of edges, as indicated by EdgeType.

   The use of this class requires that the file SimpleDirectedGraph.h exist

*/

class ROSE_DLL_API DependenceGraph : public SimpleDirectedGraph {
public:
  DependenceGraph() = default;
  virtual ~DependenceGraph();

  /* ! \brief This enum marks what type of edge connects two DependenceNodes

     This enum is used in conjunction with bit vector representations, so
     some of the values are powers of two */
  enum EdgeType // NO_STRINGIFY
  {
    // control information
    CONTROL = 0x1, /* !< A control dependence edge */
    CALL = 0x4, /* !< An edge between a call site and a function entry, or from
                   actual-in to formal-in nodes */
    CALL_RETURN = 0x5, /* !return to the call-site */
    // data information
    DATA = 0x2,    /* !< A data dependence edge */
    SUMMARY = 0x3, /* !< A summary edge between actual-in and actual-out nodes
                      (used for interprocedural */
    PARAMETER_IN = 0x7,
    PARAMETER_OUT = 0x8,
    // SYNTACTIC
    SYNTACTIC = 0xe,

    DATA_HELPER = 0x9,
    CONTROL_HELPER = 0xa,
    GLOBALVAR_HELPER = 0xb,
    COMPLETENESS_HELPER = 0xc,
    // nice, but completely useless
    BELONGS_TO = 0xd, /* shows for floating nodes, to which statement/node they
                         belong to*/
    // Jim Leek, 2009/6/3: DO_NOT_FOLLOW is A special type of edge for
    // controlling reachability. It can keep an edge from being traversed by
    // _getReachable without changing the type of the edge
    DO_NOT_FOLLOW = 0x10,
  };
  const char *getEdgeName(EdgeType type);
  /* ! \brief create a new DependenceNode which contains node

     Params: - SgNode * node: the SgNode we want to wrap in a DependenceNode

     Return: Either a new DependenceNode or, if we've already wrapped this
     SgNode, the existing DependenceNode containing node

     Side effects: If we created a new DependenceNode, we index it by node
     and DependenceNode type. */
  DependenceNode *createNode(DependenceNode::NodeType type,
                             SgNode *identifyingNode);
  DependenceNode *createNode(SgNode *node);
  void deleteNode(DependenceNode *node);

  /* ! \brief retrieve the DependenceNode that wraps node

     Params: - SgNode * node: the SgNode for which we want to find the
     associated DependenceNode

     Return: The wrapper DependenceNode, if one exists. */
  DependenceNode *getNode(SgNode *node);
  DependenceNode *getNode(DependenceNode::NodeType type,
                          SgNode *identifyingNode);

  DependenceNode *getExistingNode(SgNode *node);
  DependenceNode *getExistingNode(DependenceNode::NodeType type,
                                  SgNode *identifyingNode);

  /* ! \brief create an edge of type e between from and to

     Params: - DependenceNode * from: the source of the edge -
     DependenceNode * to: the sink of the edge - EdgeType e: the type of the
     edge

     Side effects: Inserts the Edge (from, to) into the set associated with
     e by _edgetype_map. Inserts e into the set associated with Edge(from,
     to) by _edge_map.

  */
  virtual void establishEdge(DependenceNode *from, DependenceNode *to,
                             EdgeType e = CONTROL);
  virtual void removeEdge(DependenceNode *from, DependenceNode *to,
                          EdgeType e = CONTROL);
  /* ! \brief determine if there is an edge of type e between from and to

     Params: - DependenceNode * from: the source of the edge -
     DependenceNode * to: the sink of the edge - EdgeType e: the type of the
     edge

     Return: true if e is in the set associated with Edge(from, to) by
     _edge_map. */
  bool edgeExists(DependenceNode *from, DependenceNode *to, EdgeType e);
  bool hasOutgingEdge(DependenceNode *src, EdgeType compare);

  /* ! \brief returns all edges between from and to

     Params: - DependenceNode * from: the source of the edge -
     DependenceNode * to: the sink of the edge

     Return: the set of EdgeTypes associated with Edge(from, to) by
     _edge_map.

  */
  std::set<EdgeType> edgeType(DependenceNode *from, DependenceNode *to);
  // ! writes a dot file representing this dependence graph to filename
  void writeDot(const char *filename) override;

protected:
  std::map<DependenceNode::NodeType, std::map<SgNode *, DependenceNode *>>
      nodeTypeToDepNodeMapMap;

  typedef std::pair<DependenceNode *, DependenceNode *> Edge;

  // ! a map from EdgeType to all the edges of that type
  std::map<EdgeType, std::set<Edge>> edgeTypeMap;

  // ! a map from an edge to all the variants of that edge in the graph
  std::map<Edge, std::set<EdgeType>> edgeMap;

  bool isLibraryFunction(SgFunctionDeclaration *sgFD) const {
    if (sgFD == NULL)
      return false;
    else if (sgFD->get_definition() != NULL)
      return true;
    else
      return false;
  };
};

/* ! \class ControlDependenceGraph

   This class is a DependenceGraph which expresses control dependences for a
   given procedure. Control dependences essentially link decision points in
   CFGs with the nodes affected by those decisions. The implementation of this
   class depends on the calculation of Dominator Trees and Dominance
   Frontiers.

   If interprocedural analysis is specified (by passing in a non-null
   InterproceduralInfo object), building this graph will also initialize the
   InterproceduralInfo object by creating the required formal-in, formal-out,
   actual-in, actual-out and entry nodes (as well as any required call nodes)
   for the procedure, and creating the appropriate control dependences: - all
   actual-in and actual-out nodes are control dependent on the call site - all
   formal-in and formal-out nodes are control dependent on the entry node -
   the call site node is control dependent on the DependenceNode representing
   the function call.


   The use of this class requires that DominanceFrontier.h exist. Also, the
   use of this class requires linking against libDominance.

*/

class ControlDependenceGraph : public DependenceGraph {

public:
  explicit ControlDependenceGraph(SgFunctionDefinition *head);

  void computeInterproceduralInformation(InterproceduralInfo *ii);
  void computeAdditionalFunctioncallDepencencies();

private:
  Rose_STL_Container<SgNode *> functionCalls;
  void createSyntacticDependencies();
  // control-flow-graph for this function
  SliceCFGNode source, sink;

  SliceDominatorTree dominatorTree;

  void addDependence(int source, int to);

  // ! The root of the AST that the ControlDependenceGraph represents
  SgFunctionDefinition *head;

  void buildCDG();
};

/* ! \class DataDependenceGraph

   Tracks definitions and uses within one function, including parameter and
   return dependencies. Definitions that reach the end of a function without a
   return statement are not linked to pass-by-reference actual-out nodes. */

class DataDependenceGraph : public DependenceGraph {

public:
  DataDependenceGraph(SgFunctionDefinition *head, EDefUse *du);

private:
  EDefUse *defuse;

  // ! uses def-use chains to build the data dependence graph
  void buildDDG();

  // ! The function that we are building the DDG for
  SgFunctionDefinition *_head;
};

/* ! \class MergedDependenceGraph

   This class provides a mechanism for merging together multiple
   DependenceGraphs. It also provides a mechanism for getting backwards
   reachable nodes given a specific start node and using specified edge types.
   This can be called by the abstract function getSlice to create specific
   types of slices (i.e. intraprocedural or interprocedural).

*/
class MergedDependenceGraph : public DependenceGraph {

public:
  /* ! \brief creates a backward slice starting from node

     Params: - DependenceNode * node: the slicing criterion

     Return: returns a set of DependenceNodes which belong in the slice with
     slicing criterion node. */
  virtual std::set<DependenceNode *> getSlice(DependenceNode *node) = 0;

protected:
  /* ! \brief performs a backwards reachability analysis starting from nodes
     in start

     Params: - set<DependenceNode *> start: the starting nodes for the
     backwards reachability analysis - int edgeTypesToFollow: a bit-vector
     whose set bits indicate the types of edges to consider for
     reachability. */
  std::set<DependenceNode *> _getReachable(std::set<DependenceNode *> start,
                                           int edgeTypesToFollow = 0);

  /* ! \brief merges graph into the current MergedDependenceGraph

     Params: - DependenceGraph * graph: the DependenceGraph we want to merge
     into the current graph

     Side effects: Any new nodes and edges from graph are added to the
     MergedDependenceGraph */
  void mergeGraph(DependenceGraph *graph);
};

/* ! \class SystemDependenceGraph

   This class merges control and data dependence graphs for all procedures in
   the program. A slice on this graph is interprocedural.

   @todo _getPossibleFuncs simply assumes that the AST correctly resolves
   every SgFunctionCallExp to a specific SgFunctionDeclaration with an
   existing SgFunctionDefinition. This should be redone to take advantage of
   call graph analysis, which will let us slice codes which have more
   ambiguous function calls. */

class ROSE_DLL_API SystemDependenceGraph : public MergedDependenceGraph {

  std::vector<SDGLibraryExtender *> libraryExtenders;

public:
  void addLibraryExtender(SDGLibraryExtender *le) {
    if (le != NULL)
      libraryExtenders.push_back(le);
  }
  SystemDependenceGraph() = default;
  ~SystemDependenceGraph() override;
  SgNode *getMainFunction();
  void createSafeConfiguration(SgFunctionDeclaration *fDef);
  bool isKnownLibraryFunction(SgFunctionDeclaration *fDec);
  void createConnectionsForLibaryFunction(SgFunctionDeclaration *fDec);
  void parseProject(SgProject *project);

  /*! once all functions have been added to the SystemDependenceGraph this
   * function performas the connection of callsites to all possible called
   * functions and establishes summary-edges*/
  void performInterproceduralAnalysis();
  void computeSummaryEdges();
  void cleanUp(std::set<SgNode *> preserve);

  void createFunctionStub(InterproceduralInfo *info);

  void addFunction(ControlDependenceGraph *cdg, DataDependenceGraph *ddg);

  InterproceduralInfo *
  getInterproceduralInformation(SgFunctionDeclaration *dec) {
    auto found = interproceduralInformation.find(dec);
    return found != interproceduralInformation.end() ? found->second : nullptr;
  }
  void addInterproceduralInformation(InterproceduralInfo *info) {
    ROSE_ASSERT(info != nullptr);
    SgFunctionDeclaration *declaration = info->getFunctionDeclaration();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(interproceduralInformation.count(declaration) == 0);
    interproceduralInformation.emplace(declaration, info);
    interproceduralInformationList.push_back(info);
  }
  void doInterproceduralConnections(InterproceduralInfo *ii);

  /* ! \brief performs a backwards slice with slicing criterion node

     getSlice is defined according to the paper by Horowitz et al. as a two
     phase operation. The first operation does backwards reachability to
     "mark" nodes while not traversing return edges. Thus it ignores functin
     calls. The second phase does backwards reachability from all marked
     nodes while not traversing call edges. Thus it ignores calling
     functions. The final set of reachable nodes is the interprocedural
     slice. */
  std::set<DependenceNode *> getSlice(DependenceNode *node) override;

private:
  Rose_STL_Container<InterproceduralInfo *> interproceduralInformationList;
  std::map<SgFunctionDeclaration *, InterproceduralInfo *>
      interproceduralInformation;

  /* ! \brief get a list of functions that funcCall could be referring to

     Params: - SgFunctionCallExp * funcCall: the function call we are
     analyzing

     Return: A list of SgFunctionDeclarations that funcCall could
     potentially call.

  */
  std::vector<InterproceduralInfo *>
  getPossibleFuncs(SgFunctionCallExp *funcCall);
  Rose_STL_Container<SgFunctionDeclaration *>
  _getPossibleFuncs(SgFunctionCallExp *funcCall);

  // CI (01/12/2007): add
  std::map<SgFunctionDeclaration *, InterproceduralInfo *>
      functionToInterfunctionalMap;
};

#endif
