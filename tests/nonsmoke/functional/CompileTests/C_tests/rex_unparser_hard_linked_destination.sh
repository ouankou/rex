#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0

if (($# != 3)); then
  echo "usage: $0 <translator> <specimen> <work-directory>" >&2
  exit 2
fi

translator=$1
specimen=$2
work=$3

rm -rf "${work}"
mkdir -p "${work}"
cp "${specimen}" "${work}/input.c"
printf '%s\n' 'REX_HARD_LINK_TARGET_SENTINEL' >"${work}/target.c"
cp "${work}/target.c" "${work}/target.expected"
ln "${work}/target.c" "${work}/output.c"

target_inode=$(stat -c %i "${work}/target.c")
output_inode=$(stat -c %i "${work}/output.c")
test "${target_inode}" = "${output_inode}"
test "$(stat -c %h "${work}/target.c")" -eq 2

set +e
"${translator}" -rose:verbose 0 -rose:skipfinalCompileStep \
  -rose:output "${work}/output.c" -c "${work}/input.c" \
  >"${work}/translator.log" 2>&1
status=$?
set -e

if ((status != 134)); then
  echo "expected translator SIGABRT status 134, got ${status}" >&2
  sed -n '1,160p' "${work}/translator.log" >&2
  exit 1
fi

test "$(grep -Fxc -- \
  "REX_UNPARSE_INVARIANT[output-destination]: output=${work}/output.c is a regular file with 2 links; atomic replacement requires exactly one destination link" \
  "${work}/translator.log")" -eq 1
cmp "${work}/target.expected" "${work}/target.c"
cmp "${work}/target.expected" "${work}/output.c"
test "$(stat -c %i "${work}/target.c")" = "${target_inode}"
test "$(stat -c %i "${work}/output.c")" = "${output_inode}"
test "$(stat -c %h "${work}/target.c")" -eq 2

if find "${work}" -maxdepth 1 -name '.*.rex-unparse-*' -print -quit |
    grep -q .; then
  echo "unparser left a staging file behind" >&2
  exit 1
fi
