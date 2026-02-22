# Rodinia-Derived OpenMP Lowering Tests

This suite validates lowering-specific behavior using reduced Rodinia-like
inputs and invariant checks.

Design goals:
- no dependence on legacy output reference files;
- catch semantic regressions that were observed during REX LLVM-21 migration;
- keep checks robust against unstable symbol hash IDs and format churn.

Current cases:
- `rodinia_bfs_like`: duplicate preamble/include and offload-entry integrity.
- `rodinia_gaussian_like`: three-kernel lowering shape (`target teams` + `collapse(2)`).
- `rodinia_hotspot_like`: two collapsed-kernel lowering shape in one target-data region.
- `rodinia_nn_like`: single-kernel map-list lowering integrity.
- `rodinia_pathfinder_like`: target-data + private-clause kernel lowering integrity.
- `rodinia_srad_comments_like`: commented OpenMP pragma attachment/order.
- `rodinia_srad_v2_like`: two-kernel lowering shape and trailing target-data comment.
- `rodinia_btree_kernel_like`: host offload-entry integrity and trailing comment
  relocation (`// main`).
