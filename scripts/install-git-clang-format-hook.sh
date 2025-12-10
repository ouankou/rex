#!/usr/bin/env bash
set -euo pipefail

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "This script must be run from inside a git repository." >&2
  exit 1
fi

repo_root=$(git rev-parse --show-toplevel)
hook_path="$repo_root/.git/hooks/pre-commit"

if [[ -f "$hook_path" ]] && ! grep -q "git clang-format" "$hook_path"; then
  echo "A pre-commit hook already exists at $hook_path." >&2
  echo "Integrate git clang-format manually or remove the existing hook to continue." >&2
  exit 1
fi

cat >"$hook_path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if ! command -v git-clang-format >/dev/null 2>&1; then
  echo "git-clang-format not found. Please install clang-format." >&2
  exit 1
fi

if git diff --cached --quiet; then
  exit 0
fi

mapfile -t staged_files < <(git diff --cached --name-only)

if [[ ${#staged_files[@]} -gt 0 ]]; then
  mapfile -t unstaged_conflicts < <(git diff --name-only -- "${staged_files[@]}")
  if [[ ${#unstaged_conflicts[@]} -gt 0 ]]; then
    echo "Cannot run git-clang-format: these files have unstaged changes:" >&2
    printf '  %s\n' "${unstaged_conflicts[@]}" >&2
    echo "Stash or stage them before committing." >&2
    exit 1
  fi
fi

if git rev-parse --verify HEAD >/dev/null 2>&1; then
  base=HEAD
else
  base=$(git hash-object -t tree /dev/null)
fi

status=0
output=$(git clang-format --staged "$base" 2>&1) || status=$?
if [[ -n "$output" ]]; then
  printf '%s\n' "$output"
fi

if [[ $status -gt 1 ]]; then
  exit $status
fi

if [[ ${#staged_files[@]} -gt 0 ]]; then
  mapfile -t format_targets < <(git diff --name-only --diff-filter=M -- "${staged_files[@]}")
  if [[ ${#format_targets[@]} -gt 0 ]]; then
    git add -- "${format_targets[@]}"
  fi
fi
EOF

chmod +x "$hook_path"

echo "Installed pre-commit hook at $hook_path to format staged changes with git clang-format."
