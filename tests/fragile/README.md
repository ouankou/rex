# Retained fragile CTest sets

These exact name-based manifests preserve tests that failed at least once while
hardening the frontend and unparser contracts. Passing later runs never remove
a test from the retained campaign.

- `unparser_hardening_fast.txt` contains 585 exact tests: the measured-fast
  portion of the retained campaign, its prerequisites, the
  exclusive-staging-descriptor security regression, and the no-PCH OpenMP
  validation-header, friend-template canonical-chain, and
  implicit-control-flow header-planning regressions. It runs on every push to
  `main` and every pull request targeting `main`.
- `unparser_hardening_fast_non_x86_absent.txt` names the two tests in that
  manifest which the CMake registry deliberately omits on non-x86 targets.
  Non-x86 architecture jobs pass it as an exact expected-absence contract:
  the runner rejects an unexpected registration, an absence not declared in
  the primary manifest, or any additional missing retained test.
- `unparser_hardening_full.txt` contains all 2,545 retained tests present in
  the parent registry, 24 exact dependencies and fixture owners, and four
  explicit contract regressions, for 2,573 exact tests total.
  It runs in the daily x86_64 workflow, including its
  Valgrind-dependent tests. That workflow unions the manifest with its
  pre-existing core-test regex so this campaign does not reduce prior nightly
  coverage.

The arm64, LoongArch64, and RISC-V nightly jobs run the fast manifest plus exact
architecture-sensitive frontend, STL, module, and OpenMP SIMD contracts. The
complete retained manifest and broad core regex remain mandatory in the native
x86_64 nightly.

Run a manifest with:

```bash
python3 scripts/run_ctest_name_set.py \
  --test-dir build \
  --manifest tests/fragile/unparser_hardening_fast.txt \
  --jobs "$(nproc)"
```

The runner rejects an empty or duplicate manifest, duplicate registry names,
missing configured tests, and any mismatch between requested names and CTest's
numeric selection. It does not skip absent tests. An optional exact
`--expected-absent-manifest` contract declares architecture-specific omissions
and hard-fails if either the registry or primary manifest disagrees. An
optional `--include-regex` resolves the regex against CTest's registry,
computes dependency and fixture closure, and runs the complete union through
one exact numeric selector. The runner validates that union before execution.
