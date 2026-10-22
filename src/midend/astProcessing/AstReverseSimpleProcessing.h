#ifndef ASTSIMPLEREVERSEPROCESSING_H
#define ASTSIMPLEREVERSEPROCESSING_H

#include "AstReverseProcessing.h"

class AstReversePrefixSimpleProcessing : public AstSimpleProcessing {
public:
  AstReversePrefixSimpleProcessing();

protected:
  virtual void setNodeSuccessors(SgNode *node,
                                 SuccessorsContainer &succContainer);
};

class AstReverseBranchSimpleProcessing : public AstSimpleProcessing {
public:
  AstReverseBranchSimpleProcessing();

protected:
  virtual void setNodeSuccessors(SgNode *node,
                                 SuccessorsContainer &succContainer);
};

#endif
