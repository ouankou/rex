#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-}"
if [[ -z "${repo_root}" || "${repo_root}" != /* ||
      ! -x "${repo_root}/scripts/ci-rex-wasm-build-dist" ]]; then
  echo "rex_wasm_cache_contract: expected the absolute REX source root" >&2
  exit 2
fi

temporary_root="$(mktemp -d)"
trap 'rm -rf "${temporary_root}"' EXIT

incomplete_cache="${temporary_root}/incomplete-llvm-wasm"
llvm_source_root="${temporary_root}/llvm-project"
llvm_source_dir="${llvm_source_root}/llvm"
emsdk_root="${temporary_root}/emsdk/emsdk-main"
fake_bin="${temporary_root}/bin"
probe_trace="${temporary_root}/probe.trace"
wasm_workflow="${repo_root}/.github/workflows/rex-wasm-build.yml"

# The Emscripten action downloads to RUNNER_TEMP on a first cache miss and
# copies the completed SDK into actions-cache-folder afterward. The REX LLVM
# cache therefore must consume that stable workspace copy explicitly; otherwise
# CMakeCache.txt records a per-run toolchain path that cannot exist next run.
if [[ "$(grep -Fxc \
    '      EMSDK_ACTION_CACHE_DIR: .build/rex-wasm/emsdk' \
    "${wasm_workflow}")" -ne 1 ]] ||
   [[ "$(grep -Fxc \
    '          actions-cache-folder: ${{ env.EMSDK_ACTION_CACHE_DIR }}' \
    "${wasm_workflow}")" -ne 1 ]] ||
   [[ "$(grep -Fxc \
    '      REX_WASM_EMSDK_ROOT: ${{ github.workspace }}/.build/rex-wasm/emsdk/emsdk-main' \
    "${wasm_workflow}")" -ne 1 ]]; then
  echo "REX WASM workflow does not bind Emscripten to one stable workspace root" >&2
  exit 1
fi
if [[ "$(grep -Fc \
    "hashFiles('.github/workflows/rex-wasm-build.yml', 'scripts/ci-rex-wasm-build-dist', 'cmake/rex_wasm_llvm_probe/**')" \
    "${wasm_workflow}")" -ne 2 ]]; then
  echo "REX WASM cache key does not include the complete workflow contract" >&2
  exit 1
fi
if [[ "$(grep -Fxc \
    "        if: \${{ steps.restore-llvm-wasm.outputs.cache-hit == 'true' }}" \
    "${wasm_workflow}")" -ne 1 ]] ||
   [[ "$(grep -Fxc \
    '          scripts/ci-rex-wasm-build-dist verify-llvm-cache' \
    "${wasm_workflow}")" -ne 1 ]]; then
  echo "REX WASM workflow does not hard-validate an exact restored cache" >&2
  exit 1
fi
mkdir -p \
  "${llvm_source_dir}" \
  "${llvm_source_root}/clang" \
  "${llvm_source_root}/cmake/Modules" \
  "${emsdk_root}/upstream/emscripten" \
  "${emsdk_root}/upstream/emscripten/cmake/Modules/Platform" \
  "${incomplete_cache}/lib/cmake/llvm" \
  "${incomplete_cache}/lib/cmake/clang" \
  "${incomplete_cache}/lib/clang/22/include" \
  "${incomplete_cache}/tools/clang/include/clang/Basic" \
  "${fake_bin}"
printf '# exact LLVM source fixture\n' > "${llvm_source_dir}/CMakeLists.txt"
printf '# exact Clang source fixture\n' > "${llvm_source_root}/clang/CMakeLists.txt"
printf '%s\n' \
  'set(LLVM_VERSION_MAJOR 22)' \
  'set(LLVM_VERSION_MINOR 1)' \
  'set(LLVM_VERSION_PATCH 8)' \
  > "${llvm_source_root}/cmake/Modules/LLVMVersion.cmake"
git -C "${llvm_source_root}" init -q
git -C "${llvm_source_root}" add llvm clang cmake
git -C "${llvm_source_root}" \
  -c user.name='REX cache contract' \
  -c user.email='rex-cache-contract@example.invalid' \
  commit -qm 'exact source fixture'
llvm_source_commit="$(git -C "${llvm_source_root}" rev-parse HEAD)"
printf 'set(LLVM_VERSION_MAJOR 22)\n' \
  > "${incomplete_cache}/lib/cmake/llvm/LLVMConfig.cmake"
printf '# exact Clang configuration fixture\n' \
  > "${incomplete_cache}/lib/cmake/clang/ClangConfig.cmake"
printf '#define CLANG_VERSION_MAJOR 22\n' \
  > "${incomplete_cache}/tools/clang/include/clang/Basic/Version.inc"
printf '/* exact resource header fixture */\n' \
  > "${incomplete_cache}/lib/clang/22/include/stddef.h"

printf '%s\n' \
  '#!/usr/bin/env bash' \
  'exit 0' \
  > "${emsdk_root}/emsdk"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'fixture_emsdk_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"' \
  'export EMSDK="${fixture_emsdk_root}"' \
  'export PATH="${fixture_emsdk_root}/upstream/emscripten:${PATH}"' \
  > "${emsdk_root}/emsdk_env.sh"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  "probe_trace='${probe_trace}'" \
  'if [[ "$*" == *"/cmake/rex_wasm_llvm_probe"* ]]; then' \
  '  printf "probe invoked\n" > "${probe_trace}"' \
  'fi' \
  'exit 0' \
  > "${emsdk_root}/upstream/emscripten/emcmake"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'exit 0' \
  > "${emsdk_root}/upstream/emscripten/em++"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'exit 0' \
  > "${fake_bin}/cmake"
printf '# exact stable Emscripten toolchain fixture\n' \
  > "${emsdk_root}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
chmod +x \
  "${emsdk_root}/emsdk" \
  "${emsdk_root}/upstream/emscripten/emcmake" \
  "${emsdk_root}/upstream/emscripten/em++" \
  "${fake_bin}/cmake"
export PATH="${fake_bin}:${PATH}"

set +e
missing_emsdk_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" llvm-status
} 2>&1)"
missing_emsdk_status=$?
set -e
if [[ "${missing_emsdk_status}" -ne 2 ]] ||
   [[ "$(grep -Fxc \
    'error: REX_WASM_EMSDK_ROOT must name an absolute, complete Emscripten SDK root' \
    <<< "${missing_emsdk_output}")" -ne 1 ]]; then
  echo "WASM LLVM cache inspection did not require an explicit Emscripten root" >&2
  printf '%s\n' "${missing_emsdk_output}" >&2
  exit 1
