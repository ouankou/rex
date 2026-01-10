
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface)


########### next target ###############


########### install files ###############

install(FILES  CompSliceDepGraph.h  CompSliceImpl.h      DynamicCompSlice.h CompSliceObserver.h                     CompSlice.h          CompSliceLocality.h  CompSliceRegistry.h DESTINATION ${INCLUDE_INSTALL_DIR})
