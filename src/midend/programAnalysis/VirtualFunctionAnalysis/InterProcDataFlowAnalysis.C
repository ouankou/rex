#include "InterProcDataFlowAnalysis.h"

#include "sage3basic.h"

#include <iostream>

void InterProcDataFlowAnalysis::run() {
  bool change = false;
  int iteration = 0;

  do {
    ++iteration;
    change = false;

    std::vector<SgFunctionDeclaration *> processingOrder;

    getFunctionDeclarations(processingOrder);

    for (SgFunctionDeclaration *funcDecl : processingOrder) {
      change |= runAndCheckIntraProcAnalysis(funcDecl);
    }
  } while (change);

  std::cout << "Total Interprocedural iterations: " << iteration << std::endl;
}
