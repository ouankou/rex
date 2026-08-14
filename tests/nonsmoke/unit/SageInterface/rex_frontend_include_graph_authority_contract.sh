#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
  echo "usage: $0 <rex-source-root>" >&2
  exit 2
fi

root=$1
include_dir="${root}/src/frontend/SageIII/includeDirectivesProcessing"
collector_c="${include_dir}/IncludingPreprocessingInfosCollector.C"
collector_h="${include_dir}/IncludingPreprocessingInfosCollector.h"
include_cmake="${include_dir}/CMakeLists.txt"
sage_support="${root}/src/frontend/SageIII/sage_support/sage_support.C"
rosetta_support="${root}/src/ROSETTA/src/support.C"
support_code="${root}/src/ROSETTA/Grammar/Support.code"

for input in "$collector_c" "$collector_h" "$include_cmake" \
             "$sage_support" "$rosetta_support" "$support_code"; do
  if [ ! -f "$input" ]; then
    echo "frontend include-graph authority input is missing: $input" >&2
    exit 1
  fi
done

for retired in CompilerOutputParser.C CompilerOutputParser.h \
               CompilerOutputReader.C CompilerOutputReader.h; do
  if [ -e "${include_dir}/${retired}" ]; then
    echo "retired second-compiler include parser still exists: $retired" >&2
    exit 1
  fi
done

if grep -Fq 'CompilerOutputParser' "$include_cmake" "$sage_support" ||
   grep -Eq 'collectIncludedFiles(Map|SearchPaths)' "$sage_support"; then
  echo "header unparsing still reconstructs include ownership with a second compiler invocation" >&2
  exit 1
fi

for required in get_frontendResolvedIncludeDirectivesMap \
                getNormalizedContainingFileName getIncludedFilesMap; do
  if ! grep -Fq "$required" "$collector_c" "$collector_h"; then
    echo "frontend include-graph collector lacks exact authority operation: $required" >&2
    exit 1
  fi
done

if ! grep -Fq 'includingPreprocessingInfosCollector.getIncludedFilesMap()' \
     "$sage_support"; then
  echo "header unparsing does not consume the frontend-owned include graph" >&2
  exit 1
fi

for retired_api in quotedIncludesSearchPaths bracketedIncludesSearchPaths \
                   findIncludedFile; do
  if grep -Fq "$retired_api" "$rosetta_support" "$support_code" \
       "$sage_support"; then
    echo "retired include re-resolution API remains generated: $retired_api" >&2
    exit 1
  fi
done
