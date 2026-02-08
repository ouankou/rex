include_directories(${ROSE_INCLUDES})

set(_functional_include_flags "")
foreach(include ${ROSE_INCLUDES})
  list(APPEND _functional_include_flags "-I${include}")
endforeach()

set(REX_FRONTEND_ERROR_TESTS
  rex_test2025_cfe_error_exit_code.cpp
  # Issue 99: Friend function template lookup requires error-tolerant mode.
  rex_test2025_issue99.cpp
)

add_executable(testTranslator testTranslator.C)
add_dependencies(testTranslator roseUtil)
target_link_libraries(testTranslator ROSE_DLL ${link_with_libraries})

add_executable(testGraphGeneration testGraphGeneration.C)
target_link_libraries(testGraphGeneration ROSE_DLL ${link_with_libraries})

add_executable(testTokenGeneration testTokenGeneration.C)
target_link_libraries(testTokenGeneration ROSE_DLL ${link_with_libraries})

add_executable(testAnalysis testAnalysis.C)
target_link_libraries(testAnalysis ROSE_DLL ${link_with_libraries})

add_executable(testParser testParser.C)
target_link_libraries(testParser ROSE_DLL ${link_with_libraries})

add_executable(testFrontEnd testFrontEnd.C)
target_link_libraries(testFrontEnd ROSE_DLL ${link_with_libraries})

add_executable(testCodeGeneration testCodeGeneration.C)
target_link_libraries(testCodeGeneration ROSE_DLL ${link_with_libraries})

add_executable(testTranslatorUnfoldedConstants testTranslatorUnfoldedConstants.C)
target_link_libraries(testTranslatorUnfoldedConstants ROSE_DLL ${link_with_libraries})

add_executable(testTranslatorFoldedConstants testTranslatorFoldedConstants.C)
target_link_libraries(testTranslatorFoldedConstants ROSE_DLL ${link_with_libraries})

add_executable(testTemplates testTemplates.C)
target_link_libraries(testTemplates ROSE_DLL ${link_with_libraries})

add_executable(testBackend testBackend.C)
target_link_libraries(testBackend ROSE_DLL ${link_with_libraries})

add_executable(readFileTwice readFileTwice.C)
target_link_libraries(readFileTwice ROSE_DLL ${link_with_libraries})

add_executable(testPDFGeneration testPDFGeneration.C)
target_link_libraries(testPDFGeneration ROSE_DLL ${link_with_libraries})

option(with-ROSE_LONG_MAKE_CHECK_RULE "Specify longer internal testing" ON)

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; functional tests require bash")
endif()

set(_functional_lock functional_translator_outputs)
set(_functional_input ${CMAKE_CURRENT_SOURCE_DIR}/inputFile.C)
set(_functional_minimal_input
  ${CMAKE_CURRENT_SOURCE_DIR}/input_codes/minimal/minimal.cpp)
if(NOT ROSE_BUILD_C_LANGUAGE_SUPPORT AND ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT)
  set(_functional_minimal_input
    ${CMAKE_CURRENT_SOURCE_DIR}/input_codes/minimal/minimal.f90)
endif()

if(ROSE_BUILD_C_LANGUAGE_SUPPORT)
  string(CONCAT _rex_install_cfe_command
    "set -e;"
    " install_dir=$(mktemp -d \"${CMAKE_CURRENT_BINARY_DIR}/rex-install-XXXXXX\");"
    " run_dir=$(mktemp -d \"${CMAKE_CURRENT_BINARY_DIR}/rex-install-run-XXXXXX\");"
    " cmake --install \"${CMAKE_BINARY_DIR}\" --prefix \"$install_dir\";"
    " cd \"$run_dir\";"
    " \"$install_dir/bin/rose-compiler\" -rose:skipfinalCompileStep \"${_functional_minimal_input}\";"
  )
  add_test(
    NAME rex_install_tree_clang_headers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c "${_rex_install_cfe_command}"
  )
  set_tests_properties(rex_install_tree_clang_headers PROPERTIES RESOURCE_LOCK ${_functional_lock})

  set(_astquery_input
    ${CMAKE_CURRENT_SOURCE_DIR}/roseTests/astQueryTests/input1.C)
  string(CONCAT _rex_install_astquery_command
    "set -e;"
    " install_dir=$(mktemp -d \"${CMAKE_CURRENT_BINARY_DIR}/rex-install-XXXXXX\");"
    " run_dir=$(mktemp -d \"${CMAKE_CURRENT_BINARY_DIR}/rex-install-run-XXXXXX\");"
    " cmake --install \"${CMAKE_BINARY_DIR}\" --prefix \"$install_dir\";"
    " cd \"$run_dir\";"
    " \"$install_dir/bin/testQuery\" -c \"${_astquery_input}\";"
  )
  add_test(
    NAME rex_install_tree_astQuery
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c "${_rex_install_astquery_command}"
  )
  set_tests_properties(rex_install_tree_astQuery PROPERTIES RESOURCE_LOCK ${_functional_lock})

  set(_astinterface_input
    ${CMAKE_CURRENT_SOURCE_DIR}/roseTests/astInterfaceTests/inputloopInterchange.C)
  string(CONCAT _rex_install_astinterface_command
    "set -e;"
    " install_dir=$(mktemp -d \"${CMAKE_CURRENT_BINARY_DIR}/rex-install-XXXXXX\");"
    " run_dir=$(mktemp -d \"${CMAKE_CURRENT_BINARY_DIR}/rex-install-run-XXXXXX\");"
    " cmake --install \"${CMAKE_BINARY_DIR}\" --prefix \"$install_dir\";"
    " cd \"$run_dir\";"
    " \"$install_dir/bin/loopInterchange\" -rose:loopInterchange:line 5 -rose:loopInterchange:depth 4 -rose:loopInterchange:order 1 -c \"${_astinterface_input}\";"
  )
  add_test(
    NAME rex_install_tree_astInterface
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c "${_rex_install_astinterface_command}"
  )
  set_tests_properties(rex_install_tree_astInterface PROPERTIES RESOURCE_LOCK ${_functional_lock})
