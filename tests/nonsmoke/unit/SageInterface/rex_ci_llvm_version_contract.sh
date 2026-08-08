#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 .github/workflows" >&2
  exit 2
fi

workflow_dir="$1"
if [ ! -d "$workflow_dir" ]; then
  echo "workflow directory does not exist: $workflow_dir" >&2
  exit 2
fi

checked=0
for workflow in "$workflow_dir"/*.yml; do
  if ! grep -Eq 'scripts/ci-(install-deps|llvm-env)' "$workflow"; then
    continue
  fi
  checked=$((checked + 1))

  if [ "$(grep -Ec '^[[:space:]]+LLVM_VERSION:[[:space:]]+("22"|22)[[:space:]]*$' "$workflow")" -ne 1 ]; then
    echo "LLVM-using workflow must declare exactly one LLVM_VERSION 22 pin: $workflow" >&2
    exit 1
  fi
  if grep -Eq 'scripts/ci-install-deps[[:space:]]+(native|wasm)[[:space:]]+"?22"?' "$workflow" ||
     grep -Eq 'source[[:space:]]+scripts/ci-llvm-env[[:space:]]+"?22"?' "$workflow" ||
     grep -Eq '(clang|flang)(\+\+)?-22' "$workflow" ||
     grep -Fq 'matrix.llvm' "$workflow"; then
    echo "LLVM-using workflow bypasses its LLVM_VERSION contract: $workflow" >&2
    exit 1
  fi
  if ! grep -Eq '\$\{\{[[:space:]]*env\.LLVM_VERSION[[:space:]]*\}\}|\$\{LLVM_VERSION\}' "$workflow"; then
    echo "LLVM-using workflow does not consume LLVM_VERSION: $workflow" >&2
    exit 1
  fi
done

if [ "$checked" -eq 0 ]; then
  echo "no LLVM-using workflows were checked" >&2
  exit 1
fi