fi

status_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_EMSDK_ROOT="${emsdk_root}" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" llvm-status
} 2>&1)"
if [[ "$(grep -Fxc \
    "==> wasm LLVM/Clang is not ready; a source build is required" \
    <<< "${status_output}")" -ne 1 ]]; then
  echo "WASM LLVM cache inspection did not accept the exact stable Emscripten root" >&2
  printf '%s\n' "${status_output}" >&2
  exit 1
fi

# Model the exact immutable configuration contract before exercising the
# individual missing-artifact checks below.  A cache that was configured with
# different LLVM/WASM options is already invalid, independently of which
# archives it happens to contain.
emscripten_toolchain="${emsdk_root}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
printf '%s\n' \
  'CMAKE_BUILD_TYPE:STRING=MinSizeRel' \
  'CMAKE_GENERATOR:INTERNAL=Ninja' \
  'LLVM_ENABLE_PROJECTS:STRING=clang' \
  'LLVM_TARGETS_TO_BUILD:STRING=WebAssembly' \
  'LLVM_DEFAULT_TARGET_TRIPLE:STRING=wasm32-unknown-emscripten' \
  'LLVM_HOST_TRIPLE:STRING=wasm32-unknown-emscripten' \
  'LLVM_BUILD_TOOLS:BOOL=OFF' \
  'LLVM_ENABLE_PLUGINS:BOOL=OFF' \
  'LLVM_INCLUDE_TESTS:BOOL=OFF' \
  'LLVM_INCLUDE_EXAMPLES:BOOL=OFF' \
  'LLVM_INCLUDE_BENCHMARKS:BOOL=OFF' \
  'LLVM_INCLUDE_DOCS:BOOL=OFF' \
  'CLANG_BUILD_TOOLS:BOOL=OFF' \
  'CLANG_PLUGIN_SUPPORT:BOOL=OFF' \
  'LLVM_ENABLE_THREADS:BOOL=OFF' \
  'LLVM_ENABLE_TERMINFO:BOOL=OFF' \
  'LLVM_ENABLE_ZLIB:BOOL=OFF' \
  'LLVM_ENABLE_ZSTD:BOOL=OFF' \
  'LLVM_ENABLE_LIBXML2:BOOL=OFF' \
  'LLVM_ENABLE_LIBEDIT:BOOL=OFF' \
  'LLVM_ENABLE_FFI:BOOL=OFF' \
  'LLVM_ENABLE_RTTI:BOOL=ON' \
  'LLVM_ENABLE_EH:BOOL=ON' \
  "LLVM_SOURCE_DIR:STATIC=${llvm_source_dir}" \
  "CMAKE_TOOLCHAIN_FILE:FILEPATH=${emscripten_toolchain}" \
  > "${incomplete_cache}/CMakeCache.txt"
