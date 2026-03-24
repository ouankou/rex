include_directories(${ROSE_INCLUDES})

add_executable(testVariableRenaming testVariableRenaming.C)
target_link_libraries(testVariableRenaming ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_specimen_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)

set(_excluded_specimens
  test2004_02.C
  test2005_114.C
  test2005_187.C
  test2006_47.C
  test2006_54.C
  test2011_78.C
)

set(_variable_renaming_specimens ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM _variable_renaming_specimens ${_excluded_specimens})
list(APPEND _variable_renaming_specimens test2003_24.C test2004_82.C)
list(REMOVE_DUPLICATES _variable_renaming_specimens)

set(_variable_renaming_flags
  -w -rose:verbose
  -I${_specimen_dir}
)
string(JOIN " " _variable_renaming_flags_str ${_variable_renaming_flags})

foreach(specimen IN LISTS _variable_renaming_specimens)
  string(REPLACE "." "_" _name ${specimen})
  rex_get_cxx_specimen_extra_flags(_extra_flags ${specimen})
  set(_extra_flags_str "")
  if(_extra_flags)
    string(JOIN " " _extra_flags_str ${_extra_flags})
  endif()
  set(_variable_renaming_cmd
    "$<TARGET_FILE:testVariableRenaming> ${_variable_renaming_flags_str}")
  if(_extra_flags_str)
    string(APPEND _variable_renaming_cmd " ${_extra_flags_str}")
  endif()
  string(APPEND _variable_renaming_cmd
    " -c ${_specimen_dir}/${specimen}")
  add_test(
    NAME variable_renaming_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      "CMD=${_variable_renaming_cmd}"
      ${_exit_status}
      tvr_${specimen}.passed
  )
endforeach()

set(_variable_renaming_tests)
foreach(specimen IN LISTS _variable_renaming_specimens)
  string(REPLACE "." "_" _name ${specimen})
  list(APPEND _variable_renaming_tests "variable_renaming_${_name}")
endforeach()
