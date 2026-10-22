
// Tracking IDs belong to a node's lifetime. Original source positions and
// transformation input IDs remain available after that node is deleted.
#ifndef TRANSFORMATION_TRACKING_H
#define TRANSFORMATION_TRACKING_H

#include <map>

#include <set>

#include <utility>

class SgNode;
class Sg_File_Info;

typedef unsigned int AST_NODE_ID;
namespace TransformationTracking {
//! Assign Ids and save current File Info.
void registerAstSubtreeIds(SgNode *root);

AST_NODE_ID getId(SgNode *);

// check if a node has been assigned a unique ID
bool hasId(SgNode *n);

// Return the live node for an ID, or null if it has been deleted.
SgNode *getNode(AST_NODE_ID id);

// Retrieve the original source positions for a registered ID.
std::pair<Sg_File_Info *, Sg_File_Info *> getFileInfo(AST_NODE_ID id);

// To simplify the problem,
// we keep track of information at a single line statement level.
//
// For a transformed (statement) node, add an input (statement) node.
// Internally, the input node's ID will be saved.
//
// This should be called after the transformation is completed and all IDs are
// assigned.
void addInputNode(SgNode *affected_node, SgNode *inputnode);

extern std::map<AST_NODE_ID, std::set<AST_NODE_ID>> inputIDs;
void setId(SgNode *);
// Called by the generated SgNode destructor before memory can be reused.
void clearId(SgNode *);

// return the next Id which will be used (assigned to an AST node)
// This is also the same as the total number of IDs + 1. It can be used to
// control loop iteration over all IDs
AST_NODE_ID getNextId();

} // end namespace TransformationTracking

#endif // TRANSFORMATION_TRACKING_H
