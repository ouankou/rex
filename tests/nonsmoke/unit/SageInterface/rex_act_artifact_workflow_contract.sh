#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <workflow-directory>" >&2
  exit 2
fi

workflow_dir="$1"
docs_workflow="$workflow_dir/docs-publish.yml"
wasm_workflow="$workflow_dir/rex-wasm-build.yml"
memory_workflow="$workflow_dir/weekly-memory.yml"

for workflow in "$docs_workflow" "$wasm_workflow" "$memory_workflow"; do
  if [[ ! -f "$workflow" ]]; then
    echo "missing workflow: $workflow" >&2
    exit 1
  fi
done

docs_upload="$({
  sed -n '/^      - name: Upload docs site artifact$/,/^  rex-wasm:/p' \
    "$docs_workflow"
})"
if ! grep -Fq 'if: ${{ !env.ACT }}' <<<"$docs_upload" ||
   ! grep -Fq 'uses: actions/upload-artifact@v7' <<<"$docs_upload"; then
  echo "docs artifact upload must remain GitHub-hosted while act runs the full build" >&2
  exit 1
fi

wasm_upload="$({
  sed -n '/^      - name: Upload REX WASM dist$/,$p' "$wasm_workflow"
})"
if ! grep -Fq 'if: ${{ inputs.upload-artifact && !env.ACT }}' \
     <<<"$wasm_upload" ||
   ! grep -Fq 'uses: actions/upload-artifact@v7' <<<"$wasm_upload"; then
  echo "WASM artifact upload must remain GitHub-hosted without skipping local build tests" >&2
  exit 1
fi

memory_upload_count="$(grep -Fc 'uses: actions/upload-artifact@v7' \
  "$memory_workflow")"
memory_act_guard_count="$(grep -Fc 'if: ${{ always() && !env.ACT }}' \
  "$memory_workflow")"
if [[ "$memory_upload_count" -ne 3 || "$memory_act_guard_count" -ne 3 ]]; then
  echo "weekly memory artifact uploads must be the three ACT-guarded diagnostic transfers" >&2
  exit 1
fi

for step_name in \
  'Run full tests' \
  'Run memcheck on CI selection' \
  'Run sanitizer on CI selection'; do
  if ! grep -Fq -- "- name: $step_name" "$memory_workflow"; then
    echo "weekly memory workflow lost test step: $step_name" >&2
    exit 1
  fi
done
