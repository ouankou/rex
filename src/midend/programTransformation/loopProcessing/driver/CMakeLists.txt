
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface)


########### next target ###############


########### install files ###############

install(FILES  BlockingAnal.h  InterchangeAnal.h  CopyArrayAnal.h
LoopTransformOptions.h  LoopTransformInterface.h
FusionAnal.h  ParallelizeLoop.h AutoTuningInterface.h   DESTINATION ${INCLUDE_INSTALL_DIR})
