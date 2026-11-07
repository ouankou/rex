include_guard(GLOBAL)

function(rex_add_fortran_include_region_tests family)
  set(_surface_checker
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/check_rex_fortran_include_region_surface.cmake)
  if(family STREQUAL "omp")
    set(_language openmp)
    set(_prefix OMPFORTRAN)
    set(_mode -rose:openmp:ast_only)
    set(_native_flag -fopenmp)
  elseif(family STREQUAL "acc")
    set(_language openacc)
    set(_prefix OpenACC_fortran)
    set(_mode -rose:openacc:ast_only)
    set(_native_flag -fopenacc)
  else()
    message(FATAL_ERROR "Unknown include-region family: ${family}")
  endif()
  foreach(_form IN ITEMS free fixed)
    if(_form STREQUAL "free")
      set(_extension f90)
    else()
      set(_extension f)
    endif()
    foreach(_case IN ITEMS empty nonempty repeat)
      set(_name ${_prefix}_rex_include_region_${_case}_${_form})
      set(_source ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_${_language}_include_${_case}_${_form}.${_extension})
      set(_base ${CMAKE_CURRENT_BINARY_DIR}/include-region-${_case}-${_form})
      set(_native ${_base}/native-oracle)
      if(_case STREQUAL "repeat")
        set(_count 3)
      else()
        set(_count 1)
      endif()
      file(MAKE_DIRECTORY ${_native})
      add_test(NAME ${_name}_native_oracle
        WORKING_DIRECTORY ${_native}
        COMMAND ${BASH_EXECUTABLE} -c [=[
          set -eu
          compiler=$1; family=$2; flag=$3; source=$4; directory=$5; work=$6; runtime=$7; expected=$8; cmake=$9; checker=${10}; layout=${11}
          "$compiler" "$flag" -fsyntax-only "-I$directory" "$source"
          "$compiler" -fc1 "$flag" -fdebug-unparse "-I$directory" "$source" > "$work/canonical.f90"
          test "$(grep -Eic "^[[:space:]]*![$]${family}[[:space:]]+parallel([[:space:]]|$)" "$work/canonical.f90")" -eq "$expected"
          test "$(grep -Eic "^[[:space:]]*![$]${family}[[:space:]]+end[[:space:]]+parallel([[:space:]]|$)" "$work/canonical.f90")" -eq "$expected"
          "$cmake" "-DINPUT=$source" "-DSOURCE_DIRECTORY=$directory" "-DFAMILY=$family" "-DLAYOUT=$layout" -P "$checker"
          if test "$family" = omp; then
            "$compiler" "$flag" "-I$directory" "-L$runtime" "-Wl,-rpath,$runtime" "$source" -o "$work/oracle"
            env OMP_NUM_THREADS=2 OMP_DYNAMIC=FALSE "$work/oracle" > "$work/oracle.out"
            test "$(tr -d '[:space:]' < "$work/oracle.out")" = 7
          fi
        ]=] dummy ${CMAKE_Fortran_COMPILER} ${family} ${_native_flag} ${_source}
          ${CMAKE_CURRENT_SOURCE_DIR} ${_native} "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" ${_count}
          ${CMAKE_COMMAND} ${_surface_checker} ${_case})
      set_tests_properties(${_name}_native_oracle PROPERTIES
        FIXTURES_SETUP ${_name}_valid_native
        LABELS "${_prefix};rex;empty-region;include-region;native-oracle" TIMEOUT 180)
      foreach(_checkpoint IN ITEMS native json)
        set(_work ${_base}/${_checkpoint})
        get_filename_component(_filename ${_source} NAME)
        set(_output ${_work}/rose_${_filename})
        set(_json_args)
        if(_checkpoint STREQUAL "json")
          set(_json_args -rex:ast-json-checkpoint=all -rex:ast-json-dir=${_work}/json)
        endif()
        file(MAKE_DIRECTORY ${_work})
        add_test(NAME ${_name}_${_checkpoint}_ast
          WORKING_DIRECTORY ${_work}
          COMMAND ${BASH_EXECUTABLE} -c
            "set -eu; output=\"$1\"; shift; rm -f -- \"$output\"; exec \"$@\""
            dummy ${_output} $<TARGET_FILE:rex_fortran_include_region_contract>
            --include-family=${family} --include-case=${_case}
            --include-directory=${CMAKE_CURRENT_SOURCE_DIR}
            ${_mode} ${_json_args} -I${CMAKE_CURRENT_SOURCE_DIR}
            -rose:skipfinalCompileStep -rose:output ${_output} -c ${_source})
        set_tests_properties(${_name}_${_checkpoint}_ast PROPERTIES
          FIXTURES_REQUIRED ${_name}_valid_native
          FIXTURES_SETUP ${_name}_${_checkpoint}_output
          LABELS "${_prefix};ASTJSON;UNPARSER;rex;empty-region;include-region" TIMEOUT 180)
        add_test(NAME ${_name}_${_checkpoint}_unparse
          WORKING_DIRECTORY ${_work}
          COMMAND ${BASH_EXECUTABLE} -c [=[
            set -eu
            compiler=$1; family=$2; flag=$3; source=$4; directory=$5; work=$6; runtime=$7; expected=$8; oracle=$9; cmake=${10}; checker=${11}; layout=${12}
            "$compiler" "$flag" -fsyntax-only "-I$directory" "$source"
            "$compiler" -fc1 "$flag" -fdebug-unparse "-I$directory" "$source" > "$work/canonical.f90"
            test "$(grep -Eic "^[[:space:]]*![$]${family}[[:space:]]+parallel([[:space:]]|$)" "$work/canonical.f90")" -eq "$expected"
            test "$(grep -Eic "^[[:space:]]*![$]${family}[[:space:]]+end[[:space:]]+parallel([[:space:]]|$)" "$work/canonical.f90")" -eq "$expected"
            "$cmake" "-DINPUT=$source" "-DSOURCE_DIRECTORY=$directory" "-DFAMILY=$family" "-DLAYOUT=$layout" -P "$checker"
            if test "$family" = omp; then
              "$compiler" "$flag" "-I$directory" "-L$runtime" "-Wl,-rpath,$runtime" "$source" -o "$work/translated"
              env OMP_NUM_THREADS=2 OMP_DYNAMIC=FALSE "$work/translated" > "$work/translated.out"
              cmp "$oracle/oracle.out" "$work/translated.out"
            fi
          ]=] dummy ${CMAKE_Fortran_COMPILER} ${family} ${_native_flag} ${_output}
            ${CMAKE_CURRENT_SOURCE_DIR} ${_work} "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" ${_count} ${_native}
            ${CMAKE_COMMAND} ${_surface_checker} ${_case})
        set_tests_properties(${_name}_${_checkpoint}_unparse PROPERTIES
          FIXTURES_REQUIRED "${_name}_valid_native;${_name}_${_checkpoint}_output"
          LABELS "${_prefix};ASTJSON;UNPARSER;rex;empty-region;include-region" TIMEOUT 180)
      endforeach()
    endforeach()
  endforeach()
