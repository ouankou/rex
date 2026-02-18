set(RODINIA_LOWERING_CASES
    rodinia_bfs_like
    rodinia_srad_comments_like
    rodinia_btree_kernel_like
)

function(add_rodinia_lowering_test case_name)
  set(_input_file ${CMAKE_CURRENT_SOURCE_DIR}/inputs/${case_name}.c)
  set(_workdir ${CMAKE_CURRENT_BINARY_DIR}/${case_name})

  add_test(
    NAME OMPLOWERING_RODINIA_${case_name}
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

foreach(_case_name IN LISTS RODINIA_LOWERING_CASES)
  add_rodinia_lowering_test(${_case_name})
endforeach()
