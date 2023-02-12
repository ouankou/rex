
set(TESTCODES
  findMain.C
)

# Define include directories for test executables
set(TESTCODE_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/util
  ${CMAKE_SOURCE_DIR}/src/util/StringUtility
  ${CMAKE_SOURCE_DIR}/src/util/commandlineProcessing
  ${CMAKE_SOURCE_DIR}/src/frontend
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/virtualCFG
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/sage_support
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/sageInterface
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astPostProcessing
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astFixup
  ${CMAKE_SOURCE_DIR}/src/frontend/SageIII/astVisualization
  ${CMAKE_SOURCE_DIR}/src/midend/astProcessing
  ${CMAKE_SOURCE_DIR}/src/midend/astQuery
  ${CMAKE_SOURCE_DIR}/src/midend/astDiagnostics
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/astInlining
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/constantFolding
  ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/defUseAnalysis
  ${CMAKE_SOURCE_DIR}/src/midend/programAnalysis/CallGraphAnalysis
  ${CMAKE_SOURCE_DIR}/src/backend/unparser
  ${CMAKE_SOURCE_DIR}/src/backend/unparser/languageIndependenceSupport
  ${CMAKE_SOURCE_DIR}/src/backend/unparser/formatSupport
  ${CMAKE_SOURCE_DIR}/src/backend/unparser/CxxCodeGeneration
  ${CMAKE_SOURCE_DIR}/src/backend/unparser/FortranCodeGeneration
  ${CMAKE_SOURCE_DIR}/src/3rdPartyLibraries/MSTL
  ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII
)

foreach(file_to_test ${TESTCODES})
  # Extract test name without extension
  get_filename_component(test_name ${file_to_test} NAME_WE)

  # Build executable target
  add_executable(${test_name} ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})

  # Set include directories
  target_include_directories(${test_name} PRIVATE ${TESTCODE_INCLUDE_DIRS})

  # Link against ROSE libraries
  target_link_libraries(${test_name} PRIVATE ROSE_DLL ${link_with_libraries})

  # Add test that runs the built executable with input file
  add_test(
    NAME ASTINFTEST_${file_to_test}
    COMMAND $<TARGET_FILE:${test_name}> -c ${CMAKE_CURRENT_SOURCE_DIR}/input${test_name}.C
  )

  set_tests_properties(ASTINFTEST_${file_to_test} PROPERTIES LABELS ASTINFTEST)
endforeach()
