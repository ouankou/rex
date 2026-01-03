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
  local regex=$1
  echo "Running ctest -R \"${regex}\"..."
  local count
  count=$(ctest -N -R "${regex}" | awk '/Total Tests:/ {print $3}')
  if [[ -n "${count:-}" ]] && (( count == 0 )); then
    echo "ctest -R \"${regex}\" found zero matching tests. Ensure the build matches CI configuration." >&2
    return 1
  fi
  ctest -R "${regex}" -j --output-on-failure
}

ci_regexes=(
  "astInterface"
  "Translator_"
  "testQuery"
  "rex"
)

for regex in "${ci_regexes[@]}"; do
  run_ctest_regex "$regex"
done
EOF

chmod +x "$hook_path"

echo "Installed pre-push hook at $hook_path to run CI-aligned tests before pushing."
