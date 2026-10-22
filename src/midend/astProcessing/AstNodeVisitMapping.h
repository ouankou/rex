// Author: Markus Schordan

#ifndef ASTNODEVISITMAPPING_H
#define ASTNODEVISITMAPPING_H

#include "AstProcessing.h"

#include <map>

class AstNodeVisitMapping : public SgSimpleProcessing {
public:
  AstNodeVisitMapping(int pn) : pagenum(pn) {}
  typedef std::map<const SgNode *, int> MappingType;
  MappingType address_pagenum;
  int pagenum;
  void visit(SgNode *node);
};

#endif
