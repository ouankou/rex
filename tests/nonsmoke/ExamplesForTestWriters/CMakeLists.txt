include_directories(${ROSE_INCLUDES})

add_executable(checkCompilation checkCompilation.C)
target_link_libraries(checkCompilation ROSE_DLL ${link_with_libraries})

add_executable(noInputs noInputs.C)
target_link_libraries(noInputs ROSE_DLL ${link_with_libraries})

add_executable(parser parser.C)
target_link_libraries(parser ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)
set(_with_answer ${CMAKE_SOURCE_DIR}/scripts/test_with_answer)

add_test(
  NAME nonsmoke_examples_noInputs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=run something basic [noInputs.passed]"
    USE_SUBDIR=yes
    "CMD=$<TARGET_FILE:noInputs>"
    ${_exit_status}
    noInputs.passed)
set_tests_properties(nonsmoke_examples_noInputs PROPERTIES LABELS "nonsmoke;examples")

# Parser tests with explicit specimens and answer files
set(_parser_specimens i386-fcalls i386-fld i386-lea)
foreach(spec ${_parser_specimens})
  add_test(
    NAME nonsmoke_examples_parser_${spec}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      "TITLE=parsing is accurate for ${spec} [parser_${spec}.passed]"
      USE_SUBDIR=yes
      "CMD=$<TARGET_FILE:parser> ${CMAKE_CURRENT_SOURCE_DIR}/${spec}"
      ANS=${CMAKE_CURRENT_SOURCE_DIR}/parser_${spec}.ans
      ${_with_answer}
      parser_${spec}.passed)
  set_tests_properties(nonsmoke_examples_parser_${spec} PROPERTIES LABELS "nonsmoke;examples")
endforeach()
