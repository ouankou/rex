# Retained fragile CTest sets

These exact name-based manifests preserve standard-build tests that failed at
least once while hardening the frontend and unparser contracts. Passing later
runs alone never remove a test from the retained campaign.

- `unparser_hardening_fast.txt` contains 584 exact tests: the measured-fast
  portion of the retained campaign, its prerequisites, the
  exclusive-staging-descriptor security regression, and the no-PCH OpenMP
  validation-header, friend-template canonical-chain, and
  implicit-control-flow header-planning regressions. It runs on every push to
  `main` and every pull request targeting `main`.
- `unparser_hardening_full.txt` contains the 2,354 retained and support tests
  registered by the standard image. It runs in the daily x86_64 workflow.
  Optional Valgrind-backed variants remain available in Valgrind-enabled
  source builds instead of being duplicated in the standard nightly image.

The arm64, LoongArch64, and RISC-V nightly jobs run a small manifest of portable
architecture-sensitive frontend, STL, and module tests. Intel SIMD tests remain
native x86 tests. The complete retained manifest runs in the native x86_64
nightly.

The selection runner validates that every manifest entry is registered, adds
CTest dependencies and fixtures, and then runs the exact resulting set:

```bash
python3 scripts/run_ctest_name_set.py \
  --test-dir build \
  --manifest tests/fragile/unparser_hardening_fast.txt \
  --jobs "$(nproc)"
```