printf '%s\n' \
  "source_commit=${llvm_source_commit}" \
  'source_version=22.1.8' \
  "source_root=${llvm_source_root}" \
  > "${incomplete_cache}/rex-wasm-llvm-source-contract.txt"
printf '#define CLANG_VERSION_MAJOR 22\n' \
  > "${incomplete_cache}/tools/clang/include/clang/Basic/Version.inc"
printf '%s\n' \
  'schema=rex-wasm-llvm-readiness-v1' \
  "source_commit=${llvm_source_commit}" \
  'source_version=22.1.8' \
  "source_root=${llvm_source_root}" \
  'build_type=MinSizeRel' \
  > "${incomplete_cache}/rex-wasm-llvm-readiness.txt"

# Populate every REX WASM Clang archive except clangDriver.  The old readiness
# probe accepted this cache after seeing clangFrontend alone.
for library in \
  clangFrontend \
  clangParse \
  clangSema \
  clangSerialization \
  clangAnalysis \
  clangASTMatchers \
  clangAST \
  clangEdit \
  clangLex \
  clangBasic \
  clangSupport; do
  printf 'static archive fixture\n' \
    > "${incomplete_cache}/lib/lib${library}.a"
done

set +e
cache_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_EMSDK_ROOT="${emsdk_root}" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" verify-llvm-cache
} 2>&1)"
cache_status=$?
set -e
if [[ "${cache_status}" -ne 2 ]]; then
  echo "incomplete WASM LLVM cache was not rejected with status 2" >&2
  printf '%s\n' "${cache_output}" >&2
  exit 1
fi
expected_cache_error="error: wasm LLVM/Clang build is missing or incomplete: ${incomplete_cache}"
if [[ "$(grep -Fxc "${expected_cache_error}" <<< "${cache_output}")" -ne 1 ]]; then
  echo "incomplete WASM LLVM cache did not emit the exact hard diagnostic" >&2
  printf '%s\n' "${cache_output}" >&2
  exit 1
fi
expected_archive_error="       required nonempty static archive is missing: lib/libclangDriver.a"
if [[ "$(grep -Fxc "${expected_archive_error}" <<< "${cache_output}")" -ne 1 ]]; then
  echo "incomplete WASM LLVM cache did not identify the omitted static archive" >&2
  printf '%s\n' "${cache_output}" >&2
  exit 1
fi

# Complete the archive set. A cache is ready only after the checked-in probe
# compiles and links against its exported LLVM/Clang package targets.
printf 'static archive fixture\n' \
  > "${incomplete_cache}/lib/libclangDriver.a"
rm -f "${probe_trace}"
if ! probe_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_EMSDK_ROOT="${emsdk_root}" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" verify-llvm-cache
} 2>&1)"; then
  echo "complete WASM LLVM cache failed its compile-link probe" >&2
  printf '%s\n' "${probe_output}" >&2
  exit 1
fi
if [[ ! -s "${probe_trace}" ]]; then
  echo "complete WASM LLVM cache did not run the compile-link probe" >&2
  exit 1
fi

# The readiness stamp is an exact build contract, not a generic cache marker.
printf '%s\n' \
  'schema=rex-wasm-llvm-readiness-v1' \
  'source_commit=0000000000000000000000000000000000000000' \
  'source_version=22.1.8' \
  "source_root=${llvm_source_root}" \
  'build_type=MinSizeRel' \
  > "${incomplete_cache}/rex-wasm-llvm-readiness.txt"
