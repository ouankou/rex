# rose-archive 2019-2023 selective sync (astUtil + side-effect interface)

## Scope for this sync
- Focused paths: `src/midend/astUtil/**` plus dependent callers in:
  - `src/midend/programAnalysis/bitvectorDataflow/**`
  - `src/midend/programAnalysis/pointerAnal/**`
  - `src/midend/programAnalysis/valuePropagation/ValuePropagate.C`
  - `src/midend/programTransformation/loopProcessing/**`
  - `tests/nonsmoke/functional/roseTests/programAnalysisTests/**`
- Excluded (inventory only): Autotools/Tup (`configure.ac`, `config/`, `Makefile.am`, `Tupfile`).
- Not touched: dropped frontends/components (EDG, Java, UPC, binary analysis, etc.).

## Keep/drop lists
- Keep (candidate paths):
  - `src/midend/astUtil/**`
  - `src/midend/programAnalysis/bitvectorDataflow/**`
  - `src/midend/programAnalysis/pointerAnal/**`
  - `src/midend/programTransformation/loopProcessing/**`
  - `tests/nonsmoke/functional/roseTests/**` (only those tied to astUtil interfaces)
- Drop (never reintroduce): EDG, Java, UPC, binary analysis, other dropped frontends.
- Inventory only: Autotools/Tup build metadata.

## Repeatable sync workflow
1) Add upstream remote and fetch:
   ```bash
   git remote add rose-archive https://github.com/rose-compiler/rose-archive.git
   git fetch rose-archive develop
   ```
2) Build a candidate commit list for kept paths:
   ```bash
   git log --since=2019-01-01 --name-only --pretty=format:'%H %cs %s' \
     rose-archive/develop -- src/midend/astUtil
   ```
3) Generate path-filtered patches per commit:
   ```bash
   git format-patch -1 <upstream_commit> -- \
     src/midend/astUtil \
     src/midend/programAnalysis/bitvectorDataflow \
     src/midend/programAnalysis/pointerAnal \
     src/midend/programTransformation/loopProcessing \
     tests/nonsmoke/functional/roseTests
   ```
4) Apply and resolve:
   - Use `git am -3` or `git apply`, then resolve conflicts.
   - If a commit mixes kept/dropped hunks within a file, use `git add -p` to keep only relevant hunks.
5) REX-specific adjustments (avoid reintroducing dropped deps):
   - Keep `mlog.h`/`ASSERT_require` usage where REX has standardized diagnostics.
   - Do not add `ROSE_ASSERT.h`/`ROSE_ABORT.h` includes (not present in REX).
   - Keep `std::unordered_map` in `PtrAnal` to avoid reintroducing Boost.
   - Update `CMakeLists.txt` for new/removed sources only; ignore Autotools/Tup files.
6) Validate:
   ```bash
   cmake --build build -j32
   ctest --test-dir build --output-on-failure -R astInterface
   ctest --test-dir build --output-on-failure -R rex
   ```
7) Record provenance (see table below).

## Upstream provenance for this sync
The following rose-archive commits (2019-01-01 through 2023-10-20) are reflected in the synced paths listed above. They were applied as a path-filtered sync to the astUtil surface and its direct callers.

| Upstream commit | Summary | REX commit | Notes |
| --- | --- | --- | --- |
| 9dfc73ed0bc3295d09da26b33e06909a5f45a928 | Merge commit (loop processing fixes) | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| f79ac83d980f6102b86f12d2cf12f03760c48627 | LoopProcessing Windows naming fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 1243bec075cbd36dffcc07f5e2d602ea85cf9d8c | Side-effect interface refactor (templates) | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Adjusted for REX diagnostics |
| 6725b8f8313efc40f7c04a03ac48dcdf8181bb98 | Warning cleanups | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 07b881108b3398691aaaf326e35997e78d5cee2c | Read/write set handling | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Added `IsAddressOfOp` |
| 2fdb0ff1ed1241c6706840f6e16bd9a9af024885 | NodeId constructor | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Comment sync only |
| 4e692f544fd8a5cc5a3151403f1eb2d602be2588 | Uninitialized warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| b7656a1c62fbb1a0d6ce088188b0c4e961161ae1 | Side-effect analysis updates | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 6b301bcc1b39bf0d94bc586847ff37378b47e853 | Side-effect analysis updates | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 11d961a260ff3d5e36615d0740ec9b85353177ab | Side-effect analysis updates | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 5d90087c6774787869aeb1c20b70068f8ae3788c | ROSE_ABORT replacements | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Kept `mlog.h` usage |
| 5c5691d30491c2d622b552c828ab4f046b681166 | ROSE_ASSERT/ROSE_ABORT cleanup | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | REX diagnostics preserved |
| a85b98bc3dc72b3d483c4136d595c2de8d763640 | Warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| cd40e50cffdfa19a4a28164eb3de8b30cf519748 | Warning fixes (TypeAnnotation/CallGraph) | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| d615866904cf52b47f571979451c90b9af7d92c5 | Warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| e0ab773f03d677a632f6e233ab3da20cea0b5480 | Loop-processing warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 9807d943a16fd8eb639079eb94a3ed76a3307b22 | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| ba5b9c8538d19c166ca503e4d0bb96465a55220e | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 84fbc7948d3c548ef451638fac2e890a6f78a1ad | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 64971c61ba1470301b2a06f653d4cfcaedee790e | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| c929b4e5a9ecccadfac4dfeee16fb1aa23212cf2 | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| ce5e03a0d9792dc3ccd913f349381ffd59bc4e43 | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| c4769f6167d899c4bafa586ca1c125e1bccbc84b | Klocwork midend fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 06593742ee2052749fea40255b0e7603f78ec8f5 | RoseAssert include fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | `mlog.h` kept |
| ef3387f1d4eb0d5b8fc6b97fe564f57e90fa3d46 | Added astUtil interface files | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | `AstUtilInterface.*` |
| 79402565c855b77f171dafe7c9121ec6b782b8b1 | Dependence analysis fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 018afaf01388e57f661de571ddac4bb4846c94d6 | Warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| 3f7f4553bb865af343dd83273c9051d1cbd158b8 | Warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |
| eca7c1047172b53c66f95ab5faef9db4b963991f | Warning fixes | d0d9388faf4aabdb97298cac5180e32c8b8e7c76 | Path-filtered only |

