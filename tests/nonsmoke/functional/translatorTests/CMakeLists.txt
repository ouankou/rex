include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES})

if(ENABLE-C)
  add_executable(bug125 bug125.C)
  add_executable(qualifiedName qualifiedName.C)
  add_executable(testTranslator2010_01 testTranslator2010_01.C)
  add_executable(testTranslator2010_02 testTranslator2010_02.C)
  add_executable(testTranslator2010_03 testTranslator2010_03.C)
  add_executable(ompVariableCollecting ompVariableCollecting.C)
  add_executable(label_stmt_file_info_translator label_stmt_file_info_translator.C)
  add_executable(testConstDeclarations testConstDeclarations.C)
  add_executable(testDataMemberOfConst testDataMemberOfConst.C)
  add_executable(testFortranParameter testFortranParameter.C)
  add_executable(testFortranProtected testFortranProtected.C)
  add_executable(testPragmaInBody testPragmaInBody.C)
  add_executable(rex_genericPragma rex_genericPragma.C)
  add_executable(querySubTree querySubTree.C)
  add_executable(moveStatementsBetweenBlocks moveStatementsBetweenBlocks.C)
  add_executable(testCopyAndDelete testCopyAndDelete.C)
  add_executable(testTraversalOfTemplateInstantiations testTraversalOfTemplateInstantiations.C)
  add_executable(testTranslator2012_01 testTranslator2012_01.C)
  add_executable(testTranslator2013_01 testTranslator2013_01.C)
  add_executable(testTranslator2013_02 testTranslator2013_02.C)
  add_executable(testTranslator2018_01 testTranslator2018_01.C)
  add_executable(testTranslator2018_02 testTranslator2018_02.C)
  add_executable(testPlugins testPlugins.C)
  add_executable(roseCompiler roseCompiler.C)
  add_library(PrintNamesPlugin MODULE PrintNamesPlugin.cpp)

  target_link_libraries(bug125 ROSE_DLL ${link_with_libraries})
  target_link_libraries(qualifiedName ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2010_01 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2010_02 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2010_03 ROSE_DLL ${link_with_libraries})
  target_link_libraries(ompVariableCollecting ROSE_DLL ${link_with_libraries})
  target_link_libraries(label_stmt_file_info_translator ROSE_DLL ${link_with_libraries})
  target_link_libraries(testConstDeclarations ROSE_DLL ${link_with_libraries})
  target_link_libraries(testDataMemberOfConst ROSE_DLL ${link_with_libraries})
  target_link_libraries(testFortranParameter ROSE_DLL ${link_with_libraries})
  target_link_libraries(testFortranProtected ROSE_DLL ${link_with_libraries})
  target_link_libraries(testPragmaInBody ROSE_DLL ${link_with_libraries})
  target_link_libraries(rex_genericPragma ROSE_DLL ${link_with_libraries})
  target_link_libraries(querySubTree ROSE_DLL ${link_with_libraries})
  target_link_libraries(moveStatementsBetweenBlocks ROSE_DLL ${link_with_libraries})
  target_link_libraries(testCopyAndDelete ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTraversalOfTemplateInstantiations ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2012_01 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2013_01 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2013_02 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2018_01 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testTranslator2018_02 ROSE_DLL ${link_with_libraries})
  target_link_libraries(testPlugins ROSE_DLL ${link_with_libraries})
  target_link_libraries(roseCompiler ROSE_DLL ${link_with_libraries})
  target_link_libraries(PrintNamesPlugin ROSE_DLL ${link_with_libraries})

  enable_testing()
  set(_translator_input_dir ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(
    NAME testbug125
    COMMAND bug125 -c ${CMAKE_CURRENT_SOURCE_DIR}/inputbug125.C)

  add_test(
    NAME AstConsistencyTests
    COMMAND testTranslator -rose:ast:consistencyCheck -rose:skipfinalCompileStep ${CMAKE_CURRENT_SOURCE_DIR}/inputAstConsistency.cpp)

  add_test(
    NAME testqualifiedName
    COMMAND qualifiedName -c ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests/jacobi.c)
  if(NOT HAVE_SYSTEM_TIME_H)
  endif()

  add_test(
    NAME Translator_testConstDeclarations
    COMMAND testConstDeclarations ${_translator_input_dir}/input_testConstDeclarations.C)

  add_test(
    NAME Translator_testDataMemberOfConst
    COMMAND testDataMemberOfConst
      ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/dataMemberOfConst.C
      -o /dev/null)

  add_test(
    NAME Translator_testPragmaInBody
    COMMAND testPragmaInBody -c ${_translator_input_dir}/input_testPragmaInBody.c)

  add_test(
    NAME Translator_rex_genericPragma
    COMMAND rex_genericPragma -c ${_translator_input_dir}/input_rex_genericPragma.c)

  add_test(
    NAME Translator_label_stmt_file_info
    COMMAND label_stmt_file_info_translator -rose:verbose 0 -c ${_translator_input_dir}/input_label_stmt_file_info_translator.C)

  add_test(
    NAME Translator_querySubTree
    COMMAND querySubTree -c ${_translator_input_dir}/input_querySubTree.C)

  set_tests_properties(
    Translator_testPragmaInBody
    Translator_rex_genericPragma
    Translator_querySubTree
    PROPERTIES LABELS "sanitizer")

  add_test(
    NAME Translator_ompVariableCollecting
    COMMAND ompVariableCollecting -I${ROSE_TOP_SRC_DIR}/src/frontend/SageIII -rose:OpenMP:ast_only -c ${_translator_input_dir}/input_ompVariableCollecting.C)

  add_test(
    NAME Translator_testTranslator2010_02
    COMMAND testTranslator2010_02 -rose:verbose 0 ${_translator_input_dir}/input_testTranslator2010_2.C)

  add_test(
    NAME Translator_testTranslator2010_01
    COMMAND testTranslator2010_01)

  add_test(
    NAME Translator_testTranslator2010_03
    COMMAND testTranslator2010_03 -c -rose:verbose 0 ${_translator_input_dir}/input_testTranslator2010_3.C)

  add_test(
    NAME Translator_moveStatementsBetweenBlocks
    COMMAND moveStatementsBetweenBlocks -c ${_translator_input_dir}/input_moveStatementsBetweenBlocks.C)

  add_test(
    NAME Translator_testCopyAndDelete_1
    COMMAND testCopyAndDelete -c ${_translator_input_dir}/input_testCopyAndDelete.c)

  add_test(
    NAME Translator_testCopyAndDelete_2
    COMMAND testCopyAndDelete -c ${_translator_input_dir}/input_testCopyAndDelete_2.C)

  add_test(
    NAME Translator_testTraversalOfTemplateInstantiations
    COMMAND testTraversalOfTemplateInstantiations -c ${_translator_input_dir}/input_testTraversalOfTemplateInstantiations.C)

  add_test(
    NAME Translator_testTranslator2012_01
    COMMAND testTranslator2012_01 -rose:verbose 0 ${_translator_input_dir}/input_testTranslator2012_01a.C ${_translator_input_dir}/input_testTranslator2012_01b.C)

  add_test(
    NAME Translator_testTranslator2013_01
    COMMAND testTranslator2013_01 -rose:verbose 0 ${_translator_input_dir}/input_testTranslator2013_01.C)

  add_test(
    NAME Translator_testTranslator2013_02
    COMMAND testTranslator2013_02 -rose:verbose 0 ${_translator_input_dir}/input_testTranslator2013_02.C)

  add_test(
    NAME Translator_testTranslator2018_01
    COMMAND testTranslator2018_01 -DSKIP_ROSE_BUILTIN_DECLARATIONS -rose:verbose 0 -c ${_translator_input_dir}/input_testTranslator2018_01.C)

  add_test(
    NAME Translator_testTranslator2018_02
    COMMAND testTranslator2018_02 -rose:verbose 0 -c ${_translator_input_dir}/input_testTranslator2018_02.C)

  add_test(
    NAME Translator_testPlugins
    COMMAND testPlugins
      -rose:plugin_lib $<TARGET_FILE:PrintNamesPlugin>
      -rose:plugin_action print-names
      -rose:plugin_arg_print-names op1
      -c ${_translator_input_dir}/input_testPlugins.C
  )

  add_test(
    NAME Translator_testFortranParameter
    COMMAND testFortranParameter
      ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/parameter.f
      -o /dev/null
  )

  add_test(
    NAME Translator_testFortranProtected
    COMMAND sh -c
      "${BACKEND_FORTRAN_COMPILER} -c ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/protected_mod.f03 && gdb -nx -batch --return-child-result -ex 'set pagination off' -ex 'set debuginfod enabled off' -ex 'run' --args $<TARGET_FILE:testFortranProtected> ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/protected.f03 -o /dev/null || { status=$?; gdb -nx -batch -ex 'set pagination off' -ex 'set debuginfod enabled off' -ex 'run' -ex 'bt' --args $<TARGET_FILE:testFortranProtected> ${ROSE_TOP_SRC_DIR}/tests/nonsmoke/functional/CompileTests/Fortran_tests/protected.f03 -o /dev/null; exit $status; }"
  )

  set(_translator_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_test_output)
  file(MAKE_DIRECTORY ${_translator_output_dir})
  set(_translator_tests
    testbug125
    AstConsistencyTests
    testqualifiedName
    Translator_testConstDeclarations
    Translator_testDataMemberOfConst
    Translator_testPragmaInBody
    Translator_rex_genericPragma
    Translator_label_stmt_file_info
    Translator_querySubTree
    Translator_ompVariableCollecting
    Translator_testTranslator2010_02
    Translator_testTranslator2010_01
    Translator_testTranslator2010_03
    Translator_moveStatementsBetweenBlocks
    Translator_testCopyAndDelete_1
    Translator_testCopyAndDelete_2
    Translator_testTraversalOfTemplateInstantiations
    Translator_testTranslator2012_01
    Translator_testTranslator2013_01
    Translator_testTranslator2013_02
    Translator_testTranslator2018_01
    Translator_testTranslator2018_02
    Translator_testPlugins
    Translator_testFortranParameter
    Translator_testFortranProtected
  )
  set_tests_properties(${_translator_tests} PROPERTIES
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_translator_output_dir}")

endif()