set +e
readiness_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_EMSDK_ROOT="${emsdk_root}" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" verify-llvm-cache
} 2>&1)"
readiness_status=$?
set -e
if [[ "${readiness_status}" -ne 2 ]] ||
   [[ "$(grep -Fxc \
    '       LLVM compile/link readiness stamp does not match the exact current build contract' \
    <<< "${readiness_output}")" -ne 1 ]]; then
  echo "mismatched WASM LLVM readiness stamp was not rejected exactly" >&2
  printf '%s\n' "${readiness_output}" >&2
  exit 1
fi

# A complete artifact inventory is still invalid when it was produced from a
# different source commit. This catches mutable ref/cache-key reuse directly.
printf '%s\n' \
  'schema=rex-wasm-llvm-readiness-v1' \
  "source_commit=${llvm_source_commit}" \
  'source_version=22.1.8' \
  "source_root=${llvm_source_root}" \
  'build_type=MinSizeRel' \
  > "${incomplete_cache}/rex-wasm-llvm-readiness.txt"
printf '%s\n' \
  'source_commit=0000000000000000000000000000000000000000' \
  'source_version=22.1.8' \
  "source_root=${llvm_source_root}" \
  > "${incomplete_cache}/rex-wasm-llvm-source-contract.txt"

set +e
provenance_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_EMSDK_ROOT="${emsdk_root}" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" verify-llvm-cache
} 2>&1)"
provenance_status=$?
set -e
if [[ "${provenance_status}" -ne 2 ]]; then
  echo "stale-source WASM LLVM cache was not rejected with status 2" >&2
  printf '%s\n' "${provenance_output}" >&2
  exit 1
fi

# A matching inventory and stamp are still rejected when the actual package
# cannot compile and link the probe.
printf '%s\n' \
  "source_commit=${llvm_source_commit}" \
  'source_version=22.1.8' \
  "source_root=${llvm_source_root}" \
  > "${incomplete_cache}/rex-wasm-llvm-source-contract.txt"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'if [[ "$*" == *"/cmake/rex_wasm_llvm_probe"* ]]; then' \
  '  exit 9' \
  'fi' \
  'exit 0' \
  > "${emsdk_root}/upstream/emscripten/emcmake"
chmod +x "${emsdk_root}/upstream/emscripten/emcmake"
set +e
probe_failure_output="$({
  LLVM_VERSION=22 \
  LLVM_SOURCE_DIR="${llvm_source_dir}" \
  LLVM_WASM_BUILD_DIR="${incomplete_cache}" \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_EMSDK_ROOT="${emsdk_root}" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" verify-llvm-cache
} 2>&1)"
probe_failure_status=$?
set -e
expected_probe_error="       LLVM/Clang compile-link probe failed in ${temporary_root}/build-root/llvm-probe"
if [[ "${probe_failure_status}" -ne 2 ]] ||
   [[ "$(grep -Fxc "${expected_probe_error}" \
    <<< "${probe_failure_output}")" -ne 1 ]]; then
  echo "failing WASM LLVM compile-link probe was not rejected exactly" >&2
  printf '%s\n' "${probe_failure_output}" >&2
  exit 1
fi
expected_provenance_error='       LLVM source provenance stamp does not match the exact current source checkout'
if [[ "$(grep -Fxc "${expected_provenance_error}" \
    <<< "${provenance_output}")" -ne 1 ]]; then
  echo "stale-source WASM LLVM cache did not emit the exact provenance diagnostic" >&2
  printf '%s\n' "${provenance_output}" >&2
  exit 1
fi

set +e
skip_output="$({
  REX_WASM_SKIP_TESTS=1 \
  REX_WASM_BUILD_ROOT="${temporary_root}/build-root" \
  REX_WASM_BUILD_DIR="${temporary_root}/rex" \
    "${repo_root}/scripts/ci-rex-wasm-build-dist" test
} 2>&1)"
skip_status=$?
set -e
if [[ "${skip_status}" -ne 2 ]]; then
  echo "removed REX_WASM_SKIP_TESTS knob was not rejected with status 2" >&2
  printf '%s\n' "${skip_output}" >&2
  exit 1
fi
if [[ "$(grep -Fxc \
    'error: REX_WASM_SKIP_TESTS was removed; REX WASM smoke tests are mandatory' \
    <<< "${skip_output}")" -ne 1 ]]; then
  echo "removed REX_WASM_SKIP_TESTS knob did not emit the exact diagnostic" >&2
  printf '%s\n' "${skip_output}" >&2
  exit 1
fi
