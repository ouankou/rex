#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 4 ]]; then
  echo "usage: $0 LOG EXPECTED-DIAGNOSTIC -- COMMAND [ARG ...]" >&2
  exit 2
fi

log_file=$1
expected_diagnostic=$2
shift 2
if [[ $1 != -- ]]; then
  echo "$0: missing command separator '--'" >&2
  exit 2
fi
shift
if [[ $# -eq 0 || -z $expected_diagnostic ]]; then
  echo "$0: command and expected diagnostic are required" >&2
  exit 2
fi

ulimit -c 0
set +e
"$@" >"$log_file" 2>&1
status=$?
set -e

if ((status != 134)); then
  echo "$0: command did not terminate with SIGABRT status 134 (got $status)" >&2
  sed -n '1,240p' "$log_file" >&2
  exit 1
fi
count=$(grep -Fxc -- "$expected_diagnostic" "$log_file" || true)
if ((count != 1)); then
  echo "$0: expected exactly one hard diagnostic: $expected_diagnostic" >&2
  sed -n '1,240p' "$log_file" >&2
  exit 1
fi
