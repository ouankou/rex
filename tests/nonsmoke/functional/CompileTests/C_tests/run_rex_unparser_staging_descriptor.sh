#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0

if (($# != 5)); then
  echo "usage: $0 <translator> <specimen> <preload-source> <c-compiler> <work-directory>" >&2
  exit 2
fi

translator=$1
specimen=$2
preload_source=$3
c_compiler=$4
work=$5

rm -rf "${work}"
mkdir -p "${work}"
cp "${specimen}" "${work}/input.c"

"${c_compiler}" -shared -fPIC -O2 "${preload_source}" -ldl \
  -o "${work}/rex_staging_close_substitution.so"

printf '%s\n' 'REX_STAGING_VICTIM_SENTINEL' >"${work}/victim.c"
LD_PRELOAD="${work}/rex_staging_close_substitution.so" \
REX_STAGING_SUBSTITUTION_TARGET="${work}/victim.c" \
  "${translator}" -rose:verbose 0 -rose:skipfinalCompileStep \
  -rose:output "${work}/output.c" -c "${work}/input.c" \
  >"${work}/translator.log" 2>&1

preload_count=$(grep -Fxc 'REX_STAGING_PRELOAD_LOADED' \
  "${work}/translator.log")
if ((preload_count < 1)); then
  echo "staging substitution preload was not loaded" >&2
  exit 1
fi
if grep -E 'REX_STAGING_(PRELOAD_RESOLUTION|SUBSTITUTION_SETUP)_FAILED' \
    "${work}/translator.log" >/dev/null; then
  echo "staging substitution preload failed" >&2
  sed -n '1,160p' "${work}/translator.log" >&2
  exit 1
fi
if grep -F 'REX_STAGING_SUBSTITUTION_ATTEMPTED' \
    "${work}/translator.log" >/dev/null; then
  echo "the exclusive staging descriptor was closed before commit" >&2
  exit 1
fi

grep -Fx 'REX_STAGING_VICTIM_SENTINEL' "${work}/victim.c" >/dev/null
grep -F 'rex_unparser_output_atomicity' "${work}/output.c" >/dev/null
test ! -L "${work}/output.c"
if find "${work}" -maxdepth 1 -name '.*.rex-unparse-*' -print -quit |
    grep -q .; then
  echo "unparser left a staging file behind" >&2
  exit 1
fi
