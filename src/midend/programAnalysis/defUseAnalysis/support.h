/******************************************
 * Category: DFA
 * DefUse Analysis Declaration
 * created by tps in Feb 2007
 *****************************************/

#ifndef __DFAnalysis_support_HXX_LOADED__
#define __DFAnalysis_support_HXX_LOADED__

#include <string>

class Support {
public:
  template <typename T> std::string ToString(T t) {
    std::ostringstream myStream;
    myStream << t << std::flush;
    return (myStream.str());
  }

  std::string resBool(bool val) {
    if (val)
      return "true";
    return "false";
  }

  template <typename T>
  bool isContainedinVector(T filterNode, std::vector<T> worklist) {
    bool contained = false;
    for (typename std::vector<T>::const_iterator l = worklist.begin();
         l != worklist.end(); ++l) {
      T aNode = *l;
      if (aNode == filterNode)
        contained = true;
    }
    return contained;
  }

  bool shouldAnalyzeFunction(SgFunctionDefinition *functionDef);
};

#endif
