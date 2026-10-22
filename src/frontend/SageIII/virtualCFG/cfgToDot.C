#include "sage3basic.h"

#include <fstream>

using namespace std;

namespace VirtualCFG {
ostream &cfgToDot(ostream &o, string graphName, CFGNode start) {
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<CFGNode, CFGEdge, false> impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

//! dump the filtered dot graph of a virtual control flow graph starting from
//! SgNode (start)
void cfgToDot(SgNode *start, const std::string &file_name) {
  ROSE_ASSERT(start != NULL);
  ofstream ofile(file_name.c_str(), ios::out);
  cfgToDot(ofile, "defaultName", start->cfgForBeginning());
}

//! Dump a CFG with only interesting nodes for a SgNode
void interestingCfgToDot(SgNode *start, const std::string &file_name) {
  ROSE_ASSERT(start != NULL);
  ofstream ofile(file_name.c_str(), ios::out);
  cfgToDot(ofile, "defaultName", makeInterestingCfg(start));
}

ostream &cfgToDot(ostream &o, string graphName, InterestingNode start) {
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<InterestingNode, InterestingEdge, false> impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

ostream &cfgToDotForDebugging(ostream &o, string graphName, CFGNode start) {

  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<CFGNode, CFGEdge, true> impl(o);
  impl.processNodes(start);
  o << "}\n";

  return o;
}

// dump the full dot graph of a virtual control flow graph starting from SgNode
// (start)
void cfgToDotForDebugging(SgNode *start, const std::string &file_name) {
  ROSE_ASSERT(start != NULL);
  ofstream ofile(file_name.c_str(), ios::out);
  cfgToDotForDebugging(ofile, "defaultName", start->cfgForBeginning());
}

ostream &cfgToDotForDebugging(ostream &o, string graphName,
                              InterestingNode start) {
  o << "digraph " << graphName << " {\n";
  CfgToDotImpl<InterestingNode, InterestingEdge, true> impl(o);
  impl.processNodes(start);
  o << "}\n";
  return o;
}

} // namespace VirtualCFG
