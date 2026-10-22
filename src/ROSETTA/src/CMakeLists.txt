add_executable(CxxGrammarMetaProgram
  CxxGrammarMetaProgram.C
  grammar.C
  grammarString.C
  AstNodeClass.C
  type.C
  symbol.C
  expression.C
  statement.C
  support.C
  storageSize.C
  visitorSupport.C
  poolBasedTraversal.C
  node.C
  buildConstructorsWithoutSourcePositionInformation.C)

file(GLOB GRAMMAR_SOURCE_FILES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/*.code"
  "${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/*.macro")
list(APPEND GRAMMAR_SOURCE_FILES ../astNodeList)

target_compile_options(CxxGrammarMetaProgram PRIVATE -fexceptions)

target_link_libraries(CxxGrammarMetaProgram PRIVATE roseUtil)
if(REX_ENABLE_WASM)
  target_link_options(CxxGrammarMetaProgram PRIVATE
    "-sNODERAWFS=1"
    "-sALLOW_MEMORY_GROWTH=1")
else()
  target_link_libraries(CxxGrammarMetaProgram PRIVATE Threads::Threads)
endif()

# The generator reads ../Grammar relative to its build working directory.
# Output paths are supplied explicitly and unchanged files retain their times.
add_custom_command(
  OUTPUT ${ROSETTA_HEADERS} ${ROSETTA_SRC}
  DEPENDS CxxGrammarMetaProgram ${GRAMMAR_SOURCE_FILES}
  COMMAND ${CMAKE_COMMAND} -E rm -rf
    "${ROSE_TOP_BINARY_DIR}/src/ROSETTA/Grammar"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ROSE_TOP_SRC_DIR}/src/ROSETTA/Grammar"
    "${ROSE_TOP_BINARY_DIR}/src/ROSETTA/Grammar"
  COMMAND CxxGrammarMetaProgram "${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/"
  VERBATIM)

add_custom_target(rosetta_generated ALL DEPENDS ${ROSETTA_HEADERS} ${ROSETTA_SRC})
