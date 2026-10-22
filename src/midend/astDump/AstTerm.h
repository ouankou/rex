#ifndef ASTTERM_H
#define ASTTERM_H

#include <string>

class SgNode;

class AstTerm {
public:
  static std::string astTermWithNullValuesToString(SgNode *node);
};

#endif