endfunction()

# These ordinary Fortran cases distinguish native INCLUDE source events from
# CPP include records, including macro definitions visible after the header.
function(rex_add_fortran_include_provenance_tests)
  foreach(_kind IN ITEMS cpp_state macro_native selected_scope)
    set(_name OMPFORTRAN_rex_include_${_kind})
    set(_source ${CMAKE_CURRENT_SOURCE_DIR}/rex_fortran_include_${_kind}.F90)
    set(_base ${CMAKE_CURRENT_BINARY_DIR}/include-provenance-${_kind})
    set(_native ${_base}/native-oracle)
    set(_flag "")
    set(_expected 7)
    set(_checkpoints native json)
    if(_kind STREQUAL "selected_scope")
      set(_flag -fopenmp)
      set(_expected 791)
      list(APPEND _checkpoints generic generic_json)
    endif()
    file(MAKE_DIRECTORY ${_native})
    add_test(NAME ${_name}_native_oracle
      WORKING_DIRECTORY ${_native}
      COMMAND ${BASH_EXECUTABLE} -c [=[
        set -eu
        compiler=$1; source=$2; directory=$3; work=$4; flag=$5; runtime=$6; expected=$7
        flags=()
        if test -n "$flag"; then flags+=("$flag" "-L$runtime" "-Wl,-rpath,$runtime"); fi
        rm -f -- "$work/oracle" "$work/oracle.out"
        "$compiler" "${flags[@]}" "-I$directory" "$source" -o "$work/oracle"
        env OMP_NUM_THREADS=2 OMP_DYNAMIC=FALSE "$work/oracle" > "$work/oracle.out"
        test "$(tr -d '[:space:]' < "$work/oracle.out")" = "$expected"
      ]=] dummy ${CMAKE_Fortran_COMPILER} ${_source} ${CMAKE_CURRENT_SOURCE_DIR} ${_native}
        "${_flag}" "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" ${_expected})
    set_tests_properties(${_name}_native_oracle PROPERTIES
      FIXTURES_SETUP ${_name}_valid_native
      LABELS "OMPFORTRAN;rex;include-region;include-provenance;native-oracle" TIMEOUT 180)
    foreach(_checkpoint IN LISTS _checkpoints)
      set(_work ${_base}/${_checkpoint})
      set(_output ${_work}/rose_rex_fortran_include_${_kind}.F90)
      set(_json_args)
      if(_checkpoint MATCHES "json")
        set(_json_args -rex:ast-json-checkpoint=all -rex:ast-json-dir=${_work}/json)
      endif()
      set(_rex_flags)
      set(_checker_flags)
      if(_kind STREQUAL "selected_scope")
        if(_checkpoint MATCHES "generic")
          set(_rex_flags -rose:openmp:parse_only)
          set(_checker_flags --include-generic)
        else()
          set(_rex_flags -rose:openmp:ast_only)
          if(_checkpoint STREQUAL "json")
            list(APPEND _checker_flags --include-negatives)
          endif()
        endif()
      endif()
      file(MAKE_DIRECTORY ${_work})
      add_test(NAME ${_name}_${_checkpoint}_ast
        WORKING_DIRECTORY ${_work}
        COMMAND ${BASH_EXECUTABLE} -c
          "set -eu; output=\"$1\"; shift; rm -f -- \"$output\"; exec \"$@\""
          dummy ${_output} $<TARGET_FILE:rex_fortran_include_region_contract>
          --include-family=omp --include-case=${_kind}
          --include-directory=${CMAKE_CURRENT_SOURCE_DIR}
          ${_checker_flags} ${_rex_flags} ${_json_args}
          -I${CMAKE_CURRENT_SOURCE_DIR} -rose:skipfinalCompileStep
          -rose:output ${_output} -c ${_source})
      set_tests_properties(${_name}_${_checkpoint}_ast PROPERTIES
        FIXTURES_REQUIRED ${_name}_valid_native
        FIXTURES_SETUP ${_name}_${_checkpoint}_output
        LABELS "OMPFORTRAN;ASTJSON;UNPARSER;rex;include-region;include-provenance" TIMEOUT 180)
      add_test(NAME ${_name}_${_checkpoint}_runtime
        WORKING_DIRECTORY ${_work}
        COMMAND ${BASH_EXECUTABLE} -c [=[
          set -eu
          compiler=$1; kind=$2; source=$3; directory=$4; work=$5; oracle=$6; flag=$7; runtime=$8; checkpoint=$9
          if test "$kind" = cpp_state; then
            test "$(grep -Fxc '#include "rex_fortran_include_cpp_state.inc"' "$source")" -eq 1
            test "$(grep -Eic '^[[:space:]]*include[[:space:]]' "$source")" -eq 0
            test "$(grep -Fxc '!$omp barrier ! REX_SELECTED_DISABLED' "$source")" -eq 1
          elif test "$kind" = macro_native; then
            pattern="^[[:space:]]*include[[:space:]]+['\"]rex_fortran_include_macro_native[.]inc['\"]([[:space:]]|$)"
            test "$(grep -Eic "$pattern" "$source")" -eq 1
          else
            test "$(grep -Fxc '! REX_SELECTED_CONTEXT_EOF' "$source")" -eq 1
            if [[ "$checkpoint" == generic* ]]; then
              test "$(grep -Fxc '#define REX_SCOPE_ROOT_VALUE 9' "$source")" -eq 1
              test "$(grep -Fxc '#include "rex_fortran_include_selected_cpp.inc"' "$source")" -eq 1
              test "$(grep -Eic '^[[:space:]]*include[[:space:]]' "$source")" -eq 2
            else
              test "$(grep -Eic '^[[:space:]]*(#|include[[:space:]])' "$source")" -eq 0
              test "$(grep -Eic '^[[:space:]]*![$]ompx[[:space:]]+rex_selected_opaque[(]"space payload"[)][[:space:]]*$' "$source")" -eq 1
              test "$(grep -Ec 'REX_SCOPE_THREADS|REX_SCOPE_OPAQUE_PAYLOAD' "$source")" -eq 0
              test "$(grep -Fic '!dir$ fixed' "$source")" -eq 0
            fi
          fi
          flags=()
          if test -n "$flag"; then flags+=("$flag" "-L$runtime" "-Wl,-rpath,$runtime"); fi
          rm -f -- "$work/translated" "$work/translated.out"
          "$compiler" "${flags[@]}" "-I$directory" "$source" -o "$work/translated"
          env OMP_NUM_THREADS=2 OMP_DYNAMIC=FALSE "$work/translated" > "$work/translated.out"
          cmp "$oracle/oracle.out" "$work/translated.out"
        ]=] dummy ${CMAKE_Fortran_COMPILER} ${_kind} ${_output}
          ${CMAKE_CURRENT_SOURCE_DIR} ${_work} ${_native} "${_flag}" "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" ${_checkpoint})
      set_tests_properties(${_name}_${_checkpoint}_runtime PROPERTIES
        FIXTURES_REQUIRED "${_name}_valid_native;${_name}_${_checkpoint}_output"
        LABELS "OMPFORTRAN;ASTJSON;UNPARSER;rex;include-region;include-provenance;runtime" TIMEOUT 180)
    endforeach()
  endforeach()
endfunction()
