include_directories(${ROSE_INCLUDES})

add_executable(inlineEverything inlineEverything.C)
target_link_libraries(inlineEverything ROSE_DLL ${link_with_libraries})

set(INLINE_EVERYTHING_SPECIMENS
  specimen01_1.C
  specimen02_1.C
  specimen03_1.C
  specimen04_1.C
  specimen05_1.C
  specimen06_1.C
  specimen07_1.C
  specimen08_1.C
  specimen09_1.C
  specimen10_1.C
  specimen11_3.C
  specimen12_3.C
  specimen13_1.C
  specimen14_1.C
  specimen15_1.C
  specimen16_4.C
  specimen17_10.C
  specimen18_1.C
  specimen19_1.C
  specimen20_1.C
  specimen21_1.C
  specimen22_1.C
  specimen25_1.C
)

set(INLINE_EVERYTHING_DISABLED_SPECIMENS
  specimen23_1.C
  specimen24_1.C
  specimen26_1.C
  operatorPlusOverloading.C
)

set(INLINE_EVERYTHING_CPP11_SPECIMENS
  simplest_raja.C
  simplest_raja_v2.C
  lambda_001.C
)

set(INLINE_EVERYTHING_TOKEN_SPECIMENS
  testWithTokenStreamUnparsing_01.C
  testWithTokenStreamUnparsing_02.C
)

set(_inline_everything_all_specimens
  ${INLINE_EVERYTHING_SPECIMENS}
  ${INLINE_EVERYTHING_DISABLED_SPECIMENS}
)

foreach(specimen IN LISTS _inline_everything_all_specimens)
  string(REPLACE "." "_" _inline_name ${specimen})
  add_test(
    NAME inlineEverything_v1_${_inline_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND inlineEverything -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output rose_v1_${specimen}
  )
  add_test(
    NAME inlineEverything_v2_${_inline_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND inlineEverything -skip-postprocessing
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output rose_v2_${specimen}
  )
  set_tests_properties(inlineEverything_v2_${_inline_name}
    PROPERTIES DEPENDS inlineEverything_v1_${_inline_name})
endforeach()

foreach(specimen IN LISTS INLINE_EVERYTHING_DISABLED_SPECIMENS)
  string(REPLACE "." "_" _inline_name ${specimen})
  set_tests_properties(
    inlineEverything_v1_${_inline_name}
    inlineEverything_v2_${_inline_name}
    PROPERTIES DISABLED TRUE
  )
endforeach()

foreach(specimen IN LISTS INLINE_EVERYTHING_TOKEN_SPECIMENS)
  string(REPLACE "." "_" _inline_name ${specimen})
  add_test(
    NAME inlineEverything_v3_${_inline_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND inlineEverything -rose:unparse_tokens
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output rose_v3_${specimen}
  )
endforeach()

foreach(specimen IN LISTS INLINE_EVERYTHING_CPP11_SPECIMENS)
  string(REPLACE "." "_" _inline_name ${specimen})
  add_test(
    NAME inlineEverything_cpp11_v1_${_inline_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND inlineEverything -main-only -std=c++11
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output rose_cpp11_v1_${specimen}
  )
  set_tests_properties(inlineEverything_cpp11_v1_${_inline_name}
    PROPERTIES DISABLED TRUE)
endforeach()

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_test1_conf ${CMAKE_CURRENT_SOURCE_DIR}/test1.conf)
add_test(
  NAME inlineEverything_test1_conf
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth} ${_test1_conf} test1.passed
)
