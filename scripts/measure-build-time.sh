#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/measure-build-time.sh [--fresh] [--jobs N] [--build-dir DIR] [--] [extra cmake args...]

Measures a clean or existing Ninja Debug build and summarizes compile-time
hotspots from .ninja_log. Extra CMake arguments are appended to configure.
EOF
}

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd -- "${script_dir}/.." && pwd)
source_dir_physical=$(cd -- "${source_dir}" && pwd -P)
build_dir="${source_dir}/build-time"
fresh=FALSE
extra_cmake_args=()

detect_jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu >/dev/null 2>&1; then
    sysctl -n hw.ncpu
  else
    echo 2
  fi
}

run_timed() {
  if [[ -x /usr/bin/time ]] && /usr/bin/time -v true >/dev/null 2>&1; then
    /usr/bin/time -v "$@"
  else
    time "$@"
  fi
}

require_option_value() {
  local option="$1"
  if (($# < 2)); then
    echo "Error: ${option} requires an argument" >&2
    exit 1
  fi
}

require_positive_integer() {
  local option="$1"
  local value="$2"
  if [[ ! "${value}" =~ ^[1-9][0-9]*$ ]]; then
    echo "Error: ${option} requires a positive integer" >&2
    exit 1
  fi
}

canonical_build_dir() {
  local dir="$1"
  local base
  local parent

  if [[ -d "${dir}" ]]; then
    cd -- "${dir}" && pwd -P
    return
  fi

  parent=$(dirname -- "${dir}")
  base=$(basename -- "${dir}")
  if [[ -z "${base}" || "${base}" == "." || "${base}" == ".." ]]; then
    echo "Error: Invalid build directory '${dir}'" >&2
    exit 1
  fi

  if [[ -d "${parent}" ]]; then
    printf '%s/%s\n' "$(cd -- "${parent}" && pwd -P)" "${base}"
  elif [[ "${dir}" == /* ]]; then
    printf '%s\n' "${dir}"
  else
    printf '%s/%s\n' "${PWD}" "${dir}"
  fi
}

require_safe_fresh_build_dir() {
  local dir="$1"
  local physical_dir

  if [[ -z "${dir}" || "${dir}" == "/" ]]; then
    echo "Error: Invalid or dangerous build directory '${dir}'" >&2
    exit 1
  fi

  physical_dir=$(canonical_build_dir "${dir}")
  if [[ "${physical_dir}" == "/" ||
        "${physical_dir}" == "${source_dir_physical}" ||
        "${source_dir_physical}" == "${physical_dir}/"* ]]; then
    echo "Error: Invalid or dangerous build directory '${dir}'" >&2
    exit 1
  fi
}

jobs=$(detect_jobs)

while (($#)); do
  case "$1" in
    --fresh)
      fresh=TRUE
      shift
      ;;
    --jobs)
      require_option_value "$@"
      jobs="$2"
      require_positive_integer "--jobs" "${jobs}"
      shift 2
      ;;
    --build-dir)
      require_option_value "$@"
      build_dir="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      extra_cmake_args=("$@")
      break
      ;;
    *)
      extra_cmake_args+=("$1")
      shift
      ;;
  esac
done

if [[ "${fresh}" == TRUE ]]; then
  require_safe_fresh_build_dir "${build_dir}"
  rm -rf -- "${build_dir}"
fi

mkdir -p -- "${build_dir}"

cmake_args=(
  -S "${source_dir}"
  -B "${build_dir}"
  -G Ninja
  -DCMAKE_BUILD_TYPE=Debug
  -DREX_LINKER=lld
)
cmake_args+=("${extra_cmake_args[@]}")

echo "== Configure =="
run_timed cmake "${cmake_args[@]}"

echo "== Build =="
run_timed cmake --build "${build_dir}" -j"${jobs}"

if [[ ! -f "${build_dir}/.ninja_log" ]]; then
  echo "No .ninja_log found in ${build_dir}" >&2
  exit 1
fi

echo "== Ninja Log Summary =="
awk '
BEGIN { FS = "\t" }
NR > 1 && NF >= 5 {
  duration = ($2 - $1) / 1000.0
  output = $4
  total += duration
  count++
  if (output ~ /^tests\/nonsmoke\/functional/) {
    key = "tests/nonsmoke/functional"
  } else if (output ~ /^tests\//) {
    key = "tests"
  } else if (output ~ /^src\/CMakeFiles\/ROSE_DLL.dir\/frontend\/SageIII\/Cxx_Grammar/) {
    key = "src/ROSETTA generated compile"
  } else if (output ~ /^src\/frontend\/SageIII/) {
    key = "src/frontend/SageIII"
  } else if (output ~ /^src\/frontend\/Clang/) {
    key = "src/frontend/Clang"
  } else if (output ~ /^src\/frontend\/Flang/) {
    key = "src/frontend/Flang"
  } else if (output ~ /^src\/midend\/programAnalysis/) {
    key = "src/midend/programAnalysis"
  } else if (output ~ /^src\/midend\/programTransformation/) {
    key = "src/midend/programTransformation"
  } else if (output ~ /^src\/AstNodes\/Expression/) {
    key = "src/AstNodes/Expression"
  } else if (output ~ /^src\/backend\/unparser/) {
    key = "src/backend/unparser"
  } else {
    split(output, parts, "/")
    key = parts[1] "/" parts[2]
  }
  grouped[key] += duration
  grouped_count[key]++
}
END {
  printf "records %d total_action_sec %.1f\n", count, total
  for (key in grouped) {
    printf "%10.1f %5d %s\n", grouped[key], grouped_count[key], key
  }
}' "${build_dir}/.ninja_log" | (
  read -r summary
  printf '%s\n' "${summary}"
  sort -nr | sed -n '1,30p'
)

echo "== Top Actions =="
awk '
BEGIN { FS = "\t" }
NR > 1 && NF >= 5 {
  printf "%10.3f %s\n", ($2 - $1) / 1000.0, $4
}' "${build_dir}/.ninja_log" | sort -nr | sed -n '1,30p'
