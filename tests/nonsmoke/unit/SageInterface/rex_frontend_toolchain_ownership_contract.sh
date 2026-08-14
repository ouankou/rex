#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 5 ]; then
  echo "usage: $0 <root-cmake> <cxx20-cmake> <uninitialized-cmake> <omp-cmake> <omp-semantic-runner>" >&2
  exit 2
fi

root_cmake="$1"
cxx20_cmake="$2"
uninitialized_cmake="$3"
omp_cmake="$4"
omp_semantic_runner="$5"

for input in "$root_cmake" "$cxx20_cmake" "$uninitialized_cmake" \
             "$omp_cmake" "$omp_semantic_runner"; do
  if [ ! -f "$input" ]; then
    echo "frontend toolchain contract input is missing: $input" >&2
    exit 1
  fi
done

for driver_var in ROSE_LLVM_CLANG_C_DRIVER ROSE_LLVM_CLANG_CXX_DRIVER; do
  if ! grep -Fq "set(${driver_var}" "$root_cmake"; then
    echo "root CMake does not publish exact frontend driver ${driver_var}" >&2
    exit 1
  fi
done

if ! grep -Fq '_rose_validate_backend_clang_resource(C)' "$root_cmake" ||
   ! grep -Fq '_rose_validate_backend_clang_resource(CXX)' "$root_cmake" ||
   ! grep -Fq 'COMMAND "${CMAKE_${_rose_language}_COMPILER}" --print-resource-dir' "$root_cmake" ||
   ! grep -Fq 'backend compiler and loaded' "$root_cmake" ||
   ! grep -Fq 'must own the same resource directory' "$root_cmake"; then
  echo "root CMake does not hard-check backend/frontend resource ownership" >&2
  exit 1
fi

for module_cmake in "$cxx20_cmake" "$uninitialized_cmake"; do
  if grep -Eq '\$\{CMAKE_CXX_COMPILER\}.*(--precompile|-fmodule-header|-x c\+\+-module|-xc\+\+-system-header)' \
      "$module_cmake"; then
    echo "PCM producer uses the build compiler instead of the frontend compiler: $module_cmake" >&2
    exit 1
  fi
  if ! grep -Fq '${ROSE_LLVM_CLANG_CXX_DRIVER}' "$module_cmake"; then
    echo "PCM producer does not use the exact frontend Clang++ driver: $module_cmake" >&2
    exit 1
  fi
done

for forbidden in \
    'set(_compiler ${CMAKE_C_COMPILER})' \
    'set(_compiler ${CMAKE_CXX_COMPILER})' \
    'COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir'; do
  if grep -Fq "$forbidden" "$omp_cmake"; then
    echo "OpenMP semantic contract uses a build-toolchain driver: $forbidden" >&2
    exit 1
  fi
done
for required in \
    '${ROSE_LLVM_CLANG_C_DRIVER}' \
    '${ROSE_LLVM_CLANG_CXX_DRIVER}'; do
  if ! grep -Fq "$required" "$omp_cmake"; then
    echo "OpenMP semantic contract does not use exact frontend driver: $required" >&2
    exit 1
  fi
done

if ! grep -Fq 'compiler_resource_include=' "$omp_semantic_runner" ||
   ! grep -Fq 'does not match exact OpenMP header owner' "$omp_semantic_runner" ||
   ! grep -Fq -- '-idirafter' "$omp_semantic_runner"; then
  echo "OpenMP semantic runner does not hard-check compiler/header ownership" >&2
  exit 1
fi
