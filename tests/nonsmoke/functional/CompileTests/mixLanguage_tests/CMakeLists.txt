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

