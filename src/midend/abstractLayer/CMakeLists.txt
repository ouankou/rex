
add_library(abstractLayer OBJECT RoseAst.C)

add_dependencies(abstractLayer rosetta_generated)

install(FILES RoseAst.h DESTINATION ${INCLUDE_INSTALL_DIR})
