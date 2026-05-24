set(INCLUDES
  -I${CMAKE_CURRENT_BINARY_DIR}
  -I${CMAKE_CURRENT_SOURCE_DIR})

add_test(
  NAME overtureBug_01
  COMMAND testTranslator ${INCLUDES}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/overtureBug_01.C)
set_tests_properties(overtureBug_01 PROPERTIES LABELS OVERTURETEST)

foreach(_overture_missing_prone IN ITEMS
    xFDerivative.C
    defect.C)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_overture_missing_prone}")
    message(STATUS "Skipping Overture test without source: ${_overture_missing_prone}")
    continue()
  endif()

  get_filename_component(_overture_name "${_overture_missing_prone}" NAME_WE)
  add_test(
    NAME overture_${_overture_name}
    COMMAND testTranslator ${INCLUDES}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${_overture_missing_prone})
  set_tests_properties(overture_${_overture_name} PROPERTIES LABELS OVERTURETEST)
endforeach()
