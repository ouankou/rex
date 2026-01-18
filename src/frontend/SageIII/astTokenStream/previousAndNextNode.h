#ifndef PREVIOUS_AND_NEXT_NODE_HEADER
#define PREVIOUS_AND_NEXT_NODE_HEADER

#include "AstAttributeMechanism.h"

#include <map>
#include <string>
#include <vector>

class FrontierNode;
class SgGlobal;

class PreviousAndNextNodeData {
public:
  SgNode *previous;
  SgNode *next;

  PreviousAndNextNodeData(SgNode *previous, SgNode *next);
};

class PreviousAndNextAttribute : public AstAttribute {
  // This class supports marking the AST in the normal ROSE AST graph
  // generation. We use this ROSE feature to mark the previous, and next nodes
  // in the frontier.

private:
  SgNode *from;
  SgNode *to;
  std::string name;
  std::string options;

public:
  // PreviousAndNextAttribute(SgNode* n, std::string name, std::string options);
  PreviousAndNextAttribute(SgNode *from, SgNode *to, std::string name,
                           std::string options);

  PreviousAndNextAttribute(const PreviousAndNextAttribute &X);

  // Support for graphics output of IR nodes using attributes (see the DOT graph
  // of the AST)
  virtual std::string additionalNodeOptions() override;
  virtual std::vector<AstAttribute::AttributeEdgeInfo>
  additionalEdgeInfo() override;
  virtual std::vector<AstAttribute::AttributeNodeInfo>
  additionalNodeInfo() override;

  // Support for the coping of AST and associated attributes on each IR node
  // (required for attributes derived from AstAttribute, else just the base
  // class AstAttribute will be copied).
  virtual AstAttribute *copy() const override;

  // DQ (6/11/2017): Added virtual function now required to eliminate warning at
  // runtime.
  virtual AstAttribute::OwnershipPolicy
  getOwnershipPolicy() const override; // { return CONTAINER_OWNERSHIP; }
};

std::map<SgNode *, PreviousAndNextNodeData *>
computePreviousAndNextNodes(SgGlobal *globalScope,
                            std::vector<FrontierNode *> frontierNodes);

#endif
