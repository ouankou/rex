# DQ (4/17/2010): These directories are now organized based on what sets of
# languages are enabled in ROSE.

if(ENABLE-C)
  add_subdirectory (astMergeTests)
  add_subdirectory (astDiagnostics)
  add_subdirectory (astPerformanceTests)
  add_subdirectory (astProcessingTests)
  add_subdirectory (astQueryTests)
  add_subdirectory (astSymbolTableTests)
  add_subdirectory (astTokenStreamTests)
  add_subdirectory (programTransformationTests)
  #add_subdirectory (graph_tests)
  add_subdirectory (mergeTraversal_tests)
  add_subdirectory (astLValueTests)
  add_subdirectory (varDeclNorm)
  add_subdirectory (ROSETTA)

  # TOO (2/16/2011): Errors with Thrifty (GCC 3.4.4) backend compiler. We can
  # gradually enable these tests at a later stage if necessary.
  set(c_compiler_version
  "${BACKEND_C_COMPILER_MAJOR_VERSION_NUMBER}.${BACKEND_C_COMPILER_MINOR_VERSION_NUMBER}.${BACKEND_C_COMPILER_PATCH_LEVEL_NUMBER}")

  if ( (NOT CMAKE_COMPILER_S_GNUCC) OR (NOT ${c_compiler_version} STREQUAL
  "3.4.4") )

    # TOO (2/17/11): Errors with Tensilica's Xtensa compilers.
    if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Xtensa")
      if (NOT with-insure)
        add_subdirectory (astInterfaceTests)
        add_subdirectory (astInliningTests)
        add_subdirectory (ompLoweringTests)
        add_subdirectory (programAnalysisTests)
        add_subdirectory (astOutliningTests)
        #add_subdirectory (loopProcessingTests)
        add_subdirectory (fileLocation_tests)
      endif() #insure
    endif() #xtensa
  endif() # gcc 3.4.4
endif() # C/C++

if (NOT with-insure)
  # DQ (4/23/2011): At least one of the files (process_cmdline.o) in this
  # directory does not seem to compile with Insure++ (it hangs for Hudson, but
  # I can't reproduce the problem locally).
  # This was not a problem the second time that Insure++ compiled ROSE.
  #add_subdirectory (utilTests)
endif()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=rosetta_nodeHierarchyTest
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_check-local PROPERTIES DEPENDS rosetta_nodeHierarchyTest )
