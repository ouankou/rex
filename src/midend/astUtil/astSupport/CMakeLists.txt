
include_directories(${CMAKE_SOURCE_DIR}/src/util ${CMAKE_SOURCE_DIR}/src/util/support)


########### next target ###############


########### install files ###############

install(FILES  AnalysisInterface.h  ProcessAstTree.h  StmtInfoCollect.h LoopInfoInterface.h DESTINATION ${INCLUDE_INSTALL_DIR})
