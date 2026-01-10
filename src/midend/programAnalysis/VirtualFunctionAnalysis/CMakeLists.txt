
include_directories(${CMAKE_SOURCE_DIR}/src/util ${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs ${CMAKE_SOURCE_DIR}/src/midend/astUtil ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/util/support)


########### next target ###############


########### install files ###############

install(FILES  VirtualFunctionAnalysis.h PtrAliasAnalysis.h IntraProcAliasAnalysis.h IntraProcDataFlowAnalysis.h InterProcDataFlowAnalysis.h DESTINATION ${INCLUDE_INSTALL_DIR})
