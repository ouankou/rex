#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ] || [ ! -x "$1" ]; then
  echo "usage: $0 NoTabCharacters.pl fixture-directory" >&2
  exit 2
fi

policy_dir=$(dirname "$1")
policy=$(cd "$policy_dir" && pwd -P)/$(basename "$1")
fixture=$2
fixture_parent=$(dirname "$fixture")
cmake -E rm -rf "$fixture"
cmake -E make_directory "$fixture/owned" "$fixture/external/parser/src"

printf '%s\n' \
  '[submodule "external/parser"]' \
  '  path = external/parser' \
  '  url = https://example.invalid/parser.git' >"$fixture/.gitmodules"
printf '%s\n' 'int owned_source;' >"$fixture/owned/source.c"
printf 'rule:\tTOKEN\n' >"$fixture/external/parser/src/parser.yy"

(
  cd "$fixture"
  GIT_CEILING_DIRECTORIES="$fixture_parent" "$policy" .
)

printf 'int\towned_source;\n' >"$fixture/owned/source.c"
if (
  cd "$fixture"
  GIT_CEILING_DIRECTORIES="$fixture_parent" "$policy" .
) >"$fixture/owned-failure.stdout" 2>"$fixture/owned-failure.stderr"; then
  echo "policy accepted a tab in superproject-owned source" >&2
  exit 1
fi

if ! grep -Fq 'owned/source.c' "$fixture/owned-failure.stdout"; then
  echo "policy did not report the superproject-owned source" >&2
  exit 1
fi
if grep -Fq 'external/parser' "$fixture/owned-failure.stdout"; then
  echo "policy crossed the source-snapshot submodule boundary" >&2
  exit 1
fi
