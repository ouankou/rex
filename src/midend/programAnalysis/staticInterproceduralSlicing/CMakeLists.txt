
include_directories(${CMAKE_SOURCE_DIR}/src/midend/loopProcessing ${CMAKE_SOURCE_DIR}/src/midend/astUtil ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/midend/astUtil/annotation ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs)


########### next target ###############


########### install files ###############

install(FILES  DebugTool.h  DefUseExtension.h  DependenceGraph.h  InterproceduralInfo.h  SimpleDirectedGraph.h  SlicingInfo.h EDefUse.h SDGLibraryExtender.h CreateSlice.h CreateSliceSet.h ControlFlowGraph.h DESTINATION ${INCLUDE_INSTALL_DIR})
