add_custom_target(tools)

add_subdirectory(checkFortranInterfaces)
add_subdirectory(classMemberVariablesInLambdas)
add_subdirectory(featureVector)
add_subdirectory(globalVariablesInLambdas)

add_executable(tools_moveDeclarationToInnermostScope_exe
               moveDeclarationToInnermostScope.C)

target_link_libraries(tools_moveDeclarationToInnermostScope_exe ROSE_DLL)

target_include_directories(
  tools_moveDeclarationToInnermostScope_exe
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/transformationTracking
          ${ROSE_INCLUDES})

set_target_properties(tools_moveDeclarationToInnermostScope_exe
                      PROPERTIES OUTPUT_NAME "moveDeclarationToInnermostScope")

install(TARGETS tools_moveDeclarationToInnermostScope_exe RUNTIME DESTINATION bin)

add_dependencies(tools tools_moveDeclarationToInnermostScope_exe)
