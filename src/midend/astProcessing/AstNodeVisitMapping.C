
// Author: Markus Schordan
#include "AstNodeVisitMapping.h"

#include "sage3basic.h"

void AstNodeVisitMapping::visit(SgNode *node) {
  address_pagenum.emplace(node, pagenum);
  pagenum++;
}
