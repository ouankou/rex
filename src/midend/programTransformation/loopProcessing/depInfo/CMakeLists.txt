
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal ${CMAKE_SOURCE_DIR}/src/midend/loopProcessing/driver)


########### next target ###############


########### install files ###############

install(FILES  DepEDD.h       DepInfo.h     DepInfoUpdate.h  DomainInfo.h                     DepInfoAnal.h  DepInfoSet.h  DepRel.h   DomainInfoUpdate.h                     ReuseAnalysis.h StmtDepAnal.h DESTINATION ${INCLUDE_INSTALL_DIR})
