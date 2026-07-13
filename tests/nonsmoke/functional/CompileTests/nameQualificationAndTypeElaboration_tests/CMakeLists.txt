set(TESTCODES
  testNameQalTypeElab_01.C testNameQalTypeElab_02.C testNameQalTypeElab_03.C
  testNameQalTypeElab_04.C testNameQalTypeElab_05.C testNameQalTypeElab_06.C
  testNameQalTypeElab_07.C testNameQalTypeElab_08.C testNameQalTypeElab_09.C
  testNameQalTypeElab_10.C testNameQalTypeElab_11.C testNameQalTypeElab_12.C
  testNameQalTypeElab_13.C testNameQalTypeElab_14.C testNameQalTypeElab_15.C
  testNameQalTypeElab_16.C testNameQalTypeElab_17.C testNameQalTypeElab_18.C
  testNameQalTypeElab_22.C testNameQalTypeElab_23.C testNameQalTypeElab_24.C
  testNameQalTypeElab_25.C testNameQalTypeElab_26.C testNameQalTypeElab_27.C
  testNameQalTypeElab_28.C testNameQalTypeElab_29.C testNameQalTypeElab_30.C
  testNameQalTypeElab_31.C testNameQalTypeElab_32.C testNameQalTypeElab_33.C
  testNameQalTypeElab_34.C testNameQalTypeElab_35.C testNameQalTypeElab_36.C
  testNameQalTypeElab_37.C testNameQalTypeElab_38.C testNameQalTypeElab_39.C
  testNameQalTypeElab_40.C
  testNameQalTypeElab_19.C
  testNameQalTypeElab_20.C
  testNameQalTypeElab_21.C)

# File option to accumulate performance information about the compilation
set(PERFORMANCE_REPORT_OPTION -rose:compilationPerformanceFile
  ${CMAKE_BINARY_DIR}/Cxx_ROSE_PERFORMANCE_DATA.csv)

set(ROSE_FLAGS  -w -rose:verbose 0
  ${PERFORMANCE_REPORT_OPTION})

set(TESTCODE_INCLUDES
  -I${CMAKE_CURRENT_SOURCE_DIR})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; name-qualification tests require bash")
endif()

foreach(file_to_test ${TESTCODES})
  rex_get_cxx_specimen_extra_flags(_extra_flags ${file_to_test})
  if(file_to_test STREQUAL "testNameQalTypeElab_21.C")
    add_test(
      NAME NAMEQUALIFICATIONTEST_${file_to_test}
      COMMAND ${BASH_EXECUTABLE} -c
        "ulimit -c 0; set +e; \"$@\" > testNameQalTypeElab_21.reject.log 2>&1; status=$?; set -e; test \"$status\" -eq 134; test \"$(grep -Fxc \"${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}:11:1: error: declaration conflicts with target of using declaration already in scope\" testNameQalTypeElab_21.reject.log)\" -eq 1; test \"$(grep -Fxc \"${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}:13:3: error: reference to 'X1' is ambiguous\" testNameQalTypeElab_21.reject.log)\" -eq 1; test \"$(grep -Fxc \"REX_FRONTEND_INVARIANT[clang-diagnostics]: Clang reported 2 diagnostic error(s); frontend AST translation is forbidden\" testNameQalTypeElab_21.reject.log)\" -eq 1"
        dummy $<TARGET_FILE:testTranslator> ${ROSE_FLAGS} ${_extra_flags}
        ${TESTCODE_INCLUDES} -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  else()
    add_test(
      NAME NAMEQUALIFICATIONTEST_${file_to_test}
      COMMAND testTranslator ${ROSE_FLAGS} ${_extra_flags} ${TESTCODE_INCLUDES}
       -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  endif()
  set_tests_properties(NAMEQUALIFICATIONTEST_${file_to_test}
    PROPERTIES LABELS NAMEQUALIFICATIONTEST)
endforeach()
