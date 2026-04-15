#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 10 ]]; then
  echo "usage: $0 <parseOmp> <compiler> <input.c> <workdir> <mode> <omp_header_dir> <omp_runtime_dir> <lowering_inc> <case_name> <openmp_version>" >&2
  exit 2
fi

parse_omp="$1"
compiler="$2"
input_file="$3"
workdir="$4"
mode="$5"
omp_header_dir="$6"
omp_runtime_dir="$7"
lowering_inc="$8"
case_name="$9"
openmp_version="${10}"

if [[ "$mode" != "exact" && "$mode" != "sort" ]]; then
  echo "ERROR(${case_name}): invalid mode '$mode'" >&2
  exit 2
fi

mkdir -p "$workdir"
rm -f "$workdir"/orig.exe "$workdir"/lowered.exe
rm -f "$workdir"/rose_*.c "$workdir"/rex_lib_*.c

source_name="$(basename "$input_file")"
rose_file="$workdir/rose_${source_name}"
rex_lib_file="$workdir/rex_lib_${source_name}"

compile_flags=(
  -fopenmp=libiomp5
  -O0
  -g
  -U_OPENMP
  "-D_OPENMP=${openmp_version}"
  "-I${omp_header_dir}"
  "-L${omp_runtime_dir}"
  "-Wl,-rpath,${omp_runtime_dir}"
)

canonicalize() {
  local in_file="$1"
  local out_file="$2"
  if [[ "$mode" == "sort" ]]; then
    if [[ -s "$in_file" ]]; then
      head -n 1 "$in_file" > "$out_file"
      tail -n +2 "$in_file" | sort >> "$out_file"
    else
      : > "$out_file"
    fi
  else
    cp "$in_file" "$out_file"
  fi
}

fail_with_diff() {
  local what="$1"
  local lhs="$2"
  local rhs="$3"
  echo "ERROR(${case_name}): ${what} mismatch" >&2
  diff -u "$lhs" "$rhs" >&2 || true
  exit 1
}

# Build original source with LLVM OpenMP runtime.
"$compiler" "${compile_flags[@]}" "$input_file" -o "$workdir/orig.exe"

# Lower and build transformed source.
(
  cd "$workdir"
  "$parse_omp" --rex-omp-lowering -w -rose:verbose 0 -rose:skipfinalCompileStep -c "$input_file" > lower.log 2>&1
)

if [[ ! -f "$rose_file" ]]; then
  echo "ERROR(${case_name}): missing lowered host file '$rose_file'" >&2
  if [[ -f "$workdir/lower.log" ]]; then
    echo "---- lower.log (${case_name}) ----" >&2
    cat "$workdir/lower.log" >&2
    echo "---- end lower.log ----" >&2
  fi
  exit 1
fi

lowered_sources=("$rose_file")
if [[ -f "$rex_lib_file" ]]; then
  lowered_sources+=("$rex_lib_file")
fi

"$compiler" "${compile_flags[@]}" -I"$lowering_inc" "${lowered_sources[@]}" -o "$workdir/lowered.exe"

# Repeat execution to reduce chance of false positives from schedule luck.
export LD_LIBRARY_PATH="${omp_runtime_dir}:${LD_LIBRARY_PATH:-}"
thread_counts=(2 4)
repeats=5

for threads in "${thread_counts[@]}"; do
  for i in $(seq 1 "$repeats"); do
    timeout 30s env OMP_NUM_THREADS="$threads" "$workdir/orig.exe" > "$workdir/orig_${threads}_${i}.out" 2> "$workdir/orig_${threads}_${i}.err"
    timeout 30s env OMP_NUM_THREADS="$threads" "$workdir/lowered.exe" > "$workdir/low_${threads}_${i}.out" 2> "$workdir/low_${threads}_${i}.err"

    canonicalize "$workdir/orig_${threads}_${i}.out" "$workdir/orig_${threads}_${i}.canon"
    canonicalize "$workdir/low_${threads}_${i}.out" "$workdir/low_${threads}_${i}.canon"

    if ! cmp -s "$workdir/orig_${threads}_${i}.canon" "$workdir/low_${threads}_${i}.canon"; then
      fail_with_diff "stdout" "$workdir/orig_${threads}_${i}.canon" "$workdir/low_${threads}_${i}.canon"
    fi

    if ! cmp -s "$workdir/orig_${threads}_${i}.err" "$workdir/low_${threads}_${i}.err"; then
      fail_with_diff "stderr" "$workdir/orig_${threads}_${i}.err" "$workdir/low_${threads}_${i}.err"
    fi
  done
done

exit 0
