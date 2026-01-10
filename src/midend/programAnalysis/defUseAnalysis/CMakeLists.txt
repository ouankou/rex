
include_directories(${CMAKE_SOURCE_DIR}/src/midend/loopProcessing ${CMAKE_SOURCE_DIR}/src/midend/astUtil ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/midend/astUtil/annotation ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs)


########### next target ###############


########### install files ###############

install(FILES  DefUseAnalysis.h  BottomUpTraversalLiveness.h DefUseAnalysis_perFunction.h  DFAFilter.h  DFAnalysis.h  dfaToDot.h  GlobalVarAnalysis.h  support.h LivenessAnalysis.h DefUseAnalysisAbstract.h DESTINATION ${INCLUDE_INSTALL_DIR})
