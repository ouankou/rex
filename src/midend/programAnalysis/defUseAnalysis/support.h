/******************************************
 * Category: DFA
 * DefUse Analysis Declaration
 * created by tps in Feb 2007
 *****************************************/

#ifndef __DFAnalysis_support_HXX_LOADED__
#define __DFAnalysis_support_HXX_LOADED__
// #include "rose.h"
#include <string>

// A set of utility functions
class Support {
public:
  /**********************************************************
   *  Convert anything to a string
   *********************************************************/
  template <typename T> std::string ToString(T t) {
    std::ostringstream myStream; // creates an ostringstream object
    myStream << t << std::flush;
    return (
        myStream.str()); // returns the string form of the stringstream object
  }

  /**********************************************************
   *  Resolve Boolean Value to String
   *********************************************************/
  std::string resBool(bool val) {
    if (val)
      return "true";
    return "false";
  }

  /**********************************************************
   *  Check if an element is contained in a vector
   *********************************************************/
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

  /* *****************************************
   * retrieve a specific name for functionNodes: convert function parameters to
   * a string must be the same for all retrievals, so that analysis work.
   * *****************************************/
  // DQ (6/25/2011): Moved function definition to source file (function
  // definitions should not be in the header files).
  std::string getAppName(SgFunctionDeclaration *functionDeclaration);

  std::string getFileNameString(std::string src) { return src; }

  // DQ (6/25/2011): Moved function definition to source file (function
  // definitions should not be in the header files).
  std::string
  getFullName(SgFunctionDefinition
                  *functionDef); // qualified function name+ parameter list
};

#endif
