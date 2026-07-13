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
input_path=$(realpath "${work}/input.c")

run_translator() {
  "${translator}" -rose:verbose 0 -rose:skipfinalCompileStep "$@"
}

expect_abort() {
  local log=$1
  shift
  local status

  set +e
  run_translator "$@" >"${log}" 2>&1
  status=$?
  set -e

  if ((status != 134)); then
    echo "expected translator SIGABRT status 134, got ${status}" >&2
    sed -n '1,160p' "${log}" >&2
    exit 1
  fi
}

assert_no_staging_files() {
  if find "${work}" -maxdepth 1 -name '.*.rex-unparse-*' -print -quit |
      grep -q .; then
    echo "unparser left a staging file behind" >&2
    return 1
  fi
}

# Normal replacement is atomic and removes the stale destination as one rename.
printf '%s\n' 'REX_STALE_OUTPUT_SENTINEL' >"${work}/output.c"
run_translator -rose:output "${work}/output.c" -c "${work}/input.c"
grep -F 'rex_unparser_output_atomicity' "${work}/output.c" >/dev/null
if grep -F 'REX_STALE_OUTPUT_SENTINEL' "${work}/output.c" >/dev/null; then
  echo "stale output survived replacement" >&2
  exit 1
fi
assert_no_staging_files

# Symlink and hard-link aliases of the input must fail before touching either
# path, even though their spellings differ.
cp "${work}/input.c" "${work}/input.expected"
ln -s input.c "${work}/symlink-output.c"
expect_abort "${work}/symlink.log" -rose:output \
  "${work}/symlink-output.c" -c "${work}/input.c"
test "$(grep -Fxc -- \
  "Error: refusing to overwrite unparser input without -rose:unparser:clobber_input_file: \"${input_path}\"" \
  "${work}/symlink.log")" -eq 1
cmp "${work}/input.expected" "${work}/input.c"
test -L "${work}/symlink-output.c"
assert_no_staging_files

ln "${work}/input.c" "${work}/hardlink-output.c"
expect_abort "${work}/hardlink.log" -rose:output \
  "${work}/hardlink-output.c" -c "${work}/input.c"
test "$(grep -Fxc -- \
  "Error: refusing to overwrite unparser input without -rose:unparser:clobber_input_file: \"${input_path}\"" \
  "${work}/hardlink.log")" -eq 1
cmp "${work}/input.expected" "${work}/input.c"
test "$(stat -c %i "${work}/input.c")" = \
  "$(stat -c %i "${work}/hardlink-output.c")"
assert_no_staging_files

# Output commit must inspect the destination entry itself, not follow it. A
# symlink to an unrelated regular file must fail without replacing the link or
# modifying its target.
printf '%s\n' 'REX_SYMLINK_TARGET_SENTINEL' >"${work}/symlink-target.c"
cp "${work}/symlink-target.c" "${work}/symlink-target.expected"
ln -s symlink-target.c "${work}/symlink-destination.c"
expect_abort "${work}/symlink-destination.log" -rose:output \
  "${work}/symlink-destination.c" -c "${work}/input.c"
test "$(grep -Fxc -- \
  "REX_UNPARSE_INVARIANT[output-permissions]: output=${work}/symlink-destination.c is not a regular file; symlinks and other special files are not valid output destinations" \
  "${work}/symlink-destination.log")" -eq 1
test -L "${work}/symlink-destination.c"
test "$(readlink "${work}/symlink-destination.c")" = symlink-target.c
cmp "${work}/symlink-target.expected" "${work}/symlink-target.c"
assert_no_staging_files

# A dangling symlink is still an existing special destination and must receive
# the same hard rejection rather than being treated as a nonexistent path.
ln -s missing-target.c "${work}/dangling-destination.c"
expect_abort "${work}/dangling-destination.log" -rose:output \
  "${work}/dangling-destination.c" -c "${work}/input.c"
test "$(grep -Fxc -- \
  "REX_UNPARSE_INVARIANT[output-permissions]: output=${work}/dangling-destination.c is not a regular file; symlinks and other special files are not valid output destinations" \
  "${work}/dangling-destination.log")" -eq 1
test -L "${work}/dangling-destination.c"
test "$(readlink "${work}/dangling-destination.c")" = missing-target.c
test ! -e "${work}/missing-target.c"
assert_no_staging_files

# A noclobber mismatch is generated into a sibling staging file, compared,
# rejected, and removed without changing the existing destination.
printf '%s\n' 'REX_NOCLOBBER_SENTINEL' >"${work}/noclobber.c"
expect_abort "${work}/noclobber.log" \
  -rose:noclobber_if_different_output_file \
  -rose:output "${work}/noclobber.c" -c "${work}/input.c"
test "$(grep -Ec '^Error: files are not equivalent: $' \
  "${work}/noclobber.log")" -eq 1
grep -Fx 'REX_NOCLOBBER_SENTINEL' "${work}/noclobber.c" >/dev/null
assert_no_staging_files

# Identical noclobber output succeeds without replacing the existing inode.
run_translator -rose:output "${work}/identical.c" -c "${work}/input.c"
identical_inode=$(stat -c %i "${work}/identical.c")
run_translator -rose:noclobber_if_different_output_file \
  -rose:output "${work}/identical.c" -c "${work}/input.c"
test "${identical_inode}" = "$(stat -c %i "${work}/identical.c")"
assert_no_staging_files

# Formatted character extraction used to skip whitespace during comparison.
# A same-size whitespace-only difference must still be rejected byte-for-byte.
run_translator -rose:output "${work}/whitespace-baseline.c" -c \
  "${work}/input.c"
tr ' ' '\t' <"${work}/whitespace-baseline.c" >"${work}/whitespace.c"
test "$(stat -c %s "${work}/whitespace-baseline.c")" = \
  "$(stat -c %s "${work}/whitespace.c")"
cp "${work}/whitespace.c" "${work}/whitespace.expected"
expect_abort "${work}/whitespace.log" \
  -rose:noclobber_if_different_output_file \
  -rose:output "${work}/whitespace.c" -c "${work}/input.c"
test "$(grep -Ec '^Error: files are not equivalent: $' \
  "${work}/whitespace.log")" -eq 1
cmp "${work}/whitespace.expected" "${work}/whitespace.c"
assert_no_staging_files

# The removed keep-going path must terminate during command-line processing and
# must never replace an existing output with either generated or input text.
printf '%s\n' 'REX_KEEP_GOING_SENTINEL' >"${work}/keep-going.c"
expect_abort "${work}/keep-going.log" -rose:keep_going \
  -rose:output "${work}/keep-going.c" -c "${work}/input.c"
test "$(grep -Fxc \
  'REX_COMMANDLINE_INVARIANT[keep-going]: -rose:keep_going is not supported because compiler pipeline failures terminate at their source' \
  "${work}/keep-going.log")" -eq 1
grep -Fx 'REX_KEEP_GOING_SENTINEL' "${work}/keep-going.c" >/dev/null
assert_no_staging_files
