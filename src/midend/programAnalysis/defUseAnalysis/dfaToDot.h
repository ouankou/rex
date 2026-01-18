#ifndef DFA_TO_DOT_H
#define DFA_TO_DOT_H

// #include "rose.h"

#include <cctype>

#include <iomanip>

#include <iostream>

#include <map>

#include <set>

#include <sstream>

#include <stdint.h>

#include <string>

#include "DefUseAnalysis.h"

#include "filteredCFG.h"

class LivenessAnalysis;

namespace VirtualCFG {

template <typename FilterFunction>
ROSE_DLL_API std::ostream &
dfaToDot(std::ostream &o, std::string graphName,
         std::vector<FilteredCFGNode<FilterFunction>> start,
         DefUseAnalysis *dfa);
template <typename FilterFunction>
ROSE_DLL_API std::ostream &
dfaToDot(std::ostream &o, std::string graphName,
         std::vector<FilteredCFGNode<FilterFunction>> start,
         DefUseAnalysis *dfa, LivenessAnalysis *live);
} // namespace VirtualCFG
#endif
