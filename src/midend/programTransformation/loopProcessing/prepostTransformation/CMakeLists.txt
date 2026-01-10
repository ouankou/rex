
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/bitvectorDataflow ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/CFG ${CMAKE_SOURCE_DIR}/src/util/graphs)


########### next target ###############


########### install files ###############

install(FILES  BreakupStmt.h  LoopUnroll.h  PrePostTransformation.h PrivatizeScalar.h DESTINATION ${INCLUDE_INSTALL_DIR})
