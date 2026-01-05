set(files_to_test
  empty.cl
  transpose_kernel.cl
  nbody.cl
  nbody_cpu.cl
  nbody_gpu.cl
)

set(ROSE_FLAGS -rose:skipfinalCompileStep)
set(ROSE_DISABLED_TESTCODES)
if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(ROSE_DISABLED_TESTCODES
    transpose_kernel.cl
    nbody.cl
    nbody_cpu.cl
    nbody_gpu.cl
  )
endif()

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} OPENCLTEST TRUE)
endforeach()
