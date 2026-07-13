#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 4 ]]; then
  echo "usage: $0 LOG EXPECTED_DIAGNOSTIC -- COMMAND [ARG ...]" >&2
  exit 2
fi

log_file=$1
expected_diagnostic=$2
shift 2

if [[ -z $expected_diagnostic ]]; then
  echo "$0: EXPECTED_DIAGNOSTIC must not be empty" >&2
  exit 2
fi
if [[ $expected_diagnostic == --any-clang-diagnostic ]]; then
  echo "$0: --any-clang-diagnostic has been removed; require the exact source diagnostic" >&2
  exit 2
fi
if [[ $expected_diagnostic != *error:* ]]; then
  echo "$0: EXPECTED_DIAGNOSTIC must contain a Clang error marker" >&2
  exit 2
fi

if [[ $1 != -- ]]; then
  echo "$0: missing command separator '--'" >&2
  exit 2
fi
shift

if [[ $# -eq 0 ]]; then
  echo "$0: missing frontend command" >&2
  exit 2
fi

ulimit -c 0
set +e
"$@" >"$log_file" 2>&1
status=$?
set -e

fail() {
  echo "$0: $1" >&2
  echo "----- frontend output: $log_file -----" >&2
  sed -n '1,240p' "$log_file" >&2
  exit 1
}

if ((status == 0)); then
  fail "frontend command unexpectedly succeeded"
fi
if ((status != 134)); then
  fail "frontend command did not terminate through the required hard abort (status $status)"
fi

if ! grep -Eq '^.*:[0-9]+:[0-9]+: (fatal )?error:' "$log_file"; then
  fail "frontend command did not emit a source-located Clang diagnostic"
fi

expected_diagnostic_count=0
source_diagnostic_pattern='^(.*:[0-9]+:[0-9]+): ((fatal )?error:.*)$'
while IFS= read -r diagnostic_line; do
  if [[ $diagnostic_line =~ $source_diagnostic_pattern ]]; then
    diagnostic_location=${BASH_REMATCH[1]}
    diagnostic_suffix=${BASH_REMATCH[2]}
    diagnostic_with_location="${diagnostic_location##*/}: $diagnostic_suffix"
    if [[ $expected_diagnostic == "$diagnostic_suffix" ||
          $expected_diagnostic == "$diagnostic_with_location" ]]; then
      ((++expected_diagnostic_count))
    fi
  fi
done <"$log_file"
if ((expected_diagnostic_count == 0)); then
  fail "frontend command did not emit the expected diagnostic: $expected_diagnostic"
fi

hard_stop_pattern='^REX_FRONTEND_INVARIANT\[clang-diagnostics\]: Clang reported [1-9][0-9]* diagnostic error\(s\); frontend AST translation is forbidden$'
hard_stop_count=$(grep -Ec "$hard_stop_pattern" "$log_file" || true)
if ((hard_stop_count != 1)); then
  fail "frontend command did not emit exactly one Clang frontend hard-stop"
fi
