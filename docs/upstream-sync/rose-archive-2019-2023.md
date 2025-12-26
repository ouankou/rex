# rose-archive 2019-2023 selective sync (full REX scope)

## Support target (REX)
- Platform: Linux only (Windows/mac support dropped entirely).
- Languages/features: C/C++, Fortran, OpenMP, OpenCL, CUDA.
- Hard dropped: PHP, JavaScript, EDG, Java, UPC, Python, X10, binary analysis, and any other legacy frontends not in use by REX.

## Scope for this sync
- Full repo coverage for all kept REX components (every non-generated directory in the repo, not limited to astUtil).
- Inventory only: Autotools/Tup (`configure.ac`, `config/`, `acmacros/`, `Makefile.am`, `stamp-h*.in`, `Tupfile`). Track changes for parity/purge, do not port into CMake.
- Exclude dropped components and platform-specific code (Windows/mac).

## Components in REX to keep (sync coverage)
Top-level repo components:
- Repo metadata and CI: `.github/`, `.claude/`, `.gemini/`.
- Build system (CMake): `cmake/`, `CMakeLists.txt`, `build-rex.sh`, `rose_config.h.in.cmake`, `ROSE_VERSION`.
- Build inventory (Autotools/Tup; record only): `configure.ac`, `config/`, `acmacros/`, `Makefile.am`, `stamp-h*.in`, `Tupfile`.
- Docs and guides: `docs/`, `README.md`, `BUILDING_WITH_CLANG.md`, `OPENMP_SUPPORT.md`, `FORTRAN_TESTING_GUIDE.md`, `FORTRAN_EVALUATION_STATUS.md`, `CLANG_FRONTEND_FIXES.md`, `CLANG_FRONTEND_IMPROVEMENTS.md`, `ROSE_COMPILER_FIXES.md`, `TEMPLATE_INSTANTIATION_ROADMAP.md`, `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, and other root `*.md` project notes.
- Scripts and tooling: `scripts/`, `tools/`, `tutorial/`, `exampleTranslators/`.
- Licensing and metadata: `LicenseInformation/`, `COPYRIGHT`.
- Tests: `tests/` (C/C++/Fortran/OpenMP/OpenCL/CUDA only), plus root-level OpenMP parser fixtures (`test_omp_*.c`).
- Source tree: `src/` (see below).
- Generated output (ignore): `build/`, `CMakeFiles/`, `lib/`, and `src/**/CMakeFiles/`.

`src/` subcomponents:
- `src/3rdPartyLibraries/` (existing third-party deps used by REX).
  - `libharu-2.1.0/`, `antlr-jars/`, `fortran-parser/`
- `src/frontend/`
  - `CxxFrontend/Clang/`
  - `OpenFortranParser_SAGE_Connection/`
  - `SageIII/`
    - `accparser/`, `ompparser/`
    - `astFixup/`, `astHiddenTypeAndDeclarationLists/`, `astPostProcessing/`, `astTokenStream/`
    - `includeDirectivesProcessing/`, `sage_support/`, `sageInterface/`, `virtualCFG/`
    - `docs/`, `GENERATED_CODE_DIRECTORY_Cxx_Grammar/`
- `src/midend/`
  - `abstractLayer/`, `astDiagnostics/`, `astDump/`, `astProcessing/`, `astQuery/`, `astUtil/`
  - `programAnalysis/` (bitvectorDataflow, CFG, CallGraphAnalysis, dataflowAnalysis, defUseAnalysis, dominanceAnalysis, genericDataflow, OAWrap, OpenAnalysis, pointerAnal, staticInterproceduralSlicing, valuePropagation, variableRenaming, VirtualFunctionAnalysis)
  - `programTransformation/` (astInlining, astOutlining, constantFolding, extractFunctionArgumentsNormalization, finiteDifferencing, functionCallNormalization, implicitCodeGeneration, loopProcessing, ompLowering, partialRedundancyElimination, singleStatementToBlockNormalization, transformationTracking)
- `src/backend/unparser/` (CxxCodeGeneration, FortranCodeGeneration, formatSupport, languageIndependenceSupport)
- `src/ROSETTA/` (Grammar, ROSETTA tools)
- `src/util/` (commandlineProcessing, graphs, Sawyer, StringUtility, support)
- `src/Rose/` (small shared headers like `SourceLocation.h`)

## Keep/drop lists
- Keep (candidate paths):
  - `cmake/**`, `CMakeLists.txt`, `build-rex.sh`, `rose_config.h.in.cmake`, `ROSE_VERSION`
  - `docs/**`, `scripts/**`, `tools/**`, `tutorial/**`, `exampleTranslators/**`, `LicenseInformation/**`, root `*.md`
  - `tests/**`, `test_omp_*.c`
  - `src/**` (excluding dropped components)
- Drop (never reintroduce): EDG, Java, UPC, PHP, JavaScript, binary analysis, Windows/mac-specific code paths, and other removed frontends.
- Inventory only: Autotools/Tup build metadata (`configure.ac`, `config/`, `acmacros/`, `Makefile.am`, `stamp-h*.in`, `Tupfile`) and generated build output (`build/`, `CMakeFiles/`, `lib/`, `src/**/CMakeFiles/`).

## Repeatable sync workflow
1) Fetch upstream:
   ```bash
   git fetch rose-archive develop
   ```
2) Build a candidate commit list for all kept paths:
   ```bash
   git log --since=2019-01-01 --name-only --pretty=format:'%H %cs %s' \
     rose-archive/develop -- \
     cmake CMakeLists.txt build-rex.sh rose_config.h.in.cmake ROSE_VERSION \
     docs scripts tools tutorial exampleTranslators LicenseInformation \
     src tests
   ```
3) Generate path-filtered patches per commit (drop-list filtered):
   ```bash
   git format-patch -1 <upstream_commit> -- \
     cmake CMakeLists.txt build-rex.sh rose_config.h.in.cmake ROSE_VERSION \
     docs scripts tools tutorial exampleTranslators LicenseInformation \
     src tests
   ```
4) Apply and resolve:
   - Use `git am -3` or `git apply`, then resolve conflicts.
   - If a commit mixes kept/dropped hunks within a file, use `git add -p` to keep only relevant hunks.
5) REX-specific adjustments (avoid reintroducing dropped deps):
   - Keep Clang/LLVM-only frontend.
   - Avoid Windows/mac-specific branches.
   - Keep `mlog.h`/`ASSERT_require` usage where REX has standardized diagnostics.
   - Do not add EDG/Java/UPC/PHP/JS/binary analysis code paths.
   - Update CMake sources only; ignore Autotools/Tup files.
6) Validate:
   ```bash
   cmake --build build -j32
   ctest --test-dir build --output-on-failure -R astInterface
   ctest --test-dir build --output-on-failure -R rex
   ```
7) Record provenance (see tables below).

## Triage artifacts
- `docs/upstream-sync/rose-archive-2019-2023-triage.tsv`: full commit triage output for kept paths (2019-01-01 → 2023-10-26).

## Upstream provenance
### Phase 1: astUtil + side-effect interface (completed)
The following rose-archive commits (2019-01-01 through 2023-10-20) are reflected in the astUtil sync.

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

### Phase 2: full repo sync (in progress)
- Provenance for the full-repo sync will be recorded here as commits are applied.
