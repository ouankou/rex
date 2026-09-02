include_directories(${ROSE_INCLUDES})

set(_functional_include_flags "")
foreach(include ${ROSE_INCLUDES})
  list(APPEND _functional_include_flags "-I${include}")
endforeach()

set(REX_COMPILE_TESTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/CompileTests)

set(REX_FUNCTIONAL_SYSTEM_HEADER_LIKE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/input_codes/system_header_like)
set(REX_TEST2018_34_SUPPORT_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/CompileTests/C_tests/test2018_34_support)

set(CXX_TESTCODES_GNUXX03
  # Legacy access declarations (e.g., A::foo;) require pre-C++11 mode.
  test2020_31.C
)

set(CXX_TESTCODES_GNUXX17
  # These use C++17 library/features (variant/filesystem/*_v traits/CTAD),
  # or include ROSE headers that now require at least C++17.
  rex_unparser_structured_binding_parent.cpp
  rex_test2026_template_features.cpp
  rex_test2026_stl_map.cpp
  rex_test2026_stl_meta.cpp
  rex_test2026_stl_type_traits.cpp
  rex_test2026_stl_variant.cpp
  rex_test2026_deduction_guide.cpp
  rex_test2026_std_filesystem.cpp
  rex_test2026_std_variant_visit.cpp
  rex_frontend_semantic_lambda_parent.cpp
  test2010_02.C
  test2017_99.C
)

set(CXX_TESTCODES_GNUXX20
  # These exercise C++20-only grammar such as the three-way comparison
  # operator and must use the same dialect in every shared test consumer.
  rex_frontend_operator_surface_contract.cpp
)

function(rex_get_cxx_specimen_extra_flags out_var specimen)
  set(_flags)
  get_filename_component(_specimen_name "${specimen}" NAME)
  if(_specimen_name STREQUAL "needs_reentrant.c")
    list(APPEND _flags -pthread)
  endif()
  if(_specimen_name STREQUAL "test2012_145.c")
    list(APPEND _flags [[-DSTRING_MACRO="some_text"]])
  endif()
  if(_specimen_name STREQUAL "test2015_153.c")
    list(APPEND _flags -O)
  endif()
  if(_specimen_name STREQUAL "test2018_34.c")
    list(APPEND _flags
      -I${REX_TEST2018_34_SUPPORT_DIR})
  endif()
  if(_specimen_name STREQUAL "rex_frontend_driver_argument_order.c")
    list(APPEND _flags
      -DREX_DRIVER_ARGUMENT_ORDER=1
      -UREX_DRIVER_ARGUMENT_ORDER
      -DREX_DRIVER_ARGUMENT_ORDER=2
      -DREX_DRIVER_ARGUMENT_UNDEFINED
      -UREX_DRIVER_ARGUMENT_UNDEFINED)
  endif()
  if(specimen STREQUAL "rex_test2025_issue148_system_header_mutation.cpp")
    list(APPEND _flags -isystem ${REX_FUNCTIONAL_SYSTEM_HEADER_LIKE_DIR})
  endif()
  if(_specimen_name STREQUAL "rex_unparser_asm_clobber_identity.cpp")
    # This specimen intentionally names x86 registers.  Keep that source
    # contract explicit so it exercises the same frontend semantics on every
    # architecture runner.
    list(APPEND _flags -target x86_64-unknown-linux-gnu)
  endif()
  set(${out_var} "${_flags}" PARENT_SCOPE)
endfunction()

function(rex_get_cxx_specimen_standard_flag out_var specimen)
  get_filename_component(_specimen_name "${specimen}" NAME)
  if(_specimen_name STREQUAL "rex_test2026_concepts.cpp")
    set(_flag -std=c++20)
  elseif(_specimen_name IN_LIST CXX_TESTCODES_GNUXX20)
    set(_flag -std=gnu++20)
  elseif(_specimen_name IN_LIST CXX_TESTCODES_GNUXX03)
    set(_flag -std=gnu++03)
  elseif(_specimen_name IN_LIST CXX_TESTCODES_GNUXX17)
    set(_flag -std=gnu++17)
  else()
    # The compiler itself is built as C++17, but the generic historical C++
    # specimen set has an explicit C++14 language contract.  Consumers must
    # not infer an input dialect from the build dialect: doing so makes valid
    # dynamic exception specifications and register declarations ill-formed
    # before the frontend can translate their source syntax.
    set(_flag -std=gnu++14)
  endif()
  set(${out_var} "${_flag}" PARENT_SCOPE)
endfunction()

function(rex_get_fortran_specimen_extra_flags out_var specimen)
  set(_flags)
  get_filename_component(_specimen_name "${specimen}" NAME)
  set(_cray_pointer_specimens
    test2007_81.f90
    test2007_82.f90
    test2007_83.f90
    test2007_84.f90
    rex_fortran_cray_pointer_declaration.f90
    test2011_imperial_crayPointers.f90)
  if(_specimen_name IN_LIST _cray_pointer_specimens)
    list(APPEND _flags -rose:cray_pointer_support)
  endif()
  set(${out_var} "${_flags}" PARENT_SCOPE)
endfunction()

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
    " install_dir=;"
    " run_dir=;"
    " cleanup_rex_install_tree() { rm -rf \"$install_dir\" \"$run_dir\"; };"
    " trap cleanup_rex_install_tree EXIT;"
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
    " install_dir=;"
    " run_dir=;"
    " cleanup_rex_install_tree() { rm -rf \"$install_dir\" \"$run_dir\"; };"
    " trap cleanup_rex_install_tree EXIT;"
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
    " install_dir=;"
    " run_dir=;"
    " cleanup_rex_install_tree() { rm -rf \"$install_dir\" \"$run_dir\"; };"
    " trap cleanup_rex_install_tree EXIT;"
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
  COMMAND $<TARGET_FILE:testAnalysis>
    -x c++ /dev/null -x none
    alt_Analysis_inputFile.o -o testLinkFileAnalysisExecutable
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
  COMMAND $<TARGET_FILE:testCodeGeneration>
    -x c++ /dev/null -x none
    alt_CodeGeneration_inputFile.o -o testLinkFileCodeGenerationExecutable
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
set(_functional_read_file_twice_output_dir
    ${CMAKE_CURRENT_BINARY_DIR}/test-output/readFileTwice)
file(MAKE_DIRECTORY ${_functional_read_file_twice_output_dir})
set_tests_properties(
  functional_testReadFileTwice
  PROPERTIES
    RESOURCE_LOCK ${_functional_lock}
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_functional_read_file_twice_output_dir}")

if(_identity_translator_missing)
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
add_subdirectory(roseCompiler)
add_subdirectory(moveDeclarationTool)

if(BASH_EXECUTABLE)
  add_test(
    NAME check_policies
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/policies-checker.sh)
endif()

install(TARGETS testTranslator DESTINATION bin)
