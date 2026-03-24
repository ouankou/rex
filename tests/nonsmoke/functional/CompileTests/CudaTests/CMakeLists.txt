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

set(ROSE_FLAGS -rose:skipfinalCompileStep)
set(files_to_test ${CUDA_NO_BUILT_IN_CODES} ${CUDA_CODES})

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} CUDA TRUE)
endforeach()

add_test(
  NAME CUDA_test_2011_01
  COMMAND testTranslator -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_2011_01.cu
)
set_tests_properties(CUDA_test_2011_01 PROPERTIES LABELS CUDA)

add_test(
  NAME nonsmoke_functional_CompileTests_CudaTests_test_2011_01.cu
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ../../testTranslator
    -DSKIP_ROSE_BUILTIN_DECLARATIONS
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_2011_01.cu
)
set_tests_properties(nonsmoke_functional_CompileTests_CudaTests_test_2011_01.cu PROPERTIES LABELS CUDA)

add_executable(deviceTranformation deviceTranformation.C)
target_link_libraries(deviceTranformation ROSE_DLL ${link_with_libraries})

set(_device_transform_tests
  inputCode_01.cu
  test2016_03.cu
)

foreach(file_to_test ${_device_transform_tests})
  add_test(
    NAME CUDATEST_deviceTranformation_${file_to_test}
    COMMAND $<TARGET_FILE:deviceTranformation> -std=c++11 -rose:skipfinalCompileStep
            -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
endforeach()

