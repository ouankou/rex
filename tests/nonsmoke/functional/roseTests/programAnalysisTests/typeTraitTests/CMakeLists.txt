include_directories(${ROSE_INCLUDES})

add_executable(typeTraitExerciseWithRetCode typeTraitExerciseWithRetCode.C)
target_link_libraries(typeTraitExerciseWithRetCode ROSE_DLL ${link_with_libraries})

add_executable(typeTraitExerciseWithoutRetCode typeTraitExerciseWithoutRetCode.C)
target_link_libraries(typeTraitExerciseWithoutRetCode ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_rose_include_flags)
foreach(entry IN LISTS ROSE_INCLUDES)
  if(entry MATCHES "^-I" OR entry MATCHES "^-isystem")
    list(APPEND _rose_include_flags ${entry})
  else()
    list(APPEND _rose_include_flags -I${entry})
  endif()
endforeach()

set(_type_trait_tests_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(_type_trait_specimens
  test1.C
  test2.C
  test3.C
  test4.C
  test5.C
  test6.C
  test7.C
  test8.C
  test9.C
  test10.C
  test11.C
  test12.C
  test13.C
  test14.C
  test15.C
  test16.C
)

set(_type_traits_disabled "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.4")
  set(_type_traits_disabled
    "not supported for gcc version ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if(_type_traits_disabled STREQUAL ""
   AND BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH MATCHES "g\\+\\+|gcc")
  if(BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER EQUAL 4
     AND BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER LESS 4)
    set(_type_traits_disabled
      "not supported for gcc version ${BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER}.${BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER}")
  endif()
endif()

foreach(specimen IN LISTS _type_trait_specimens)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME type_trait_with_ret_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:typeTraitExerciseWithRetCode> -w -c ${_type_trait_tests_dir}/${specimen}"
      ${_exit_status}
      nt_${specimen}.passed
  )
  if(NOT _type_traits_disabled STREQUAL "")
    set_tests_properties(type_trait_with_ret_${_name} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

add_test(
  NAME type_trait_without_ret_roseHeader
  COMMAND typeTraitExerciseWithoutRetCode -w -I${CMAKE_BINARY_DIR}
    ${_rose_include_flags}
    -c ${_type_trait_tests_dir}/roseHeader.C
)

set_tests_properties(type_trait_without_ret_roseHeader PROPERTIES TIMEOUT 600)
set_tests_properties(type_trait_without_ret_roseHeader PROPERTIES DISABLED TRUE)
