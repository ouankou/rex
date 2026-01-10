
include_directories(${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/OAWrap ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis)


########### next target ###############


########### install files ###############

install(FILES  OAWrap.h SAGE2OA.h OAConfig.h DESTINATION ${INCLUDE_INSTALL_DIR})
