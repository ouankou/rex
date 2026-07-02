set(RODINIA_LOWERING_CASES
    rodinia_axpy_multi_like
    rodinia_bfs_like
    rodinia_gaussian_like
    rodinia_hotspot_like
    rodinia_inactive_output_like
    rodinia_nn_like
    rodinia_pathfinder_like
    rodinia_srad_comments_like
    rodinia_srad_v2_like
    rodinia_btree_kernel_like
)

set(_omp_rodinia_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/test-output")
file(MAKE_DIRECTORY "${_omp_rodinia_test_output_dir}")

function(add_rodinia_lowering_test case_name)
  set(_input_file ${CMAKE_CURRENT_SOURCE_DIR}/inputs/${case_name}.c)
  set(_workdir ${_omp_rodinia_test_output_dir}/${case_name})

  add_test(
    NAME OMPLOWERING_RODINIA_${case_name}
    WORKING_DIRECTORY "${_omp_rodinia_test_output_dir}"
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_and_check.sh
            $<TARGET_FILE:parseOmp>
            ${_input_file}
            ${_workdir}
            ${case_name})

  set_tests_properties(
    OMPLOWERING_RODINIA_${case_name}
    PROPERTIES
      LABELS "OMPLOWERING;OMPLOWERING_RODINIA"
  )
endfunction()

function(add_rodinia_ast_json_lowering_test case_name)
  set(_input_file ${CMAKE_CURRENT_SOURCE_DIR}/inputs/${case_name}.c)
  set(_workdir ${_omp_rodinia_test_output_dir}/ast-json/${case_name})

  add_test(
    NAME ASTJSON_OMPLOWERING_RODINIA_${case_name}
    WORKING_DIRECTORY "${_omp_rodinia_test_output_dir}"
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_checkpoint_compare.sh
            $<TARGET_FILE:parseOmp>
            ${_input_file}
            ${_workdir}
            ${case_name})

  set_tests_properties(
    ASTJSON_OMPLOWERING_RODINIA_${case_name}
    PROPERTIES
      LABELS "ASTJSON;OMPLOWERING;OMPLOWERING_RODINIA"
  )
endfunction()

foreach(_case_name IN LISTS RODINIA_LOWERING_CASES)
  add_rodinia_lowering_test(${_case_name})
  add_rodinia_ast_json_lowering_test(${_case_name})
endforeach()
