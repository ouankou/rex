################################################################################
# The test programs, how to run them, and on which specimens they should run.
# All information about a particular test is localized to its few contiguous
# lines in this file.
################################################################################

if (HAVE_SYS_TIME_H)

  # NOTE: These tests were previously disabled due to graph header removals.
  # Re-enabled for investigation of current failures.

  set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
  set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

  #-----------------------------------------------------------------------------
  add_executable(createTest createTest.C)
  target_link_libraries(createTest ROSE_DLL ${link_with_libraries})

  add_test(
    NAME createTest_input1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:createTest> -c ${CMAKE_CURRENT_SOURCE_DIR}/input1.C"
      ${_exit_status}
      createTest_input1.C.passed
  )
  set_tests_properties(createTest_input1 PROPERTIES DISABLED TRUE)

  #-----------------------------------------------------------------------------
  add_executable(astTraversalTest astTraversalTest.C)
  target_link_libraries(astTraversalTest ROSE_DLL ${link_with_libraries})

  add_test(
    NAME astTraversalTest_input1C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:astTraversalTest> -c ${CMAKE_CURRENT_SOURCE_DIR}/input1.C"
      ${_exit_status}
      att_input1.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(proFunSIG proFunSIG.C)
  target_link_libraries(proFunSIG ROSE_DLL ${link_with_libraries})

  set(proFunSIG_SPECIMENS mf1.C mf3.C mf4.C mf5.C)
  foreach(specimen ${proFunSIG_SPECIMENS})
    add_test(
      NAME pfs_${specimen}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:proFunSIG> ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
        ${_exit_status}
        pfs_${specimen}.passed
    )
  endforeach()

  #-----------------------------------------------------------------------------
  add_executable(astProcessingTestInterproceduralCFG interproceduralCFG.C)
  target_link_libraries(astProcessingTestInterproceduralCFG ROSE_DLL ${link_with_libraries})

  set(interproceduralCFG_SPECIMENS test11.C test12.C test13.C test14.C)
  set(proFunSIG_SPECIMENS mf1.C mf3.C mf4.C mf5.C)
  foreach(specimen ${interproceduralCFG_SPECIMENS})
    add_test(
      NAME ipcfg_${specimen}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:astProcessingTestInterproceduralCFG> ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
        ${_exit_status}
        ipcfg_${specimen}.passed
    )
  endforeach()

  #-----------------------------------------------------------------------------
  add_executable(e0 efor2.C.maintest.C)
  target_link_libraries(e0 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME e0_efor2.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:e0> ${CMAKE_CURRENT_SOURCE_DIR}/efor2.C"
      ${_exit_status}
      e0_efor2.C.passed
  )
  set_tests_properties(e0_efor2.C PROPERTIES DISABLED TRUE)

  #-----------------------------------------------------------------------------
  add_executable(e1 efor3.C.maintest.C)
  target_link_libraries(e1 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME e1_efor3.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:e1> ${CMAKE_CURRENT_SOURCE_DIR}/efor3.C"
      ${_exit_status}
      e1_efor3.C.passed
  )
  set_tests_properties(e1_efor3.C PROPERTIES DISABLED TRUE)

  #-----------------------------------------------------------------------------
  add_executable(f1 tfif1.C.maintest.C)
  target_link_libraries(f1 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME f1_tfif1.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:f1> ${CMAKE_CURRENT_SOURCE_DIR}/tfif1.C"
      ${_exit_status}
      f1_tfif1.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(f2 tfif2.C.maintest.C)
  target_link_libraries(f2 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME f2_tfif2.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:f2> ${CMAKE_CURRENT_SOURCE_DIR}/tfif2.C"
      ${_exit_status}
      f2_tfif2.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(f3 tfif3.C.maintest.C)
  target_link_libraries(f3 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME f3_tfif3.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:f3> ${CMAKE_CURRENT_SOURCE_DIR}/tfif3.C"
      ${_exit_status}
      f3_tfif3.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(f4 tfif4.C.maintest.C)
  target_link_libraries(f4 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME f4_tfif4.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:f4> ${CMAKE_CURRENT_SOURCE_DIR}/tfif4.C"
      ${_exit_status}
      f4_tfif4.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(ff1 tf1.C.maintest.C)
  target_link_libraries(ff1 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME ff1_tf1.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:ff1> ${CMAKE_CURRENT_SOURCE_DIR}/tf1.C"
      ${_exit_status}
      ff1_tf1.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(ff2 tf2.C.maintest.C)
  target_link_libraries(ff2 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME ff2_tf2.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:ff2> ${CMAKE_CURRENT_SOURCE_DIR}/tf2.C"
      ${_exit_status}
      ff2_tf2.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(ff3 tf3.C.maintest.C)
  target_link_libraries(ff3 ROSE_DLL ${link_with_libraries})

  add_test(
    NAME ff3_tf3.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:ff3> ${CMAKE_CURRENT_SOURCE_DIR}/tf3.C"
      ${_exit_status}
      ff3_tf3.C.passed
  )

  #-----------------------------------------------------------------------------
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/strictGraphTest.C)
    add_executable(strictGraphTest strictGraphTest.C)
    target_link_libraries(strictGraphTest ROSE_DLL ${link_with_libraries})

    add_test(
      NAME strictGraphTest
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:strictGraphTest>"
        ${_exit_status}
        strictGraphTest.passed
    )
  endif()

  #-----------------------------------------------------------------------------
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/strictGraphTest2.C)
    add_executable(strictGraphTest2 strictGraphTest2.C)
    target_link_libraries(strictGraphTest2 ROSE_DLL ${link_with_libraries})

    add_test(
      NAME strictGraphTest2
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:strictGraphTest2>"
        ${_exit_status}
        strictGraphTest2.passed
    )
  endif()

  #-----------------------------------------------------------------------------
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/strictGraphTest3.C)
    add_executable(strictGraphTest3 strictGraphTest3.C)
    target_link_libraries(strictGraphTest3 ROSE_DLL ${link_with_libraries})

    add_test(
      NAME strictGraphTest3
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:strictGraphTest3>"
        ${_exit_status}
        strictGraphTest3.passed
    )
  endif()

  #-----------------------------------------------------------------------------
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/smtlibParser.C)
    add_executable(smtlibParser smtlibParser.C)
    target_link_libraries(smtlibParser ROSE_DLL ${link_with_libraries})

    add_test(
      NAME smtlibParser_test11.C
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:smtlibParser> ${CMAKE_CURRENT_SOURCE_DIR}/test11.C"
        ${_exit_status}
        smtlib_test11.C.passed
    )
  endif()

  #-----------------------------------------------------------------------------
  add_executable(sourcePTP sourcePTP.C)
  target_link_libraries(sourcePTP ROSE_DLL ${link_with_libraries})

  add_test(
    NAME sourcePTP_test14.C
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:sourcePTP> ${CMAKE_CURRENT_SOURCE_DIR}/test14.C"
      ${_exit_status}
      sptp_test14.C.passed
  )

  #-----------------------------------------------------------------------------
  add_executable(processnew3Down4SgIncGraph2 processnew3Down4SgIncGraph2.C)
  target_link_libraries(processnew3Down4SgIncGraph2
    ROSE_DLL ${link_with_libraries})
  set(processnew3Down4SgIncGraph2_SPECIMENS test11.C test12.C test13.C test14.C)

  foreach(specimen ${processnew3Down4SgIncGraph2_SPECIMENS})
    add_test(
      NAME pnew2_${specimen}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:processnew3Down4SgIncGraph2> ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
        ${_exit_status}
        pnew2_${specimen}.passed
    )
  endforeach()

  #-----------------------------------------------------------------------------
  add_executable(processnew3Down4SgIncGraph3 processnew3Down4SgIncGraph3.C)
  target_link_libraries(processnew3Down4SgIncGraph3
    ROSE_DLL ${link_with_libraries})
  set(processnew3Down4SgIncGraph3_SPECIMENS eif.C eif2.C eif3.C eif4.C)
  foreach(specimen ${processnew3Down4SgIncGraph3_SPECIMENS})
    add_test(
      NAME pnew3_${specimen}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        CMD="$<TARGET_FILE:processnew3Down4SgIncGraph3> ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
        ${_exit_status}
        pnew3_${specimen}.passed
    )
  endforeach()

endif()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-astTraversalTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-astTraversalTest PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-astTraversalTest PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-bPTP
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-bPTP PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-bPTP PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-binaryPaths
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-binaryPaths PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-binaryPaths PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-createTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-createTest PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-createTest PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-e0
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=e0_efor2.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-e0 PROPERTIES
  LABELS "disabled"
  DEPENDS "e0_efor2.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-e0 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-e1
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=e1_efor3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-e1 PROPERTIES
  LABELS "disabled"
  DEPENDS "e1_efor3.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-e1 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-f1
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=f1_tfif1.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f1 PROPERTIES
  LABELS "disabled"
  DEPENDS "f1_tfif1.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f1 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-f2
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=f2_tfif2.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f2 PROPERTIES
  LABELS "disabled"
  DEPENDS "f2_tfif2.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f2 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-f3
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=f3_tfif3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f3 PROPERTIES
  LABELS "disabled"
  DEPENDS "f3_tfif3.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f3 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-f4
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=f4_tfif4.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f4 PROPERTIES
  LABELS "disabled"
  DEPENDS "f4_tfif4.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-f4 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-ff1
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ff1_tf1.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-ff1 PROPERTIES
  LABELS "disabled"
  DEPENDS "ff1_tf1.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-ff1 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-ff2
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ff2_tf2.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-ff2 PROPERTIES
  LABELS "disabled"
  DEPENDS "ff2_tf2.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-ff2 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-ff3
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ff3_tf3.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-ff3 PROPERTIES
  LABELS "disabled"
  DEPENDS "ff3_tf3.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-ff3 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-interproceduralCFG
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-interproceduralCFG PROPERTIES
  LABELS "disabled"
  DEPENDS "ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-interproceduralCFG PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-local PROPERTIES
  DEPENDS "astTraversalTest_input1C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-proFunSIG
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-proFunSIG PROPERTIES
  LABELS "disabled"
  DEPENDS "pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-proFunSIG PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-processnew3Down4SgIncGraph2
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-processnew3Down4SgIncGraph2 PROPERTIES
  LABELS "disabled"
  DEPENDS "pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-processnew3Down4SgIncGraph2 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-processnew3Down4SgIncGraph3
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-processnew3Down4SgIncGraph3 PROPERTIES
  LABELS "disabled"
  DEPENDS "pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-processnew3Down4SgIncGraph3 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-smtlibParser
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-smtlibParser PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-smtlibParser PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-sourcePTP
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-sourcePTP PROPERTIES
  LABELS "disabled"
  DEPENDS "sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-sourcePTP PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest2
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest2 PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest2 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest3
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest3 PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-strictGraphTest3 PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_roseTests_astProcessingTests_check-yicesParser
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astTraversalTest_input1C\;createTest_input1\;e0_efor2.C\;e1_efor3.C\;f1_tfif1.C\;f2_tfif2.C\;f3_tfif3.C\;f4_tfif4.C\;ff1_tf1.C\;ff2_tf2.C\;ff3_tf3.C\;ipcfg_test11.C\;ipcfg_test12.C\;ipcfg_test13.C\;ipcfg_test14.C\;pfs_mf1.C\;pfs_mf3.C\;pfs_mf4.C\;pfs_mf5.C\;pnew2_test11.C\;pnew2_test12.C\;pnew2_test13.C\;pnew2_test14.C\;pnew3_eif.C\;pnew3_eif2.C\;pnew3_eif3.C\;pnew3_eif4.C\;sourcePTP_test14.C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-yicesParser PROPERTIES
  LABELS "disabled"
  DEPENDS "astTraversalTest_input1C;createTest_input1;e0_efor2.C;e1_efor3.C;f1_tfif1.C;f2_tfif2.C;f3_tfif3.C;f4_tfif4.C;ff1_tf1.C;ff2_tf2.C;ff3_tf3.C;ipcfg_test11.C;ipcfg_test12.C;ipcfg_test13.C;ipcfg_test14.C;pfs_mf1.C;pfs_mf3.C;pfs_mf4.C;pfs_mf5.C;pnew2_test11.C;pnew2_test12.C;pnew2_test13.C;pnew2_test14.C;pnew3_eif.C;pnew3_eif2.C;pnew3_eif3.C;pnew3_eif4.C;sourcePTP_test14.C"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_roseTests_astProcessingTests_check-yicesParser PROPERTIES DISABLED TRUE)
endif()
