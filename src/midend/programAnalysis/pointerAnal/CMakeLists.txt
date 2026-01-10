
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs ${CMAKE_SOURCE_DIR}/src/midend/astUtil ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface)


########### next target ###############


########### install files ###############

install(FILES  steensgaard.h PtrAnal.h DESTINATION ${INCLUDE_INSTALL_DIR})
