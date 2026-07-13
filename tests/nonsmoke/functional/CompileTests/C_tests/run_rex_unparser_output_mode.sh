#!/usr/bin/env bash
set -euo pipefail

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

run_translator() {
  "${translator}" -rose:verbose 0 -rose:skipfinalCompileStep \
    -rose:output "$1" -c "${work}/input.c"
}

assert_no_staging_files() {
  if find "${work}" -maxdepth 1 -name '.*.rex-unparse-*' -print -quit |
      grep -q .; then
    echo "unparser left a staging file behind" >&2
    return 1
  fi
}

# A new output must acquire the ordinary 0666 creation mode filtered by the
# caller's umask, not mkstemp's fixed 0600 staging mode.
(
  umask 0027
  run_translator "${work}/new-output.c"
)
test "$(stat -c %a "${work}/new-output.c")" = 640
assert_no_staging_files

# Replacing an existing output must retain its complete permission mode even
# when the current umask would remove those bits from a newly created file.
printf '%s\n' 'REX_STALE_OUTPUT_SENTINEL' >"${work}/existing-output.c"
chmod 0751 "${work}/existing-output.c"
(
  umask 0077
  run_translator "${work}/existing-output.c"
)
test "$(stat -c %a "${work}/existing-output.c")" = 751
grep -F 'rex_unparser_output_atomicity' \
  "${work}/existing-output.c" >/dev/null
assert_no_staging_files
