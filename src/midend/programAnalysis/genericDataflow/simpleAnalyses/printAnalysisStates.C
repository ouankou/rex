#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "printAnalysisStates.h"

using namespace VirtualCFG;

printAnalysisStates::printAnalysisStates(Analysis *creator,
                                         vector<int> &factNames,
                                         vector<int> &latticeNames, ab latSide,
                                         string indent = "") {
  this->creator = creator;
  this->factNames = factNames;
  this->latticeNames = latticeNames;
  this->latSide = latSide;
  this->indent = indent;
}

void printAnalysisStates::visit(const Function &func, const DataflowNode &n,
                                NodeState &state) {
  ostringstream funcName;
  funcName << "function " << func.get_name().getString() << "() node=["
           << n.getNode()->class_name() << " | "
           << Dbg::escape(Dbg::diagnosticNodeText(n.getNode())) << " | "
           << n.getIndex() << "] state=" << (&state) << " n=" << (&n)
           << " sgn=" << n.getNode() << " creator=" << creator;
  Dbg::enterFunc(funcName.str());
  for (vector<int>::iterator it = factNames.begin(); it != factNames.end();
       it++) {
    NodeFact *fact = state.getFact(creator, *it);
    if (fact)
      Dbg::dbg << indent << "    fact" << *it << " = \n"
               << fact->str(indent + "    ") << "\n";
    else
      Dbg::dbg << indent << "    fact" << *it << " = None\n";
  }

  for (vector<int>::iterator it = latticeNames.begin();
       it != latticeNames.end(); it++) {
    Lattice *lat;
    if (latSide == above)
      lat = state.getLatticeAbove(creator, *it);
    else
      lat = state.getLatticeBelow(creator, *it);
    if (lat)
      Dbg::dbg << indent << "    lattice" << *it << " = \n"
               << lat->str(indent + "    ") << "\n";
    else
      Dbg::dbg << indent << "    lattice" << *it << " = None\n";
  }
  Dbg::exitFunc(funcName.str());
}

#endif
