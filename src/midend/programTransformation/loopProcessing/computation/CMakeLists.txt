
include_directories(${CMAKE_SOURCE_DIR}/src/util/support ${CMAKE_SOURCE_DIR}/src/util/graphs ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astSupport ${CMAKE_SOURCE_DIR}/src/midend/astUtil/astInterface ${CMAKE_SOURCE_DIR}/src/midend/astUtil/symbolicVal)


########### next target ###############


########### install files ###############

install(FILES  LoopTreeDummyNode.h  LoopTreeShadow.h LoopTreeLocality.h     LoopTreeBuild.h    LoopTree.h           LoopTreeObserver.h  LoopTreeTransform.h     LoopTreeDepComp.h  LoopTreeHoldNode.h    DepCompTransform.h  DESTINATION ${INCLUDE_INSTALL_DIR})
