#include "sage3basic.h"

#include "linearizeAST.h"

#include "rose_config.h"

class PostSynthesizedAttribute {
public:
  std::vector<SgNode *> nodes;
};

class PostAST : public SgBottomUpProcessing<PostSynthesizedAttribute> {
private:
  SgNode *rootNode;
  std::vector<SgNode *> nodes;

  PostSynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *astNode, SubTreeSynthesizedAttributes synthesizedAttributeList) {
    PostSynthesizedAttribute synAttrib;

    if (synthesizedAttributeList.size() > 2) {
      if (isSgConditionalExp(astNode) == NULL &&
          isSgExprListExp(astNode) == NULL)
        ROSE_ABORT();

      if (isSgExprListExp(astNode) != NULL)
        synAttrib.nodes.push_back(astNode);

      for (size_t i = 0; i < synthesizedAttributeList.size(); i++) {
        if (isSgConditionalExp(astNode) != NULL && i == 1)
          synAttrib.nodes.push_back(astNode);

        for (size_t j = 0; j < synthesizedAttributeList[i].nodes.size(); j++)
          synAttrib.nodes.push_back(synthesizedAttributeList[i].nodes[j]);
      }

    } else if (synthesizedAttributeList.size() > 0) {
      if (isSgUnaryOp(astNode) != NULL) {
        SgUnaryOp *unaryOp = isSgUnaryOp(astNode);

        synAttrib.nodes = synthesizedAttributeList[0].nodes;
        if (unaryOp->get_mode() == SgUnaryOp::prefix)
          synAttrib.nodes.insert(synAttrib.nodes.begin(), astNode);
        else
          synAttrib.nodes.push_back(astNode);

      } else {
        synAttrib.nodes = synthesizedAttributeList[0].nodes;
        synAttrib.nodes.push_back(astNode);
      }

      if (synthesizedAttributeList.size() >= 2) {
        for (size_t i = 0; i < synthesizedAttributeList[1].nodes.size(); i++)
          synAttrib.nodes.push_back(synthesizedAttributeList[1].nodes[i]);
      }
    } else
      synAttrib.nodes.push_back(astNode);

    if (astNode == rootNode)
      nodes = synAttrib.nodes;

    return synAttrib;
  };

public:
  std::vector<SgNode *> traversePreorder(SgNode *node) {
    rootNode = node;

    traverse(node);

    return nodes;
  };
};

LinearizeInheritedAttribute LinearizeAST::evaluateInheritedAttribute(
    SgNode *astNode, LinearizeInheritedAttribute inheritedAttribute) {
  if (inheritedAttribute.mustBeInherited == true)
    return inheritedAttribute;

  if (isSgBinaryOp(astNode) != NULL || isSgConditionalExp(astNode) != NULL ||
      isSgExprListExp(astNode) != NULL) {
    // This is needed when the root node that is given the traversal is part of
    // an expression
    inheritedAttribute.mustBeInherited = true;
    PostAST postAST;
    std::vector<SgNode *> inSubTree = postAST.traversePreorder(astNode);
    for (size_t i = 0; i < inSubTree.size(); i++)
      nodes.push_back(inSubTree[i]);

  } else {
    nodes.push_back(astNode);
  }

  return inheritedAttribute;
}

std::vector<SgNode *> LinearizeAST::get_linearization() { return nodes; }

std::vector<SgNode *> linearize_subtree(SgNode *node) {
  LinearizeInheritedAttribute inheritedAttribute;
  LinearizeAST myTraversal;

  myTraversal.traverse(node, inheritedAttribute);
  return myTraversal.get_linearization();
}
