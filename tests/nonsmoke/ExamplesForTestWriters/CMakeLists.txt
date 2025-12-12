include_directories(${ROSE_INCLUDES} ${Boost_INCLUDE_DIRS})

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
    TITLE="run something basic [noInputs.passed]"
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:noInputs>"
    ${_exit_status}
    noInputs.passed)
set_tests_properties(nonsmoke_examples_noInputs PROPERTIES LABELS "nonsmoke;examples")

# Optional binary specimens used by the parser/runAlgorithm examples
set(NONSMOKE_BINARY_SPECIMENS_ROOT "${ROSE_TOP_SRC_DIR}/tests/nonsmoke/specimens/binary" CACHE PATH "Root directory for non-smoke binary specimens")
set(_specimen_sets "${NONSMOKE_BINARY_SPECIMENS_ROOT}/SpecimenSets.inc")

function(parse_make_list OUT_VAR MAKE_FILE VAR_NAME)
  if(NOT EXISTS "${MAKE_FILE}")
    set(${OUT_VAR} "" PARENT_SCOPE)
    return()
  endif()
  file(READ "${MAKE_FILE}" _make_content)
  # Flatten line continuations to simplify extraction
  string(REGEX REPLACE "\\\\[ \t]*\n" " " _make_content "${_make_content}")
  string(REGEX MATCH "${VAR_NAME}[ \t]*=[^\n]*" _match "${_make_content}")
  if(NOT _match)
    set(${OUT_VAR} "" PARENT_SCOPE)
    return()
  endif()
  string(REGEX REPLACE "${VAR_NAME}[ \t]*=" "" _values "${_match}")
  string(REGEX REPLACE "#[^\\n]*" "" _values "${_values}")
  separate_arguments(_list UNIX_COMMAND "${_values}")
  set(${OUT_VAR} "${_list}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# parser tests with explicit specimens and answer files
set(_parser_specimens i386-fcalls i386-fld i386-lea)
set(_parser_found_inputs "")
if(EXISTS "${NONSMOKE_BINARY_SPECIMENS_ROOT}")
  foreach(spec ${_parser_specimens})
    if(EXISTS "${NONSMOKE_BINARY_SPECIMENS_ROOT}/${spec}")
      list(APPEND _parser_found_inputs ${spec})
    endif()
  endforeach()
endif()

if(_parser_found_inputs)
  set(_parser_enabled TRUE)
  foreach(spec ${_parser_found_inputs})
    add_test(
      NAME nonsmoke_examples_parser_${spec}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        TITLE="parsing is accurate for ${spec} [parser_${spec}.passed]"
        USE_SUBDIR=yes
        CMD="$<TARGET_FILE:parser> ${NONSMOKE_BINARY_SPECIMENS_ROOT}/${spec}"
        ANS=${CMAKE_CURRENT_SOURCE_DIR}/parser_${spec}.ans
        ${_with_answer}
        parser_${spec}.passed)
    set_tests_properties(nonsmoke_examples_parser_${spec} PROPERTIES LABELS "nonsmoke;examples")
  endforeach()
else()
  message(STATUS "Skipping nonsmoke ExamplesForTestWriters parser answer tests; specimens not found in ${NONSMOKE_BINARY_SPECIMENS_ROOT}")
endif()

# ---------------------------------------------------------------------------
# parser2 tests using the broader specimen set (excluding mips)
parse_make_list(_all_binary_exe "${_specimen_sets}" "nonsmoke_specimens_binary_elf_exe_any")
if(_all_binary_exe)
  set(_parser2_specimens "")
  foreach(item ${_all_binary_exe})
    if(NOT item MATCHES "mips")
      list(APPEND _parser2_specimens ${item})
    endif()
  endforeach()
endif()

if(_parser2_specimens)
  foreach(spec ${_parser2_specimens})
    add_test(
      NAME nonsmoke_examples_parser2_${spec}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        TITLE="parsing passes (no answer) for ${spec} [parser2_${spec}.passed]"
        USE_SUBDIR=yes
        CMD="$<TARGET_FILE:parser> ${NONSMOKE_BINARY_SPECIMENS_ROOT}/${spec}"
        ${_exit_status}
        parser2_${spec}.passed)
    set_tests_properties(nonsmoke_examples_parser2_${spec} PROPERTIES LABELS "nonsmoke;examples")
  endforeach()
elseif(EXISTS "${NONSMOKE_BINARY_SPECIMENS_ROOT}")
  message(STATUS "Skipping nonsmoke parser2 tests; SpecimenSets.inc not available at ${_specimen_sets}")
endif()

# ---------------------------------------------------------------------------
# runAlgorithm tests reusing the smoke checker; requires binary specimen sets
parse_make_list(_binary_amd64 "${_specimen_sets}" "nonsmoke_specimens_binary_elf_exe_amd64")
if(TARGET runAlgorithm AND _binary_amd64)
  foreach(spec ${_binary_amd64})
    add_test(
      NAME nonsmoke_examples_runAlgorithm_${spec}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        TITLE="algorithm foo on ${spec} [runAlgorithm_${spec}.passed]"
        USE_SUBDIR=yes
        CMD="$<TARGET_FILE:runAlgorithm> --algorithm=foo ${spec}"
        ${_exit_status}
        runAlgorithm_${spec}.passed)
    set_tests_properties(nonsmoke_examples_runAlgorithm_${spec} PROPERTIES LABELS "nonsmoke;examples")
  endforeach()
elseif(TARGET runAlgorithm)
  message(STATUS "Skipping nonsmoke runAlgorithm specimen tests; SpecimenSets.inc not available at ${_specimen_sets}")
else()
  message(STATUS "Skipping nonsmoke runAlgorithm specimen tests; smoke checker target not built")
endif()
