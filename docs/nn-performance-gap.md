# `nn` Remaining GPU Performance Gap

Date: 2026-03-11

## Status

The original `nn` comparison in
`/home/ouankou/Projects/NeoRodinia-Old/rex/LLVM_vs_REX_GPU_Comparison.md`
reported:

- native LLVM: `0.303069s`
- REX: `0.998628s`

That number is now stale for `nn`.

A compiler bug placed `rex_offload_init()` after a timed declaration
(`long long time0 = clock();`), so one-time cubin registration was being counted
inside the benchmark timer. That issue is now fixed in
`src/midend/programTransformation/ompLowering/omp_lowering.cpp`.

After the fix, repeated local runs on the same input gave:

- native LLVM average: `0.3040s`
- REX average: `0.4113s`

So the large startup inflation is gone, but REX still remains about `35%` slower
than native LLVM on `nn`.

This document describes the remaining gap, the clean long-term fix, and how to
verify it without redoing the investigation.

## Benchmark Shape

Relevant benchmark source:

- `/home/ouankou/Projects/NeoRodinia-Old/rex/nn/nn.c`

Key constants:

- `NUM_TEAMS = 256`
- `NUM_THREADS = 1024`
- `REC_WINDOW = 10`

Workload used during investigation:

- `filelist_4 5 30 90`

Input size:

- 4 database files
- each file has `10691` records
- records are processed in windows of `10`
- total target launches: `4 * ceil(10691 / 10) = 4280`

Implication:

- each GPU offload launches a large grid
- each launch computes at most `10` distances
- per-launch overhead dominates

This is a launch-overhead benchmark much more than a throughput benchmark.

## What Was Fixed Already

The generated REX host stub previously looked like:

```c
long long time0 = clock();
...
rex_offload_init();
```

That counted:

- cubin file loading
- `__tgt_register_lib`
- first-time runtime image registration

inside the benchmark timer.

The compiler now emits:

```c
rex_offload_init();
long long time0 = clock();
```

Relevant files:

- fixed insertion logic:
  `src/midend/programTransformation/ompLowering/omp_lowering.cpp`
- regression coverage:
  `tests/nonsmoke/functional/CompileTests/OpenMP_tests/lowering_rodinia/inputs/rodinia_nn_like.c`
  `tests/nonsmoke/functional/CompileTests/OpenMP_tests/lowering_rodinia/scripts/verify_outputs.sh`

This fix removed most of the observed `nn` gap, but not all of it.

## Remaining Root Cause

The remaining slowdown is device-side lowering/runtime overhead.

### REX path

REX lowers the loop to a generic XOMP device scheduler.

Generated device file:

- `/home/ouankou/Projects/NeoRodinia-Old/rex/nn/rex_lib_nn.cu`

The generated kernel does:

```c
int _dev_thread_num = getCUDABlockThreadCount(1);
int _dev_thread_id = getLoopIndexFromCUDAVariables(1);
XOMP_static_sched_init(...);
while (XOMP_static_sched_next(...)) {
  for (...) {
    ...
  }
}
```

Relevant helpers:

- `src/midend/programTransformation/ompLowering/xomp_cuda_lib_inlined.cu`
- `getLoopIndexFromCUDAVariables`
- `getCUDABlockThreadCount`
- `XOMP_static_sched_init`
- `XOMP_static_sched_next`

This path is generic and handles normalized OpenMP loops with scheduler-style
bookkeeping.

### Native LLVM path

Native LLVM lowers the same simple `target teams distribute parallel for` more
directly.

The PTX generated with:

```bash
clang -O3 -fopenmp -fopenmp-targets=nvptx64-nvidia-cuda \
  --offload-arch=sm_52 -save-temps -c nn.c -o nn_native.o
```

shows direct device control flow:

- compute block base from `%ctaid.x`
- early-exit blocks with no work
- use direct loop/index arithmetic in the kernel body
- no XOMP scheduler helper calls

The native path is still OpenMP offloading. The difference is not "OpenMP vs
CUDA". The difference is:

- REX: generic scheduler-based CUDA lowering
- LLVM: direct OpenMP device lowering for this canonical loop shape

## Why the Performance Diff Appears

For `nn`, the loop trip count per launch is tiny: at most `10`.

That means the overhead of loop dispatch matters more than the arithmetic.

### Cost paid by REX on every launch

For each of the `4280` launches, the generated device kernel pays for:

- helper-based computation of total thread count
- helper-based computation of global thread id
- scheduler initialization
- scheduler next-step logic
- extra control flow around the scheduler loop
- extra device calls in the cubin generated with `nvcc -rdc=true`

This is a reasonable fallback for complex loops, but it is too heavy for a
simple 1-D canonical loop with almost no work per launch.

### Why native LLVM wins here

Native LLVM emits a direct loop structure that:

- computes the thread/block mapping inline
- exits empty blocks quickly
- avoids the generic scheduler helper layer
- carries less per-launch control overhead

After the init-placement fix, the remaining gap is about:

- `0.4113s - 0.3040s = 0.1073s`

Spread across `4280` launches, that is about:

- `25 microseconds` extra per launch

That scale is consistent with repeated small-launch scheduler overhead.

## Clean Long-Term Solution

The right fix is not benchmark-specific tuning. The right fix is to add a
direct lowering path for simple canonical target loops and keep the existing
XOMP scheduler as the fallback for complex cases.

