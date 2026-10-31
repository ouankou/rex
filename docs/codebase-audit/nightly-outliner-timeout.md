# Nightly outliner timeout and validation provenance

Current local checkpoint (2026-10-14, AUD-1979): plain dlopen and copied-file
dlopen2 pass for both triangle and polygon on
`source-outliner-canonical-publication-20261014.sha256`, with source/binary/registry
guards. The copied-file cases take72.47/69.44 seconds under the unchanged timeout.
Logs: `container-outliner-canonical-publication-{ctest,polygon-ctest}-20261014.log`.
These focused Release results do not establish full or current-source remote CI
acceptance; historical results below retain their original revisions and scope.

The retained [nightly run 36921106337](https://github.com/ouankou/rex/actions/runs/36921106337)
on 2026-10-01 fails amd64 `outline_dlopen2_triangle_cpp`: the harness terminates
`outline` after the unchanged 900-second limit; CTest records 905.05 seconds,
one failure among 17,264 selected tests, and exit 8. ARM64, RISC-V and LoongArch
pass their selections. These results do not establish complete cross-platform
coverage or current audit acceptance.

Manifest J (`typed-modifier-consumption-source-20261002.sha256`)
passes that exact `outline_dlopen2_triangle_cpp` case locally in 10.04 seconds,
with the unchanged harness limit. Plain CTest run 39639 exits 8 with 3/4 passes:
the distinct `outline_dlopen_triangle_cpp`, which lacks `copy_orig_file`, fails
generated `findFunctionUsingDlopen` declaration visibility. The earlier local
invalid-free reproduction also ran plain dlopen, not the nightly dlopen2 case.
AUD-1082 records the modifier-consumption fix and verified source identity;
`container-modifier-consumption-ctest-20261002.log` preserves both outcomes.
These focused results do not establish full nightly or memory acceptance.

The full amd64 log is `build-audit/nightly-amd64-full-36921106337-20261001.log`.
It identifies the pulled image digest
`sha256:6ad50984f4cb9878cba3215ab4fa5b34338fc4d646c68d2adc00cf9c058e7664`.
Read-only manifest/configuration queries verify its amd64 revision label is
`eceaca2ef565d0bf89795335423655994f92220a`; the downloaded configuration's SHA-256
matches the manifest's configuration digest. Image metadata is retained in
`build-audit/nightly-{image-manifest,amd64-image-manifest,amd64-image-config}-36921106337-20261001.json`.
No image layers or containers were created. The uncommitted traversal fix and
other audit changes are absent from that image. This is a baseline failure,
not current-source CI acceptance. Its timeout is consistent with the earlier
profiled defect below; this new remote log does not provide a fresh stack profile.

The amd64 job in [nightly run 33980803291](https://github.com/ouankou/rex/actions/runs/33980803291)
failed `outline_dlopen2_polygon_cpp`: the test harness terminated `outline`
after its normal 900-second limit. CTest reported 905.04 seconds and one failure
among 17,264 selected tests. The triangle variant passed in 889.09 seconds,
leaving very little margin. ARM64 passed the polygon and triangle variants in
686.83 and 670.67 seconds. RISC-V and LoongArch ran their smaller selections;
their passing jobs do not establish coverage of this case.

In the [preceding nightly run 33904261916](https://github.com/ouankou/rex/actions/runs/33904261916),
amd64 polygon had already taken 889.25 seconds and triangle 886.77 seconds.
That run's ARM64 failures were the target-dependent `C_tests_test2018_26_c`
specimen and two Fortran OpenMP critical-lowering cases, which the subsequent
commit addressed. The timeout is a pre-existing performance defect with almost
no margin, not evidence that those specific semantic corrections were lost.

## Image and validation provenance

The failing job used revision `eceaca2ef565d0bf89795335423655994f92220a`.
The downloaded image's OCI revision label confirms that same revision:

```
ghcr.io/ouankou/rex@sha256:17ad17ddff8cc4ea0058589914da1db01bb40b019f0b4b002617b5094224afd8
```

The image build for that revision had completed before the nightly run. This
failure is not explained by a stale image or a lost compiler exit status: the
nightly job correctly propagated CTest's exit code 8.
The downloaded amd64 image is Debian forky/sid, LLVM/Clang 22.1.8 (1+b2),
CMake 4.3.4, and `CMAKE_BUILD_TYPE=Debug`.

The earlier local audit runs exported `ROSE_TEST_TIMEOUT_SCALE=2`. In particular,
the typed3 Debug run reported this case passing in 1,465.33 seconds, which exceeds
the nightly's unscaled limit. The unchanged local Debug baseline passed in
834.27 seconds; a Release run was faster. Neither result establishes reliable
execution within 900 seconds on the nightly runner. A claim that these results
proved nightly fixed was unsupported. Results from a focused selection, a
different build, an incomplete run, or an expanded timeout must retain that
scope when reported.

## Computational root cause

The generated `get_traversalSuccessorByIndex()` builds the complete validated
successor vector before returning one element. `get_numberOfTraversalSuccessors()`
also builds that vector. Each build checks ownership, required children, and
duplicate identities.

`RoseAst::iterator` repeatedly calls these accessors while pushing, dereferencing,
and advancing children. For an owner with many children, such as an
`SgAuxiliaryDeclarationList` or `SgDeclarationScope`, the same list is rebuilt
for every child. A full scan therefore incurs at least quadratic work in the
owner's child count, with additional ordered-set validation cost. Repeated
scans of a copied C++ translation unit amplify that cost.

Stack samples from the unmodified baseline and downloaded nightly binary reach
this path in copied-preprocessing publication, function declaration-family
collection and severing, dependent-declaration handling, destination-symbol
lookup, prototype-family collection, and friend validation. Both profiling
runs exited normally on the local host; those runs establish the hot paths,
not reproduction of the remote machine's elapsed time.

## Implementation

Eleven scans in `Transform.cc`, `GenerateFunc.cc`, `Insert.cc`, and
`sageInterface.C` now use the existing
`NodeQuery::querySubTree` traversal. Its `AstSimpleProcessing` implementation
materializes the validated successor vector once per node. These scans either
collect declarations before structural mutation, inspect symbol tables, or update
semantic declaration references and file/preprocessing metadata. They do not
need the mutable indexed iterator.

The replacement visits the same typed traversal edges in preorder. Queries
filter only where the old body already filtered by node kind. The physical-file
retargeting scan still visits **all** nodes so its structural-parent cycle
check retains its original coverage. Generated child validation, declaration
ownership checks, and all hard assertions remain active. The generic `RoseAst`
iterator's mutation behavior is unchanged. No timeout, test selection, expected
output, or memory suppression changes are part of this fix.

The root cause is in post-construction outliner traversal, so this correction
belongs in those consumers rather than altering frontend AST ownership.

## Validation record

Evidence is retained under ignored `build-audit/`:

- `latest-nightly-run.json`, `latest-nightly-failed.log`, and
  `latest-nightly-arm64.log`: remote results, including actual failure status.
- `previous-nightly-{amd64,arm64}.log`: the preceding barely passing outliner
  timings and the earlier, different ARM failures.
- `latest-nightly-image-provenance.txt`: downloaded image's revision label.
- `nightly-outline-{baseline,image}-profile/`: commands, stack samples, and
  normal-exit timing from the unmodified binaries.
- `nightly-traversal-fix.patch`: the isolated traversal change against the
  nightly revision, excluding the broader ongoing audit.
- `nightly-traversal-provenance.json`: patch/source hashes and the exact image,
  registry size, job count, and unscaled nightly settings.
- `nightly-traversal-fix/`: build and test results inside the downloaded image.
- `nightly-result-ledger.json`, `nightly-selection-verification.log`: complete
  per-test results checked against the exact dependency-closed nightly selection.
- `nightly-traversal-{sanitizer,memcheck}/`: separate baseline-build copies with
  the isolated change and unchanged instrumentation configurations.

The first four scans passed all ten focused tests without timeout scaling;
polygon took 272.81 seconds and triangle 269.31 seconds. This is preliminary
evidence, not acceptance of the complete patch. A subsequent preliminary
run was stopped to preserve cycle-check coverage for non-located nodes.

The seven-scan iteration passed all ten focused tests in 242.40 seconds;
polygon took 208.31 seconds and triangle 239.64 seconds. Its broader runs were
stopped after reviewing every profiling stack and identifying the remaining
declaration-chain, destination-symbol, and prototype-family scans. These partial
runs are retained with `seven-incomplete` names and do not establish acceptance
of the expanded eleven-scan patch.

The eleven-scan patch passes all **10/10 focused tests** in **86.23 seconds**,
including polygon in **72.58 seconds** and triangle in **83.50 seconds**, using
the downloaded Debug image, four jobs, and the unchanged 900-second limit.
Its separate **797/797 ASan/LSan/UBSan outliner and ownership checks pass** in
**924.70 seconds** with the baseline instrumentation settings.

The complete regular suite ran independently in a local snapshot of the rebuilt
image (`sha256:83849523fe5e9b47bbfe73b6aa5f27b921b06f303219cae7a50a6e210c626177`).
The runner verifies all four changed source hashes before testing. Its logs and
JUnit results use the `regular-parallel` prefix; no source or binary changes
occur between the snapshot and either regular test selection.

The **exact nightly selection passes 17,264/17,264 tests in 5,224.42 seconds**.
An independent name/status check confirms no missing or unexpected tests,
duplicate execution records, failures, or incomplete ordinal coverage. During
this full run, polygon passes in **190.09 seconds** and triangle in **193.91
seconds** under the unchanged 900-second harness limit. The two final large
comment-collection cases pass in 545.59 and 756.62 seconds; the failed remote
nightly had already passed those cases in 750.19 and 974.02 seconds.

The **complete regular suite passes 31,733/31,733 tests in 7,762.47 seconds**,
with CTest exit 0. The then-current audit checker verified the JUnit results
against the full registry and rejects missing, skipped, or failed tests.
Memcheck outliner/ownership validation completes all 797 unique tests with the
five baseline-reproduced failures documented below. The old collector also has
a known overwritten child report; complete execution does not establish clean
memory acceptance. `nightly-memcheck-coverage-only.json` records that distinction.
The redundant regular run automatically
started by the original serial runner was stopped after nightly coverage
verification; only the independent `regular-parallel` run is an acceptance run.
Memcheck uses the existing name-hash sharding mechanism across four independent
build copies. Outliner tests reserve 16 CTest processor slots each; sharding
allows independent cases to run concurrently while preserving every registered
property. The union of completed shard test names must cover the entire 797-test
selection, including fixture/dependency closure. The earlier serial Memcheck
attempt is incomplete and is not an acceptance result.

The unchanged-baseline Memcheck configuration reports five conditional-read
errors in one Flang `UnwrapWholeSymbolDataRef` context for each of
`outline_noswitch_function_f`, `outline_noswitch_function2_f`,
`outline_seq7b_function_f`, and `outline_seq7b_function2_f`. Independent
runs of the **unmodified baseline compiler** report the same five errors, one
context, and zero definitely, indirectly, or possibly lost bytes for each
specimen; both versions exit 1. Raw evidence is in
`nightly-traversal-memcheck/baseline-function{,2}/`,
`baseline-seq7b-function{,2}/`, and the four shard reports
`MemoryChecker.{32858,32859,33395,33396}.log`. This is the already documented
[EXT-FLANG-INACTIVE-DATAREF](external-exceptions.md#ext-flang-inactive-dataref-verified-report-exception),
whose narrow audited exceptions are absent from this frozen baseline
configuration. The shard remains a failed run; the report is not counted as
a clean Memcheck result or a new outliner regression. The audit image also
lacks `tclsh`, which produces a separate graph-rendering diagnostic in both
versions. Neither diagnostic is hidden by this patch.

`outline_complexStruct` also aborts under Memcheck with
`REX_COPY_INVARIANT[copied-type-reverse-map]`: a copied pointer type and its
base's canonical pointer cache disagree. The unmodified baseline reproduces
the same assertion and exit 134, with zero Valgrind errors or lost bytes;
evidence is in `nightly-traversal-memcheck/baseline-complexStruct/` and shard 4's
`MemoryChecker.33423.log`. AUD-037 fixes its construction root cause separately:
copied types begin with empty reverse caches, and later materialization preserves
published canonical wrappers. The new cache contract fails against the baseline;
both it and this outliner case pass in regular, sanitizer, and Memcheck checks.
The assertion remains active. The isolated nightly patch's original failed
result remains recorded; it does not contain the separate cache correction.

Report-integrity review also finds a separate Memcheck false-success defect
(AUD-038). Traced children reopen the same fixed CTest log filename and can
erase diagnostics. In a retained standalone CTest reproducer, an undefined read
in a child is hidden and CTest exits 0. Separate process logs retain the error,
but CTest itself still exits 0 when the parent succeeds, despite recording one
memory defect. The wrapper must also return the configured error status when
any process reports memory errors. The same reproducer then fails with CTest
exit 16. The regular
nightly timeout is a different defect; this finding invalidates clean Memcheck
claims based on incomplete logs, not the verified regular nightly results.

Per-process files have a further proven failure: exec keeps the same PID and
truncates the earlier report. `%p.%n` also loses that earlier error. Both probes
return 0 in `build-audit/wrapper-instrumented-exec/`, so the typed11 collector
and its incomplete full runs cannot establish clean memory acceptance.

The final typed12 collector uses a FIFO for Valgrind's log destination and one
reader writing CTest's continuous report. Reopening this stream cannot erase
earlier diagnostics. It gates on immediate error markers, including when an
excluded exec omits the final summary, and preserves the command's own failure
status. Four actual negative CTest cases fail with exit 16: an ignored child
error, same-PID instrumented exec, an error between fork and exec, and concurrent
failing children. The complete wrapper contract passes directly and nested under
the project's production Memcheck options. Evidence is retained in
`build-audit/wrapper-report-validation-stream/`. The matching Valgrind 3.27.1
source confirms report writes are bounded below Linux's atomic pipe-write size;
all eight concurrent child errors are retained by the contract.

CTest's output-matching properties need an independent final check.
`PASS_REGULAR_EXPRESSION`, used by two existing version-reporting tests, can
accept the wrapper's nonzero exit. The `wrapper-pass-regex` counterexample
reports one defect yet exits 0. The configured post-Memcheck hook now parses
the completed dashboard's XML defect counts and fails on any defect. Its
end-to-end contract proves the unhooked false success and the gated failure,
as well as clean subsequent runs, machine-readable inventories, and rejection
of missing or incomplete reports. This typed13 addition changes result
validation only; the compiler and continuous collector match the ongoing runs.
The weekly memory workflow also archives `Testing/TAG`, the completed
`DynamicAnalysis.xml`, and `LastDynamicAnalysis*.log` alongside raw Valgrind
reports. Previously the uploaded artifact omitted the dashboard that records
defect counts. This artifact-only change preserves CI's test commands and
selection and makes the final gate's evidence available for later review.

The isolated polygon and triangle cases also finish under the original
Memcheck settings in 2,884.86 and 2,724.26 seconds. These instrumented times
cannot establish nightly timing. Fresh full baseline and current memory runs
use the continuous-stream collector under `build-audit/stream-memcheck/`; their
outstanding results remain required. No incomplete or superseded collector run
counts as clean acceptance.

The broader audit compiler also passes the full **37,402/37,402 regular tests**
in **9,039.41 seconds**, with actual exit 0 and complete JUnit coverage verified.
Its full-run polygon and triangle times are 178.85 and 128.32 seconds, with no
timeout scaling. The source manifest is unchanged throughout this run; its
compiler sources match typed13, which adds the separately validated reporting
contracts and result hook. `build-audit/typed8-regular-verified.json` records
the process state, source verification, and full-registry result check.

The regular image runs use four jobs and
unset `ROSE_TEST_TIMEOUT_SCALE`, matching nightly. Instrumented runs retain the
baseline's documented instrumentation timeout scales; their times cannot be
used to establish nightly performance. Memory baseline failures and the broader
audit's unresolved findings remain separate from this isolated fix. No passing
remote rerun or full-current-worktree acceptance is claimed.
