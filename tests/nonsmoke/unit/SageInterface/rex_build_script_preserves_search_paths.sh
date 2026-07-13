#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 build-rex.sh" >&2
  exit 2
fi

build_script="$1"
if [ ! -f "$build_script" ]; then
  echo "build script does not exist: $build_script" >&2
  exit 2
fi

environment_assignments="$(
  sed -n -E \
    '/^export (PATH|LD_LIBRARY_PATH|CMAKE_PREFIX_PATH)=/p' \
    "$build_script"
)"
if [ "$(printf '%s\n' "$environment_assignments" | wc -l)" -ne 3 ]; then
  echo "build script must define exactly three LLVM environment assignments" >&2
  exit 1
fi

LLVM_BINDIR=/rex/llvm/bin
LLVM_LIBDIR=/rex/llvm/lib
LLVM_PREFIX=/rex/llvm
PATH=/caller/bin
LD_LIBRARY_PATH=/caller/lib:/caller/lib64
CMAKE_PREFIX_PATH=/caller/package-a:/caller/package-b

eval "$environment_assignments"

test "$PATH" = "/rex/llvm/bin:/caller/bin"
test "$LD_LIBRARY_PATH" = "/rex/llvm/lib:/caller/lib:/caller/lib64"
test "$CMAKE_PREFIX_PATH" = \
  "/rex/llvm:/caller/package-a:/caller/package-b"