### Target solution

Teach REX lowering to recognize a simple class of loops, starting with the
shape used by `nn`:

- 1-D canonical `for`
- no `collapse`
- no reduction
- no complex schedule semantics needing runtime scheduling
- no constructs that require the generic XOMP fallback

For that class, emit a direct CUDA loop in the device kernel, conceptually:

```c
int global_tid = blockIdx.x * blockDim.x + threadIdx.x;
int global_stride = gridDim.x * blockDim.x;

for (int i = lower + global_tid * step; in_range(i); i += global_stride * step) {
  body(i);
}
```

This should be generated from normalized loop information, not from benchmark
pattern matching.

### What to implement

1. Add a fast-path predicate in OpenMP lowering for canonical target loops.
2. Emit direct index arithmetic in the generated CUDA outlined kernel for that
   fast path.
3. Do not emit calls to:
   - `getCUDABlockThreadCount`
   - `getLoopIndexFromCUDAVariables`
   - `XOMP_static_sched_init`
   - `XOMP_static_sched_next`
   for the fast path.
4. Keep the existing XOMP scheduler path as the fallback for loops that do not
   satisfy the fast-path predicate.
5. Extend regression tests so the fast-path shape is structural, not just a
   performance expectation.

### What to avoid

Avoid these, even if they appear to help `nn`:

- no special-casing by benchmark name, file name, or function name
- no special-casing `REC_WINDOW == 10`
- no host-side hacks that rewrite `nn` only
- no changing benchmark source constants such as `NUM_TEAMS` or `NUM_THREADS`
- no "fix" that only excludes time from the benchmark timer while leaving the
  device scheduler gap unchanged
- no global removal of the XOMP scheduler without a correctness-preserving
  fallback for complex loops

If launch geometry adaptation is added later, it should be a general policy with
clear OpenMP semantics, not a hand-tuned `nn` workaround.

## Recommended Implementation Plan

1. Identify the current device-loop emission point in OpenMP lowering for
   `target teams distribute parallel for`.
2. Introduce a new simple-loop emission path that uses direct grid-stride index
   arithmetic.
3. Guard that path with explicit structural checks.
4. Fall back to the current XOMP scheduler path when any unsupported feature is
   present.
5. Add lowering tests that assert the simple path does not mention XOMP
   scheduler helpers.
6. Re-run correctness and performance validation on `nn`, then spot-check other
   benchmarks to confirm there is no regression in complex cases.

## Verification Plan

### Structural verification

Keep the existing init-order regression and add a second regression for the
future fast path.

Current structural test already in tree:

- `rodinia_nn_like` verifies `rex_offload_init()` is emitted before a timed
  declaration.

Add a future structural test for the fast path:

- lower a simple `nn`-like kernel
- assert generated CUDA does not contain:
  - `XOMP_static_sched_init`
  - `XOMP_static_sched_next`
  - `getCUDABlockThreadCount`
  - `getLoopIndexFromCUDAVariables`

### Functional verification

Benchmark:

- `/home/ouankou/Projects/NeoRodinia-Old/rex/nn`

Compare:

```bash
./llvm_native_nn.out filelist_4 5 30 90
./nn.out filelist_4 5 30 90
```

The nearest-neighbor output in `stderr` must match.

### Performance verification

Use repeated runs, not a single run:

```bash
python3 - <<'PY'
import subprocess, re, statistics
pat = re.compile(r'total time\\s*:\\s*([0-9.]+)')
cmds = {
    "native": ["bash", "-lc", "cd /home/ouankou/Projects/NeoRodinia-Old/rex/nn && ./llvm_native_nn.out filelist_4 5 30 90"],
    "rex": ["bash", "-lc", "cd /home/ouankou/Projects/NeoRodinia-Old/rex/nn && ./nn.out filelist_4 5 30 90"],
}
for name, cmd in cmds.items():
    vals = []
    for _ in range(3):
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        vals.append(float(pat.search(p.stdout).group(1)))
    print(name, vals, statistics.mean(vals))
PY
```

Do not put a hard performance threshold in standard CI on a shared machine.
Use structural tests in CI and treat performance checks as local validation or
dedicated-runner validation.

## Files To Read First

Compiler and runtime:

- `src/midend/programTransformation/ompLowering/omp_lowering.cpp`
- `src/midend/programTransformation/ompLowering/xomp_cuda_lib_inlined.cu`
- `src/midend/programTransformation/ompLowering/rex_nvidia.cu`

Benchmark and generated outputs:

- `/home/ouankou/Projects/NeoRodinia-Old/rex/nn/nn.c`
- `/home/ouankou/Projects/NeoRodinia-Old/rex/nn/rose_nn.c`
- `/home/ouankou/Projects/NeoRodinia-Old/rex/nn/rex_lib_nn.cu`

Reference report:

- `/home/ouankou/Projects/NeoRodinia-Old/rex/LLVM_vs_REX_GPU_Comparison.md`

## Current Conclusion

The init-placement problem is fixed.

The remaining `nn` gap is a real lowering/runtime issue caused by using a
generic XOMP device scheduler for a tiny, repeatedly launched canonical loop.

The clean fix is to implement a general direct-lowered fast path for simple
target loops and keep XOMP scheduling only as the fallback for complex cases.
