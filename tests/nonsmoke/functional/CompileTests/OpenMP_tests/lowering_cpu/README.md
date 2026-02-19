# OpenMP Lowering CPU Equivalence Tests

This suite validates CPU lowering semantics by comparing:

1. Original OpenMP source compiled and executed with LLVM OpenMP runtime (`libiomp5`)
2. REX-lowered source (`rose_*.c` + optional `rex_lib_*.c`) compiled and executed with the same runtime

For each test case, the harness runs both binaries repeatedly with `OMP_NUM_THREADS=2` and `OMP_NUM_THREADS=4`.

Comparison modes:

- `exact`: strict stdout/stderr match
- `sort`: preserves first line and sorts the remaining stdout lines (used for expected print interleaving differences)

The suite intentionally stages only `omp.h` from LLVM into a local include directory so the compiler still resolves its native C standard headers.
