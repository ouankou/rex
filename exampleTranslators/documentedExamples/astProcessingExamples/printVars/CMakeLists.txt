
########### next target ###############

set(printVars_SRCS printVars.C MyTraversal.C)

add_executable(printVars ${printVars_SRCS})

install(TARGETS printVars DESTINATION bin)


########### install files ###############

install(FILES  MyTraversal.h DESTINATION ${INCLUDE_INSTALL_DIR})
