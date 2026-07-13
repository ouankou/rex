#!/bin/sh

set -eu

mode=$1
output=$2
compiler=$3

test -s "$output"
"$compiler" -std=gnu++14 -Wno-attributes -fsyntax-only "$output"

test "$(grep -Fo '/* exact declarator boundary */' "$output" | wc -l)" -eq 1
test "$(grep -Eo '__attribute__[[:space:]]*\(\([[:space:]]*unused[[:space:]]*\)\)' "$output" | wc -l)" -eq 1
test "$(grep -Eo '__attribute__[[:space:]]*\(\([[:space:]]*used[[:space:]]*\)\)' "$output" | wc -l)" -eq 1

tr '\n' ' ' <"$output" >"${output}.single-line"
grep -Eq 'int[[:space:]]+rex_group_comment_a[[:space:]]*=[[:space:]]*1[[:space:]]*,[[:space:]]*/\* exact declarator boundary \*/[[:space:]]*rex_group_comment_b[[:space:]]*=[[:space:]]*2[[:space:]]*;' "${output}.single-line"
grep -Eq 'rex_group_attribute_a[[:space:]]+__attribute__[[:space:]]*\(\([[:space:]]*unused[[:space:]]*\)\)[[:space:]]*=[[:space:]]*1[[:space:]]*,[[:space:]]*rex_group_attribute_b[[:space:]]+__attribute__[[:space:]]*\(\([[:space:]]*used[[:space:]]*\)\)[[:space:]]*=[[:space:]]*2[[:space:]]*;' "${output}.single-line"

file_invocations=$(grep -Ec '^[[:space:]]*REX_GROUP_FILE_TERMINATED_DECLARATION[[:space:]]*;[[:space:]]*$' "$output" || true)
macro_invocations=$(grep -Ec '^[[:space:]]*REX_GROUP_MACRO_DECLARATION[[:space:]]*$' "$output" || true)

case "$mode" in
ast)
  test "$file_invocations" -eq 0
  test "$macro_invocations" -eq 0
  test "$(grep -ow 'rex_group_file_macro_a' "$output" | wc -l)" -eq 3
  test "$(grep -ow 'rex_group_macro_a' "$output" | wc -l)" -eq 3
  ;;
token)
  test "$file_invocations" -eq 1
  test "$macro_invocations" -eq 1
  test "$(grep -ow 'rex_group_file_macro_a' "$output" | wc -l)" -eq 2
  test "$(grep -ow 'rex_group_macro_a' "$output" | wc -l)" -eq 2
  ;;
*)
  echo "unknown declaration-group surface mode: $mode" >&2
  exit 2
  ;;
esac

rm -f "${output}.single-line"
