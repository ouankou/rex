set(CUDA_NO_BUILT_IN_CODES
  test_2011_01.cu
  test_2011_02.cu
  rose-2123.cu
)
set(CUDA_CODES
  test_2011_11.cu
  test_2011_12.cu
  test_2011_13.cu
  header_test.cu
  mm_single.cu
)

set(_cuda_tests_disabled FALSE)
if(NOT ENABLE-CUDA)
  set(_cuda_tests_disabled TRUE)
endif()

set(ROSE_FLAGS -rose:skipfinalCompileStep)
set(files_to_test ${CUDA_NO_BUILT_IN_CODES} ${CUDA_CODES})
set(ROSE_DISABLED_TESTCODES)
if(_cuda_tests_disabled)
  list(APPEND ROSE_DISABLED_TESTCODES ${files_to_test})
endif()
if(NOT ROSE_USING_GCC_VERSION_LATER_6_1 EQUAL 1)
  list(APPEND ROSE_DISABLED_TESTCODES rose-2123.cu)
endif()

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} CUDA TRUE)
endforeach()

add_executable(deviceTranformation deviceTranformation.C)
target_link_libraries(deviceTranformation ROSE_DLL ${link_with_libraries})

set(_device_transform_tests
  inputCode_01.cu
  test2016_03.cu
)

set(_gcc_later_4_9 FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0")
  set(_gcc_later_4_9 TRUE)
endif()

foreach(file_to_test ${_device_transform_tests})
  add_test(
    NAME CUDATEST_deviceTranformation_${file_to_test}
    COMMAND $<TARGET_FILE:deviceTranformation> -std=c++11 -rose:skipfinalCompileStep
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  if(_gcc_later_4_9 OR _cuda_tests_disabled)
    set_tests_properties(CUDATEST_deviceTranformation_${file_to_test}
      PROPERTIES DISABLED TRUE)
  endif()
endforeach()
