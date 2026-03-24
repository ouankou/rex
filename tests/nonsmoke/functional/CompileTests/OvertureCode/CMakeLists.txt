set(INCLUDES
  -I${CMAKE_CURRENT_BINARY_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR})

add_test(
  NAME overtureBug_01
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/overtureBug_01.C)
set_tests_properties(overtureBug_01 PROPERTIES LABELS OVERTURETEST)

add_test(
  NAME overture_xFDerivative
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/xFDerivative.C)
set_tests_properties(overture_xFDerivative PROPERTIES  LABELS OVERTURETEST)

add_test(
  NAME overture_defect
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/defect.C)
set_tests_properties(overture_defect PROPERTIES  LABELS OVERTURETEST)

