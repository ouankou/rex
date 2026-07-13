# Retained fragile CTest sets

These exact name-based manifests preserve tests that failed at least once while
hardening the frontend and unparser contracts. Passing later runs never remove
a test from the retained campaign.

- `unparser_hardening_fast.txt` contains 564 exact tests: the measured-fast
  portion of the retained campaign, its prerequisites, the
  exclusive-staging-descriptor security regression, and the no-PCH OpenMP
  validation-header, friend-template canonical-chain, and
  implicit-control-flow header-planning regressions. It runs on every push to
  `main` and every pull request targeting `main`.
- `unparser_hardening_full.txt` contains all 2,524 retained tests present in
  the parent registry, 24 exact dependencies and fixture owners, the
  exclusive-staging-descriptor regression, and the no-PCH header regression.
  It runs in the daily x86_64 workflow, including its
  Valgrind-dependent tests. That workflow unions the manifest with its
  pre-existing core-test regex so this campaign does not reduce prior nightly
  coverage.

Run a manifest with:

```bash
python3 scripts/run_ctest_name_set.py \
  --test-dir build \
  --manifest tests/fragile/unparser_hardening_fast.txt \
  --jobs "$(nproc)"
```

The runner rejects an empty or duplicate manifest, duplicate registry names,
missing configured tests, and any mismatch between requested names and CTest's
numeric selection. It does not skip absent tests. An optional `--include-regex`
unions the manifest with CTest's own exact `-R` selection; the runner validates
that union before execution.
