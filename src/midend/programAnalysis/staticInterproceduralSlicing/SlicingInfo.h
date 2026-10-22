#ifndef _SLICING_INFO_H_
#define _SLICING_INFO_H_

#include "AstSimpleProcessing.h"

#include <list>

class ROSE_DLL_API SlicingInfo : public AstSimpleProcessing {
public:
  std::list<SgNode *> getSlicingTargets() { return targets; }

protected:
  void visit(SgNode *node) override;

private:
  std::list<SgNode *> targets;
};

#endif
