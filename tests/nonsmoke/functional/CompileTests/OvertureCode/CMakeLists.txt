set(INCLUDES
  -I${CMAKE_CURRENT_BINARY_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR}/../A++Code)

add_test(
  NAME overtureBug_01
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/overtureBug_01.C)
set_tests_properties(overtureBug_01 PROPERTIES LABELS OVERTURETEST)

add_test(
  NAME overture_xFDerivative
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/xFDerivative.C)
set_tests_properties(overture_xFDerivative PROPERTIES DISABLED TRUE LABELS OVERTURETEST)

add_test(
  NAME overture_defect
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/defect.C)
set_tests_properties(overture_defect PROPERTIES DISABLED TRUE LABELS OVERTURETEST)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_OvertureCode_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=overtureBug_01
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OvertureCode_check-local PROPERTIES
  DEPENDS "overtureBug_01"
)
