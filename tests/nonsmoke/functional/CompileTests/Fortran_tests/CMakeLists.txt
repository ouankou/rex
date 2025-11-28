if(NOT "${gfortran_version}" VERSION_EQUAL "4.0")
  add_subdirectory(LANL_POP)
  add_subdirectory(gfortranTestSuite)
endif()

include(Fortran_F90_Testcodes.cmake)
include(Fortran_F77_Testcodes.cmake)
include(Fortran_F03_Testcodes.cmake)
include(Fortran_Comments_Testcodes.cmake)

set(ROSE_FLAGS -rose:verbose 0 -rose:detect_dangling_pointers 2
               -I${CMAKE_CURRENT_SOURCE_DIR})

function(fortran_test input_file)
  add_test(
    NAME FORTRANTEST_${input_file}_translation
    COMMAND testTranslator ${ROSE_FLAGS} ${FORTRAN_FLAGS}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_tests_properties(FORTRANTEST_${input_file}_translation
    PROPERTIES LABELS FORTRANTEST)
  add_test(
    NAME FORTRANTEST_${input_file}_graph_generation
    COMMAND testGraphGeneration ${ROSE_FLAGS} ${FORTRAN_FLAGS}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  add_test(
    NAME FORTRANTEST_${input_file}_token_generation
    COMMAND testTokenGeneration ${ROSE_FLAGS} ${FORTRAN_FLAGS}
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_tests_properties(FORTRANTEST_${input_file}_graph_generation
    PROPERTIES DEPENDS FORTRANTEST_${input_file}_translation LABELS FORTRANTEST)
  set_tests_properties(FORTRANTEST_${input_file}_token_generation
    PROPERTIES DEPENDS FORTRANTEST_${input_file}_graph_generation LABELS FORTRANTEST)
endfunction()

set(FORTRAN_FLAGS -rose:f77)
foreach(F77_test ${F77_TESTCODES})
  fortran_test(${F77_test})
endforeach()

set(FORTRAN_FLAGS -rose:f90)
foreach(F90_test ${F90_TESTCODES})
  fortran_test(${F90_test})
endforeach()

foreach(F90_comment_test ${F90_COMMENTS_TESTCODES})
  fortran_test(${F90_comment_test})
endforeach()

set(FORTRAN_FLAGS -rose:skip_syntax_check -rose:skipfinalCompileStep
                  -rose:f2003)
foreach(F03_test ${F03_TESTCODES})
  fortran_test(${F03_test})
endforeach()
