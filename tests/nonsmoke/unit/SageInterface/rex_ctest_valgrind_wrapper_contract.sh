#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 7 ]]; then
  echo "usage: $0 <wrapper> <bash> <c-compiler> <cxx-compiler> <fortran-compiler> <work-directory> <configured-memcheck-options>..." >&2
  exit 2
fi

wrapper="$1"
shift
bash_executable="$1"
shift
compilers=("$1" "$2" "$3")
work_directory="$4"
configured_options=("${@:5}")

run_wrapper() {
  "$bash_executable" "$wrapper" "${configured_options[@]}" "$@"
}

if [[ ! -x "$wrapper" ]]; then
  echo "configured CTest Valgrind wrapper is not executable: $wrapper" >&2
  exit 1
fi
if [[ ! -x "$bash_executable" ]]; then
  echo "configured Bash executable is not executable: $bash_executable" >&2
  exit 1
fi

rm -rf "$work_directory"
mkdir -p "$work_directory"

for compiler in "${compilers[@]}"; do
  if [[ ! -x "$compiler" ]]; then
    echo "configured compiler is not executable: $compiler" >&2
    exit 1
  fi
  compiler_name="$(basename "$compiler")"
  compiler_log="$work_directory/$compiler_name.log"
  run_wrapper --log-file="$compiler_log" \
    "$compiler" --version >/dev/null
  if [[ -e "$compiler_log" ]]; then
    echo "compiler-only fixture was incorrectly instrumented: $compiler" >&2
    exit 1
  fi
done

probe_source="$work_directory/rex-valgrind-boundary-probe.c"
probe_binary="$work_directory/rex-valgrind-boundary-probe"
printf '%s\n' 'int main(void) { return 0; }' >"$probe_source"
"${compilers[0]}" -std=c11 "$probe_source" -o "$probe_binary"
if [[ ! -x "$probe_binary" ]]; then
  echo "configured C compiler did not produce the Valgrind boundary probe" >&2
  exit 1
fi

rex_boundary_log="$work_directory/rex-boundary.log"
run_wrapper --log-file="$rex_boundary_log" \
  "$probe_binary"
if [[ ! -s "$rex_boundary_log" ]] ||
   ! grep -Fq "Memcheck" "$rex_boundary_log"; then
  echo "non-compiler command did not remain under Valgrind" >&2
  exit 1
fi

