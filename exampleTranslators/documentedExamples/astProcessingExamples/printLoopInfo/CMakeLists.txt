
########### next target ###############

set(printLoopInfo_SRCS printLoopInfo.C LoopNestingInfoProcessing.C)

add_executable(printLoopInfo ${printLoopInfo_SRCS})

install(TARGETS printLoopInfo DESTINATION bin)


########### install files ###############

install(FILES  LoopNestingInfoProcessing.h DESTINATION ${INCLUDE_INSTALL_DIR})
