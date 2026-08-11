#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 .dockerignore ci/docker/rex-nightly.Dockerfile" >&2
  exit 2
fi

dockerignore=$1
dockerfile=$2
for input in "$dockerignore" "$dockerfile"; do
  if [ ! -f "$input" ]; then
    echo "required image input does not exist: $input" >&2
    exit 2
  fi
done

if grep -Eq '^[[:space:]]*(/|\*\*/)?\.github(/(\*\*)?)?[[:space:]]*$' \
  "$dockerignore"; then
  echo ".dockerignore excludes workflow sources required by CI-contract tests" >&2
  exit 1
fi

builder_start=$(grep -nE '^FROM[[:space:]].*[[:space:]]AS[[:space:]]builder[[:space:]]*$' \
  "$dockerfile" | cut -d: -f1)
builder_end=$(awk -v start="$builder_start" \
  'NR > start && /^FROM[[:space:]]/ { print NR; exit }' "$dockerfile")
configure_line=$(grep -n 'build-rex\.sh' "$dockerfile" | cut -d: -f1)
builder_arg_line=$(grep -nE '^ARG[[:space:]]+REX_ENABLE_VALGRIND=0[[:space:]]*$' \
  "$dockerfile" | awk -F: -v start="$builder_start" -v end="$builder_end" \
  '$1 > start && $1 < end { print $1 }')
builder_install_line=$(grep -n 'apt-get install.*valgrind' "$dockerfile" | \
  awk -F: -v start="$builder_start" -v end="$builder_end" \
  '$1 > start && $1 < end { print $1 }')

if [ -z "$builder_start" ] || [ -z "$builder_end" ] || \
   [ -z "$configure_line" ] || [ -z "$builder_arg_line" ] || \
   [ -z "$builder_install_line" ]; then
  echo "nightly image has no exact builder-side Valgrind registration contract" >&2
  exit 1
fi
if [ "$builder_arg_line" -ge "$configure_line" ] || \
   [ "$builder_install_line" -ge "$configure_line" ]; then
  echo "Valgrind must be selected and installed before CMake registers tests" >&2
  exit 1
fi

test_stage_arg_count=$(awk \
  '/^FROM[[:space:]].*[[:space:]]AS[[:space:]]test[[:space:]]*$/ { in_test=1; next }
   in_test && /^FROM[[:space:]]/ { in_test=0 }
   in_test && /^ARG[[:space:]]+REX_ENABLE_VALGRIND=0[[:space:]]*$/ { ++count }
   END { print count + 0 }' "$dockerfile")
if [ "$test_stage_arg_count" -ne 1 ]; then
  echo "nightly test stage must consume the same Valgrind selection" >&2
  exit 1
fi
