
include_directories(${CMAKE_SOURCE_DIR}/src/util ${CMAKE_SOURCE_DIR}/src/util/support)


########### next target ###############


########### install files ###############

install(FILES  AstInterface.h  CPPAstInterface.h AstInterface_ROSE.h DESTINATION ${INCLUDE_INSTALL_DIR})
