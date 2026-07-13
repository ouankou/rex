
/* Unparser debug-output declarations. */

#ifndef UNPARSER_DEBUG
#define UNPARSER_DEBUG

#include "unparser.h"

class Unparser;

class Unparse_Debug {
private:
  Unparser *unp;

public:
  Unparse_Debug(Unparser *unp) : unp(unp) {};
  virtual ~Unparse_Debug() {};

  void printDebugInfo(int, bool);
  void printDebugInfo(const std::string &, bool);
};

#endif
