install(FILES
    analysis/analysis.h analysis/analysisCommon.h analysis/dataflow.h
    AnalysisDebuggingUtils.h
    cfgUtils/CallGraphTraverse.h
    cfgUtils/cfgUtils.h cfgUtils/DataflowCFG.h
    cfgUtils/VirtualCFGIterator.h genericDataflowCommon.h
    lattice/lattice.h
    lattice/latticeFull.h rwAccessLabeler/rwAccessLabeler.h
    simpleAnalyses/divAnalysis.h
    simpleAnalyses/liveDeadVarAnalysis.h
    simpleAnalyses/printAnalysisStates.h
    simpleAnalyses/taintAnalysis.h simpleAnalyses/VariableStateTransfer.h
    state/functionState.h state/nodeState.h
    variables/variables.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
