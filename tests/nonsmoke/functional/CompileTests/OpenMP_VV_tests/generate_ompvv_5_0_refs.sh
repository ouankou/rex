#!/usr/bin/env bash
set -euo pipefail

repo_root=$(git rev-parse --show-toplevel)
source_root="$repo_root/third_party/OpenMP_VV/tests/5.0"
reference_root="$repo_root/tests/nonsmoke/functional/CompileTests/OpenMP_VV_tests/referenceResults/5.0"
extract_script="$repo_root/tests/nonsmoke/functional/CompileTests/OpenMP_VV_tests/extract_omp_directives.sh"

if [[ ! -d "$source_root" ]]; then
  echo "OpenMP_VV source tree not found at $source_root" >&2
  exit 1
fi

if [[ ! -x "$extract_script" ]]; then
  echo "Extractor script is not executable: $extract_script" >&2
  exit 1
fi

rm -rf "$reference_root"
mkdir -p "$reference_root"

count=0
while IFS= read -r src_file; do
  rel_path=${src_file#"$source_root"/}
  ref_file="$reference_root/$rel_path.output"
  mkdir -p "$(dirname "$ref_file")"
  "$extract_script" "$src_file" "$ref_file"
  count=$((count + 1))
done < <(find "$source_root" -type f \
  \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.C' \
     -o -name '*.f' -o -name '*.F' -o -name '*.f90' -o -name '*.F90' \) | sort)

echo "Generated OpenMP_VV 5.0 reference outputs: $count files"
