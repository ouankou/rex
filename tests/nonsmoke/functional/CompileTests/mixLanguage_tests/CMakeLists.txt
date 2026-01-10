if(enable-fortran)
  set(_mix_tests
    test1
    test2
  )

  foreach(specimen IN LISTS _mix_tests)
    add_test(
      NAME mixLanguage_${specimen}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND testTranslator -lstdc++
        ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}_c.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}_f.f
        -o ${specimen}
    )
  endforeach()
endif()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_mixLanguage_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astQuery_test2_input1\;astSymbolTable_test1
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_mixLanguage_tests_check-local PROPERTIES DEPENDS "astQuery_test2_input1;astSymbolTable_test1" )
