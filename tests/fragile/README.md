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
- `unparser_hardening_full.txt` contains 2,350 portable retained and support
  tests. The native x86 policy also selects four x86-only registrations.

The native amd64 and arm64 nightlies use the same large policy: a superset of
main CI adding the full retained manifest, broad C/C++ and AST infrastructure
suites, and OpenACC. The emulated LoongArch64 and RISC-V jobs instead run eight
architecture-sensitive tests plus `rex_`, astInterface, and `OMPTEST_` tests.

The selection runner validates that every manifest entry is registered, adds
CTest dependencies and fixtures, and then runs the exact resulting set:

```bash
python3 scripts/run_ctest_name_set.py \
  --test-dir build \
  --manifest tests/fragile/unparser_hardening_fast.txt \
  --jobs "$(nproc)"
```
