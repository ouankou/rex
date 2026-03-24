set(files_to_test
  empty.cl
  transpose_kernel.cl
  nbody.cl
  nbody_cpu.cl
  nbody_gpu.cl
)

set(ROSE_FLAGS -rose:skipfinalCompileStep)

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} OPENCLTEST TRUE)
endforeach()

add_test(
  NAME OPENCLTEST_empty
  COMMAND testTranslator ${ROSE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/empty.cl
)
set_tests_properties(OPENCLTEST_empty PROPERTIES LABELS "OPENCLTEST")