for script_mode in plain split; do
  shebang='#!/usr/bin/env bash'
  if [[ "$script_mode" == split ]]; then
    shebang='#!/usr/bin/env -S bash -eu'
  fi
  env_bash_script="$work_directory/rex-env-bash-$script_mode-probe.sh"
  printf '%s\n' \
    "$shebang" \
    'set -euo pipefail' \
    '[[ ${PERL_DESTRUCT_LEVEL:?} == 2 ]]' \
    '"$1"' >"$env_bash_script"
  chmod +x "$env_bash_script"

  script_log_pattern="$work_directory/rex-env-bash-$script_mode-%p.log"
  run_wrapper --trace-children=yes \
    --log-file="$script_log_pattern" "$env_bash_script" "$probe_binary"
  shopt -s nullglob
  script_logs=("$work_directory"/rex-env-bash-"$script_mode"-*.log)
  shopt -u nullglob
  if ((${#script_logs[@]} < 2)); then
    echo "env-bash script and its child were not both instrumented by Valgrind: $script_mode" >&2
    exit 1
  fi
  if ! grep -Fq "Command: $probe_binary" "${script_logs[@]}"; then
    echo "env-bash script child did not remain under Valgrind: $script_mode" >&2
    exit 1
  fi
done

perl_executable="$(command -v perl)"
perl_cleanup_log="$work_directory/rex-perl-cleanup.log"
run_wrapper --leak-check=full \
  --errors-for-leak-kinds=definite,indirect,possible --error-exitcode=79 \
  --log-file="$perl_cleanup_log" "$perl_executable" \
  -e 'for (1..10000) { my $value = "needle" x 10; }'
if ! grep -Fq 'ERROR SUMMARY: 0 errors from 0 contexts' "$perl_cleanup_log"; then
  echo "Perl interpreter teardown did not complete cleanly" >&2
  exit 1
fi

child_probe_source="$(dirname "$0")/rex_ctest_valgrind_child_report.c"
child_probe_binary="$work_directory/rex_ctest_valgrind_child_report"
"${compilers[0]}" -std=c11 -O0 "$child_probe_source" -o "$child_probe_binary"
child_report="$work_directory/rex_ctest_valgrind_child_report.log"
parent_status=0
run_wrapper --trace-children=yes --error-exitcode=79 \
  --log-file="$child_report" "$child_probe_binary" || parent_status=$?
if [[ "$parent_status" -ne 79 ]]; then
  echo "a successful parent hid its child's memory failure: $parent_status" >&2
  exit 1
fi
if [[ "$(grep -Ec '^==[0-9]+== Memcheck,' "$child_report")" -ne 2 ]] ||
   [[ "$(grep -Ec '^==[0-9]+== ERROR SUMMARY:' "$child_report")" -ne 2 ]] ||
   ! grep -Eq 'ERROR SUMMARY: [1-9][0-9]* errors' "$child_report" ||
   ! grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$child_report"; then
  echo "the successful parent lost its failing child's Valgrind report" >&2
  exit 1
fi

child_status=0
run_wrapper --error-exitcode=79 \
  --log-file="$work_directory/rex_ctest_valgrind_child_status.log" \
  "$child_probe_binary" --child || child_status=$?
if [[ "$child_status" -ne 79 ]]; then
  echo "Valgrind's failure exit code was not preserved: $child_status" >&2
  exit 1
fi

# A config's VALGRIND placeholder is optional. Both forms must instrument the
# actual child, even when a shell command deliberately ignores its error exit.
rth="$(dirname "$0")/../../../../scripts/rth_run.pl"
for placeholder in absent present; do
  rth_config="$work_directory/rex_rth_${placeholder}.conf"
  rth_report="$work_directory/rex_rth_${placeholder}.log"
  prefix=""
  if [[ "$placeholder" == present ]]; then
    prefix='${VALGRIND} '
  fi
  printf 'cmd = %s"${PROBE}" --child; true\n' "$prefix" >"$rth_config"
  rth_status=0
  run_wrapper --trace-children=yes --error-exitcode=79 \
    --log-file="$rth_report" "$rth" "PROBE=$child_probe_binary" \
    "$rth_config" "$work_directory/rex_rth_${placeholder}.passed" || rth_status=$?
  if [[ "$rth_status" -ne 79 ]] ||
     ! grep -Fq "Command: $child_probe_binary --child" "$rth_report" ||
     ! grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$rth_report"; then
    echo "RTH config with $placeholder VALGRIND placeholder hid its child: $rth_status" >&2
    exit 1
  fi
done

disabled_trace_status=0
run_wrapper --trace-children=no \
  --log-file="$work_directory/rex_rth_disabled_trace.log" "$rth" \
  "PROBE=$child_probe_binary" "$rth_config" \
  "$work_directory/rex_rth_disabled_trace.passed" \
  2>"$work_directory/rex_rth_disabled_trace.stderr" || disabled_trace_status=$?
if [[ "$disabled_trace_status" -ne 1 ]] ||
   ! grep -Fq 'RTH Memcheck requires --trace-children=yes' \
     "$work_directory/rex_rth_disabled_trace.stderr"; then
  echo "RTH accepted disabled child tracing: $disabled_trace_status" >&2
  exit 1
fi

true_executable="$(type -P true)"
exec_status=0
run_wrapper --trace-children=yes \
  --trace-children-skip="$true_executable" --error-exitcode=79 \
  --log-file="$work_directory/rex_ctest_valgrind_exec_status.log" \
  "$child_probe_binary" --exec-after-error "$true_executable" || exec_status=$?
if [[ "$exec_status" -ne 79 ]]; then
  echo "an exec into an excluded tool hid an earlier memory error: $exec_status" >&2
  exit 1
fi

instrumented_exec_report="$work_directory/rex_ctest_valgrind_instrumented_exec.log"
exec_status=0
run_wrapper --trace-children=yes --error-exitcode=79 \
  --log-file="$instrumented_exec_report" \
  "$child_probe_binary" --exec-after-error "$probe_binary" || exec_status=$?
if [[ "$exec_status" -ne 79 ]] ||
   [[ "$(grep -Ec '^==[0-9]+== Memcheck,' "$instrumented_exec_report")" -ne 2 ]] ||
   ! grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$instrumented_exec_report"; then
  echo "an instrumented exec lost an earlier report from the same PID: $exec_status" >&2
  exit 1
fi

fork_exec_report="$work_directory/rex_ctest_valgrind_fork_exec.log"
fork_status=0
run_wrapper --trace-children=yes --error-exitcode=79 \
  --log-file="$fork_exec_report" \
  "$child_probe_binary" --fork-before-exec "$probe_binary" || fork_status=$?
if [[ "$fork_status" -ne 79 ]] ||
   ! grep -Fq 'Conditional jump or move depends on uninitialised value(s)' "$fork_exec_report"; then
  echo "a child's report between fork and exec was lost: $fork_status" >&2
  exit 1
fi

parallel_report="$work_directory/rex_ctest_valgrind_parallel.log"
parallel_status=0
run_wrapper --trace-children=yes --error-exitcode=79 \
  --log-file="$parallel_report" "$child_probe_binary" --parallel || parallel_status=$?
if [[ "$parallel_status" -ne 79 ]] ||
   [[ "$(grep -Ec '^==[0-9]+== ERROR SUMMARY:' "$parallel_report")" -ne 9 ]] ||
   [[ "$(grep -Fc 'Conditional jump or move depends on uninitialised value(s)' "$parallel_report")" -ne 8 ]]; then
  echo "concurrent children did not retain all eight memory errors: $parallel_status" >&2
  exit 1
fi

command_status=0
run_wrapper \
  --log-file="$work_directory/rex_ctest_valgrind_command_status.log" \
  "$bash_executable" -c 'exit 23' || command_status=$?
if [[ "$command_status" -ne 23 ]]; then
  echo "a command's own failure exit code was not preserved: $command_status" >&2
  exit 1
fi

# A report directory can also own Valgrind's crash artifacts. Exercise their
# retention without requiring the host to permit core dumps or allocating a
# compiler-sized core file. The real instrumented command must still fail.
artifact_report="$work_directory/rex_ctest_valgrind_artifact.log"
artifact_status=0
run_wrapper --log-file="$artifact_report" "$bash_executable" -c '
  artifacts=("$1".stream.*)
  test "${#artifacts[@]}" -eq 1 || exit 31
  printf "%s\n" "retained diagnostic" >"${artifacts[0]}/diagnostic.core"
  exit 23
' dummy "$artifact_report" || artifact_status=$?
artifact_directories=("$artifact_report".stream.*)
if [[ "$artifact_status" -ne 23 ]] ||
   [[ "${#artifact_directories[@]}" -ne 1 ]] ||
   [[ ! -s "${artifact_directories[0]}/diagnostic.core" ]] ||
   [[ -e "${artifact_directories[0]}/report.pipe" ]] ||
   ! grep -Fq 'ERROR SUMMARY: 0 errors' "$artifact_report"; then
  echo "report artifact cleanup lost diagnostics or command status: $artifact_status" >&2
  exit 1
fi

malformed_script="$work_directory/rex-malformed-env-boundary-probe.sh"
printf '%s\n' '#!/usr/bin/env' 'exit 0' >"$malformed_script"
chmod +x "$malformed_script"
malformed_stderr="$work_directory/rex-malformed-env.stderr"
if run_wrapper "$malformed_script" \
  2>"$malformed_stderr"; then
  echo "malformed env shebang was not rejected" >&2
  exit 1
fi
if ! grep -Fq "env shebang does not name an interpreter" \
  "$malformed_stderr"; then
  echo "malformed env shebang did not report its exact contract violation" >&2
  exit 1
fi
