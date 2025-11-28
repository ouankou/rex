if(NOT "${gfortran_version}" VERSION_EQUAL "4.0")
  add_subdirectory(LANL_POP)
  add_subdirectory(gfortranTestSuite)
endif()

include(Fortran_F90_Testcodes.cmake)
include(Fortran_F77_Testcodes.cmake)
include(Fortran_F03_Testcodes.cmake)
include(Fortran_Comments_Testcodes.cmake)

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
