
#include <utility>

namespace NewCallGraph {

class CallGraphNodeInfo {
public:
  CallGraphNodeInfo(SgFunctionDeclaration *x);
  SgFunctionDeclaration *functionDeclaration;
};

class CallGraphEdgeInfo {
public:
  CallGraphEdgeInfo(CallGraphNodeInfo *from, CallGraphNodeInfo *to);
  CallGraphNodeInfo *from;
  CallGraphNodeInfo *to;
};

class CallGraph : public SgSimpleProcessing {
public:
  using EdgeKey = std::pair<std::string, std::string>;
  std::map<std::string, CallGraphNodeInfo *> nodeMap;
  std::map<EdgeKey, CallGraphEdgeInfo *> edgeMap;

  CallGraph();
  ~CallGraph() override;
  void visit(SgNode *astNode) override;
};

class CallGraphFileStructure {
  // This data type is used to write the call graph nodes and edges to a file
  // to support accumulation of multiple file data.

#define functionNameLength 256
#define fileNameLength 256
#define directoryLength 1000
#define commandlineLength 2000

public:
  bool isNode; // if not a node then this is a call graph edge.
  bool isDefiningDeclaration;
  char function_name[functionNameLength];      // If this is an edge this is the
                                               // "from" function.
  char function_call_name[functionNameLength]; // if this is an edge then this
                                               // is the "to" function.
  char file_name[fileNameLength]; // if this is a node then this is the absolute
                                  // path with the filename for the function.
  // If this is an node then this is the location from which to compile the
  // source code (using the saved command line).
  char directoryToUseInCompilingSourceFile[directoryLength];

  // This is a potentially very long string.
  // If this is an node then this is the command line required to compile the
  // source file to generate the AST for the defining function declaration.
  char commandlineToCompileSourceFile[commandlineLength];

  CallGraphFileStructure();
};

// File support
int getLock();
void releaseLock(int fd);
void generateCallGraphFile(SgProject *project, CallGraph &cg);
std::vector<CallGraphFileStructure> *
readCallGraphFile(std::string binaryFilename);

// API function
int buildCallGraph(SgProject *project);
} // namespace NewCallGraph