endif()

set(_keep_going_cmd $<TARGET_FILE:KeepGoingTranslator>)
set(_keep_going_missing FALSE)
if(NOT TARGET KeepGoingTranslator)
  set(_keep_going_cmd KeepGoingTranslator)
  set(_keep_going_missing TRUE)
endif()

set(_identity_translator_cmd $<TARGET_FILE:identityTranslator>)
set(_identity_translator_missing FALSE)
if(NOT TARGET identityTranslator)
  set(_identity_translator_cmd identityTranslator)
  set(_identity_translator_missing TRUE)
endif()

add_test(
  NAME functional_testObjectFileAnalysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputObjectFileAnalysis.C alt_Analysis_inputFile.o; cp ${_functional_input} inputObjectFileAnalysis.C && $<TARGET_FILE:testAnalysis> -c inputObjectFileAnalysis.C -o alt_Analysis_inputFile.o"
)
set_tests_properties(functional_testObjectFileAnalysis PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testLinkFileAnalysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testAnalysis> alt_Analysis_inputFile.o -o testLinkFileAnalysisExecutable
)
set_tests_properties(functional_testLinkFileAnalysis PROPERTIES DEPENDS functional_testObjectFileAnalysis RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testCppFileAnalysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testAnalysis> -E ${_functional_input}
)
set_tests_properties(functional_testCppFileAnalysis PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testExecutableFileAnalysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputFileAnalysis.C testExecutableFileAnalysisExecutable; cp ${_functional_input} inputFileAnalysis.C && $<TARGET_FILE:testAnalysis> inputFileAnalysis.C -o testExecutableFileAnalysisExecutable"
)
set_tests_properties(functional_testExecutableFileAnalysis PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testObjectFileCodeGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputObjectFileCodeGeneration.C alt_CodeGeneration_inputFile.o; cp ${_functional_input} inputObjectFileCodeGeneration.C && $<TARGET_FILE:testCodeGeneration> -c inputObjectFileCodeGeneration.C -o alt_CodeGeneration_inputFile.o"
)
set_tests_properties(functional_testObjectFileCodeGeneration PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testLinkFileCodeGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testCodeGeneration> -rose:Cxx alt_CodeGeneration_inputFile.o -o testLinkFileCodeGenerationExecutable
)
set_tests_properties(functional_testLinkFileCodeGeneration PROPERTIES DEPENDS functional_testObjectFileCodeGeneration RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testCppFileCodeGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testCodeGeneration> -E ${_functional_input}
)
set_tests_properties(functional_testCppFileCodeGeneration PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testExecutableFileCodeGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputFileCodeGeneration.C testExecutableFileCodeGenerationExecutable; cp ${_functional_input} inputFileCodeGeneration.C && $<TARGET_FILE:testCodeGeneration> inputFileCodeGeneration.C -o testExecutableFileCodeGenerationExecutable"
)
set_tests_properties(functional_testExecutableFileCodeGeneration PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testObjectFileTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputObjectFileTranslator.C alt_Translator_inputFile.o; cp ${_functional_input} inputObjectFileTranslator.C && $<TARGET_FILE:testTranslator> -c inputObjectFileTranslator.C -o alt_Translator_inputFile.o"
)
set_tests_properties(functional_testObjectFileTranslator PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testLinkFileTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator> alt_Translator_inputFile.o -o testLinkFileTranslatorExecutable
)
set_tests_properties(functional_testLinkFileTranslator PROPERTIES DEPENDS functional_testObjectFileTranslator RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testCppFileTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator> -E ${_functional_include_flags} ${_functional_input}
)
set_tests_properties(functional_testCppFileTranslator PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testExecutableFileTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputFileTranslator.C testExecutableFileTranslatorExecutable; cp ${_functional_input} inputFileTranslator.C && $<TARGET_FILE:testTranslator> inputFileTranslator.C -o testExecutableFileTranslatorExecutable"
)
set_tests_properties(functional_testExecutableFileTranslator PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testSimpleLinkFileTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f inputSimpleLinkFileTranslator.C; cp ${_functional_input} inputSimpleLinkFileTranslator.C && $<TARGET_FILE:testTranslator> inputSimpleLinkFileTranslator.C -lm"
)
set_tests_properties(functional_testSimpleLinkFileTranslator PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testObjectFileTokenGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f alt_ObjectFileTokenGeneration_inputFile.C alt_TokenGeneration_inputFile.o; cp ${_functional_input} alt_ObjectFileTokenGeneration_inputFile.C && $<TARGET_FILE:testTokenGeneration> -c alt_ObjectFileTokenGeneration_inputFile.C -o alt_TokenGeneration_inputFile.o"
)
set_tests_properties(functional_testObjectFileTokenGeneration PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testGraphGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testGraphGeneration> -c ${_functional_input}
)
set_tests_properties(functional_testGraphGeneration PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testPDFGeneration
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testPDFGeneration> ${_functional_input}
)
set_tests_properties(functional_testPDFGeneration PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testTemplates
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTemplates> -c ${_functional_input}
)
set_tests_properties(functional_testTemplates PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testTranslatorFoldedConstants
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslatorFoldedConstants> -c ${_functional_input}
)
set_tests_properties(functional_testTranslatorFoldedConstants PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testTranslatorUnfoldedConstants
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslatorUnfoldedConstants> -c ${_functional_input}
)
set_tests_properties(functional_testTranslatorUnfoldedConstants PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testKeepGoingTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_keep_going_cmd} -c ${_functional_minimal_input}
    ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/restrict_template_parameter.C
    ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/test2017_13_Liao.C
    ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/test2017_12_Liao.C
)
set_tests_properties(functional_testKeepGoingTranslator PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testExampleIdentityTranslator
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "rm -f alt_identityTranslator_inputFile.C; cp ${_functional_input} alt_identityTranslator_inputFile.C && ${_identity_translator_cmd} -c alt_identityTranslator_inputFile.C && ${_identity_translator_cmd} -I${CMAKE_CURRENT_BINARY_DIR}/XXX -c alt_identityTranslator_inputFile.C"
)
set_tests_properties(functional_testExampleIdentityTranslator PROPERTIES RESOURCE_LOCK ${_functional_lock})

