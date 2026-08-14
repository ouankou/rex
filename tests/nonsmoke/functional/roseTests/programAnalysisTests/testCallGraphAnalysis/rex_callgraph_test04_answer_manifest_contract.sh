#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 4 ] || [ ! -f "$1" ] || [ ! -d "$2" ] ||
   [ ! -d "$3" ] || ! [[ "$4" =~ ^[1-9][0-9]*$ ]]; then
  echo "usage: $0 manifest answer-dir specimen-dir expected-count" >&2
  exit 2
fi

manifest=$1
answer_dir=$2
specimen_dir=$3
expected_count=$4
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

manifest_entries="$work_dir/manifest"
directory_entries="$work_dir/directory"
cp "$manifest" "$manifest_entries"

if [ "$(wc -l < "$manifest_entries")" -ne "$expected_count" ]; then
  echo "callgraph test04 manifest does not contain exactly $expected_count entries" >&2
  exit 1
fi
if ! LC_ALL=C sort -c "$manifest_entries"; then
  echo "callgraph test04 manifest is not sorted" >&2
  exit 1
fi
if [ -n "$(uniq -d "$manifest_entries")" ]; then
  echo "callgraph test04 manifest contains duplicate entries" >&2
  exit 1
fi

shopt -s nullglob
answer_files=("$answer_dir"/*.C.cg.dmp)
if [ "${#answer_files[@]}" -ne "$expected_count" ]; then
  echo "callgraph test04 answer directory does not contain exactly $expected_count files" >&2
  exit 1
fi
printf '%s\n' "${answer_files[@]##*/}" | LC_ALL=C sort > "$directory_entries"
if ! cmp -s "$manifest_entries" "$directory_entries"; then
  echo "callgraph test04 manifest and answer directory differ" >&2
  diff -u "$manifest_entries" "$directory_entries" >&2 || true
  exit 1
fi

while IFS= read -r answer_name; do
  if [[ ! "$answer_name" =~ ^[A-Za-z0-9_.+-]+[.]C[.]cg[.]dmp$ ]]; then
    echo "malformed callgraph test04 manifest entry: $answer_name" >&2
    exit 1
  fi
  specimen=${answer_name%.cg.dmp}
  if [ ! -f "$specimen_dir/$specimen" ]; then
    echo "callgraph test04 answer has no source specimen: $specimen" >&2
    exit 1
  fi
done < "$manifest_entries"
