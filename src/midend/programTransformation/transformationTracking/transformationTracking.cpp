#include "transformationTracking.h"
#include "sage3basic.h"

#include <limits>

using namespace std;

namespace TransformationTracking {

namespace {
bool trackingActive = false;
bool tablesAlive = true;
std::map<SgNode *, unsigned int> AstToIdTable;
std::map<unsigned int, SgNode *> IdToAstTable;
AST_NODE_ID nextId = 1;
std::map<AST_NODE_ID, std::pair<Sg_File_Info *, Sg_File_Info *>>
    IdToFileInfoTable;
} // namespace

// IDs of all input Nodes of a transformation
std::map<AST_NODE_ID, std::set<AST_NODE_ID>> inputIDs;

namespace {
struct TrackingTableLifetime {
  ~TrackingTableLifetime() {
    trackingActive = false;
    tablesAlive = false;
  }
} trackingTableLifetime;
} // namespace

// To simplify the problem,
// we keep track of information at a single line statement level for now.
//
// For a transformed (statement) node, add an input (statement) node.
// Internally, the input node's ID will be saved.
//
// This should be called after the transformation is completed and all IDs are
// assigned.
void addInputNode(SgNode *affected_node, SgNode *input_node) {
  ROSE_ASSERT(affected_node != NULL);
  ROSE_ASSERT(input_node != NULL);

  AST_NODE_ID affected_id = getId(affected_node);
  ROSE_ASSERT(affected_id != 0);
  AST_NODE_ID input_id = getId(input_node);
  ROSE_ASSERT(input_id != 0);

  inputIDs[affected_id].insert(input_id);
}

bool hasId(SgNode *n) { return (getId(n) != 0); }

SgNode *getNode(AST_NODE_ID id) {
  ROSE_ASSERT(tablesAlive);
  const auto found = IdToAstTable.find(id);
  return found == IdToAstTable.end() ? nullptr : found->second;
}

// public interface implementation
AST_NODE_ID getNextId() { return nextId; }

AST_NODE_ID getId(SgNode *n) {
  ROSE_ASSERT(tablesAlive);
  if (!trackingActive)
    return 0;
  const auto found = AstToIdTable.find(n);
  return found == AstToIdTable.end() ? 0 : found->second;
}

// an internal helper function
static std::pair<Sg_File_Info *, Sg_File_Info *>
copyBeginAndEndFileInfo(SgNode *n) {
  std::pair<Sg_File_Info *, Sg_File_Info *> rtvalue;
  ROSE_ASSERT(n != NULL);
  if (isSgPragma(n) || isSgLocatedNode(n)) {
    rtvalue = std::make_pair(SageInterface::deepCopy(n->get_startOfConstruct()),
                             SageInterface::deepCopy(n->get_endOfConstruct()));
  }
  return rtvalue;
}

std::pair<Sg_File_Info *, Sg_File_Info *> getFileInfo(AST_NODE_ID id) {
  ROSE_ASSERT(tablesAlive);
  return IdToFileInfoTable.at(id);
}

void clearId(SgNode *n) {
  if (!trackingActive)
    return;
  const auto found = AstToIdTable.find(n);
  if (found != AstToIdTable.end()) {
    IdToAstTable.erase(found->second);
    AstToIdTable.erase(found);
  }
}

void setId(SgNode *n) {
  ROSE_ASSERT(n != NULL);
  ROSE_ASSERT(tablesAlive);
  trackingActive = true;
  AST_NODE_ID cur_id = getId(n);
  // only set when the node has no ID assigned right now
  if (cur_id == 0) {
    ROSE_ASSERT(nextId < std::numeric_limits<AST_NODE_ID>::max());
    AstToIdTable[n] = nextId;
    IdToAstTable[nextId] = n;
    // We store the file info at the same time
    IdToFileInfoTable[nextId] = copyBeginAndEndFileInfo(n);
    nextId++;
  }
}
class visitorTraversal : public AstSimpleProcessing {
protected:
  void virtual visit(SgNode *n) {
    if (isSgProject(n) || isSgSourceFile(n) || isSgInitializedName(n) ||
        isSgLocatedNode(n) || isSgFileList(n)) {
      setId(n);
    }
  }
};

void registerAstSubtreeIds(SgNode *root) {
  visitorTraversal myTraversal;
  // we want to traverse full AST to ID them all
  myTraversal.traverse(root, preorder);
}

} // namespace TransformationTracking
