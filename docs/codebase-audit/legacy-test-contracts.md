# Legacy contracts exposed by correct process status

These findings are not accepted exceptions or passing results. The initial
revision is eceaca2ef5. AUD-066 corrects packed wait statuses: a backend exit 1
previously became 256 and then exit 0 when returned from a translator's main.
The full typed45 run exposes the previously hidden failures.

## OpenMP private storage

AUD-243 independently exposes an undefined read in stock-compiled `parallel.c`:
the private scalar has no initial value, so `assert(i != 100)` cannot test its
privacy. The control now requires a distinct address, writes the private scalar,
and checks that the shared original remains 100 afterwards. Existing firstprivate
and shared-value assertions remain. Both original and lowered programs remain
instrumented; no compiler initialization, exception, skip or relaxed expected
status is introduced. The original failure is retained in the through-AUD-230
Memcheck report for `MemoryChecker.30924.log`.

## Invalid positive specimens

Stock LLVM 22.1.8 independently rejects these unchanged specimens with exit 1
under `flang -fsyntax-only`; rechecked 2026-10-27 with stock Flang 23.1.5
(`flang-23 -fsyntax-only -fcoarray`), which reports the identical rows,
columns and messages. REX reports the same source diagnostics. Making
REX accept them would weaken the frontend. Implemented contract: retain the
specimens and CTest names, require exit 1 and the exact source diagnostics, and
require no generated source/object. A crash, wrong diagnostic, success, or
memory defect must fail the negative test.

| Specimen | Exact reason for rejection |
| --- | --- |
| R511-F2015-N2007.f90 | FAIL IMAGE has extra operands at lines 7 and 8 |
| R1164-F2018.f90 | STAT is REAL instead of INTEGER; ERRMSG is REAL instead of CHARACTER |
| R1166-F2018.f90 | STAT is REAL instead of INTEGER; ERRMSG is REAL instead of CHARACTER |
| R1168-F2018.f90 | STAT is REAL instead of INTEGER; ERRMSG is REAL instead of CHARACTER |
| R1169-F2018.f90 | Two team values lack TEAM_TYPE; STAT and ERRMSG have incorrect types |
| R1179-F2018.f90 | Two lock variables lack LOCK_TYPE |
| R1181-F2018.f90 | Two unlock variables lack LOCK_TYPE |

Evidence: `build-audit/fortran-grammar-controls/results.json`, individual stock
compiler logs, `build-audit/typed45/failures-additional.json`, and
`build-audit/typed45/hidden-baseline-failures.json` (including the original
"Passed" R1164 result containing semantic errors).

## Translation units without a program entry point

`unparse_tokens_test14`, `test15`, `test16`, and `test19` invoke the translator
without `-c`, although their primary source files contain no main. They fail
linking with an undefined reference to main. The test14 README describes header
and preprocessing emission coverage. Implemented contract: retain specimens,
transformations, output comparisons, and names; add explicit compile-only `-c`
to these four test commands so the generated translation units must compile.

## Valid-input compiler defects and external limits

`Translator_testTranslator2010_02`, `testTranslator2012_01`, and
`testTranslator2013_01` expose AUD-102, where compilation relocates objects but
linking uses a basename. These require the compiler fix, without test changes.

`R507-F2015-N2007.f90` passes stock frontend syntax/semantic analysis. Stock
Flang code generation rejects its coarray operations without experimental
coarray support. It remains unresolved; the negative-contract corrections
above do not include it. `R1123-F2018.f90` passes stock frontend analysis and
is unchanged.
