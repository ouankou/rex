#!/usr/bin/env bash
set -euo pipefail

if (($# < 4)); then
  echo "usage: $0 <translator> <source> <output> <translator-args...>" >&2
  exit 2
fi

translator=$1
source_file=$2
output_file=$3
shift 3

count_exact_occurrences() {
  local file=$1
  local needle=$2
  awk -v needle="$needle" '
    {
      line = $0
      while ((at = index(line, needle)) != 0) {
        ++count
        line = substr(line, at + length(needle))
      }
    }
    END { print count + 0 }
  ' "$file"
}

check_source_tokens() {
  local file=$1
  local expected
  for expected in \
    '#define REX_SOURCE_TOKEN_VALUE 7' \
    '#define REX_SOURCE_TOKEN_TEXT "macro bang ! remains preprocessing text"' \
    '#define REX_SOURCE_TOKEN_SUM(lhs, rhs) \' \
    '  ((lhs) + (rhs))' \
    '#if 0' \
    'this inactive source is intentionally not valid Fortran' \
    '#define REX_SOURCE_TOKEN_INACTIVE 1' \
    '#endif' \
    'literal bang ! remains character data' \
    '! standalone source-token comment' \
    '! trailing source-token comment' \
    '!$ompx rex_source_token_opaque' \
    'message(REX_SOURCE_TOKEN_TEXT)'; do
    local count
    count=$(count_exact_occurrences "$file" "$expected")
    if [[ $count -ne 1 ]]; then
      echo "expected exactly one exact source token in ${file}: ${expected} (found ${count})" >&2
      exit 1
    fi
  done
}

rm -f "$output_file" "${output_file}.roundtrip"
"$translator" "$@" -rose:output "$output_file" -c "$source_file"
test -s "$output_file"
check_source_tokens "$output_file"

"$translator" "$@" -rose:output "${output_file}.roundtrip" -c "$output_file"
test -s "${output_file}.roundtrip"
check_source_tokens "${output_file}.roundtrip"
