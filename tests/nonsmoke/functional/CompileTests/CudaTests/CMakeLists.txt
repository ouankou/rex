if(NOT ENABLE-CUDA)
  add_test(
    NAME CUDA_rex_cuda_frontend_requires_enabled_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=rex_cuda_frontend_requires_enabled_support.log; rm -f \"\$log\"; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 134; test \"\$(grep -Fxc 'REX CUDA frontend input requires ENABLE-CUDA=ON.' \"\$log\")\" -eq 1"
      dummy
      $<TARGET_FILE:testTranslator>
      -rose:skip_unparse
      -rose:skipfinalCompileStep
      -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_cuda_kernel_local_pointer.cu)
  set_tests_properties(CUDA_rex_cuda_frontend_requires_enabled_support
    PROPERTIES LABELS "CUDA;rex;death")
  return()
endif()

add_executable(rex_cuda_ast_contract_translator
  rex_cuda_ast_contract_translator.cpp)
target_link_libraries(rex_cuda_ast_contract_translator
  ROSE_DLL ${link_with_libraries})

set(CUDA_NO_BUILT_IN_CODES
  test_2011_01.cu
  rose-2123.cu
)
set(CUDA_CODES
  test_2011_11.cu
  test_2011_12.cu
  test_2011_13.cu
  header_test.cu
  mm_single.cu
  rex_cuda_kernel_local_pointer.cu
)

set(ROSE_FLAGS -rose:skipfinalCompileStep)
set(files_to_test ${CUDA_NO_BUILT_IN_CODES} ${CUDA_CODES})

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} CUDA TRUE)
endforeach()

rex_add_clang_frontend_negative_test(
  CUDA_test_2011_02_cu
  CUDA
  ${CMAKE_CURRENT_BINARY_DIR}
  rex_cuda_reject_device_local_variable.log
  "error: __constant__, __device__, and __managed__ are not allowed on non-static local variables"
  $<TARGET_FILE:${translator}>
  -rose:skipfinalCompileStep
  -c ${CMAKE_CURRENT_SOURCE_DIR}/test_2011_02.cu)

add_test(
  NAME CUDA_rex_cuda_deduced_template_kernel_launch_structure
  COMMAND
    $<TARGET_FILE:rex_cuda_ast_contract_translator>
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_cuda_deduced_template_kernel_launch.cu
)
set_tests_properties(
  CUDA_rex_cuda_deduced_template_kernel_launch_structure
  PROPERTIES LABELS CUDA)

add_test(
  NAME CUDA_rex_cuda_dependent_logical_result_type_structure
  COMMAND
    $<TARGET_FILE:rex_cuda_ast_contract_translator>
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_cuda_dependent_logical_result_type.cu
)
set_tests_properties(
  CUDA_rex_cuda_dependent_logical_result_type_structure
  PROPERTIES LABELS CUDA)

add_test(
  NAME CUDA_test_2011_01
  COMMAND testTranslator -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_2011_01.cu
)
set_tests_properties(CUDA_test_2011_01 PROPERTIES LABELS CUDA)

add_test(
  NAME nonsmoke_functional_CompileTests_CudaTests_test_2011_01.cu
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator>
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
