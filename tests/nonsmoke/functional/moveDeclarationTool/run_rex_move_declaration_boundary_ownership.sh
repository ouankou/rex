#!/usr/bin/env bash
set -euo pipefail

if (($# != 4)); then
  echo "usage: $0 <tool> <compiler> <specimen> <work-directory>" >&2
  exit 2
fi

tool=$1
compiler=$2
specimen=$3
work=$4

rm -rf "${work}"
mkdir -p "${work}"

run_mode() {
  local mode=$1
  shift
  local output="${work}/${mode}.C"

  "${tool}" \
    -rose:verbose 0 \
    -std=gnu++14 \
    -rose:debug \
    -rose:trans-tracking \
    -rose:unparse_tokens \
    "$@" \
    -c "${specimen}" \
    -rose:output "${output}"

  "${compiler}" -std=gnu++14 -fsyntax-only "${output}"
  "${compiler}" -std=gnu++14 -fsyntax-only \
    -DREX_MOVE_DECLARATION_LONG_RESULT=1 "${output}"
  "${compiler}" -std=gnu++14 -fsyntax-only \
    -DREX_MOVE_DECLARATION_INACTIVE_BRANCH=1 "${output}"

  test "$(grep -Fc '#define REX_MOVE_DECLARATION_VALUE(index)' \
    "${output}")" -eq 1
  test "$(grep -Fc '#ifdef REX_MOVE_DECLARATION_LONG_RESULT' \
    "${output}")" -eq 1
  test "$(grep -Fc '#ifdef REX_MOVE_DECLARATION_INACTIVE_BRANCH' \
    "${output}")" -eq 1
  test "$(grep -Fc '#else' "${output}")" -eq 2
  test "$(grep -Fc '#endif' "${output}")" -eq 2
}

run_mode v1
run_mode v2 -rose:merge_decl_assign
run_mode v3 \
  -rose:merge_decl_assign

for output in "${work}/v1.C" "${work}/v2.C" "${work}/v3.C"; do
  if grep -Eq '(struct|union|enum)[[:space:]]+__anonymous_0x' "${output}"; then
    echo "REX_MOVE_DECLARATION_INVARIANT[anonymous-tag]: generated identity leaked into ${output}" >&2
    exit 1
  fi
  test "$(grep -Ec 'union[[:space:]]*\{' "${output}")" -eq 1
done

# Transformation tracking records metadata; it must not choose a different
# source-formatting path.  Hold token replay and the transformation constant,
# then prove that enabling tracking cannot change the generated source.
no_tracking_output="${work}/v2_no_tracking.C"
"${tool}" \
  -rose:verbose 0 \
  -std=gnu++14 \
  -rose:debug \
  -rose:unparse_tokens \
  -rose:merge_decl_assign \
  -c "${specimen}" \
  -rose:output "${no_tracking_output}"
"${compiler}" -std=gnu++14 -fsyntax-only "${no_tracking_output}"
cmp -s "${work}/v2.C" "${no_tracking_output}"

# The non-merge mode must preserve the complete spelled macro invocation.
# Compiling catches malformed if headers; this assertion distinguishes a
# complete invocation from semantic AST spelling in the merge modes.
grep -Eq 'result[[:space:]]*=[[:space:]]*REX_MOVE_DECLARATION_VALUE[[:space:]]*\([[:space:]]*index[[:space:]]*\)[[:space:]]*\+[[:space:]]*tagged_value\.integer_value[[:space:]]*;' \
  "${work}/v1.C"
