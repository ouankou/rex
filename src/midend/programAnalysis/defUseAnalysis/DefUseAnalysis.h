/******************************************
 * Category: DFA
 * DefUse Analysis Declaration
 * created by tps in Feb 2007
 *****************************************/

#ifndef __DefUseAnalysis_HXX_LOADED__
#define __DefUseAnalysis_HXX_LOADED__
#include <string>

#include "DFAFilter.h"

#include "DFAnalysis.h"

#include "filteredCFG.h"

#include "support.h"

#include <iostream>

class ROSE_DLL_API DefUseAnalysis : public DFAnalysis, Support {
private:
  SgProject *project;
  bool DEBUG_MODE;
  std::vector<SgInitializedName *> globalVarList;

  // def-use-specific --------------------
  typedef std::vector<std::pair<SgInitializedName *, SgNode *>> multitype;

  typedef std::map<SgNode *, multitype> tabletype;
  typedef rose_hash::unordered_map<SgNode *, int> convtype;

  // local functions ---------------------
  void find_all_global_variables();
  bool start_traversal_of_functions();
  bool searchMap(const tabletype *ltable, SgNode *node);
  bool searchVizzMap(SgNode *node);
  std::string getInitName(SgNode *sgNode);

  // the main table of all entries
  tabletype table;    // definition table
  tabletype usetable; // use table
  // the helper table for visualization
  convtype vizzhelp;
  static int sgNodeCounter;
  int nrOfNodesVisited;

  // functions to be printed in DFAtoDOT
  std::vector<VirtualCFG::FilteredCFGNode<IsDFAFilter>> dfaFunctions;

  void addAnyElement(tabletype *tabl, SgNode *sgNode,
                     SgInitializedName *initName, SgNode *defNode);
  void mapAnyUnion(tabletype *tabl, SgNode *before, SgNode *other,
                   SgNode *current); // current = before Union other
  void printAnyMap(tabletype *tabl);

public:
  DefUseAnalysis(SgProject *proj) : project(proj), DEBUG_MODE(false) {};
  virtual ~DefUseAnalysis() {}

  std::map<SgNode *, multitype> getDefMap() { return table; }
  std::map<SgNode *, multitype> getUseMap() { return usetable; }
  void setMaps(std::map<SgNode *, multitype> def,
               std::map<SgNode *, multitype> use) {
    table = def;
    usetable = use;
  }

  // def-use-public-functions -----------
  int run();
  int run(bool debug);
  multitype getDefMultiMapFor(SgNode *node);
  multitype getUseMultiMapFor(SgNode *node);
  std::vector<SgNode *> getAnyFor(const multitype *mul,
                                  SgInitializedName *initName);
  // for any given node, return all definitions of initName
  std::vector<SgNode *> getDefFor(SgNode *node, SgInitializedName *initName);
  std::vector<SgNode *> getUseFor(SgNode *node, SgInitializedName *initName);
  bool isNodeGlobalVariable(SgInitializedName *node);
  std::vector<SgInitializedName *> getGlobalVariables();
  // the following one is used for parallel traversal
  int start_traversal_of_one_function(SgFunctionDefinition *proc);

  // helpers -----------------------------
  bool searchMap(SgNode *node);
  int getDefSize();
  int getUseSize();
  void printMultiMap(const multitype *type);
  void printMultiMap(const multitype &type);
  void printDefMap();
  void printUseMap();

  bool addID(SgNode *sgNode);
  void addDefElement(SgNode *sgNode, SgInitializedName *initName,
                     SgNode *defNode);
  void addUseElement(SgNode *sgNode, SgInitializedName *initName,
                     SgNode *defNode);
  void replaceElement(SgNode *sgNode, SgInitializedName *initName);
  void mapDefUnion(SgNode *before, SgNode *other, SgNode *current);
  void mapUseUnion(SgNode *before, SgNode *other, SgNode *current);

  void clearUseOfElement(SgNode *sgNode, SgInitializedName *initName);

  int getIntForSgNode(SgNode *node);
  void dfaToDOT();

  // clear the tables if necessary
  void flush() {
    table.clear();
    usetable.clear();
    globalVarList.clear();
    vizzhelp.clear();
    sgNodeCounter = 1;
  }

  void flushHelp() {
    vizzhelp.clear();
    sgNodeCounter = 1;
  }

  void flushDefuse() {
    table.clear();
    usetable.clear();
  }
};

#endif
