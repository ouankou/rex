include_guard(GLOBAL)

# Both directive families use the same typed empty-body and trivia contract.
function(rex_add_fortran_empty_region_tests family form source)
  if(family STREQUAL "omp")
    set(_name OMPFORTRAN_rex_empty_region_${form})
    set(_flags -rose:openmp:ast_only)
    set(_labels "OMPFORTRAN;ASTJSON;UNPARSER;rex;empty-region")
    set(_family_argument)
  elseif(family STREQUAL "acc")
    set(_name OpenACC_fortran_rex_empty_region_${form})
    set(_flags -rose:openacc:ast_only)
    set(_labels "OpenACC_fortran;ASTJSON;UNPARSER;rex;empty-region")
    set(_family_argument --empty-region-family=acc)
  else()
    message(FATAL_ERROR "Unknown empty-region test family: ${family}")
  endif()
  get_filename_component(_filename ${source} NAME)
  set(_work ${CMAKE_CURRENT_BINARY_DIR}/empty-region-${form})
  set(_output ${_work}/rose_${_filename})
  file(MAKE_DIRECTORY ${_work})
  add_test(NAME ${_name}_ast_json
    WORKING_DIRECTORY ${_work}
    COMMAND $<TARGET_FILE:rex_fortran_empty_region_contract>
      ${_family_argument} ${_flags} -rex:ast-json-checkpoint=all
      -rex:ast-json-dir=${_work}/json
      -rose:skipfinalCompileStep -rose:output ${_output} -c ${source})
  set_tests_properties(${_name}_ast_json
    PROPERTIES FIXTURES_SETUP ${_name}_output LABELS "${_labels}")
  add_test(NAME ${_name}_surface
    COMMAND ${BASH_EXECUTABLE} -c [=[
      set -eu
      output=$1
      family=$2
      previous=0
      for marker in REX_EMPTY_BEFORE REX_EMPTY_BEGIN_INLINE REX_EMPTY_INSIDE_FIRST REX_EMPTY_INSIDE_LAST REX_EMPTY_END_INLINE REX_EMPTY_AFTER; do
        test "$(grep -Fc -- "$marker" "$output")" -eq 1
        position=$(grep -nF -- "$marker" "$output")
        position=${position%%:*}
        test "$position" -gt "$previous"
        previous=$position
      done
      begin_pattern="^[[:space:]]*![$]${family}[[:space:]]+parallel([[:space:]]|$)"
      end_pattern="^[[:space:]]*![$]${family}[[:space:]]+end[[:space:]]+parallel([[:space:]]|$)"
      test "$(grep -Eic "$begin_pattern" "$output")" -eq 2
      test "$(grep -Eic "$end_pattern" "$output")" -eq 2
      first_begin=$(grep -nEi "$begin_pattern" "$output" | head -n 1)
      begin=$(grep -nEi "$begin_pattern" "$output" | tail -n 1)
      end=$(grep -nEi "$end_pattern" "$output" | tail -n 1)
      before=$(grep -nF REX_EMPTY_BEFORE "$output")
      begin_inline=$(grep -nF REX_EMPTY_BEGIN_INLINE "$output")
      first=$(grep -nF REX_EMPTY_INSIDE_FIRST "$output")
      last=$(grep -nF REX_EMPTY_INSIDE_LAST "$output")
      after=$(grep -nF REX_EMPTY_AFTER "$output")
      end_inline=$(grep -nF REX_EMPTY_END_INLINE "$output")
      test "${before%%:*}" -lt "${first_begin%%:*}"
      test "${begin_inline%%:*}" -ge "${begin%%:*}"
      test "${first%%:*}" -gt "${begin%%:*}"
      test "${last%%:*}" -lt "${end%%:*}"
      test "${after%%:*}" -gt "${end%%:*}"
      test "${end_inline%%:*}" -ge "${end%%:*}"
    ]=] dummy ${_output} ${family})
  set_tests_properties(${_name}_surface
    PROPERTIES FIXTURES_REQUIRED ${_name}_output LABELS "${_labels}")
endfunction()
