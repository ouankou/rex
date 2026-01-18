#ifndef TYPED_FUSION
#define TYPED_FUSION

#include "rosedll.h"
#include <GraphAccess.h>

class TypedFusionOperator {
public:
  virtual int GetNodeType(GraphAccessInterface::Node *n) = 0;
  virtual void MarkFuseNodes(GraphAccessInterface::Node *n1,
                             GraphAccessInterface::Node *n2) = 0;
  virtual bool PreventFusion(GraphAccessInterface::Node *src,
                             GraphAccessInterface::Node *snk,
                             GraphAccessInterface::Edge *e) = 0;
  virtual ~TypedFusionOperator() {}
};

class ROSE_UTIL_API TypedFusion {
public:
  void operator()(GraphAccessInterface *dg, TypedFusionOperator &op,
                  int fusetype);
};

#endif
