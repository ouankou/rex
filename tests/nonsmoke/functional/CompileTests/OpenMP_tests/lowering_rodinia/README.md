# Rodinia-Derived OpenMP Lowering Tests

This suite validates lowering-specific behavior using reduced Rodinia-like
inputs and invariant checks.

Design goals:
- no dependence on legacy output reference files;
- catch semantic regressions that were observed during REX LLVM-21 migration;
- keep checks robust against unstable symbol hash IDs and format churn.

Current cases:
- `rodinia_axpy_multi_like`: three-kernel lowering shape with repeated calls to
  the same lowered offload helper from `main`.
- `rodinia_bfs_like`: duplicate preamble/include and offload-entry integrity.
- `rodinia_gaussian_like`: three-kernel lowering shape (`target teams` + `collapse(2)`).
- `rodinia_hotspot_like`: two collapsed-kernel lowering shape in one target-data region.
- `rodinia_nn_like`: single-kernel map-list lowering integrity and
  `rex_offload_init()` ordering before timed declarations.
- `rodinia_pathfinder_like`: target-data + private-clause kernel lowering integrity.
- `rodinia_srad_comments_like`: commented OpenMP pragma attachment/order.
- `rodinia_srad_v2_like`: two-kernel lowering shape and trailing target-data comment.
- `rodinia_btree_kernel_like`: two-kernel lowering with repeated host calls,
  multiple `map(to)` clauses, implicit pointer captures, and trailing comment
  relocation (`// main`).