add_test(
  NAME functional_testReadFileTwice
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:readFileTwice> -c ${_functional_input}
)
set_tests_properties(functional_testReadFileTwice PROPERTIES RESOURCE_LOCK ${_functional_lock})

if(_keep_going_missing)
  set_tests_properties(functional_testKeepGoingTranslator PROPERTIES DISABLED TRUE)
endif()
if(_identity_translator_missing)
  set_tests_properties(functional_testExampleIdentityTranslator PROPERTIES DISABLED TRUE)
endif()

add_subdirectory(CompileTests)
add_subdirectory(testSupport)
add_subdirectory(UnitTests)
add_subdirectory(roseTests)
add_subdirectory(RunTests)
#add_subdirectory(PerformanceTests)
add_subdirectory(CompilerOptionsTests)
add_subdirectory(translatorTests)
add_subdirectory(Utility)
add_subdirectory(moveDeclarationTool)

if(BASH_EXECUTABLE)
  add_test(
    NAME check_policies
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/policies-checker.sh)
endif()

install(TARGETS testTranslator DESTINATION bin)

# Makefile check targets.
set(_functional_check_tests
  functional_testObjectFileTokenGeneration
  functional_testObjectFileAnalysis
  functional_testLinkFileAnalysis
  functional_testCppFileAnalysis
  functional_testExecutableFileAnalysis
  functional_testObjectFileCodeGeneration
  functional_testLinkFileCodeGeneration
  functional_testCppFileCodeGeneration
  functional_testExecutableFileCodeGeneration
  functional_testObjectFileTranslator
  functional_testLinkFileTranslator
  functional_testCppFileTranslator
  functional_testExecutableFileTranslator
  functional_testTemplates
  functional_testGraphGeneration
  functional_testPDFGeneration
  functional_testSimpleLinkFileTranslator
  functional_testTranslatorFoldedConstants
  functional_testTranslatorUnfoldedConstants
  functional_testKeepGoingTranslator
  functional_testExampleIdentityTranslator
  functional_testReadFileTwice
)
add_test(
  NAME nonsmoke_functional_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_functional_check_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_conditional-check-local PROPERTIES DEPENDS "${_functional_check_tests}" )
