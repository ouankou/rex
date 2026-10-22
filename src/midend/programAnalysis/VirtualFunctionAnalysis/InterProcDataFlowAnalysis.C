#include "InterProcDataFlowAnalysis.h"

#include "sage3basic.h"

void InterProcDataFlowAnalysis::run() {
  bool change = false;

  do {
    change = false;

    std::vector<SgFunctionDeclaration *> processingOrder;

    getFunctionDeclarations(processingOrder);

    for (SgFunctionDeclaration *funcDecl : processingOrder) {
      change |= runAndCheckIntraProcAnalysis(funcDecl);
    }
  } while (change);
}
