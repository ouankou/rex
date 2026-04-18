# Debug Notes

## Coupled Failure Group

- Reference doc reviewed: `../ctest-failure-investigation-2026-04-17.md`
- The currently tracked bounce group is the `moveDeclarationToInnermostScope`
  diff family, not the execution/token tests.
- Representative coupled shard re-run on April 18, 2026:
  `ctest --test-dir build --output-on-failure -j8 -R '^(moveDecl_v[123]_inputmoveDeclarationToInnermostScope_(2|11|14)_C|moveDecl_v[123]_inputmoveDeclarationToInnermostScope_test2014_(18|22)_C|moveDecl_v[123]_diff_inputmoveDeclarationToInnermostScope_(2|11|14)_C|moveDecl_v[123]_diff_inputmoveDeclarationToInnermostScope_test2014_(18|22)_C|tokenStream_CXX_test2006_151_C|tokenStream_CXX_test2011_83_C|tokenLinearization_testLin1_c|tokenStream_CXX_test2005_62_C)$'`
- Result at this point:
  - `tokenStream_CXX_test2006_151_C` passed
  - `tokenStream_CXX_test2011_83_C` passed
  - `tokenLinearization_testLin1_c` passed
  - `tokenStream_CXX_test2005_62_C` passed
  - All 15 `moveDecl_v[123]_diff_*` checks for `_2`, `_11`, `_14`,
    `test2014_18`, and `test2014_22` still fail

## Current Shape Of The Remaining Drift

- The regular `moveDecl` execution tests pass, so the remaining issue is output
  fidelity, not transformation execution.
- The current failure surface is still coupled across all 15 diffs:
  - `inputmoveDeclarationToInnermostScope_2.C`
    - file-prefix token replay still duplicates the leading `#define`
    - untouched global declaration spelling/layout still bounces between
      token-like and AST-normalized surfaces
  - `inputmoveDeclarationToInnermostScope_11.C`
    - comments and declaration/function surface formatting drift together
  - `inputmoveDeclarationToInnermostScope_14.C`
    - comment placement around the transformed loop still bounces
  - `test2014_18` and `test2014_22`
    - large untouched header/global regions still drift together, which means
      this is still a shared token-vs-AST surface problem rather than isolated
      per-test logic

## Constraints For Follow-Up

- Validate fixes against the whole coupled shard above, not one test at a time.
- Do not update reference outputs.
- Do not reintroduce temp debug instrumentation in the unparser or
  `moveDeclarationToInnermostScope`.
- Keep test artifacts in the build tree only. `git ls-files --others
  --exclude-standard src tests` was clean when this note was written.
