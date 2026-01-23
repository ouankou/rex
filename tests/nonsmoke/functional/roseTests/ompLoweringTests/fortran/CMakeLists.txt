set(_omp_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran)

set(_omp_fortran_includes
  -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
  -I${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering)

set(_omp_fortran_flags ${_omp_fortran_includes} -rose:openmp:lowering -g)

set(_omp_fortran_f77_compile_tests
  array-one.f
  shared-array.f
)

set(_omp_fortran_f77_run_tests
  array_initialize.f
  helloworld.f
  jacobi.f
  master.f
  ompdo-decremental.f
  ompdo-default.f
  ompdo-orphaned.f
  ompdo-static.f
  ompdo-multiple.f
  task_largenumber.f
  task_scalar.f
  task_simplest.f
  private.f
  reduction.f
  shared.f
  single.f
)

set(_omp_fortran_f77_failing_tests
  parallel-if.f
  parallel-if-numthreads.f
  section1.f
)

if(enable-fortran)
  foreach(specimen IN LISTS _omp_fortran_f77_compile_tests _omp_fortran_f77_run_tests)
    string(REPLACE "." "_" _name ${specimen})
    add_test(
      NAME omp_lowering_fortran_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:roseomp> ${_omp_fortran_flags}
        -c ${_omp_fortran_dir}/${specimen}
    )
  endforeach()

  foreach(specimen IN LISTS _omp_fortran_f77_failing_tests)
    string(REPLACE "." "_" _name ${specimen})
    add_test(
      NAME omp_lowering_fortran_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND $<TARGET_FILE:roseomp> ${_omp_fortran_flags}
        -c ${_omp_fortran_dir}/${specimen}
    )
  endforeach()
endif()
