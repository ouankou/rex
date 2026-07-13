add_subdirectory(astPostProcessing)
add_subdirectory(sageInterface)
add_subdirectory(virtualCFG)
set(OMPPARSER_BUILD_TESTING OFF CACHE BOOL
    "Build ompparser standalone tests" FORCE)
add_subdirectory(ompparser)
set(USE_REX true)
set(USE_OMPPARSER true)
if(NOT REX_ENABLE_WASM)
  add_subdirectory(accparser)
endif()
if(TARGET accparser)
  # accparser is a submodule; inject ROSE_ABORT via forced include without editing submodule sources.
  target_include_directories(accparser PUBLIC ${ROSE_TOP_SRC_DIR}/src/util)
  target_compile_options(accparser PRIVATE -include ROSE_ABORT.h)
  if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
    get_property(_rose_tests GLOBAL PROPERTY TESTS)
    foreach(_rose_test IN LISTS _rose_tests)
      if(_rose_test MATCHES "^openacc_vv_")
        set_property(TEST ${_rose_test} APPEND PROPERTY ENVIRONMENT "FC=flang")
      endif()
    endforeach()
    unset(_rose_tests)
  endif()
  set(_accparser_test_runtime_dir ${CMAKE_BINARY_DIR}/tests)
  foreach(_accparser_exec acc_tester.out acc_roundtrip.out lang_flag_test.out)
    if(TARGET ${_accparser_exec})
      set_target_properties(${_accparser_exec}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_accparser_test_runtime_dir})
    endif()
  endforeach()
  unset(_accparser_exec)
  unset(_accparser_test_runtime_dir)
endif()

if(NOT ENABLE-INTERNALFRONTENDDEVELOPMENT)
  ##AS Commented out for simpler build
  add_subdirectory(astHiddenTypeAndDeclarationLists)
  add_subdirectory(astTokenStream)
  add_subdirectory(includeDirectivesProcessing)
endif()

set(SAGE3_SOURCES
  rose_attributes_list.C
  attachPreprocessingInfo.C
  attachPreprocessingInfoTraversal.C
  manglingSupport.C
  sage_support/sage_support.C
  sage_support/cmdline.C
  sage_support/ast_memory_cleanup.C
  sage_support/utility_functions.C
  astJson/sageAstJsonApi.cpp
  astJson/sageAstJsonCollect.cpp
  astJson/sageAstJsonDeserializeFinalize.cpp
  astJson/sageAstJsonDeserializeNodes.cpp
  astJson/sageAstJsonDeserializeSymbols.cpp
  astJson/sageAstJsonDeserializeTypes.cpp
  astJson/sageAstJsonFormat.cpp
  astJson/sageAstJsonProject.cpp
  astJson/sageAstJsonSerializeExternal.cpp
  astJson/sageAstJsonSerializeNodes.cpp
  astJson/sageAstJsonSerializeSupport.cpp
  openMPConstantInteger.cpp
  fixupCopy_scopes.C
  fixupCopy_symbols.C
  fixupCopy_references.C
  IncludeDirective.C
  rtiHelpers.C
  rose_graph_support.C
  Utf8.C
  memory-pool-snapshot.C)

if(NOT REX_ENABLE_WASM)
  list(PREPEND SAGE3_SOURCES accAstConstruction.cpp)
endif()

configure_file(sage3basic.h.in ${CMAKE_CURRENT_BINARY_DIR}/sage3basic.h @ONLY)

#GENERATING the license_string.h file
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/src/frontend/SageIII/license_string.h
  COMMAND ${PERL_EXECUTABLE} -t
    ${ROSE_TOP_SRC_DIR}/scripts/to-c-string.pl < ${ROSE_TOP_SRC_DIR}/LicenseInformation/ROSE_BSD_License.txt > ${CMAKE_BINARY_DIR}/src/frontend/SageIII/license_string.h
  DEPENDS  ${ROSE_TOP_SRC_DIR}/LicenseInformation/ROSE_BSD_License.txt)

