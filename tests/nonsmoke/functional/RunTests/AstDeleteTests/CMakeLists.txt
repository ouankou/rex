
include_directories(${ROSE_INCLUDES})


########### next target ###############

add_executable(memoryTranslator test.C)
target_link_libraries(memoryTranslator ROSE_DLL ${link_with_libraries})

install(TARGETS memoryTranslator DESTINATION bin)

add_test(
  NAME ast_delete_memory_translator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND memoryTranslator -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test1.C
)


########### install files ###############
