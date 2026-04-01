#!/usr/bin/env bash
set -euo pipefail

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "This script must be run from inside a git repository." >&2
  exit 1
fi

repo_root=$(git rev-parse --show-toplevel)
hook_path="$repo_root/.git/hooks/pre-push"

if [[ -f "$hook_path" ]] && ! grep -q "REX CI pre-push tests" "$hook_path"; then
  echo "A pre-push hook already exists at $hook_path." >&2
  echo "Integrate the CI test invocations manually or remove the existing hook to continue." >&2
  exit 1
fi

cat >"$hook_path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

# REX CI pre-push tests

# Drain input from git push to avoid interfering with hooks that expect to read it.
if [[ ! -t 0 ]]; then
  cat >/dev/null
fi

if ! command -v ctest >/dev/null 2>&1; then
  echo "ctest not found. Please install CMake/CTest before pushing." >&2
  exit 1
fi

repo_root=$(git rev-parse --show-toplevel)
build_dir=${REX_BUILD_DIR:-"$repo_root/build"}

if [[ ! -d "$build_dir" ]]; then
  echo "Build directory not found at '$build_dir'. Configure and build before pushing." >&2
  exit 1
fi

if [[ ! -f "$build_dir/CTestTestfile.cmake" ]]; then
  echo "CTest metadata missing in '$build_dir'. Re-configure with testing enabled." >&2
  exit 1
fi

cd "$build_dir"
cmake --build . -- -j"$(nproc)" || {
  echo "Build failed; aborting push." >&2
  exit 1
}

run_ctest_regex() {
  local test_dir=$1
  local regex=$2
  echo "Running ctest --test-dir \"${test_dir}\" -R \"${regex}\"..."
  local count
  count=$(ctest -N --test-dir "${test_dir}" -R "${regex}" | awk '/Total Tests:/ {print $3}')
  if [[ -n "${count:-}" ]] && (( count == 0 )); then
    echo "ctest selection found zero matching tests. Ensure the build matches CI configuration." >&2
    return 1
  fi
  ctest --test-dir "${test_dir}" -R "${regex}" -j"$(nproc)" --output-on-failure
}

run_ctest_regex \
  "${build_dir}" \
  "rex|astInterface|testQuery|fortran|f90|f03|gfortran|OMPTEST_|OMPACCTEST_|OMPFORTRAN_|OMPANALYZE_|OMPVV_5_0_|OMPVV_4_5_|OMPVV_5_1_|OMPVV_5_2_|OMPVV_6_0_|omp_lowering_|OMPLOWERING_CPU_|OMPLOWERING_RODINIA_|^Cxx_tests_test2013_69_C$|^Cxx_tests_test2013_198_C$"
EOF

chmod +x "$hook_path"

echo "Installed pre-push hook at $hook_path to run CI-aligned tests before pushing."
