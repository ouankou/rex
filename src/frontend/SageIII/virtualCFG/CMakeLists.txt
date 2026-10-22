add_library(virtualCFG OBJECT
  virtualCFG.C cfgToDot.C memberFunctions.C staticCFG.C
  interproceduralCFG.C)
add_dependencies(virtualCFG rosetta_generated)

install(
  FILES virtualCFG.h staticCFG.h cfgToDot.h filteredCFG.h
        filteredCFGImpl.h customFilteredCFG.h interproceduralCFG.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
