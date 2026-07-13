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

output="${work}/collision-output.c"
staging_manifest="${work}/staging-name.txt"
(
  stale_staging="${work}/.collision-output.c.rex-unparse-${BASHPID}-0"
  printf '%s\n' 'REX_STALE_STAGING_SENTINEL' >"${stale_staging}"
  printf '%s\n' "${stale_staging}" >"${staging_manifest}"
  exec "${translator}" -rose:verbose 0 -rose:skipfinalCompileStep \
    -rose:output "${output}" -c "${work}/input.c"
)

stale_staging=$(<"${staging_manifest}")
test -f "${stale_staging}"
grep -Fx 'REX_STALE_STAGING_SENTINEL' "${stale_staging}" >/dev/null
grep -F 'rex_unparser_output_atomicity' "${output}" >/dev/null

rm "${stale_staging}"
if find "${work}" -maxdepth 1 -name '.*.rex-unparse-*' -print -quit |
    grep -q .; then
  echo "unparser left a newly created staging file behind" >&2
  exit 1
fi
