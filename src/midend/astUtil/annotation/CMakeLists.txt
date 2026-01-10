
include_directories(${CMAKE_SOURCE_DIR}/src/util ${CMAKE_SOURCE_DIR}/src/util/support)


########### next target ###############


########### install files ###############

install(FILES  AnnotCollect.h      AnnotExpr.h  AnnotStmt.h           OperatorDescriptors.h     AnnotDescriptors.h  AnnotIO.h    OperatorAnnotation.h  TypeAnnotation.h     CPPAnnotation.h DESTINATION ${INCLUDE_INSTALL_DIR})
