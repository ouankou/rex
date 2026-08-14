#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 6 ]]; then
  echo "usage: $0 <wrapper> <bash> <c-compiler> <cxx-compiler> <fortran-compiler> <work-directory>" >&2
  exit 2
fi

wrapper="$1"
shift
bash_executable="$1"
shift
compilers=("$1" "$2" "$3")
work_directory="$4"

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
  "$bash_executable" "$wrapper" --log-file="$compiler_log" \
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
"$bash_executable" "$wrapper" --log-file="$rex_boundary_log" \
  "$probe_binary"
if [[ ! -s "$rex_boundary_log" ]] ||
   ! grep -Fq "Memcheck" "$rex_boundary_log"; then
  echo "non-compiler command did not remain under Valgrind" >&2
  exit 1
fi

env_bash_script="$work_directory/rex-env-bash-boundary-probe.sh"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'set -euo pipefail' \
  '"$1"' >"$env_bash_script"
chmod +x "$env_bash_script"

script_log_pattern="$work_directory/rex-env-bash-%p.log"
"$bash_executable" "$wrapper" --trace-children=yes \
  --log-file="$script_log_pattern" "$env_bash_script" "$probe_binary"
shopt -s nullglob
script_logs=("$work_directory"/rex-env-bash-*.log)
shopt -u nullglob
if ((${#script_logs[@]} < 2)); then
  echo "env-bash script and its child were not both instrumented by Valgrind" >&2
  exit 1
fi
if ! grep -Fq "Command: $probe_binary" "${script_logs[@]}"; then
  echo "env-bash script child did not remain under Valgrind" >&2
  exit 1
fi

malformed_script="$work_directory/rex-malformed-env-boundary-probe.sh"
printf '%s\n' '#!/usr/bin/env' 'exit 0' >"$malformed_script"
chmod +x "$malformed_script"
malformed_stderr="$work_directory/rex-malformed-env.stderr"
if "$bash_executable" "$wrapper" "$malformed_script" \
  2>"$malformed_stderr"; then
  echo "malformed env shebang was not rejected" >&2
  exit 1
fi
if ! grep -Fq "env shebang does not name an interpreter" \
  "$malformed_stderr"; then
  echo "malformed env shebang did not report its exact contract violation" >&2
  exit 1
fi