if(ENABLE-CPP)
  # GENERATING THE ROSE PREPROCESSOR
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/src/frontend/SageIII/lex.yy.C
    COMMAND ${FLEX_EXECUTABLE} -t
      ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/preproc-c.ll > ${CMAKE_BINARY_DIR}/src/frontend/SageIII/lex.yy.C
    DEPENDS  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/preproc-c.ll)

  add_custom_target(ROSE_PREPROCESSOR ALL DEPENDS
    ${CMAKE_BINARY_DIR}/src/frontend/SageIII/lex.yy.C COMMENT "Generating files for preprocessor" )

  #GENERATING the OpenMP parser
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/src/frontend/SageIII/omp_exprparser_lexer.cc
    COMMAND ${FLEX_EXECUTABLE} -t
      ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/omp_exprparser_lexer.ll > ${CMAKE_BINARY_DIR}/src/frontend/SageIII/omp_exprparser_lexer.cc
    DEPENDS  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/omp_exprparser_lexer.ll)

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/src/frontend/SageIII/omp_exprparser_parser.cc
    COMMAND ${BISON_EXECUTABLE}
      -Wall -Werror
      ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/omp_exprparser_parser.yy -o
      ${CMAKE_BINARY_DIR}/src/frontend/SageIII/omp_exprparser_parser.cc
      #    && mv omp_exprparser_parser.H omp_exprparser_parser.hh
    DEPENDS  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/omp_exprparser_parser.yy)

  set(SAGE3_SOURCES ${SAGE3_SOURCES}
    ompSupport.C
    ompAstConstruction.cpp ompFortranParser.C
    omp_exprparser_lexer.ll omp_exprparser_parser.yy
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.C
    ${CMAKE_CURRENT_BINARY_DIR}/omp_exprparser_parser.cc
    ${CMAKE_CURRENT_BINARY_DIR}/omp_exprparser_lexer.cc)
endif()

set(SAGE3_SOURCES ${SAGE3_SOURCES}
  ${CMAKE_CURRENT_BINARY_DIR}/license_string.h)

set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.C
  ${CMAKE_CURRENT_BINARY_DIR}/omp_exprparser_parser.cc
  ${CMAKE_CURRENT_BINARY_DIR}/omp_exprparser_lexer.cc
  PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)

########### next target ###############
add_library(sage3 OBJECT ${SAGE3_SOURCES})
add_dependencies(sage3 rosetta_generated)
target_include_directories(sage3 PRIVATE ${LLVM_INCLUDE_DIRS})
if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG AND FLANG_INTRINSIC_INCLUDE_DIR)
  target_compile_definitions(sage3 PRIVATE
    "ROSE_FLANG_INTRINSIC_INCLUDEDIR=\"${FLANG_INTRINSIC_INCLUDE_DIR}\"")
endif()

########### install files ###############

install(
  FILES
    sage3.h ${CMAKE_CURRENT_BINARY_DIR}/sage3basic.h rose_attributes_list.h attachPreprocessingInfo.h
    astJson/sageAstJson.h
    attachPreprocessingInfoTraversal.h attach_all_info.h manglingSupport.h
    C++_include_files.h fixupCopy.h general_token_defs.h rtiHelpers.h
    ompAstConstruction.h omp.h ompSupport.h
    rosedll.h fileoffsetbits.h rosedefs.h
    sage3basic.hhh sage_support/cmdline.h sage_support/sage_support.h
    sage_support/utility_functions.h sage_support/utility_functionsImpl.C
    memory-pool-snapshot.h
    ${CMAKE_CURRENT_BINARY_DIR}/Cxx_Grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/Cxx_GrammarDeclarations.h
    ${CMAKE_CURRENT_BINARY_DIR}/Cxx_GrammarVariants.h
    ${CMAKE_CURRENT_BINARY_DIR}/Cxx_GrammarTreeTraversalAccessEnums.h
    ${CMAKE_CURRENT_BINARY_DIR}/AstQueryMemoryPool.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
