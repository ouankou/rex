#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <ctest> <build-directory> <fortran-enabled:0|1>" >&2
  exit 2
fi

ctest_command=$1
build_directory=$2
fortran_enabled=$3

if [[ ! -x "$ctest_command" || ! -d "$build_directory" ]]; then
  echo "Fortran registration contract received an invalid CTest or build path" >&2
  exit 1
fi
if [[ "$fortran_enabled" != 0 && "$fortran_enabled" != 1 ]]; then
  echo "Fortran registration contract requires an exact 0/1 feature state" >&2
  exit 1
fi

expected_count=$fortran_enabled
registry_output=$(
  "$ctest_command" --test-dir "$build_directory" -N \
    -R '^(RunTests_traverseCommonBlock|Translator_testFortranParameter|Translator_testFortranProtected)$'
)
for test_name in \
  RunTests_traverseCommonBlock \
  Translator_testFortranParameter \
  Translator_testFortranProtected; do
  registered_count=$(grep -Ec "Test +#[0-9]+: ${test_name}$" \
    <<<"$registry_output" || true)
  if [[ "$registered_count" -ne "$expected_count" ]]; then
    echo "Fortran registration contract expected $expected_count registration(s) for $test_name, found $registered_count" >&2
    exit 1
  fi
done
