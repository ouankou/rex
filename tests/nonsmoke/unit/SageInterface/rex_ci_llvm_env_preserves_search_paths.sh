#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 scripts/ci-llvm-env" >&2
  exit 2
fi

environment_script="$1"
if [ ! -f "$environment_script" ]; then
  echo "LLVM environment script does not exist: $environment_script" >&2
  exit 2
fi

fixture="$(mktemp -d)"
trap 'rm -rf "$fixture"' EXIT
llvm_root="$fixture/llvm-22"
mkdir -p \
  "$llvm_root/bin" \
  "$llvm_root/lib/cmake/llvm" \
  "$llvm_root/lib/cmake/clang" \
  "$llvm_root/lib/clang/22/include"
printf 'set(LLVM_VERSION_MAJOR 22)\n' \
  >"$llvm_root/lib/cmake/llvm/LLVMConfig.cmake"
printf 'set(Clang_VERSION_MAJOR 22)\n' \
  >"$llvm_root/lib/cmake/clang/ClangConfig.cmake"
printf '/* exact LLVM 22 stddef fixture */\n' \
  >"$llvm_root/lib/clang/22/include/stddef.h"
printf '/* exact LLVM 22 OpenMP fixture */\n' \
  >"$llvm_root/lib/clang/22/include/omp.h"

PATH="/caller/bin:/usr/bin"
LD_LIBRARY_PATH="/caller/lib:/caller/lib64"
CMAKE_PREFIX_PATH="/caller/package-a:/caller/package-b"
LLVM_ROOT="$llvm_root"

source "$environment_script" 22

test "$PATH" = "$llvm_root/bin:/caller/bin:/usr/bin"
test "$CMAKE_PREFIX_PATH" = \
  "$llvm_root:/caller/package-a:/caller/package-b"
case "$LD_LIBRARY_PATH" in
  "$llvm_root/lib"*":/caller/lib:/caller/lib64") ;;
  *)
    echo "ci-llvm-env discarded caller LD_LIBRARY_PATH: $LD_LIBRARY_PATH" >&2
    exit 1
    ;;
esac
test "$LLVM_ROOT" = "$llvm_root"
test "$Clang_ROOT" = "$llvm_root"
test "$LLVM_DIR" = "$llvm_root/lib/cmake/llvm"
test "$Clang_DIR" = "$llvm_root/lib/cmake/clang"
test "$LLVM_NATIVE_TOOL_DIR" = "$llvm_root/bin"
test "$REX_WASM_OMP_INCLUDE_DIR" = "$llvm_root/lib/clang/22/include"
