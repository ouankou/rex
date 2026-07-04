include_directories( ${ROSE_INCLUDES} )

########### next target ###############

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

# source files for the code generator
set(CxxGrammarMetaProgram_SRCS
  CxxGrammarMetaProgram.C
  grammar.C
  GrammarFile.C
  grammarString.C
  grammarGenerator.C
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
  buildConstructorsWithoutSourcePositionInformation.C
  outputClassesAndFields.C)

# input files to generate source files
set(GRAMMAR_SOURCE_FILES
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Node.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Statement.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarClassDefinitionMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarVariantFunctionDefinitionMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Common.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Support.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarConstructorDefinitionMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/node.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Cxx_GlobalDeclarations.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Symbol.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarDestructorDefinitionMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/parserCode.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Cxx_Support_Header.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Type.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarIsClassNameFunctionDefinitionMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/parserPrototype.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/Expression.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/copyMemberFunction.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarMainClassDeclatationMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/parserSourceCode.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/LocatedNode.code
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/dataMemberAccessFunctions.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarMainClassParseFunctionSourceCode.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarClassDeclarationMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarMainClassSourceCodeMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarNewDeleteOperatorMacros.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarTraverseMemoryPool.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarCheckingIfDataMembersAreInMemoryPool.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarReturnDataMemberPointers.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarProcessDataMemberReferenceToPointers.macro
  ${CMAKE_SOURCE_DIR}/src/ROSETTA/Grammar/grammarGetChildIndex.macro
  ../astNodeList)

add_executable(CxxGrammarMetaProgram ${CxxGrammarMetaProgram_SRCS})
add_dependencies(CxxGrammarMetaProgram roseUtil)

# build ROSETTA code generator
set(link_libs ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(CxxGrammarMetaProgram ${link_libs} roseUtil)
if(REX_ENABLE_WASM)
  target_link_options(CxxGrammarMetaProgram PRIVATE
    "-sNODERAWFS=1"
    "-sALLOW_MEMORY_GROWTH=1")
endif()

if(VERBOSE)
  message(STATUS "Smaller generated files disabled")
endif()

# define a custom command to call the code generator and generate source/header files
# Generate the Sage III grammar using ROSETTA
add_custom_command(
  OUTPUT ${ROSETTA_HEADERS} ${ROSETTA_SRC}
  # depend on the code generator and input files
  DEPENDS CxxGrammarMetaProgram ${GRAMMAR_SOURCE_FILES}
  # We need to copy the input files to the build tree since the code generator will output the generated source files
  # into the same directory as where the input files locate
  COMMAND ${CMAKE_COMMAND}  -E copy_directory  ${ROSE_TOP_SRC_DIR}/src/ROSETTA/Grammar ${ROSE_TOP_BINARY_DIR}/src/ROSETTA/Grammar
  # create this directory is needed since a few generated files will be put there
  # COMMAND echo  "META_EXE_LOC =  ${META_EXE_LOC} ARGS ${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/"
  # STRING(REGEX REPLACE "\\\\" "/" META_EXE_LOC ${META_EXE_LOC} )
  # COMMAND echo  "META_EXE_LOC =  ${META_EXE_LOC} ARGS ${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/"
  COMMAND CxxGrammarMetaProgram
  ARGS ${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/)

# a custom target depending on generated rosetta source and header files to trigger the building process
add_custom_target(rosetta_generated ALL DEPENDS ${ROSETTA_HEADERS} ${ROSETTA_SRC} ${STORAGECLASSES_SRC})
add_dependencies(rosetta_generated generate_rosePublicConfig)
