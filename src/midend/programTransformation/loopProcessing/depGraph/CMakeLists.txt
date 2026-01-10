
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal)


########### next target ###############


########### install files ###############

install(FILES  DepGraphBuild.h  DepGraph.h  DepGraphTransform.h  LoopAnalysis.h  TransDepGraph.h TransDepGraphImpl.h DESTINATION ${INCLUDE_INSTALL_DIR})
