#ifndef LINEARIZE_AST_H
#define LINEARIZE_AST_H

#include <vector>

class LinearizeInheritedAttribute {
public:
  bool mustBeInherited = false;
};

class LinearizeAST : public SgTopDownProcessing<LinearizeInheritedAttribute> {
private:
  std::vector<SgNode *> nodes;

  LinearizeInheritedAttribute
  evaluateInheritedAttribute(SgNode *astNode,
                             LinearizeInheritedAttribute inheritedAttribute);

public:
  std::vector<SgNode *> get_linearization();
};

std::vector<SgNode *> linearize_subtree(SgNode *node);

#endif
