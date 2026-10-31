# External exceptions

Policy and required evidence are in [README.md](README.md#external-behavior-policy).
**An entry awaiting evidence is not an accepted exception.** Existing suppressions
remain under review; a broad historical suppression is not automatically valid.

Five historical dynamic-loader leak masks were removed from
`scripts/rex-suppressions-for-valgrind`: they matched all leak kinds through
wildcard frames and had no accepted exception record. Classify any newly
exposed loader report by exact origin before considering a narrow exception.

## EXT-FLANG-KIND4-SUBSTRING — unresolved native lowering limitation

Follow-up: [REX issue862](https://github.com/ouankou/rex/issues/862), including an
independently verified minimal reproducer and conditions for restoring runtime
coverage.

Both Flang fixtures below were introduced by this PR. Their native reproductions
explain current failures; they are not pre-PR baseline evidence or passing gates.

Stock Ubuntu Flang 22.1.8 (`ca7933e47d3a`) aborts when compiling the unchanged
`rex_flang_include_main.f90:27` KIND4 literal substring. Local CTest
`rex_flang_include_paths` reproduces this in its original-source native control,
before invoking REX: `ConvertExprToHLFIR.cpp:514`, “not yet implemented:
StaticDataObject::Pointer substring with kind > 1”. Evidence and source/binary
guards: `build-audit/pr-round10-flang-include-reproduce.log` and
`build-audit/pr-round10-flang-include-reproduce-status.txt`.
The user approved an exact negative contract for this limitation. The include-path
harness first requires all existing REX import, substring, copy and JSON assertions
to pass, then requires both original and emitted source to fail with exit1 and
exactly one KIND4 lowering diagnostic for each of the three path spellings.
Missing output, a different error and unexpected native success all fail the test;
no broad CTest WILL_FAIL property is used. Restore the original/translated runtime
comparison when stock Flang implements this lowering. No LLVM patch is carried.

Reaching those REX assertions exposed two separate REX defects: constant LEN
expressions were not folded before type construction, and string emission dropped
the typed KIND selector. Both are fixed in source733. The negative contract and
26 adjacent controls pass733; source734 also passes the new positive Unicode
runtime specimen through original Flang and all four REX compile/runtime stages.
All31 distinct focused CTests pass, with matching inventories and source guards.
This does not close the separate coarray or template-binding failures.

The source review717 checks both stock lowering paths: `ConvertExpr.cpp:1411`
and `ConvertExprToHLFIR.cpp:514` contain this same explicit unimplemented case.
The deprecated no-HLFIR option therefore does not supply the missing lowering.
No backend-selection workaround is installed. Stock source commit, exact source
excerpts and native-tool guards are in `build-audit/pr-round717-*`.
Rechecked 2026-10-29 with Debian Flang 23.1.5 on aarch64: the unchanged
original-source control still exits1 at `rex_flang_include_main.f90:27` with
`ConvertExprToHLFIR.cpp:530`, “not yet implemented: StaticDataObject::Pointer
substring with kind > 1”.

## EXT-FLANG-COARRAY-LOCK — unsupported native lowering

Follow-up: [REX issue864](https://github.com/ouankou/rex/issues/864).

Stock Ubuntu Flang 22.1.8 (`ca7933e47d3a`) cannot compile the unchanged
`rex_flang_image_control_statements.f90`. Enabling its advertised `-fcoarray`
option advances past the experimental-feature gate, then fails at line15:
`flang/lib/Lower/Runtime.cpp:161`, “not yet implemented: coarray: LOCK runtime”.
This independent original-source compilation exits1 without invoking REX.
Commands, compiler version, diagnostics and source/binary guards are preserved in
`build-audit/pr-round12-image-control-*`.
Rechecked 2026-10-29 with Debian Flang 23.1.5 on aarch64: `-fcoarray`
compilation of the unchanged specimen still exits1 at line15 with
`flang/lib/Lower/Runtime.cpp:156`, “not yet implemented: coarray: LOCK runtime”.

The user approved converting native compilation to an exact negative contract.
The dedicated typed AST/unparse check and generic translation, graph and token
stages stay positive. A separate native test requires original and emitted source
to exit1 with exactly one LOCK-lowering diagnostic under -fcoarray; missing output,
other failures and unexpected success fail. All22 focused checks pass741. Existing
fixtures and typed assertions are unchanged, and no blanket WILL_FAIL is set.
Restore native compilation when support exists; no LLVM patch or no-op lowering
is carried. This approval does not establish coarray runtime correctness.

## EXT-CLANG-UBSAN-CONSTANT-P — unresolved instrumentation behavior

Verified2026-10-19 with stock Clang22.1.8, Linux x86_64, in the existing read-only
audit image. The independent `build-audit/rex_pre_call_prefix.cpp` calls this
function with zero:

```cpp
int unevaluated(int divisor) {
  return __builtin_constant_p(42 / divisor + 42 / divisor);
}
```

Compile with `clang++-22 -O0 -g -fsanitize=undefined
-fno-sanitize-recover=all`. The untouched original and REX output compile0, then
exit1 with division by zero in the builtin operand. GDB reproduces the original
failure without REX. Both plain compilations succeed; the original completes0
and the transformed program completes0 under Memcheck with zero errors/leaks.
The native AST records BuiltinFnToFnPtr; the transformed expression remains
inside the intrinsic, without a hoisted division.

Raw diagnostics, real exits, complete source/artifact guards and version/image
identity are under `build-audit/pre-call-prefix-*20261019*` (AUD2276/2277).
Keep stock LLVM22; no REX rewrite, no_sanitize annotation, suppression, excluded
test or optimization-level change is authorized by this evidence. This is an
open sanitizer issue, not an accepted waiver, and explains only this control.
Recheck the unchanged source and both instrumented/plain commands when LLVM or
compiler-rt changes.

## EXT-CLANG-FILE-MACRO-CR — unresolved filename fidelity

Verified2026-10-18 with stock Debian Clang22.1.8 on x86_64. A correctly escaped
`#line 37 "line-\rbad.cpp"` records the CR filename, but `__FILE__` expands to a
string containing LF. LLVM22's
[file-macro expansion](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Lex/PPMacroExpansion.cpp#L1545)
calls
[Lexer::Stringify](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Lex/Lexer.cpp#L260),
whose implementation explicitly maps CR and LF to the newline escape. This
establishes a dependency normalization policy, not a REX escaping failure.
Rechecked 2026-10-27 with Debian Clang 23.1.5 on aarch64: `clang-23 -E` still
expands `__FILE__` after `#line 1 "x\ry.c"`, and for a physical filename
containing CR, to a string containing the LF escape.

The unchanged `build-audit/rex_line_directive_native_spelling.cpp` is the
independent reproducer. Native spelling exits0; the separate command with
`-DREX_VERIFY_RAW_CR_FILENAME` exits1 with the exact byte-identity assertion.
Source hashes match AUD-2177's preserved reproducer. Current version, raw logs,
real exit files and toolchain hashes are in
`build-audit/container-external-boundaries-20261018-*`.

Keep the raw-CR identity assertion and exact REX filename; normalizing REX's
filename would lose information without correcting this expansion. No dependency
patch, suppression, exclusion or acceptance waiver is introduced. The shared
line-directive formatter links this record. Recheck the native identity command
when LLVM changes its file-macro/stringification implementation. This affects
source fidelity and is not an accepted memory-report exception.

## EXT-FLANG-FRONTEND-EXPORTS — narrow dependency-discovery boundary

Verified2026-10-18 against stock Debian LLVM/Flang22.1.8 on x86_64. FlangConfig
loads the whole Flang target export set; that set requires MLIR and
MLIRTestAnalysis even when REX consumes only parsing, semantics and evaluation.
The independent four-line project in `build-audit/rex_flang_export_metadata`
imports Clang and Flang without REX. Its configure exits1 specifically because
those two targets are missing; neither is fabricated or ignored in a claimed
successful package import. Exact installed metadata and hashes are preserved in
`build-audit/external-flang-{config,targets}-20261018.cmake` and
`external-flang-metadata-20261018.sha256`.

REX's existing frontend-only boundary validates the required installed headers
and libraries directly. FortranEvaluate's export declares quadmath in this
package; the selected archive's unresolved sinq reference determines whether the
configured frontend needs that dependency. The archive inspection and library
lookup fail hard on inspection errors or a missing required quadmath library.
The existing root-CMake comment now links this ID. It does not parse CMake export
text, synthesize missing targets, import lowering, accept malformed ASTs or alter
reporting. Prefer usable frontend-only package metadata when LLVM supplies it;
recheck both the export closure and archive criterion on dependency/toolchain
changes. No LLVM fork or memory/test waiver is introduced. This diagnostic does
not establish full installed/alternate-toolchain REX acceptance.

## EXT-CLANG-TEMPORARY-DURATION — typed frontend integration

LLVM 22.1.8's
[LifetimeExtendedTemporaryDecl::getStorageDuration](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/AST/DeclCXX.cpp#L3088)
classifies field defaults as automatic and derives a BindingDecl's duration from
its lexical context; both branches carry upstream FIXME comments. REX instead
retains a field's containing-object lifetime and gets decomposition storage from
the exact typed DecompositionDecl. Preserve this distinction when reviewing
VisitMaterializeTemporaryExpr; replace it only when the dependency API provides
the same semantics. This is frontend integration, not a memory-test waiver.

## EXT-CLANG-OUTLINER-PARSING — reproduced reports, classification open

AUD-1569's copied class-conversion control reports 12 errors in two initial
Clang parsing contexts: CheckBaseSpecifier and BuildArrayType. This occurs
before outlining. The raw REX report is
build-audit/container-outliner-native-copied-type-projection-memcheck-raw-20261009.log;
CTest exits 16, with zero lost bytes and 40 reachable bytes.

One stock `clang++-22 -fsyntax-only` run on the exact specimen reproduces both
use locations under Valgrind 3.27.1, origin tracking and the unchanged suppression
policy. Debian LLVM 22.1.8 (`1+b2`), x86_64. Input SHA-256:
5a0d1c0756dc3ecafe46281baa78129aa1fcedf742ed5b242729d3cc1e00c793.
Suppression SHA-256:
901512c1ffafa422e10c6f040014f88c92a3e4ca6ca2694451ed0fc31053b399.
The use addresses differ by the same library relocation 0x5b2e000. Stock origins
are arena allocations reached during CFConstantString FieldDecl creation and
global new/delete ParmVarDecl creation. ASTContext::Allocate uses a bump
allocator; these backing-allocation stacks do not identify the particular
AST object or member containing the undefined state. Evidence:
build-audit/container-outliner-copied-stock-origin-{raw,output,results}-20261009.log;
both complete source guards pass, diagnostic exits 99. Stock additionally reports
192 possibly lost bytes in three blocks, absent from the REX report.

Independent reproduction rules out requiring REX to trigger these locations;
it does not prove harmlessness or matching REX storage origins. No suppression,
adapter, dependency patch or acceptance waiver is added. Trace the affected
fields and writes before accepting an exception; recheck on compiler/checker
changes. The memory gate remains failed.

## EXT-CLANG-ENUM-TEMPLATE-EVALUATION — reproduced reports, classification open

AUD-1370 records the enum/template fixture's two uninitialized-conditional
contexts in `Expr::EvaluateAsRValue` and `Expr::EvaluateAsInitializer`, through
variable initializer instantiation. REX Memcheck reports six errors, eight
suppressed reports in six contexts, zero lost bytes and 40 reachable bytes.
Stock Clang reproduces the same two evaluator stacks and six errors on both
the current specimen and its archived pre-AUD1368 version. Origins identify a
stack allocation in `Sema::CheckTemplateArgument`. Both stock runs additionally
report a separate 96-byte possibly-lost SourceManager allocation absent from
the REX result; they exit99. The REX CTest memory batch exits16.

Toolchain: Debian LLVM22.1.8 (`1:22.1.8-1+b2`), x86_64, Valgrind3.27.1,
libclang-cpp build ID `1234ae7a7b9f9ec56546d67ee10c8c617c378680`.
Raw evidence is `build-audit/container-variable-template-semantic-memcheck-native-raw-20261006.log`
and `build-audit/container-variable-template-stock-clang{,-before}-memcheck-20261006.log`.
The prior input is `build-audit/rex_template_enum_identity_before.cpp`; its
unchanged archived source also remains in the batch's before tarball.

These reports predate the semantic-argument change and reproduce without REX.
Their exact undefined-state mechanism and any valid integration fix remain
unresolved. No suppression, exclusion, LLVM patch or acceptance waiver is added.
The specimen's comment points here for future LLVM rechecks. The separate
required-fields Memcheck passes with zero errors, suppressions or lost bytes.

## EXT-CLANG-NEW-DECLTYPE-RANGE — typed frontend integration

Stock LLVM22 accepts `new decltype(array_type_operand)` but its AST dump has
an invalid CXXNewExpr end location. The existing fifteen-form allocation
specimen reproduces it independently; evidence is
`build-audit/new-array-alias-member-decltype-clang-ast-20261003.log`.
[ActOnCXXNew](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Sema/SemaExprCXX.cpp)
passes Declarator::getEndLoc() into the stored range while retaining the exact
allocated TypeLoc separately.

The frontend derives New ranges consistently from typed source roles:
written initializer delimiters, parenthesized type-id delimiters, the retained
outer-array declarator range, or the allocated TypeLoc. It validates the range
before translating types/operands and uses the same rule for macro ownership.
There is no invalid-range recovery branch, string parsing, dependency patch or
relaxed assertion. The decltype case passes native construction/copy/CFG and
emitted syntax checks under AUD-1145. Graph member-alias construction and
template substitution defects remain open; this integration accepts no memory
report or malformed AST. Recheck the stock range on LLVM upgrades and keep
the typed source rule consistent with any changed frontend API.

## EXT-CUDA-HOST-LAUNCH-BOUNDS — unresolved source fidelity

LLVM 22 intentionally drops `CUDALaunchBoundsAttr::MaxBlocks` when the AST
target is unknown or below SM90. `Sema::CreateLaunchBoundsAttr` keeps the first
two operands and warns before clearing the third. A host AST has an unknown
offload architecture even when the driver also requests an SM90 device job.
This is documented target behavior, not evidence that the attribute was absent
from the source. See [LLVM's Sema implementation](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Sema/SemaDeclAttr.cpp).

The unchanged LLVM 22 driver with genuine NVIDIA CUDA 12.8.1 reproduces the
host warning for `rex_cuda_launch_bounds.cu`; explicit
`--cuda-device-only --cuda-gpu-arch=sm_90` preserves all three typed operands.
Evidence: `container-native-cuda-launch-bounds-{stock-clang,sm90-stock-clang,device-stock-clang}-20261002.log`;
SDK checksums: `cuda-sdk-checksums-20261002.log` and
`nvidia-cuda-12.8.1-redistrib.json` in `build-audit`.

No exception is accepted: host/default source fidelity remains open under
AUD-1050. Investigate supported integration of the host and device typed ASTs;
do not infer the missing operand from tokens, silently choose device mode,
suppress the warning, or maintain a dependency fork. Recheck on LLVM/SDK
changes and remove this limitation when the ordinary supported frontend
retains the complete source payload. Clang's CUDA builtin property accesses
are a separate REX representation gap, not a sanitizer exception.

## EXT-FLANG-INACTIVE-DATAREF — verified report exception

AUD-1770's BOZ control on the guarded schema 206 source has zero reported errors
or lost bytes and 496,014 reachable bytes. Diagnostic 69193 identifies 24 reports
under unchanged exact rules: eight `assumed_rank`, fourteen `intrinsic_match`
and two `argument_rank` reports. Raw evidence is
`build-audit/container-boz-memcheck-suppression-review-native-raw-20261011.log`.
No rule is added or broadened; this scoped result does not establish whole-suite
memory acceptance. The companion both-reader/copy control has no suppressions.

AUD-1245's inheritance/copy control uses six reports in three contexts under
the unchanged rules: four assumed-rank and two assignment reports. The short
`-s` diagnostic identifies both names in
`build-audit/container-fortran-extends-suppression-names-raw-20261005.log`.
There are zero unsuppressed errors or lost bytes; 496,266 bytes remain reachable.
The selected reader control has no suppressions. This does not classify any
other outstanding memory report or establish whole-suite acceptance.

- Verified 2026-09-05: Debian LLVM/Flang 22.1.8 (`1+b2`), Linux x86_64,
  Valgrind 3.27.1. Recheck on every dependency/package/tool change.
- Reproducer: `tests/nonsmoke/functional/CompileTests/Fortran_tests/rex_flang_inactive_dataref.f90`.
  Run `LD_LIBRARY_PATH=/usr/lib/llvm-22/lib valgrind --track-origins=yes
  --error-exitcode=99 flang-22 -fc1 -fsyntax-only <specimen>`. This does not load
  REX. The standalone loader requires that library path in the audited image.
- Unsuppressed result: seven conditional-read reports in six contexts, all at
  `UnwrapWholeSymbolDataRef(const optional<DataRef>&)`. With the three exact
  caller-specific entries: zero reports; seven reports in six contexts matched.
- Origin: the [LLVM 22.1.8 source](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/flang/lib/Evaluate/tools.cpp#L1373)
  tests engagement before accessing the contained variant. The packaged
  instructions instead inspect the variant tag at offset `0x50`, optionally load
  its pointer, then test engagement at `0x58` and conditionally select that
  pointer. A disengaged optional returns the zero initialized result regardless
  of the inactive tag/pointer; the speculative pointer is never dereferenced.
  The standalone driver and the Flang object linked into librose have the same
  instruction sequence. This is inactive storage speculation, not a semantic
  field that REX fails to initialize.
- Scope: `Memcheck:Cond`, exact optional overload plus the exact assumed-rank,
  assignment, or argument-rank caller. No wildcard library match or REX caller
  suppression. Located in `scripts/rex-suppressions-for-valgrind`.
- Alternatives: REX has no ownership of this local optional inside Flang's
  expression analysis. Reimplementing semantic analysis or modifying private
  storage would be a dependency workaround. A dependency fork is unnecessary.
- Negative control: an independently injected REX-owned undefined heap read
  still exits 99 with one unsuppressed report under these entries.
- Remove when the unsuppressed reproducer becomes clean or a supported
  integration removes the report. Investigate changed frames before expanding
  the match. This exception does not accept any other Flang report.
- Raw evidence: ignored `build-audit/external-reproducers/flang-inactive-dataref*.log`,
  `flang-optional-rex.asm`, `flang-suppression-negative-control.log`.
  Full REX suite acceptance remains pending.

- Reverified 2026-09-06 after source110 full Memcheck reports an additional
  `IntrinsicInterface::Match` caller. The independent
  `rex_flang_inactive_intrinsic.f90` specimen uses `KIND(1.0)`; stock Flang exits
  99 with four conditional reports in three contexts and no lost allocations.
  The fourth exact caller-specific entry brings this reproducer to zero reports
  (exit 0); a separate REX-owned undefined heap read still exits 99 with one
  unsuppressed report. Only `Memcheck:Cond` in the exact optional overload and
  exact intrinsic-match caller is covered. The source110 library disassembly
  has the same tag read at `0x50`, engagement read at `0x58`, conditional result
  selection and no speculative dereference as the original proof. Evidence is
  `external-reproducers/flang-inactive-intrinsic-*` and the new retained specimen.
  The obsolete exception requiring the old four-argument `CompileFortran`
  symbol is deleted; the current five-argument symbol takes `SgSourceFile *`
  and cannot match that old stack entry. These four individually verified callers replace its broad stack
  gap. Full-suite results remain required, and unrelated reports stay visible.

- Reverified 2026-09-07 with `rex_flang_inactive_interface.f90`: the additional
  `CheckExplicitInterface` / `CheckArguments` path is reached by
  `a = min(abs(b), abs(b))` for two-element real arrays. Standalone Flang reports
  ten conditions in five contexts, no lost allocations; the four previous
  rules plus one exact three-frame rule report zero errors, matching all ten
  conditions. Disassembly of both stock Flang and the frozen REX library has
  the same inactive-tag read at `+0x50`, engagement check at `+0x58`, conditional
  return, and no speculative pointer dereference. The existing external-boundary
  control still exposes caller-owned undefined storage and direct/indirect leaks;
  initialized controls remain clean. No caller instruction or leak is suppressed.
  Raw bounded-workspace evidence: `/mnt/probes/flang-interface/`, including
  standalone/candidate logs, both disassemblies and `negative-controls/`.
  The active frozen full Memcheck run predates this rule and retains its failures;
  final current-source full validation remains required.

- Reverified 2026-09-07 for association analysis (AUD-273). Stock Flang
  reproduces the same optional-tag condition for `associate(x=>1)` and
  `associate(x=>a+b)`, one report each; association to a variable is clean.
  The unchanged `test2019_multi_associate.f90` reproduces 118 conditions in
  13 contexts without rules. Its REX translation reports ten conditions in the
  additional optional-expression `IsAssumedRank` / `ConstructVisitor::Post`
  call chain after the previously reviewed rules apply. Both packaged function
  disassemblies still read the inactive tag at `+0x50`, select through the
  initialized engagement byte at `+0x58`, and never dereference the speculative
  pointer. One exact three-frame condition rule covers this additional caller.
  All four standalone candidates and the four corresponding REX translations
  with teardown have zero errors and zero lost bytes; no leak exception is added. All 30 existing caller-owned external-boundary
  controls pass. A direct call to the same real Flang overload, outside the
  association chain, still reports the inactive-tag condition; making that
  caller's engagement state undefined also remains an error. Initial diagnostic
  loader/link setup failures are retained separately. Commands, minimal inputs,
  source hashes, both disassemblies, actual statuses and raw reports remain in
  `/mnt/probes/flang-associate272/`. The earlier full-run failures remain failed;
  full current-policy validation is still required.

## EXT-OPENMP-TOPOLOGY-IDS — native CI integration; default leak unresolved

- Verified 2026-09-05: Debian libomp 22.1.8, Linux x86_64, ASan/LSan;
  library build ID `337bdc411cd43e14d781696c6f5f1d7d19448b35`.
- The original, compiler-only OpenMP semantic-test executable leaks 192 bytes
  before the lowered executable is tested. A standalone two-thread reduction
  reproduces it without REX. One and 1,000 ordinary parallel regions each leak
  one allocation. Ten cycles using supported `omp_pause_resource_all(omp_pause_hard)`
  shutdown/restart leak ten allocations, 1,920 bytes.
- Origin: [the x2APIC topology routine](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/openmp/runtime/src/kmp_affinity.cpp#L3037)
  allocates temporary cache-layer IDs, copies them into topology storage, and
  never frees the temporary array. Matching packaged debug symbols identify the
  exact allocation. This is a real external leak that grows across reinitialization.
- A proposed `leak:__kmp_affinity_create_x2apicid_map` match was tested in ignored
  artifacts. It requires matching debug symbols and full malloc stack unwinding;
  default stripped-library traces are insufficient. A separately injected
  317-byte REX-owned leak remains reported. A library-wide/allocator-wide match
  is not justified.
- **No repository suppression is installed.** Growing leaks remain errors.
  Native weekly CI selects the documented Linux `cpuinfo` topology method in
  its regular, sanitizer and Memcheck profiles; it avoids the leaking x2APIC
  allocation without altering REX construction, generated source or reporting.
  This configuration is an integration, not a report exception or a full gate pass.
- Raw evidence: ignored `build-audit/external-reproducers/openmp-*.stderr`,
  `openmp-suppression-negative-control.log`, and `kmp_affinity.cpp`.
  Those historical reproducer files and the typed8/typed90 indexes below are
  absent from the current workspace; their summaries are not currently
  inspectable raw evidence.
- Reproduced 2026-10-01 without REX: stock Clang 22 builds the existing
  `OpenMP_tests/barrier.c` with ASan/UBSan. The default topology method reports
  the same 192-byte allocation; the same binary with LLVM's documented Linux
  `KMP_TOPOLOGY_METHOD=cpuinfo` exits cleanly with leak detection enabled.
  Evidence: `build-audit/container-openmp-topology-boundary-{stock-build,default,cpuinfo}-20261001.log`.
  The follow-up stock diagnostic
  `build-audit/rex_openmp_topology_lifecycle.c` checks ten supported hard
  shutdown/restart cycles, two-thread teams, allowed processor membership and
  actual thread placement. Default topology still leaks 1,920 bytes in ten
  allocations; `cpuinfo` exits cleanly for cores/close, cores/spread,
  threads/close, sockets/spread and last-level-cache/spread. Core and cache
  place lists match the default exactly on the audited Linux x86_64 host.
  Both methods give the same pre-existing unsupported-NUMA warning and core
  place list; this does not establish NUMA support.
  The existing caller-owned leak control still reports exactly 317 bytes after
  the same clean runtime cycles, and its released control exits cleanly.
  Commands use stock Clang 22 ASan/UBSan, leak detection enabled and no LSan
  suppressions. Source SHA-256:
  `566c1471f4cfe5665c010b83a42e61029ad108a6ae06e6bb1ec6a14aa4bfc682`.
  Raw evidence is `build-audit/container-openmp-topology-lifecycle-*-20261001.log`.
  Three existing REX barrier/parallel/reduction lowering contracts pass with
  sanitizer instrumentation in both original and generated programs:
  `container-openmp-topology-sanitized-original-lowered-ctest-20261001.log`.
  The corresponding binary-symbol log confirms ASan/UBSan instrumentation.
  The REX library in this focused run is Release, not sanitizer-instrumented;
  complete REX sanitizer/Memcheck gates and actual weekly CI remain required.
  Scope is native Linux x86_64 weekly CI only; installed programs and cross
  architecture nightly images keep their caller-selected runtime configuration.
  Recheck placement, lifecycle and negative controls on runtime/package,
  host topology or tooling changes. Remove the CI selection when the default
  topology path passes both ordinary and repeated-initialization leak checks.
- Recheck/remove: repeat both ordinary-region and shutdown/restart experiments
  when libomp/package/tooling changes; remove any future exception only after
  both unsuppressed runs are clean. Do not inject private-runtime cleanup or
  change generated-program semantics to conceal it.

The completed typed8 full sanitizer run executes all 36,061 expected tests and
fails with exit 8. All 103 failed cases have exactly one raw 192-byte report at
`libomp.so.5+0x60d49`, with the build ID above. C/C++ controls fail in `orig.exe`;
Fortran controls reach `lowered.exe`. The complete index retains the registered
commands and raw diagnostic bytes in
`build-audit/typed8-sanitizer-raw-failures.json`. No case is classified merely by
its failed test name. This evidence still does not accept the growing leak.

The source90 full sanitizer run reproduces exactly the same allocation in all
103 failed cases (36,099 complete eligible results, exit 8, zero skipped).
The per-case raw index is
`build-audit/completed-full-review/typed90-sanitizer-raw-reports.json`.
Source88 Debug and configured source90 Release pass their complete regular
registries; that does not waive this unresolved memory finding.

The historical, incomplete source129 sanitizer run emitted 84 failures. Each has
one saved 192-byte report at the same libomp offset and build ID recorded in
EXT-OPENMP-TOPOLOGY-IDS. The incomplete-run review recorded all error files,
including ordinary compiler warnings and Fortran STOP diagnostics, and hashes
in `build-audit/typed129/sanitizer-memory-report-review.json`. This is evidence
about those reports, not acceptance of the growing leak or the unfinished run.

## EXT-CLANG-FUNCTION-PROTO-COMPARE — verified packed comparison report

The 2026-10-11 loaded source-parameter contract reports three conditions in two
contexts under the unchanged C1 rule, zero unsuppressed errors/lost bytes and
40 reachable bytes. Diagnostic94808 adds only `-s`; the LLVM22.1.8 package and
Clang build ID match the verified binary below. Evidence:
`build-audit/container-loaded-parameter-identity-suppression-review-raw-20261011.log`.

The 2026-10-02 checkpoint cache-eviction namespace control has one report in
one context under the unchanged C1 rule, zero unsuppressed errors/lost bytes
and 40 reachable bytes. Reporting-only attribution is preserved in
`build-audit/container-typed-cache-dependency-scanner-namespace-suppression-names-raw-20261002.log`.

The repaired RAJA inliner case under AUD-1036 passes Memcheck with zero
unsuppressed errors/lost bytes and 40 reachable bytes. All 45 suppressed reports
in 13 contexts name only `rex_upstream_clang22_function_proto_uninit_cond_c1`.
The rule is unchanged. Evidence:
`build-audit/container-copy-signature-dependencies-final-raja-memcheck-raw-20261001.log`.

- The 2026-10-01 copied-scope inliner control matches 12 reports in seven
  contexts to the existing C1 rule, with zero unsuppressed errors or lost bytes.
  The reporting-only CTest diagnostic preserves the unchanged policy and adds
  `-s`: `build-audit/container-inliner-validator-cleanup-suppression-names-raw-20261001.log`.
- Verified 2026-09-07: Debian Clang 22.1.8, x86_64, Valgrind 3.27.1,
  libclang-cpp build ID `1234ae7a7b9f9ec56546d67ee10c8c617c378680`.
- `int rex_function(void) { return 0; }` independently reports one condition
  under stock `clang-22 -cc1 -std=c11 -fsyntax-only`, with no lost allocations.
  The new atomic-type runtime contract reaches this same constructor report.
- The constructor at object offset `0x1deb5c0` stores its parameter count in
  bits 48–63. At `0x1deb8be` it compares the complete packed word against
  `2^48`; the condition at `0x1deb8c1` selects the parameter-copy loop.
  For every possible lower 48-bit pattern, this comparison is exactly the
  test that the upper parameter count is nonzero. Undefined lower padding
  cannot change the branch or the loop's parameter count.
- `rex_memcheck_packed_compare.c` reproduces the exact compare without Clang
  or REX. Fully initialized upper counts zero and one both produce a false
  Valgrind condition (exit 99), while wholly initialized storage is clean
  (exit 0). Leaving the upper count undefined remains an error (exit 99).
  Compile with `clang-22 -O2 -g`, then run with
  `valgrind --default-suppressions=no --track-origins=yes --error-exitcode=99
  --exit-on-first-error=yes`, passing `zero`, `one`, `defined`, or `negative`.
- Scope: retain the existing C1/C2 constructor condition rules, requiring the
  exact ASTContext caller and a further packaged-Clang frame. No rule is
  broadened; no caller field, library, allocation or other report is hidden.
- `rex_clang_function_proto_boundary.cpp` calls the actual public Clang API
  from a separate application. Its explicit negative control marks only the
  caller's ExtParameterInfos pointer undefined. The complete current policy
  exposes all three caller-origin condition contexts, with the client-request
  origin retained. The initialized application still reports one packed
  constructor condition because this caller lies outside the narrow parser
  exception. Neither application result is claimed to be a clean memory run.
- This diagnoses instrumentation precision, so replacing semantic types,
  annotating production memory or changing LLVM ownership is inappropriate.
  Raw assembly, commands, four comparison controls, stock-Clang report and
  both API-boundary reports are in `/mnt/probes/clang-function-proto`.
- Recheck the exact comparison, both count controls and caller-owned negative
  control after any LLVM package, target or Valgrind change. Remove the rules
  when the packaged constructor no longer reports the comparison. This entry
  does not classify other Clang constructors, Sema, instantiation, FoldingSet
  or BuildArrayType reports.

## EXT-CLANG-RECORD-FIELD-FLAG — instantiated record field flag

- Verified 2026-09-07 with Debian Clang 22.1.8, x86_64, Valgrind 3.27.1,
  libclang-cpp build ID `1234ae7a7b9f9ec56546d67ee10c8c617c378680`.
- Stock `clang-22 -cc1 -std=c++14 -fsyntax-only` on
  `rex_copy_typedef_identity_input.cpp` reproduces two conditions in one
  context and zero lost bytes under the preceding policy. REX is not involved.
- The branch is object offset `0x226314c`, immediately following
  `testb $0x80,0x4a(%rbx)`. The matching LLVM source reads the nested record's
  `HasFlexibleArrayMember` in `Sema::ActOnFields`. CXX record completion calls
  `RecordDecl::completeDefinition`, whose packed increment precision loss is
  independently traced in EXT-CLANG-RECORD-ARRAY-FLAG. This is the same flag and
  packaged instruction behavior, reached through class instantiation.
- Scope: one condition rule with four adjacent frames: exact `ActOnFields`,
  `InstantiateClassImpl`, `InstantiateClassTemplateSpecialization`, and the
  packaged `libclang-cpp.so.22.1` frame. No leak rule or REX integration change
  is involved; other Clang paths remain outside this classification.
- The permanent manual diagnostic `rex_clang_record_fields_boundary.cpp`
  constructs valid records through public Clang APIs and supplies a field to
  the actual `Sema::ActOnFields` call. Initialized storage exits 0. Its negative
  control supplies an undefined caller-owned flexible-array flag and exits 99
  at the **same branch**, before and after adding the rule. Only this diagnostic
  annotates memory; production code does not.
- The candidate makes the stock input and the explicit copy-identity contract
  clean. The complete caller-owned undefined-value and leak controls pass,
  including unrelated programs named `clang` and `clang++`. Retained commands,
  raw reports, assembly and candidate boundary results are under
  `/mnt/probes/copy-alias/clang-fields` inside the capped audit filesystem.
- Recheck the branch, independent packed-increment controls and real API
  negative control after any LLVM package, target or Valgrind change. Remove
  this rule when the stock report disappears. This entry does not accept
  growing external leaks or classify other template-instantiation reports.

### Open anonymous-aggregate parser path (AUD-993)

The 2026-10-01 anonymous-aggregate ownership contract fails plain CTest Memcheck
with one unsuppressed conditional context, zero lost bytes and two existing
suppressed contexts. Its stack is `ActOnFields` →
`ActOnFinishCXXMemberSpecification` → `ParseCXXMemberSpecification`, outside the
instantiation path classified above. The unchanged input independently fails
stock `clang-22 -cc1 -std=gnu++17 -fsyntax-only` under Valgrind with default
suppressions disabled and origin tracking enabled: three errors/three contexts,
zero lost bytes. One is the same field-completion condition; two are function
prototype conditions. This is independent reproduction, not proof of the
record-field write chain or an accepted extension of the existing exception.

Evidence is preserved in
`build-audit/container-anonymous-aggregate-focused-memcheck-raw-20261001.log` and
`build-audit/container-anonymous-aggregate-stock-clang-memcheck-20261001.log`.
The packages are Clang/libclang-cpp 22.1.8-1+b2 and Valgrind 3.27.1-0.2.
The condition must remain visible until the object/write chain and narrow
caller-owned negative boundary are verified. No suppression is broadened.

### VFA parser write chains (AUD-1292)

The 2026-10-11 read-only VFA trace classifies its seven unsuppressed reports
with the same Clang build ID and Valgrind version as above. The constructor
defines HasFlexibleArrayMember at offset 0x4a, bit 0x80. Each exact record then
enters completion with shadow 0x40; masking the byte's low two bits and
incrementing changes shadow to 0xc0 while the actual flag remains false.
The later parser reads use those same records:

| Record in PID 142600 | Address | Field reads | Base reads |
| --- | --- | --- | --- |
| Anonymous header record | 0x1a43dbe0 | 1 | 0 |
| Anonymous header record | 0x1a43da80 | 2 | 0 |
| Animal | 0x1a606430 | 0 | 2 |
| Silky | 0x1a60d700 | 0 | 1 |
| dog | 0x1a60e1d8 | 0 | 1 |

These are proven packed-byte tracking reports, not omitted record flag
initializers. No memory annotations, watchpoints, dependency patches or
production changes are used. The driver completes analysis, but Valgrind still
exits 99 with seven errors/two contexts, zero lost bytes and 40 reachable bytes.
All 199 suppressed reports use the unchanged function-prototype constructor
rule. Source and binary guards pass. Evidence:
`build-audit/container-vfa-record-writes-{gdb,raw,driver,results}-20261011.log`.

Disposition: retain this external memory-tool issue explicitly. The current
strict gate remains failed. A parser/base-specifier rule must distinguish the
proven flag reports from genuine caller-owned undefined values at the same
branches before it can be accepted. This trace does not classify another
specimen or permit a function-wide/parser-wide exception. Recheck these exact
write/read chains after a Clang package, target or Valgrind change; retire the
issue when completion preserves flag definedness. Do not rerun allocation-only
origin diagnostics for these five already-traced records.

The 2026-10-16 standalone increment control still reports one error with
[`--expensive-definedness-checks=yes`](https://valgrind.org/docs/manual/mc-manual.html),
as with auto. The equivalent OR control is clean; the genuinely undefined flag
still exits99. No suppression or checking option changes. Source/binary guards
pass; evidence: `build-audit/container-memcheck-precision-20261016-*`.

## EXT-CLANG-RECORD-ARRAY-FLAG — verified packed increment report

- Reverified 2026-09-07 with Debian Clang 22.1.8, x86_64, Valgrind 3.27.1,
  libclang-cpp build ID `1234ae7a7b9f9ec56546d67ee10c8c617c378680`.
- Two independent C11 inputs reproduce one condition each under stock
  `clang-22 -cc1 -std=c11 -fsyntax-only`: after
  `typedef struct { int value; } record;`, declare either
  `record records[16];` or `void rex_function(record records[16]);`.
  Both reports use the branch at object offset `0x2809466`, immediately after
  `testb $0x80,0x4a(%rax)`, reading HasFlexibleArrayMember. Neither loses memory.
- The matching-record instruction trace described below establishes that
  RecordDecl's `andb $0xfc; incb` update introduces the undefined flag despite
  being unable to carry into it. The permanent independent
  `rex_memcheck_bitfield_increment.c` control is rerun: increment exits 99,
  equivalent OR exits 0, and an actually undefined flag exits 99. No production
  memory annotation or altered Clang state is used.
- Scope: two `Memcheck:Cond` rules, each with four adjacent frames: exact
  BuildArrayType, the packaged Clang 22.1 frame, exact GetTypeForDeclarator,
  and exact HandleDeclarator or ActOnParamDeclarator. No stack gaps, general
  Sema rule, caller instruction or allocation rule is added. Both independent
  inputs become clean under this candidate policy.
- `rex_clang_array_boundary.cpp` calls the actual Sema API from an application.
  Its initialized control exits 0 both before and after the rules. Marking only
  the caller's QualType undefined exits 99 in both cases, preserving its client
  request origin and 24 error contexts. The complete external-boundary contract
  also passes, retaining caller-owned undefined values and direct/indirect leaks.
- Build the API diagnostic with `clang++-22 -std=c++17 -g -O0`, the installed
  LLVM 22 include directory, and matching `libclang-cpp.so.22.1` and `libLLVM.so`.
  Run under `valgrind --default-suppressions=no --track-origins=yes
  --error-exitcode=99 --suppressions=scripts/rex-suppressions-for-valgrind`;
  no argument is the initialized control, `negative` injects the undefined type.
- Raw commands, original/candidate reports, the standalone sources and all
  controls reside in `/mnt/probes/clang-record-array`. Recheck the exact flag,
  package instructions and both caller controls whenever LLVM, target or
  Valgrind changes. Remove these entries when the unsuppressed inputs are clean.
  Other Sema, instantiation, FoldingSet and record reports remain unclassified.

## EXT-CLANG-PACKAGED-DEFINEDNESS — unresolved classification

- Captured-lambda members (2026-10-18, AUD-2243): one CTest Memcheck control
  fails16, with1000 reported contexts (146 conditional/854 read) and reporting
  capped; the actual REX child reports1189 errors and zero lost bytes. The first
  stock control reproduces CheckBaseSpecifier through Clang arena allocation.
  AUD2287 recovers the exact historical source and runs stock LLVM22.1.8 with
  origin tracking and uncapped reporting:49,352 errors in6,677 contexts, exit97,
  zero lost bytes. All1000 primary instruction sites and available shared stack
  prefixes occur natively (931 eight-frame prefixes;69 shorter at the frontend
  boundary). Source/tool hashes pass; no REX process or suppression file is used.
  Matching use paths do not prove identical objects/members/write chains,
  harmlessness or the unreported capped tail. Keep the gate failed; no exception,
  suppression or workaround is accepted. Recheck after dependency/integration
  changes and complete exact-origin/semantic classification before acceptance.
  Evidence: build-audit/closure-members-{memcheck,native-first-error}-20261018*
  and closure-native-full-20261019-* (address anchors and comparison limits).

- Default-template parameter ownership (2026-10-08, AUD-1492): native CTest
  Memcheck fails with two conditions in reserveInits and
  FillInEmptyInitializations (SemaInit.cpp:945), before REX translation. Origin
  control68400 exits16; both share the Clang arena allocation through
  FieldDecl::Create/getCFConstantStringDecl. Stock Clang19163 on the unchanged
  GNU C++17 specimen reproduces both, exits99 and also reports96 possibly lost
  bytes in SourceManager/PagedVector. REX reports zero lost bytes and28 existing
  suppressed errors/eight contexts. LLVM22.1.8 build ID remains
  1234ae7a7b9f9ec56546d67ee10c8c617c378680; Valgrind3.27.1. Matching use/allocation
  stacks do not establish the exact object/write chain or harmlessness. No
  exception, suppression, exclusion or integration workaround is accepted.
  Recheck the unchanged specimen and both origins after dependency or integration
  changes. Logs: native-memcheck-default-parameter-origins-20261008.log and
  container-default-parameter-stock-origins-20261008.log in build-audit; source:
  source-frontend-strict-retirement-20261008.sha256. The separate lambda control
  passes with zero errors/lost bytes and21 existing suppressed errors/eight
  contexts; it does not clear this failure.

- AUD-1147's C++14 no-elision constructor roundtrip passes natively but fails
  Memcheck with six errors in three contexts: CheckBaseSpecifier, reserveInits,
  and FillInEmptyInitializations (SemaInit.cpp:945, ELF 0x2463897). A stock LLVM22
  origin run on the identical specimen reproduces all three use paths, originating in heap
  allocations through FieldDecl::Create/getCFConstantStringDecl. Stock also
  reports other contexts:54 errors in19, plus96 possibly lost bytes. Matching
  paths and arena origins do not prove every REX write or harmlessness.
  AUD-1277 subsequently traces the first current-REX uses to the exact
  VirtualBase/NonvirtualBase objects. Their constructor defines the false
  flexible-array flag; the carry-free completion increment changes its shadow
  from defined to undefined. Initializer counts remain zero and vector storage
  is defined at both first fill comparisons. This identifies precision loss on
  those paths. The current diagnostic exits 99 with 14 errors in the same three
  contexts and 92 existing suppressed reports in 22 contexts. Register shadows
  were unavailable; later repetitions and suppressed contexts remain open.
  No exception, suppression, annotation or exclusion is accepted. Recheck these
  object/instruction chains after LLVM/package/Valgrind changes; do not reset flags
  or transfer this classification to another specimen.
  Logs: `container-typed-default-argument-constructor-memcheck-raw-20261003.log`
  and `container-typed-default-argument-stock-clang-memcheck-20261003.log`;
  current trace: `container-clang22-constructor-object-origin-20261005.log`
  and `container-clang22-constructor-object-origin-gdb-20261005.log`, source
  `policy-legacy-skips-source-20261005.sha256`. Debug Build ID:
  `1234ae7a7b9f9ec56546d67ee10c8c617c378680`, LLVM22.1.8 (`1:22.1.8-1+b2`).

- Operator-result review (2026-10-01, AUD-945): the unchanged source passes
  frontend/JSON identity contracts, including the newly retained selected delete
  function. Memcheck fails before Sage translation at `CheckBaseSpecifier`:
  two errors in one context, zero lost bytes, 17 existing suppressed reports in
  four contexts. Stock Clang 22 on the identical input reproduces the first use
  and allocation chain through `FieldDecl::Create` and
  `ASTContext::getCFConstantStringDecl`; both origin runs exit 99.
  Stock's unsuppressed run also reports other contexts (19 errors in five).
  [LLVM 22.1.8 base-specifier construction](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Sema/SemaDeclCXX.cpp#L2594)
  inspects a semantic flexible-array flag here; matching stacks alone do not
  prove that its undefined bit is harmless. A post-parse REX repair cannot
  prevent this read. Keep the object/write-chain investigation open; add no
  suppression, annotation, test exclusion or dependency fork.
  Evidence: `container-selected-delete-operator-frontend-memcheck-raw-20261001.log`
  and `container-selected-delete-input-{stock-clang,rex}-origin-memcheck-20261001.log`.
  Recheck this exact input, flag and paired origins when LLVM or Valgrind changes.

- JSON parameter-scope review (2026-10-01, AUD-974), Debian Clang 22.1.8
  (`1+b2`), x86_64 and Valgrind 3.27.1: the unchanged finalization
  specimen (SHA-256
  `ccbc29ccaf340722a783feee5e30cd4042b0d7d4d33e16ce1e487c8b0c254578`)
  passes regular reconstruction but fails Memcheck with 1,002 errors in 467
  contexts, zero lost bytes. First frames belong to Clang in 358 contexts and
  REX in 109; frame location alone does not classify ownership. Stock Clang 22
  and REX origin-tracked controls both stop with status 99 on their first error
  in `InitListExpr::reserveInits`, with the same
  `Parser::ParseCXXClassMemberDeclaration` stack-allocation origin. This occurs
  before JSON reconstruction. No change to the suppression policy, annotation,
  exclusion or accepted exception follows from this comparison. Classify the
  actual object/write chain and remaining contexts; recheck the exact specimen
  and paired origins when LLVM, Valgrind or frontend integration changes.
  Raw evidence: `build-audit/container-json-finalization-positive-memcheck-raw-20261001.log`;
  first-error controls: `container-json-finalization-positive-{stock-clang,rex}-first-origin-20261001.log`.
  The two external-scope memory controls pass under the existing policy with
  zero unsuppressed errors or lost bytes; that does not clear this failure.
  After AUD-986, a complete origin-tracked plain CTest diagnostic still fails
  (exit 16): 1,006 errors/467 contexts, zero lost bytes and 15 existing suppressed
  reports/eight contexts. All 109 REX-first contexts have Clang stack-allocation
  origins: 99 at library offset `0x1765bee`, ten at `0x19c9820` (same build ID).
  A stock `clang-22 -cc1 -std=gnu++17 -ast-dump` control, without suppressions,
  fails with exit 99 and 5,510 reports/987 contexts; 512 contexts share the first
  allocation site. This establishes an independent origin match for that site,
  not the object/write chain or a disposition for every downstream report.
  The second site is not reproduced by that control. Matching debug symbols
  and `clang-query` are unavailable in the bounded container; no dependency
  fork, annotation, suppression or accepted exception is added.
  Evidence: `container-json-finalization-positive-{complete-origin-memcheck,
  complete-origin-memcheck-raw,rex-origin-counts,stock-ast-origin,
  stock-ast-dump,origin-symbols}-20261001.log` in `build-audit/`.

  AUD-1895 resolves those two unnamed stack origins using cached debug ELF
  SHA-256 `ac8e55a89aa5bc6a3c5f45892f9647b5acabf3548de7170ae86c6623667f242c`
  and the same Build ID. Offset `0x1765bee` names the function_ref callback in
  [ASTContext exception-type rewriting](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/AST/ASTContext.cpp#L3456);
  `0x19c9820` names ParentMap ASTVisitor::dataTraverseStmtPre at
  ParentMapContext.cpp:468. Parser class-member, InitListExpr::reserveInits and
  Sema initializer frames verify load bias `0xa380000` in the October1 report.
  The October3 report's named parser frame gives bias `0xa4f5000` and the same
  two origin offsets. Evidence:
  `container-json-finalization-origin-{named-symbols,verified}-20261013.log`.
  Exploratory alternate-bias output fails named-frame validation and is not
  accepted evidence. These names locate stack frames, not the particular
  undefined members or writes. The locality cleanup avoids impossible nonlocal
  statement-parent queries; no memory run, harmlessness conclusion, suppression
  or waiver follows. Continue object/write tracing at these sites.

- Class-include review (2026-09-30, AUD-924): the existing class-include fixture
  now includes a pragma with an asserted exact class owner and include-expansion
  role. All related regular checks pass. Memcheck reports 2,312 errors in
  956 contexts, zero lost bytes, and one existing suppressed report. Its
  origin-tracked repeat exits 16 with the same counts. Stock Clang 22 exits 99
  with 1,162 errors in 524 contexts and a 96-byte possible leak; its first
  `InitListExpr::reserveInits` use and `RecordDecl::Create` allocation path match
  REX's first report. This starts during Clang parsing, before lexical pragma
  planning or Sage construction. Later reports include the new typed visitor
  and existing translation paths; matching the first report does not classify
  all those contexts or prove harmlessness. No new exception, suppression,
  annotation or exclusion is accepted. Recheck the actual object/write chain
  and remaining contexts before changing this disposition.
  Evidence: `container-pragma-class-include-{memcheck-raw,origins-memcheck-raw,stock-clang-origins}-20260930.log`;
  CTest summaries: `container-pragma-{lexical-construction,class-include-origins}-memcheck-20260930.log`.
  The separate exact namespace/record/block owner case has zero unsuppressed
  errors or lost bytes under the unchanged policy; that pass does not clear
  this class-include failure.

- Requires-expression review (2026-09-30, AUD-868): the unchanged abbreviated
  constraints specimen (`f03f98e9e71300dcf14bddcdae04ac132f24d2e40add855f992bc830e075846d`)
  passes regular JSON/runtime checks, but focused CTest Memcheck exits 16 with
  99 conditions in five contexts and zero lost bytes. Stock Clang 22 reproduces
  all five use contexts: two `CheckBaseSpecifier`, one `reserveInits`, and two
  unnamed Sema frames. Both direct origin-tracked runs exit 99 under the same
  suppression policy. Stock allocation origins differ from REX's origins;
  shared arena allocation alone does not prove the undefined fields equivalent
  or harmless. No exception is accepted and no suppression is added. Raw
  evidence: `container-requires-json-complete-construction-memcheck-raw-20260930.log`
  and `container-abbreviated-constraints-{stock-clang,rex}-origin-20260930.log`.
  Recheck the paired origins before changing this disposition.

  Instruction review in `container-clang-base-initlist-report-instructions-20260930.log`
  locates the reports at object offsets `0x22b125c` and `0x19eb65a`: the first
  branches on a single flag bit; the second compares initializer-vector bounds.
  The packaged `ASTVector` header initializes both bounds to null. These reads
  cannot yet be classified as harmless padding. Trace the actual object and
  writes before accepting an exception; no memory policy changed.

- Typeof lifetime review (2026-09-30, AUD-861): Debian Clang 22.1.8 (`1+b2`)
  and Valgrind 3.27.1. The unchanged
  `rex_ast_json_typeof_ownership.c` (SHA-256
  `5fc31234a36723925bc9e5ad93f85f602f0f7d8c3b7e62f64e6856dace733e3a`)
  passes native JSON/copy/deletion/runtime, but its CTest Memcheck reports
  4,306 errors from 1,000 contexts before the reporting cutoff. Raw evidence:
  `build-audit/container-lexical-operand-forward-ownership-memcheck-raw-20260930.log`.
  With the same current suppression file, origin tracking and first-error
  cutoff, stock `clang-22 -std=gnu11 -fsyntax-only` and REX both exit 99 in
  `InitListExpr::reserveInits`. Their first use and allocation frames match
  with Clang library relocation delta `0x5ac2000`: allocation enters
  `llvm::allocate_buffer`, `ConstantExpr::Create`, integer constant evaluation
  and `BuildArrayType`. The paired reports are
  `build-audit/container-typeof-{stock-clang,rex}-current-policy-origin-20260930.log`.
  This independently reproduces the first external report; it does not classify
  later REX callers or establish harmlessness. No suppression or test exclusion
  was added, and the memory gate remains failed. Recheck this origin when the
  package or instrumentation changes; classify subsequent reports separately.
  The unsuppressed stock first-error run stops earlier in `FunctionProtoType`
  and is retained separately, rather than treated as a matching-origin result.

  Saved first-frame inventory also finds 41 reports in REX `Traverse(Stmt*)`
  and nine in REX's Clang-statement pointer hash-table insertion. These are
  retained diagnostic contexts, not independent origin proofs. They prevent
  treating this complete run as external merely because the first report is
  independently reproduced. No new run or suppression was needed to identify
  them; actual storage origins and correctness remain unclassified.

  A single origin-tracked CTest run without the reporting cutoff now records
  12,660 errors in 1,330 contexts, zero lost bytes and 40 reachable bytes;
  CTest exits 16. Evidence is `container-typeof-rex-all-context-origins-{ctest,raw}-20260930.log`.
  The first REX `Traverse` report is its incoming-pointer null test (object
  offset `0x2a373e9`), not merely a statement-class flag read. Sampled traversal
  and child pointer-table reports share a Clang arena allocation through
  `FunctionDecl::Create`. Arena provenance does not identify the exact object
  or write responsible. The packaged function-body getter instructions are
  retained separately; its [LLVM 22 source](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/AST/Decl.cpp#L3024)
  initializes and retrieves the body through typed lazy storage. Actual object
  writes and harmlessness remain unproven; no exception, suppression or exclusion
  is accepted. The previous cutoff run remains historical evidence.

  Read-only instruction breakpoints now trace this specimen's exact `RexValue`
  record (source-name location `0x16a`). `RecordDecl::completeDefinition`
  changes its byte at `+0x4a` from value `0x02`, validity mask `0x40`, to
  value `0x01`, mask `0xc0`, through `and $0xfc; inc`. The flexible-array bit
  therefore loses definedness despite remaining physically zero. The same
  record reaches the count producer, which extracts bit 23 and adds its signed
  value to the field count. The first `reserveInits` call then changes the
  allocator cursor at ASTContext `+0x9f8` from all-defined to all-undefined.
  Evidence: `container-typeof-record-cursor-chain-{debug,raw}-20260930.log`;
  the count and cursor-write instructions are preserved separately. This
  connects this allocation to the already reproduced packed-byte instrumentation
  limitation without watchpoints or writes to client state.

  Separate `container-typeof-arena-input-debug-20260930.log` and
  `container-typeof-setter-debug-20260930.log` inspect the affected main-function
  allocation and setter before REX translation. Their cursor/body validity
  masks have only low alignment bits defined. These diagnostics were explicitly
  killed after inspection; debugger exit zero is not a test pass. The sampled
  pointer propagation is external instrumentation evidence, not a classification
  of all 1,330 contexts. No REX-pointer suppression, memory annotation, test
  exclusion or new exception is accepted. Remaining contexts and an accurate
  supported instrumentation boundary still prevent memory acceptance.

- Eighteen broad Clang masks for initialization, self-reference, base-specifier
  and call checking, declaration attributes, and two unrestricted object-frame
  cases are removed. Their stack gaps and wildcards could hide reports beyond
  the individually investigated paths below. A further 44 unaccepted masks are
  removed: two parser record-field paths and 42 historical Clang/LLVM
  instantiation, FoldingSet, type-location, and leak rules. Those rules lacked
  exact stack and caller-owned negative-control evidence; the three LLVM
  TimerGroup leak masks also lacked an accepted exception record.
  Any newly exposed contexts require origin review and a caller-owned negative
  control before a narrow exception can be considered. The final Memcheck gate
  must use the reduced suppression file.

- Saved specialization-report review (2026-09-16): all 1,000 reported primary
  frames in `container-specialization-input-cleanup-valgrind.log` are either
  packaged Clang (996) or `memmove` called directly by Clang (four, at log lines
  7974, 8007, 8264 and 8294). The latter callers are `DeclGroup::Create` and
  the `Sema::ClassifyName` path. This locates reported uses, not allocation
  responsibility: the report lacks origins and hits the reporting cutoff.
  It cannot establish absence of REX defects, justify broader suppressions,
  or replace the missing full classification. No test was rerun for this review.

- Specialization first-context classification (2026-09-16): stock Clang and
  REX independently stop with status 99 at `ASTVector<Stmt*>::grow` on the
  current `rex_frontend_template_operator_selected_symbol.cpp`, using C++20,
  unchanged project suppressions, origin tracking, and first-error cutoff.
  `build-audit/{clang,rex}-specialization-first-origin.log` preserves both
  commands and reports. Input SHA-256 is
  `c03aa3b114746913749a2dd3d4952df59092a2fc21b1fb1b7dc472a8224ccd27`.
  The first 19 Clang use frames match with relocation
  delta `0x5b4b000`. Allocation origins both enter Clang AST storage through
  `llvm::allocate_buffer`, but differ: stock Clang records
  `getLValueReferenceType`; REX records `ParmVarDecl::Create`. These slab origins
  do not prove the same individual object was uninitialized. The first use
  reproduces externally; the 1,000-context cutoff in the targeted REX Memcheck
  still prevents full classification or memory acceptance. No suppression was
  added. Recheck with an updated LLVM 22 package and retain visibility of REX
  allocation/caller defects.

- First-context classification (2026-09-16): the first unsuppressed context in
  `container-function-argument-lifetime-valgrind.log` reproduces in stock
  Debian Clang 22.1.8 (1+b2), without REX, on the same `Cxx_tests/helloworld.cpp`.
  Both stock Clang and `genericCoverage` stop with status 99 at an undefined
  conditional in `InitializationSequence::Perform` / copy initialization /
  return-statement parsing. Origin tracking in both points to LLVM's allocation
  buffer through `ASTContext::getTemplateSpecializationType` and
  `Sema::CheckTemplateIdType`; the use and allocation PCs have the same library
  relocation delta. This context has an independent external reproduction.
  It does not classify the other contexts or establish harmless behavior.
  Reports are `build-audit/{clang,rex}-helloworld-matched-first-origin.log`;
  their headers preserve the commands. Both use Valgrind 3.27.1 with
  `--track-origins=yes --exit-on-first-error=yes --error-exitcode=99
  --leak-check=no --num-callers=24` and the existing suppression file, unchanged
  (SHA-256 `c5c934c54b1511bef529046c8cc945e115e2f14212807b9ce397d330fc28103d`).
  Stock Clang uses `-std=gnu++17 -fsyntax-only`; the REX command is the saved
  genericCoverage invocation. Input SHA-256:
  `457bda18dcc4b014e60d2c38af68a4af7c0ede30c4634158f7a500fa697855e3`.
  `libclang-cpp.so.22.1` SHA-256:
  `4ca755443928f3f6a5d69d2ef88c520bf239816ed66c2b9a01b526e1c9d725aa`.
  An initial run without project suppressions stopped earlier at an independent
  Clang `FunctionProtoType` report (`clang-helloworld-first-origin.log`).
  No new exception or suppression is enabled. Recheck this context when the
  LLVM 22 package changes; do not mask REX AST defects using this evidence.
- Additional matched input recheck (2026-09-08):
  `Cxx_tests/rex_frontend_injected_member_pointer_owner.cpp`, SHA-256
  `c6e5d07f8978f1390279915e23bb7904fc133d9bdfb1f4693acd777b5052f6f6`,
  using C++11 in both tools. The existing standalone Clang API diagnostic reports
  2,802 errors / 1,799 contexts; stock `clang-22 -cc1 -fsyntax-only` reports
  2,541 / 1,688. Both exit 99, with no suppressions or lost bytes. Their raw hashes
  are `9b2f552b4ab585e3f992507f777c4bd9d121a23516943c77926bddf574104f40`
  and `94ab863862ba390f1868fe2530c00181e301d67520f117e2f7477e3d0049ee0f`.
  Commands and reports are retained in `member-clang291` within the bounded
  workspace. This proves an independent external reproduction for this input;
  it does not classify every REX report or establish harmless semantic behavior.
  Existing suppression scope remains unchanged.
- Rechecked 2026-09-08 against the same stock Clang 22.1.8 and Valgrind
  3.27.1 packages. The permanent standalone diagnostic
  `tests/nonsmoke/unit/SageInterface/rex_clang_ast_definedness.cpp` links only
  Clang/LLVM and visits implicit declarations and template instantiations. It
  queries the actual function body, variable initializer and declaration source
  range through typed public APIs. All three inputs execute natively with status
  0 and reproduce unsuppressed Memcheck reports without REX. Missing files and
  malformed source reject with statuses 2 and 3 respectively.
- The specimens below come from the exact failing REX log commands; numeric
  Memcheck report suffixes are not treated as IDs in another test registry.
  An earlier top-level-only diagnostic selected two different C++ inputs through
  that mistaken association. Its results cannot establish a matched baseline.
  The corrected runs preserve specimen hashes, commands, dependency resolution,
  statuses and complete reports in `rex-origins283`, `clang-matched283` and
  `clang-permanent284` under the bounded workspace's `probes` directory.

  | Existing specimen | Standalone API reports / contexts | Stock syntax-only reports / contexts |
  | --- | ---: | ---: |
  | `Cxx_tests/rex_test2026_template_instantiation_reachable_scope.cpp` | 656 / 504 | 569 / 462 |
  | `C_tests/test2015_118.c` | 647 / 318 | 557 / 292 |
  | `Cxx_tests/rex_frontend_static_member_publication.cpp` | 1,154 / 945 | 989 / 852 |

- The permanent API reports have zero suppressed contexts and zero definite,
  indirect or possible lost bytes. Their SHA-256 hashes, in table order, are
  `6dc1307387380da95d1c76ab7d9f49a792dc74a4e0c6d35c779acd6a83b0834c`,
  `e903e30ac02d95ed112a2401cc36e3a482684401e042f7aea069789c662f399f`
  and `64f8b2402be1723ae7853cc3abfee8cbbc020add137951778c95ef8a65d6193e`.
  The function-body conditional has the same library stack-allocation origin
  at offset `0x1765bee` in both the standalone API and REX traces. Disassembly
  is retained as diagnostic evidence; a nearby exported-symbol label must not
  be mistaken for the stripped function's identity.
- Reproduce on the recorded Debian layout from the repository root:

  ```sh
  clang++-22 -std=c++17 -O0 -g -I/usr/lib/llvm-22/include \
    tests/nonsmoke/unit/SageInterface/rex_clang_ast_definedness.cpp \
    /usr/lib/llvm-22/lib/libclang-cpp.so.22.1 \
    -L/usr/lib/llvm-22/lib -lLLVM-22 -o /tmp/rex_clang_ast_definedness
  valgrind --track-origins=yes --leak-check=full \
    --show-leak-kinds=definite,indirect,possible \
    --errors-for-leak-kinds=definite,indirect,possible \
    --error-exitcode=99 --num-callers=32 \
    /tmp/rex_clang_ast_definedness \
    tests/nonsmoke/functional/CompileTests/Cxx_tests/rex_test2026_template_instantiation_reachable_scope.cpp \
    -std=gnu++14
  ```

  Substitute the other table specimens; use `-std=gnu11` for the C input.
  Status 99 is unresolved failure evidence, not a successful memory check.
  Recheck these unsuppressed public-API probes and exact REX commands when the
  package, target, integration or Valgrind changes. The reproducer supplies no
  accepted suppression and does not replace or exclude any regular test.
  External reproduction alone does not establish harmlessness, classify every
  report in the full suite, or authorize masking a REX caller.


- AUD-137 filtering control, 2026-09-06: a private two-frame DSO matching
  libclang-cpp's installed-library pattern consumes an undefined integer
  allocated by its caller. The original complete suppression file hides that
  report (exit 0, one suppressed context). Removing the six library/parser/
  instantiation-boundary masks exposes it (exit 99, exact caller allocation
  origin); the initialized controls both exit 0. This tests filter integrity,
  not an external Clang defect. Raw sources, policy files, and reports are in
  `build-audit/suppression-boundary/`. The permanent
  `rex_memcheck_external_boundary` test exercises the configured wrapper and
  complete suppression file. Other existing entries still require individual
  classification; prior memory passes do not establish that their masks are
  justified.

- Verified origin 2026-09-05: Debian Clang 22.1.8, x86_64, Valgrind 3.27.1;
  libclang-cpp build ID `1234ae7a7b9f9ec56546d67ee10c8c617c378680`.
- Existing `rex_test2026_compound_literal_typeloc_valgrind_defined_reads.c`
  reproduces with standalone `clang-22 -cc1 -ast-dump`; 1,411 reports in 486
  contexts were captured without REX. `-fsyntax-only` with the most precise
  definedness checks still reports 413 errors in 339 contexts.
- The earliest FunctionProtoType constructor report compares a complete packed
  bitfield word against the threshold for one parameter. Lower padding bits
  are irrelevant to that comparison, but this observation alone does not
  classify later reports or prove all semantic fields defined.
- The 2026-10-24 focused `rex_ast_deletion_ownership` Memcheck run reports one
  conditional in that constructor before REX translation. Stock Clang 22.1.8
  reproduces the same use and allocation stacks on the identical input with
  Valgrind 3.26.0 and no suppressions. Both runs fail; neither establishes memory
  acceptance. Evidence: `build-audit/pr-round43-deletion-{memcheck-raw,stock-clang-memcheck}.log`.
- A reduced compound-literal specimen reports 287 errors in 242 contexts.
  Inspection with matching debug symbols and Valgrind's GDB monitor shows that
  the InitListExpr vector's begin/end/capacity storage is defined. The undefined
  reserve count instead comes from `numStructUnionElements`: its load of
  `RecordDecl::HasFlexibleArrayMember` selects bit 23 of the word at object
  offset `0x48`, and the monitor marks that bit undefined. The source constructor
  initializes the flag to false. Precise instruction breakpoints now locate
  the validity change inside `RecordDecl::completeDefinition`: the byte at
  offset `0x4a` has validity mask `0x40` before the update and `0xc0` afterwards.
  The generated `andb $0xfc; incb` update clears the low two bits, then sets the
  low bit. It cannot carry into the flexible-array flag. Valgrind nevertheless
  propagates uncertainty from the adjacent unused bit into that flag.
- Independent x86_64 tool reproducer:
  `tests/nonsmoke/unit/SageInterface/rex_memcheck_bitfield_increment.c`.
  Compile with `clang-22 -O2 -g <specimen> -o <executable>` and run
  `valgrind --track-origins=yes --error-exitcode=99 <executable>`.
  No argument exercises the increment (one false report, exit 99); `or` uses
  the equivalent bitwise update (zero reports, exit 0); `negative` leaves the
  tested bit undefined (one report, exit 99). No Clang/REX library or annotation
  is involved. This establishes one source of downstream false reports, not
  a blanket disposition for all packaged-Clang diagnostics.
- A simulated hardware-watchpoint experiment made the downstream reports
  disappear, but that result is invalid definedness evidence: removing these
  watchpoints marks their memory defined, as documented in
  [Valgrind's debugger limitations](https://valgrind.org/docs/manual/manual-core-adv.html#manual-core-adv.gdbserver-limitations).
  Retain that log only as a record of the intrusive diagnostic experiment.
  Breakpoints and read-only validity queries are required for further tracing.
  Separate runs with full debugger instrumentation, lower IR optimization,
  single-instruction blocks, and expensive definedness checks still report
  errors; none is an established integration correction.
- The record-array report newly exposed by AUD-137 reproduces in stock Clang
  with `typedef struct { int value; } record; record records[16];`, using
  `clang -cc1 -std=gnu11 -fsyntax-only`. The two-record reduced input reports
  two errors in one context and zero lost bytes, with and without the current
  suppression file. A read-only instruction trace matches the exact record
  identity before/after completion and at BuildArrayType's flag read. The
  validity byte changes from `0x40` to `0xc0` at the same `andb; incb` sequence;
  `testb $0x80,0x4a(%rax)` at library offset `0x2809462` consumes that flag.
  This establishes this specific report as the previously reproduced Valgrind
  precision loss. The first new trace inspected different record identities
  and is not sufficient proof; `vgdb-v2-inspection.log` records the matching
  identity. Commands, minimal source, disassembly, and raw reports are under
  `build-audit/external-reproducers/clang-record-array/`. It does not classify
  unrelated Sema, template-instantiation, or FoldingSet reports.
- The specific record-array flag is now covered by the separately verified
  EXT-CLANG-RECORD-ARRAY-FLAG entry; this does not justify other suppressions or
  source annotations. Source-level parser/cleanup masks, six broad entries,
  69 one-frame Clang masks, and one such LLVM mask have been removed from
  `scripts/rex-suppressions-for-valgrind` (AUD-137). The remaining entries need
  individual classification. Trace the earliest relevant cause and inspect
  downstream definedness; external origin alone is insufficient for acceptance.
- The existing explicit-instantiation identity input also reproduces without
  REX: stock `clang -cc1 -std=gnu++14 -ast-dump` reports 1,897 errors in 701
  contexts, with no definite, indirect, or possible leaks. An earlier unmasked
  REX build reports 338 errors in 187 contexts, and typed7 reports 342 in 187
  contexts. Both reach `FunctionDecl::doesThisDeclarationHaveABody` before
  AST copying. Of 151 distinct eight-frame signatures, 150 match between the
  two REX runs; the differing signature is a statement-identity hash lookup.
  This is evidence of pre-existing diagnostics, not a complete classification
  or justification for suppressing the remaining reports. Commands, origins,
  and outputs are retained in
  `build-audit/external-reproducers/clang-explicit-instantiation/`, with the
  normalized comparison in `build-audit/clang-explicit-context-comparison.json`.
- Typed7's full Memcheck run exposes `OMPTEST_copyin.c`: 2,934 errors in 790
  contexts, beginning in the packaged Clang library and declaration-context
  iteration. The initial masked baseline reports zero errors. The earlier
  **unmasked** REX build reproduces 2,947 errors in 791 contexts with zero lost
  bytes; stock Clang's independent AST dump reports 2,716 errors in 396
  contexts and one 96-byte possibly-lost allocation. Commands and reports are
  retained in `build-audit/external-reproducers/clang-copyin/`. This comparison
  demonstrates that a passing masked baseline cannot classify the newly
  visible reports; it does not accept their correctness or add an exception.
- Evidence: ignored `clang-compound-literal-*.log`,
  `clang-function-proto-constructor.asm`, `clang-field-vgdb-inspection.log`,
  `clang-initializable-members.asm`, `clang-record-completion.asm`,
  `clang-record-completion-vgdb-inspection.log`, `valgrind-bitfield-*.log`,
  and the unmasked focused CTest reports,
  under `build-audit/external-reproducers/`.
- The complete stream collector exposes `sage_genericCoverage_helloworld_cpp`.
  Uncapped, origin-tracked controls report 10,246 errors in 295 contexts in the
  earlier unmasked REX build and 9,935 in 296 contexts in the current build;
  both exit 99 with zero definitely, indirectly, or possibly lost bytes.
  Their first complete normalized error stack and allocation origin match:
  `memmove` through `Sema::ClassifyName`, with storage allocated by Clang.
  Stock Clang, using the same default language mode and suppression rules,
  independently reproduces that error stack. Its allocation origin differs,
  and its total report is larger (1,816,997 errors in 1,550 contexts, 32
  definitely lost and 192 possibly lost bytes). These observations establish
  pre-existing diagnostics, not complete report equivalence or acceptance.
  The first standalone control specified GNU C++14 and must not be used as a
  matched-language comparison. Commands and complete uncapped reports are in
  `build-audit/helloworld-definedness/`, `helloworld-matched-clang/`, and
  `helloworld-classify-name-comparison.json`. Full-suite C++ reports that hit
  Valgrind's default 1,000-context reporting limit remain failed and incomplete
  for diagnostic classification. No additional suppression is introduced.
- The generated `testStaticCFG_C99_test2008_01.temp.c` input also fails before
  CFG construction, while REX iterates the Clang function body. Uncapped runs
  with identical report options produce 338 errors in 167 contexts in both
  the current and earlier unmasked REX compilers, with zero lost bytes and
  exit 99. Origin tracking reaches Clang's `Parser::ParseFunctionDeclarator`
  stack allocation. Standalone Clang reports 3,446 errors in 165 contexts and
  96 possibly lost bytes. The private specimen's content hash, all commands,
  and reports are in `build-audit/cfg-definedness/`. REX frames at the point of
  a reported read do not establish a newly introduced REX defect, and these
  controls do not prove all affected values benign. Classification stays open.

Source101's actual Memcheck source-copy controls report 672 AST-mode and 674
token-mode definedness contexts, with no lost allocations. The first parsing
reports again reach ASTVector growth and record initialization. Running that
exact input with standalone `clang-22 -cc1 -std=gnu17 -ast-dump` under Valgrind
3.27.1, origin tracking, no report cap, and no suppression file produces 2,158
errors in 794 contexts (exit 99). Origins include Parser::ParseFunctionDeclarator
stack storage and RecordDecl::Create allocation. This establishes independent
reproduction; it does not classify every REX report or justify a broad mask.
Raw evidence: `build-audit/external-reproducers/source-copy-definedness/` and
`typed101/memcheck/Testing/Temporary/MemoryChecker.{14173,14174}.log`.

## EXT-CLANG-DEDUCTION-LEAK — obsolete suppression removed

- Former location: `leak:clang::MakeDeductionFailureInfo` in
  `scripts/rex-suppressions-for-lsan`. The file now contains no active exceptions.
- Verified with Debian LLVM/Clang 22.1.8, x86_64, compiler-rt ASan/LSan/UBSan,
  stock shared Clang and LLVM libraries. The historically cited tuple and variant
  AST-interface tests both exit zero with all LSan suppressions disabled and
  full allocation unwinding enabled. Outputs are isolated from full-suite runs.
- Independent `rex_clang_deduction_lifetime.cpp` uses only Clang Tooling and
  destroys each ASTUnit before the next parse. The same two input specimens
  pass one and ten iterations under unsuppressed instrumentation in C++17 mode.
  The retained probe also passes all four C++20 runs, matching the REX
  tests. No external diagnostic is inferred merely from the old comment.
- Evidence: `build-audit/external-reproducers/clang-deduction-*.log` and
  `rex-deduction-{tuple,variant}/run.log`. The probe is retained under
  `tests/nonsmoke/unit/SageInterface/` for future dependency rechecks.
- Disposition: remove the obsolete exception. The required complete sanitizer
  run on the final snapshot is pending; any new report must be investigated
  afresh, without restoring this broad historical match by default.
- Last verified: 2026-09-05. Recheck after dependency or diagnostic-lifetime
  changes using the probe below and the two existing REX tests unsuppressed.

The independent probe can be built with the selected LLVM installation (adjust
paths for that installation):

```sh
clang++-22 -std=c++17 -g -O1 -fsanitize=address,leak,undefined \
  -fno-omit-frame-pointer -I/usr/lib/llvm-22/include \
  tests/nonsmoke/unit/SageInterface/rex_clang_deduction_lifetime.cpp \
  -L/usr/lib/llvm-22/lib -Wl,-rpath,/usr/lib/llvm-22/lib \
  -lclang-cpp -lLLVM -o /tmp/rex_clang_deduction_lifetime
ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 \
  LSAN_OPTIONS=exitcode=99 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/rex_clang_deduction_lifetime \
  tests/nonsmoke/functional/roseTests/astInterfaceTests/inputrex_test2026_stl_tuple.C \
  10 /usr/lib/llvm-22/bin/clang++ /usr/lib/llvm-22/lib/clang/22
```

Repeat with the variant specimen and iteration counts 1 and 10. The executable
links no REX library and uses no suppression file.

## EXT-CLANG-FOLDINGSET-PADDING — pending revalidation

- Location: historical ASTContext/FoldingSet entries removed from
  `scripts/rex-suppressions-for-valgrind`; no FoldingSet exception remains.
- Existing evidence: the historical audit records an independent Clang 22 driver
  reproduction from the preprocessed constructor-family specimen and mandatory
  type-constructor and integration frames. It attributes reports to undefined
  padding.
- Ten generic FoldingSet masks were removed earlier: six matched a single LLVM
  frame, and four allowed any later Clang frame. The remaining historical
  FoldingSet rules were also removed because their stack gaps and integration
  frames were not backed by exact stacks and caller-owned negative controls.
- Missing evidence: captured commands/specimen/report for the selected toolchain,
  analysis that semantic bits remain defined, exact matching entries, and proof
  that adjacent REX-origin errors remain visible.
- Disposition: investigate newly exposed reports at their origin. Audit
  source-level definedness helpers independently; this entry does not justify
  the helpers or restore a suppression without the missing evidence.
- Recheck: baseline and changes to Clang, Valgrind, or the written-type boundary.
- Remove when: the unsuppressed focused report disappears or a supported API
  integration eliminates it without changing semantics.
- Last independently verified by this audit: not yet.

## EXT-FLANG-SYMBOL-ARENA — unresolved growing external allocation

- Supersedes the proposed EXT-FLANG-SEMANTICS-INTERIOR-POINTERS exception. Nine
  existing possible-leak suppressions are removed; an external stack alone did
  not establish bounded lifetime.
- Verified with Debian LLVM/Flang 22.1.8, x86_64, Valgrind 3.27.1. Independent
  `rex_flang_symbol_arena.cpp` and `.f90` specimens are under
  `tests/nonsmoke/unit/SageInterface/`. They link only LLVM/Flang libraries and
  destroy all parser and semantic-context objects after each parse.
- The minimal module has no reported lost memory after one parse. After 100
  parses, 5,848,196 bytes are possibly lost; after 1,000 parses, 112 bytes are
  definitely lost, 51,564,266 indirectly lost, and 6,911,552 possibly lost.
  Classification depends on the pointers retained at shutdown; allocation
  growth is the decisive result. No suppressions or annotations are used.
- LLVM's installed `flang/Semantics/scope.h:328` owns a private static
  `Symbols<1024> allSymbols`. `flang/Semantics/symbol.h:1075` explicitly describes
  symbols as never deleted; the allocator stores raw array pointers in a list
  and provides no release API. Symbol-owned constants and their references keep
  other semantic storage alive across context destruction.
- The larger `mpi.f90` specimen also reproduces the REX report's constant-copy
  allocation in `NonPointerInitializationExpr` without REX: ten parses lose
  491,600 bytes directly and 1,050,746 indirectly. A single compiler invocation
  or one context teardown is insufficient evidence for this embedding lifetime.
- Local artifacts: `external-reproducers/flang-symbol-arena-{1,100,1000}.log`,
  `flang-raii-*.log`, `check-flang-raii.sh`, and the standalone CMake project in
  `flang-raii/`, under `build-audit/`.
- Alternatives: ordinary RAII teardown does not release the static arena.
  Accessing its private storage or modifying the dependency is not an approved
  integration. A frontend worker process with a typed AST transport could bound
  lifetime, but requires a separately validated architectural change. No such
  correction is currently implemented, and no report exception is accepted.
- Recheck after Flang, toolchain packaging, or frontend lifetime changes. Close
  only when repeated parsing has bounded storage through a supported integration.
- Last independently verified: 2026-09-05.

The standalone diagnostic can be built against the installed LLVM 23 archives:

```sh
clang++-23 -std=c++17 -I/usr/lib/llvm-23/include \
  tests/nonsmoke/unit/SageInterface/rex_flang_symbol_arena.cpp \
  -L/usr/lib/llvm-23/lib -Wl,--start-group \
  -lFortranSemantics -lFortranEvaluate -lFortranParser -lFortranDecimal \
  -lFortranSupport -lFortranUtils -lclangBasic -Wl,--end-group \
  -lLLVM -lquadmath -o /tmp/rex_flang_symbol_arena
```

Run from a temporary working directory so generated `.mod` files remain outside
the source tree, passing the absolute specimen path, a module search directory,
Flang's intrinsic module directory (`$(flang-23 -print-resource-dir)/finclude/flang/$(flang-23 -print-target-triple)`),
and repetition count. Use `valgrind --leak-check=full --show-leak-kinds=all
--error-exitcode=99`; repeat with counts 1, 100, and 1,000.

## EXT-LEGACY-RUNTIME — removal candidates

- Locations: JVM-wide LSan matches, JVM Memcheck entries, and
  alloc_shareable_constant Memcheck entries.
- Evidence so far: no JNI/JVM embedding references or alloc_shareable_constant
  implementation in active REX source/configuration. Java/ECJ and EDG are dropped.
- Required checks: build/link/runtime reachability and instrumented regression;
  distinguish external ANTLR build tooling from nonexistent REX JVM embedding.
- Current disposition: the JVM LSan entries, all eight JVM Memcheck entries,
  and the old `alloc_shareable_constant` entries are removed. Active source and
  configuration have no JNI/JVM embedding, and the full registry contains no
  Java execution. Required full instrumented validation remains pending.
- Disposition: remove proven obsolete entries; do not replace them with broader
  library or runtime patterns.
- Last independently verified by this audit: not yet.

## EXT-COREUTILS-SORT-ARGUMENTS

Status: avoid redundant invocation; no suppression or tracing exclusion.
Verified 2026-09-06 with Debian GNU coreutils 9.10-1, x86_64, Valgrind 3.27.1.
The Rodinia checkpoint comparison sorts its list of output paths before diffing
each path independently. Ordering does not affect that comparison. Full child
tracing reveals an 8-byte definitely-lost allocation from malloc through sort
at offsets 0x1579c/0x5890; the REX translator reports zero defects.

Independent sort invocations with stdin containing 1, 2, and 10,000 lines all
retain exactly 8 bytes with the same allocation stack. File-argument variants
retain 16 bytes through realloc at 0x158a4/0x51c9. Evidence, complete reports,
versions, inputs, outputs, and exit codes are in build-audit/sort-lifetime/.
These controls establish an external process report and input-size independence;
they do not justify suppressing arbitrary coreutils memory reports.

The comparison script removes its unused ordering operation. File discovery,
its nonempty assertion, and every diff remain present; REX and all remaining
children remain instrumented. Recheck this record if ordering acquires semantic
meaning or the utility is reintroduced. No dependency fork, uninitialized-memory
annotation, or report exception is introduced.

## EXT-VALGRIND-NESTED-LAUNCHER — independent checker startup

- Verified 2026-09-06 with Valgrind 3.27.1, x86_64, Debian LLVM 22.1.8.
- The external-boundary contract runs its own configured Valgrind to prove an
  undefined caller allocation is reported through external-library frames.
  Under outer CTest Memcheck, Python's launch path instead instruments that
  Valgrind process; the inner runtime cannot map its startup memory
  (`mmap(0x58000000, 4096)` returns EINVAL), and no inner report exists.
- Preserve instrumentation of the Python harness and its normal children.
  Add only the exact configured Valgrind launcher and its resolved path to
  outer child-tracing exclusions. The inner checker starts independently and
  instruments its requested program. Do not skip a shell, Python, RTH, the
  REX wrapper, or a REX executable. No memory-report suppression is added.
- The real production-options control fails before this integration correction
  and passes afterwards. It still requires the undefined control's exit 79,
  allocation origin, source/library frames, and the initialized control's exit
  zero and zero errors. Raw commands and both attempts are retained under
  `build-audit/nested-memcheck-control/`. Source91 passes all five native instrumentation/packaging controls. Actual
  CTest Memcheck passes the three checker controls, including the existing
  fork/exec ignored-child and post-dashboard-defect contracts, with zero
  reported defects for them. The separate packaging control fails because its
  nested project selects host cc instead of the configured Clang compiler;
  that failure is retained and the fixture must receive the exact compiler.
- Recheck on Valgrind or process-launch integration changes; remove the launcher
  exception if nested instrumentation becomes supported without startup failure.
  This covers checker startup, not a REX correctness or leak exception.

## EXT-LDD-LOADER-TRACE — retired with unreachable packaging

Retired by AUD-333: the only dependency-inspector consumer was the audit-added
standalone test for packaging code unreachable from every supported root
configuration. That code and test are removed, along with required ldd discovery
and its exact child-tracing exclusion. No replacement exclusion or suppression is
installed. The historical protocol reproduction below remains evidence about the
inspector; it does not establish a remaining REX integration requirement.


- Verified 2026-09-06: the audit's Debian LLVM22 image, CMake 4.3.4 and
  Valgrind 3.27.1, Linux x86_64. This is an instrumentation protocol conflict,
  not a justification to suppress memory diagnostics.
- Standalone ldd on the fixture executable lists its required shared library.
  Under Valgrind --trace-children=yes it exits 0 but omits that library, listing
  only libc and the loader. The same happens when an instrumented Python parent
  launches it. ldd's LD_TRACE_LOADED_OBJECTS environment reaches Valgrind's
  launcher rather than the executable selected for dependency inspection.
- An exact --trace-children-skip=/usr/bin/ldd restores the complete dependency
  listing while the parent remains under Memcheck. The production integration
  resolves ldd and its canonical path at configuration and exempts only that
  inspector, with no basename wildcard, interpreter exclusion, or suppression.
  Executing installed applications remains instrumented. The package contract
  requires the copied library to work after the original locations are moved.
- The failed modern CMake alternative and direct-loader --list experiment are
  retained separately. The former reports ldconfig errors; the latter reports
  one loader definedness error. Neither tool gets an exclusion or suppression.
- Evidence: `build-audit/ldd-instrumentation/{results.json,parent/results.json}`,
  full commands, stdout/stderr and all per-process Memcheck logs. The original
  packaging failure is preserved in typed92/instrumentation. Source96 validates the integration under actual CTest Memcheck: all four
  packaging/checker controls pass, raw/batch exits 0, complete JUnit, and an
  empty memory-defect table. The ordinary-child negative controls still fire.
- Recheck the standalone before/after listing, relocated runtime check, and
  ordinary-child negative controls on changes to libc, CMake or Valgrind.
  Remove the inspector boundary once child instrumentation preserves the
  dependency-query protocol without changing its output.


## EXT-VALGRIND-REPORT-ARTIFACTS — diagnostic ownership

- Verified 2026-09-06 with Valgrind 3.27.1, Linux x86_64. A real frontend
  invariant failure in source108 creates `report.pipe.core.403` (476,819,456
  bytes) beside the wrapper's FIFO. `rmdir` then replaces the command's abort
  status with its own status 1. The memory stream itself is retained.
- The wrapper owns FIFO cleanup; additional diagnostic files remain available
  for investigation. It removes an empty directory and retains a nonempty one,
  reports its location, and preserves command/reader/report error checks. This
  changes no instrumentation, suppression, or accepted test result.
- A contract creates a small diagnostic artifact from a real instrumented
  command and requires its status 23, complete report, removed FIFO, and retained
  artifact. It does not require permission to create large OS core dumps.
  The old wrapper fails that contract with status 1. The first corrected probe
  used incorrect compiler realpaths and failed at Flang's loader; that setup
  attempt is separate from the rerun with exact configured paths.
- Evidence: `build-audit/report-artifact-review/`,
  `build-audit/parameter-publication-review/`, and the interrupted source108
  full-Memcheck dashboard. Recheck report artifact locations when Valgrind or
  stream collection changes; retain this ownership rule while the checker can
  produce auxiliary diagnostics beside its log destination.

## EXT-VALGRIND-X87-EMULATION — exact storage integration, no suppression

- Verified 2026-09-06 with Clang/LLVM 22.1.8 and Valgrind 3.27.1 on x86_64.
  The [Valgrind core manual](https://valgrind.org/docs/manual/manual-core.html#manual-core.limits)
  documents that x87 arithmetic and register transfers use binary64 internally.
- The initial standalone extended-value probe passes natively but fails under
  Memcheck when `numeric_limits<long double>::denorm_min()` returns through the
  x87 ABI. The caller has already converted the value to zero before import.
  This is a numerical instrumentation limitation; zero reported memory errors
  does not make that failed assertion a successful test.
- The REX numeric API imports native storage by const reference and performs
  arithmetic through typed APFloat values. Importing a stored x87 subnormal,
  a value above binary64's maximum exponent and an extended-precision mantissa
  preserves the exact words in native, ASan/UBSan and actual Memcheck controls.
  The same controls retain binary128 arithmetic and exact classifications.
- No tool detection, annotation, suppression, exclusion, or altered expected
  result is installed. The permanent extended AST contract also checks those
  native-storage imports. Frontend Clang values enter through exact APFloat bits
  and never pass through native x87 registers.
- Evidence: `build-audit/extended-floating-design/current-{native,memcheck}.log`
  retains the initial native pass/Memcheck failure. `storage-{native,memcheck,
  sanitized}.log` retains the exact-storage controls: all exit 0, zero memory
  defects, zero lost allocations. Calls that perform native x87 arithmetic or
  return through its ABI remain subject to the documented tool limitation.
- Recheck on architecture, LLVM or instrumentation changes. This integration
  remains useful independently of Valgrind because numeric storage transport
  must not evaluate or round the value it copies.

## EXT-VALGRIND-MULTICALL-ENV — executable-script dispatch integration

- CMake checks `/usr/bin/env` under the configured Valgrind for split arguments
  and arbitrary interpreter `argv[0]`. Weekly CI selects Ubuntu's GNU alternative
  for affected uutils versions. Valgrind handles native script dispatch.
- Reason: Valgrind supplies its synthetic process name to script interpreters.
  A multicall env implementation dispatches from that name and rejects it before
  executing the requested script. The existing memory-tooling guide records the
  affected Ubuntu 26.04 configuration. The independent 2026-09-07 control now
  reproduces it with uutils 0.8.0 and Valgrind 3.26.0: raw primary and nested
  scripts fail with "unknown program 'memcheck-amd64-linux'". The former primary
  script adapter could not repair nested dispatch.
- Scope: interpreter dispatch only. The primary interpreter and REX descendants
  remain instrumented; no memory report, test, shell or Python interpreter is
  excluded. Missing/incompatible env is a configure error. The wrapper rejects
  an empty env command before it can re-execute the script indefinitely.
- Alternatives: a primary-script adapter alone cannot change nested scripts'
  kernel-selected interpreters. A supported standalone utility avoids dependency
  patches and broad child-tracing exclusions. The capability probe replaces the
  former GNU version-banner regex; branding does not establish dispatch behavior.
- Switching through Ubuntu's supported coreutils-from-gnu alternative makes
  native, raw-Valgrind and wrapper controls pass for both primary and nested
  scripts. The nested leaking child still prints its marker, exits 79 and reports
  its exact 317-byte allocation. All six GNU controls are checked explicitly.
  Evidence: `build-audit/ubuntu-memory-review/{before2,after}/results.json`,
  raw reports, package versions, env versions and `after/verified.json`.
  Full current Memcheck remains pending.
- AUD-2134 removes the obsolete primary-script adapter. Its unchanged-wrapper
  witness rejects valid GNU `env -S bash -eu`; native dispatch passes that form,
  ordinary env scripts and all four existing wrapper/error-boundary contracts.
  Source: `source-wrapper-native-script-20261017.sha256`; evidence:
  `build-audit/container-wrapper-{script-before,native-script}-20261017-*`.
- AUD-2140 verifies uutils 0.10.0 under Valgrind 3.27.1: the traced synthetic-name
  probe succeeds, and two nested script launches preserve instrumentation.
  The released 317-byte control and both shells have zero errors/lost bytes and
  zero suppressions. Its leaking counterpart reports the exact 317-byte loss
  and exits79. Five existing configuration/wrapper boundary contracts pass;
  compiler, generated AST and registry hashes are unchanged. Evidence:
  `build-audit/container-env-native-capability-20261017-*` and
  `build-audit/env-uutils-010-probe-20261017/` reports. The temporary interpreter
  copy and two-line launch fixture are removed after all consumers finish;
  their hashes and raw reports remain. No memory-report rule changes.
- Recheck on Valgrind, env implementation or shebang-handling changes; remove the
  distribution-specific selection when its default passes the nested traced
  controls. Full current Memcheck validation remains pending.

## EXT-UTILITY-LIFETIMES — broad masks removed; exact reports classified

- AUD-186: 32 historical malloc/realloc/calloc/new entries match any executable
  with one of eight utility basenames. A byte-identical 317-byte leaking caller
  fails under its original name but exits zero as bash, dash, grep, cmp, mktemp,
  awk, gawk or find. Removing those entries exposes every leak (exit 79); the
  released control remains clean. The strengthened production boundary contract
  checks leaking/released variants under all nine names and passes with the
  complete updated policy. The later source/stack review below replaces only
  four proven allocation paths, never the historical wildcard masks.
- Independent controls with Valgrind 3.27.1 in the LLVM22 audit image: bash, dash,
  cmp and find have zero reported errors without repository suppressions.
  A 10,000-line grep workload has one 128-byte possible leak. mktemp has two
  definite filename allocations totaling 118 bytes for this path. The installed
  awk resolves to mawk and reports one 2,048-byte possible leak; gawk is absent.
  These initial observations alone did not justify exceptions. The later exact
  source, debug-symbol and lifetime review is recorded under
  EXT-GREP-PATTERN-ARRAY, EXT-MAWK-BUILTIN-ARENA and
  EXT-COREUTILS-MKTEMP-FILENAMES. The old wildcard awk entries did not match mawk.
- Evidence: `build-audit/utility-suppression-review/{results.json,real/results.json}`
  and all command/stdout/report files; `production-command.json` and
  `production-exit.txt` verify the actual configured wrapper integration.
- Remaining checks: full Memcheck with the narrow patterns, exact package
  debug-symbol availability in CI, and rechecks for changed package versions.
  Preserve every report. Do not infer harmlessness solely from a utility frame
  or introduce process-name exclusions. Verified filter defect: 2026-09-07.

## EXT-PERL-INTERPRETER-CLEANUP — supported lifecycle integration

- Perl documents [full interpreter cleanup for Valgrind](https://perldoc.perl.org/5.45.2/perlhacktips#PERL_DESTRUCT_LEVEL)
  through PERL_DESTRUCT_LEVEL=2. The CTest Memcheck wrapper exports it for the
  instrumented interpreter and descendants; normal uninstrumented execution is
  unchanged. Four allocator-wide Perl leak suppressions are removed.
- Verified 2026-09-07: Debian Perl 5.42.3, Linux x86_64, Valgrind 3.27.1. One and
  10,000 lexical-allocation loop iterations at cleanup level 0 each report 244
  contexts. At level 2 both exit zero with no definite, indirect or possible
  leaks and no suppressed reports. Commands, complete Perl configuration and
  raw logs are in `build-audit/utility-suppression-review/perl-cleanup/`.
- This uses the interpreter's supported teardown, without altering its private
  arena or discarding diagnostics. The wrapper contract requires the setting to
  reach a child and runs the allocation workload under the checker. Independent
  caller-owned undefined/leaking controls must continue failing as expected.
- Recheck on Perl/package/wrapper changes, and keep unsupported cleanup behavior
  visible as a failed contract. Full Memcheck with this integration is pending.

## EXT-GREP-PATTERN-ARRAY — owned interior pointer

- Verified 2026-09-07: GNU grep 3.12-1, Linux x86_64, Valgrind 3.27.1;
  build ID `cc4a7bf2d4291f64d7fb7b998e6406d008138318`. One and 100,000
  input lines each produce one 128-byte possible-leak report without suppressions.
- Exact package debug symbols identify realloc/rpl_realloc/xrealloc/xpalloc,
  GEAcompile, then grep's main. The [matching package source](https://sources.debian.org/src/grep/3.12-1/src/dfasearch.c/)
  stores the pattern-array pointer one element past its allocation base and
  subtracts one when resizing. The live pattern storage is represented by an
  interior pointer, not an abandoned REX allocation or undefined semantic value.
- The exception requires every allocation frame and the grep executable frame,
  and accepts only possible leaks. Six independently repeated utility controls
  pass with the candidate complete policy; every renamed 317-byte REX leak and
  the caller-owned undefined-value control still fails with status 79.
- Debug symbols are required for these frame names. Missing symbols or a changed
  stack leave the report visible; do not substitute object-wide masks. Recheck
  the source ownership and both input sizes when grep, its package, or Valgrind
  changes. Full-matrix and CI debug-symbol integration remain pending.

## EXT-MAWK-BUILTIN-ARENA — pooled builtin-variable storage

- Verified 2026-09-07: mawk 1.3.4.20260302-1, Linux x86_64, Valgrind 3.27.1;
  build ID `17cd622e5d4f42add0511b9f798ce301659f2a2a`. One and 100,000
  input lines each produce one 2,048-byte possible-leak report without suppressions.
- Exact symbols identify malloc/zmalloc/insert/bi_vars_init/initialize/main.
  The [allocator](https://sources.debian.org/src/mawk/1.3.4.20260302-1/zmalloc.c/)
  subdivides chunks and advances its available-storage pointer. The
  [builtin-variable initializer](https://sources.debian.org/src/mawk/1.3.4.20260302-1/bi_vars.c/)
  stores live entries in the symbol table through those interior pointers.
- The exception covers only possible leaks from that complete initialization
  path in mawk. It does not cover arbitrary zmalloc calls. The complete candidate
  policy passes the existing external-boundary controls, including all renamed
  leaking and released executables and the undefined caller-allocation control.
- Recheck package identity, pool ownership, input-size controls and negative
  controls on updates. Exact debug symbols are required; unresolved frames remain
  failures. Full-matrix and CI debug-symbol integration remain pending.

## EXT-COREUTILS-MKTEMP-FILENAMES — two process-lifetime names

- Verified 2026-09-07: GNU coreutils 9.10-1, Linux x86_64, Valgrind 3.27.1;
  mktemp build ID `34d11b1d9e9bb2d083722dd2fc9d1d88b7f92cad`.
- The [matching implementation](https://sources.debian.org/src/coreutils/9.10-1/src/mktemp.c/)
  constructs its full filename and a diagnostic copy, creates one file, prints
  the resulting name, and exits. Both buffers remain allocated until that exit.
  The two exact stacks are malloc/mfile_name_concat/file_name_concat/main and
  malloc/xstrdup/main. Increasing the template by 72 characters increases total
  retained bytes by 144, with exactly two allocations in both controls.
- Two definite-leak exceptions require the full allocation paths and mktemp's
  caller frame. No arbitrary malloc or process-basename exception is restored.
  All renamed REX leak controls remain visible under the complete policy.
- These executable-local names have no supported caller cleanup API; no private
  cleanup hook or patched utility is introduced. Recheck both source paths,
  package identity, two filename lengths, and the negative controls on updates.
  Exact debug symbols and final full-matrix/CI integration remain pending.

The evidence shared by these three entries is under
`build-audit/utility-suppression-review/`: `lifetimes/` contains unsuppressed
commands and reports, `symbols/` contains build-ID-verified debug files and
symbolized locations, `sources/` records exact package source URLs and hashes,
`symbolized/` retains the unsuppressed annotated stacks, and `candidate-tested/`
contains all six passing utility cases plus the complete production-wrapper
external-boundary contract. No result is inferred solely from a process exit or
an external library name.

### Ubuntu 26.04 utility integration recheck (2026-09-07)

The real Ubuntu package run validates EXT-GREP-PATTERN-ARRAY,
EXT-MAWK-BUILTIN-ARENA and EXT-COREUTILS-MKTEMP-FILENAMES with Valgrind 3.26.0.
Ubuntu grep 3.12-1 and mawk 1.3.4.20260129-1 match their existing exact paths.
Ubuntu's standalone `gnumktemp` is owned by `gnu-coreutils` 9.7-3ubuntu2 (source
package `coreutils`), and exposes xmalloc/xmemdup inline frames on its diagnostic
copy. Two additional complete paths require this exact executable name and these
frames. No wildcard is inserted between allocation and owner.

Package-matched symbols come from Ubuntu's signed debug archive; their ELF
build IDs are grep `9601f9b5d319a231b14e6c6289e998da074aa543`, gnumktemp
`5fa328f2e79f635cf2a732c6bbe1c963f846f9f2`, and mawk
`c83bc5be5a3d9d0da590b193aec96cc44846455d`. The verifier records their hashes in
`build-audit/ubuntu-memory-review/verified-symbols.json`. The exact Ubuntu source
archive and packaging patch series confirm the two mktemp filename allocations
at src/mktemp.c:299 and :307; none of its six patches changes this file.
The file SHA-256 is
`176f2db23caa6cde6086d669d905d2c6ab0ba229e88f73aa853db76f2fa14113`.
The native package remains unchanged; no dependency fork is required.

All three unsuppressed utility controls report their expected retained storage;
all three report-specific controls pass. The actual configured wrapper also
passes all caller-owned boundary controls, including eleven executable names
with leaking/released allocations and the undefined/initialized shared-library
control. The gnumktemp name is now a permanent negative control. Evidence is in
`build-audit/ubuntu-memory-review/utility-reports-final/` and `boundary-final/`.

The debuginfod server timed out, and its failed attempt remains recorded.
CI now uses the documented signed package archive directly, pins debug packages
to installed versions, and verifies build IDs before testing. Its first HTTPS
archive attempt failed on the provider's HTTP redirect; the documented HTTP
endpoint passes APT signature verification. Missing or mismatched symbols remain
hard errors. Earlier source142 Memcheck and source143 Release do not include
this integration revision. Final full Memcheck acceptance remains pending.

## EXT-BASH-TERMINAL-COMMAND — parser ownership at process exit

- **Status:** Narrow external exception; full Memcheck revalidation pending.
- **Affected packages:** Debian Bash 5.3-3+b1 (source 5.3-3), Valgrind 3.27.1;
  Ubuntu Bash 5.3-2ubuntu1, Valgrind 3.26.0. Verified 2026-09-07. Exact build IDs
  are `63f5da93ce9a5477a8c2e42543a0ce32e2cda783` and
  `a292b52b679fb438b940f1c17e2e4951ac52ac89` respectively.
- **Origin:** Bash reader_loop nulls its current-command pointer for terminal
  exit paths before reaching dispose_command. Its command and parser-owned
  children are then retained until process termination. The
  [Debian source](https://sources.debian.org/data/main/b/bash/5.3-3/eval.c)
  and exact signed Ubuntu source archive agree on eval.c SHA-256
  `21bfede320d640a219fc0ffa66d7b90d8f1fae87605847d4d06e6e058c4e7051`;
  no Ubuntu packaging patch changes this file. This occurs without REX loaded.
- **Lifetime/semantic evidence:** On both platforms a simple terminal if-command
  retains 32 direct and 88 indirect bytes; a compound conditional/redirection
  command retains 32 direct and 424 indirect bytes. Each amount is identical
  after one or 10,000 preceding loop iterations. The corresponding normally
  completed scripts report no losses. Only the terminating command is involved;
  no executing AST, caller-owned data, returned semantic value or continuing
  process depends on the retained storage.
- **Disposition and alternatives:** REX must preserve valid script exit and
  failure behavior. Bypassing tested script behavior, disabling child tracing,
  excluding shell tests, and restoring basename/allocator-wide masks would hide
  failures. Prefer supported normal cleanup in REX-owned control flow when it
  preserves immediate error reporting and test semantics. No supported Bash shutdown API releases this discarded command, and
  carrying a patched shell is unnecessary for a bounded terminal allocation.
  Fourteen complete allocation paths name each parser owner and the final Bash
  executable, preserve exact optimized Debian/Ubuntu frame variants, and cover
  only the observed definite or indirect leak kinds. Other reports remain errors.
- **Verification:** Sixteen unsuppressed lifecycle controls reproduce the above
  boundary. All sixteen controls pass with the candidate rules, and all eleven
  renamed leaking/released executables plus the undefined/defined shared-library
  controls retain their expected results on both platforms. Exact ELF companion
  verification is required in CI. Candidate policy, raw reports, source hashes
  and commands are retained under `build-audit/bash-exit-lifetime/`;
  `build-audit/bash-ubuntu-source-review.json` verifies source archive hashes.
- **Recheck/removal:** Repeat unsuppressed terminal/ordinary completion and
  workload-size controls on Bash, compiler or Valgrind updates. Remove these
  rules when Bash releases the terminal command. Changed stack shapes remain
  failures until independently reviewed. Any growth with execution count or
  evidence of a live semantic dependency invalidates this exception.

The same Bash owner has an additional terminal boundary: a top-level simple
command calling a shell function that invokes exit. Independent Debian/Ubuntu
controls retain 32 direct and 24 indirect bytes after one or 10,000 iterations;
returning normally from the function is clean. Direct top-level exit controls
are also clean and must not be cited as reproducing this report. The nested CTest
result contract's clean program has zero errors; its outer Bash wrapper loses
this calling command when run_checked_command exits the process. The existing
exact make_bare_simple_command allocation path now covers the observed definite
kind as well as its earlier indirect kind. No new wildcard or allocation path
is added. All eight function-return/exit controls and both sets of 24 caller-owned
boundary controls pass with the candidate. Evidence is retained under
`build-audit/bash-function-terminal/`; the non-reproducing direct-exit controls
remain under `build-audit/bash-simple-terminal/`.


The production policy harness now uses normal function returns before process
termination (AUD-218). Its intentional failure-path contracts had exposed 32
direct and 1,614 indirect bytes in the still-active for/conditional command.
Returning from the harness immediately preserves failure/warning statuses and
stops policy execution while letting Bash finish command cleanup. No new Bash
rule is added for those 27 contexts. Both unchanged full inventory contracts
pass with all 62/29 child summaries zero; final formatting and literal-name
controls pass separately in policy-inventory209 and policy-format209. The dead
error counter and obsolete rosegit comment are removed from the production
harness. This integration avoids terminal abandonment before it occurs; it does
not change external shell fixtures or conceal their reports.


## EXT-GREP-DFA-STATE-TABLES — owned interior transition arrays

- **Status:** Narrow external exception; full Memcheck revalidation pending.
- **Affected packages:** GNU grep 3.12-1, Debian Valgrind 3.27.1 and Ubuntu
  Valgrind 3.26.0; verified 2026-09-07. Exact grep executable/debug identities
  are recorded with EXT-GREP-PATTERN-ARRAY and required by the CI verifier.
- **Origin:** In the [packaged DFA implementation](https://sources.debian.org/data/main/g/grep/3.12-1/lib/dfa.c),
  realloc_trans_if_necessary stores the transition-array pointer two elements
  past its allocation for negative sentinel indices. build_state stores owned
  transition tables in this array. Memcheck classifies both the interior root
  and its tables as possibly lost. dfafree frees every table and the original
  pointer (trans minus two); ownership is intact. Grep retains its compiled
  automaton until process termination. The noninitial transition cache has a
  1,024-table eviction threshold. This is not a REX AST or a discarded owner.
- **Source verification:** The signed Ubuntu source package is also 3.12-1.
  Applying its exact distribution patch to the pristine source reproduces the
  reviewed Debian file byte for byte, SHA-256
  `fd88cdc3768d0e5039ca7f5d414e5d5633631598feb4a944afabf759156c5927`.
  That patch changes stray-backslash diagnostics, not allocation ownership.
  Archive and patch hashes are retained in
  `build-audit/grep-dfa-source-review.json`.
- **Lifetime and semantics:** Three actual report-validator regexes return the
  correct counts on both platforms. Their possible-storage totals are 31,040,
  41,280 and 47,520 bytes respectively, including the separately classified
  pattern array. Each total is identical for one or 10,000 input repetitions;
  definite and indirect losses remain zero. All twelve unsuppressed runs fail
  only for the three observed possible-allocation contexts.
- **Disposition:** Six exact allocation paths cover the two owners and the
  observed Debian/Ubuntu single-byte DFA frame variants, require the owning
  grep executable, and apply only to possible leaks. No frames use ellipses.
  Keep the utility's valid matching behavior and all report-validation checks;
  rewriting tests, disabling child tracing, and allocator/basename-wide masks
  would weaken the audit. A patched system utility is unnecessary for this
  intact process-lifetime ownership.
- **Verification:** All twelve candidate runs preserve their exact match counts
  and report zero errors. Both platforms also pass all 24 caller-owned boundary
  controls: eleven renamed leaking/released executables and the undefined/
  initialized shared-library pair. Commands, raw reports and policy hashes are
  retained in `build-audit/grep-dfa-review/`. Initial setup/download failures
  remain recorded separately.
- **Initial-array extension:** The full source160 Memcheck focus exposes a
  further 128-byte path in the unchanged OpenMP pragma-output comparator.
  dfaexec_main allocates the first transition array directly, without build_state.
  Two complete Debian/Ubuntu stack variants cover only this possible-leak path.
  The exact generated input gives 15 matches and 22,656 possible bytes in twelve
  blocks. With 10,000 and 100,000 copies, output is exactly 150,000 and 1,500,000
  matches; storage stabilizes at 22,720 bytes in twelve blocks on both platforms.
  The extra 64 bytes replace the initial root through the already reviewed
  build_state resize path, not an additional lost allocation. Definite and
  indirect losses stay zero. Both small/large candidate pairs report zero errors,
  and all 24 caller-owned boundary controls pass independently on each platform.
  Raw unsuppressed and candidate reports, exact commands and output/policy hashes
  remain in `build-audit/grep-initial-table162/`. The initial synthetic three-branch
  input exercises the old path and is not evidence for the new initial path.
  Source160's frozen policy predates this extension and its failures remain failed.
- **Recheck/removal:** Reproduce without these rules on grep, compiler or
  Valgrind changes. Remove them when the report no longer occurs. Changed
  allocation paths, actual owner loss, incorrect matches or growth with repeated
  input require new investigation and are not covered by this exception.

## EXT-ASAN-MEMCHECK-STARTUP — unresolved instrumentation boundary

- **Status:** Reproduced incompatibility; no test exclusion or suppression added.
- **Affected environment:** Debian LLVM/compiler-rt 22.1.8 and Valgrind 3.27.1
  in the audit Docker environment; verified 2026-09-07.
- **Evidence:** The unchanged rex_cmake_linker_policy contract executes its
  ASan/UBSan-linked empty C fixture under outer Memcheck child tracing. It fails
  before main in asan_linux.cpp:148. An independently compiled empty C program
  exits 0 natively and 1 under Memcheck, with the same failed personality call
  and zero Memcheck errors. Neither executable links REX.
- **Origin:** The [exact compiler-rt startup source](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/compiler-rt/lib/asan/asan_linux.cpp)
  reaches this personality call after rejecting the process memory layout.
  The Docker environment refuses the requested ASLR change. This proves the
  observed startup boundary; it does not establish a REX memory defect or justify
  disabling instrumentation around REX processes.
- **Disposition:** Remains a failed Memcheck case. The full regular and sanitizer
  matrices retain the runtime-link check. A supported boundary for this separate
  runtime check, or an explicitly documented matrix exclusion that preserves its
  coverage, still needs review. No test assertion is changed and no instrumented
  REX execution is bypassed.
- **Evidence/removal:** `build-audit/asan-memcheck-boundary/` retains the complete
  compiler command, native/Memcheck statuses and logs; source146's original
  failure remains in its focused memory evidence. Recheck runtime startup when
  Valgrind, compiler-rt or container execution policy changes. A native-only pass
  cannot close the failing combined-instrumentation case.

## EXT-COREUTILS-TR-SETS — resolved REX utility integration

- **Status:** Removed the unnecessary external operation; no suppression or
  instrumentation exclusion added. This is not an accepted leak exception.
- **Affected environment:** GNU coreutils 9.10-1 (tr 9.10), Debian audit image,
  Valgrind 3.27.1; verified 2026-09-07.
- **Evidence:** Source160's actual policy-checker child reports show 64 direct
  and 64 indirect bytes in four blocks from `tr A-Z a-z`. The parent shell
  exits zero despite its child's Memcheck status. Reproducing the formatting
  operation independently retains both native status and every child report.
  This identifies the responsible external operation, without claiming source
  ownership or bounded lifetime from stripped frames alone.
- **Disposition:** The Bash harness now performs ASCII lowercasing with its
  builtin case conversion. Its quoted argument preserves spaces, and printf
  preserves option-like names. The existing sed formatting operation retains
  its original locale; LC_ALL=C applies only to Bash's ASCII conversion.
  Policy discovery, policy execution and policy error handling are unchanged.
- **Verification:** Three before/after pairs cover ordinary CamelCase names,
  embedded spaces and an option-like name. Ordinary output is byte-identical;
  the latter two preserve the full literal name. Every final child report has
  zero errors without suppressions. Commands, report/output hashes and raw
  reports are retained in `build-audit/policy-format181/`. Earlier candidates
  accidentally changed sed's locale and exposed additional DFA allocations;
  those failed attempts are retained and are not passing evidence.
- **Actual harness:** The complete source182 policy checker passes its configured
  Memcheck wrapper with native status 0 and all 79 child error summaries zero.
  Commands and the exact complete report hash remain in
  `build-audit/policy-current186/`; source173's earlier failed reports are retained.
- **Recheck:** Keep ASCII and literal-filename behavior when changing the
  formatter. The separate Git-init reports from policy inventory tests remain
  unresolved; these formatting controls do not close the complete Memcheck gate.

## EXT-GIT-INIT-CHDIR-LIST — verified callback-entry ownership

- **Status:** One exact entry-constructor rule, possible leaks only. No
  process-wide mask, test exclusion or private dependency cleanup.
- **Versions/platforms:** Debian Git 1:2.55.0-1 with Valgrind 3.27.1, ELF build ID
  ea41370ebbf927a97e2101471b576063cf787052; Ubuntu Git 1:2.53.0-1ubuntu1 with
  Valgrind 3.26.0, ELF build ID 0a7e365778bff1c615d0f57cd5c4953b151c2c0e.
  Exact signed package/source metadata and matching debug companions are retained.
  Verified 2026-09-07. Nightly setup installs Git's exact debug package and checks
  the ELF identity before relying on this symbol-specific path.
- **Source ownership:** In both packages, chdir-notify.c's sole allocation
  constructs a fixed 40-byte callback entry and links its embedded list member
  into the static chdir_notify_entries list. Three pointer fields precede that
  member; list_entry reconstructs the owner from its interior pointer. The
  unregister operation unlinks and frees the entry. This implementation is
  byte-identical in the reviewed sources. The allocation never constructs a
  caller-provided payload: name, callback and data are borrowed fields. Ordinary
  Git exit terminates the process while its global callback list remains intact.
- **Initialization and inventory:** Init registers three reference-store
  callbacks on Debian and also an object-database callback on Ubuntu. File
  inventory instead registers two object-source callbacks on Debian and one
  object-database callback on Ubuntu. The latter reports became reachable in
  actual policy tests only after their preceding init command stopped failing.
  Five initial command-specific paths therefore did not resolve these tests;
  their partial result is retained, and those patterns have been replaced.
- **Exact scope:** Match malloc, do_xmalloc, xmalloc, chdir_notify_register and
  its caller in the Git executable. This identifies the only allocation in the
  reviewed entry constructor, independently of the command that registers it.
  No ellipsis is used. Caller payloads, unrelated Git allocation functions,
  definite/indirect losses and undefined reads cannot match this rule. The
  complete constructor implementation establishes ownership; a utility basename
  alone does not. REX does not change repository behavior or library internals.
- **Controls:** Init copies exactly one and 10,000 template files, preserving
  every payload, HEAD and configuration. Unsuppressed storage stays at 120 bytes
  in three blocks on Debian and 160/four on Ubuntu. Exact NUL-delimited inventories
  of one and 10,000 worktree files retain 80/two on Debian and 40/one on Ubuntu.
  All raw runs have zero definite/indirect loss. All eight candidate operations
  pass with zero errors. Each platform independently passes 26 caller-owned
  controls, including the identical 317-byte leaking/released executable renamed
  git and the undefined/initialized shared-library pair.
- **Actual test boundary:** The callback-entry rule lets both unchanged policy
  inventory tests reach their Bash failure-path checks. Those expose a separate
  active-command cleanup defect; normal function returns resolve it without any
  added Bash suppression (AUD-218). Both complete tests then pass, retaining all
  62 and 29 child error summaries with zero errors in policy-inventory208.
- **Alternatives/evidence:** Removing Git from inventory fixtures would weaken
  real-repository coverage; disabling tracing would hide subprocess errors. No
  supported CLI teardown is needed for this intact static list. Package sources,
  binaries, exact hashes, commands and raw reports remain in git-symbols192/,
  git-review194/, git-ubuntu196/, git-ubuntu198/ and git-candidate205-{debian,ubuntu}/
  under `build-audit/`. The earlier command-specific candidate results and the
  Ubuntu .ddeb setup correction are retained separately.
- **Recheck/removal:** Reproduce without the rule on Git, compiler, platform or
  Valgrind changes. Review the complete constructor/list teardown before adapting
  a changed stack. Remove the rule if the report disappears. A changed allocation
  purpose, loss of the owning list, freed callback payload access or unexpected
  growth requires new investigation; the current evidence does not accept such
  defects merely because they occur in Git.

## EXT-CLANG-GNU-ZERO-ALIGNMENT — unresolved language boundary

The attribute-plugin alternative was reviewed after batch 478. In the published
[Clang attribute dispatch implementation](https://clang.llvm.org/doxygen/ParsedAttr_8cpp_source.html),
`ParsedAttrInfo::get` returns built-in attribute information before consulting
plugin spellings. Merely registering another `aligned` spelling therefore does
not provide the required override. No plugin, token rewrite, diagnostic
suppression or test-expectation change was introduced. The pinned-toolchain
compatibility boundary remains unresolved.

Verified with stock Clang 22 in the bounded LLVM 22.1.8 image on Linux x86_64.
The inherited `test2013_71.C` contains three GNU `aligned(0)` attributes. Stock
Clang rejects them as non-power-of-two alignments. The former mandatory C++
preinclude substituted one for zero and broke legal functions and members named
`aligned`. That macro, its header, and frontend/backend/install plumbing are now
removed. The expanded `rex_frontend_aligned_attribute_semantics.cpp` passes
translation and backend compilation with both legal identifiers.

The unmodified inherited CTest now fails at all three zero-alignment attributes.
The exact stock command `clang++ -std=gnu++14 -fsyntax-only test2013_71.C`
produces the same errors. Evidence under `build-audit/`:
`container-alignment-preinclude-removal-{build,ctest}.log` and
`container-alignment-zero-stock-clang.log`. Historical four-way controls remain
in `/mnt/probes/alignment336/`.

This is an unresolved compatibility blocker, not an accepted test exception or
regression-free result. No test was disabled or given a passing expectation,
and no diagnostic was suppressed. A supported zero-alignment language policy
remains necessary; recheck both controls on dependency changes.

## EXT-GNU-STD: GNU implicit standard namespace — regular regression fixed

`Cxx_tests_test2012_168_C` contains `namespace s = std;` without declaring
`std`. Stock GCC accepts it; stock Clang 22 rejects it with `expected namespace
name`. Reproduce with either compiler and `-std=gnu++17 -fsyntax-only` on
`tests/nonsmoke/functional/CompileTests/Cxx_tests/test2012_168.C`.

[Clang deliberately hides its implicit namespace from ordinary lookup](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Sema/SemaDeclCXX.cpp#L11230-L11244).
Calling `getOrCreateStdNamespace()` before parsing did not resolve the failure;
that trial was removed. Evidence: `build-audit/host-compiler-ctest-continue.log`
and `build-audit/host-gnu-namespace-ctest.log`.

In GNU C++ mode, frontend initialization defines `std` through Sema's
namespace-definition action before parsing, then marks the declaration implicit.
That makes it the first real `std` definition: visible to lookup, cached as the
standard namespace and known to typo correction. This implements GNU lookup
using typed compiler state, without a source preinclude.

Rechecked 2026-10-29: the earlier hand-built declaration skipped Sema's
known-namespace registration, so a later `namespace std` was only an extension
and typo correction lost its `std::` suggestions. The unchanged
`ElsaTestCases/notCompilable/gnu/g0023.cc` then omitted Clang's
`did you mean 'std::bad_alloc'?`.
AUD-967 corrects its accidental application to strict ISO C++: the construction
now requires Clang's typed `LangOptions::GNUMode`. The unchanged strict rejection,
inherited GNU alias and tuple/vector controls pass in
`build-audit/container-gnu-std-typed-language-mode-ctest-20261001.log`.
Recheck both language modes when the LLVM package or this integration changes.
The original diagnostic reproduced in `container-gnu-std-before-ctest.log`;
the build passed (`container-gnu-std-build.log`). Afterward, REX translation
succeeded, but stock Clang rejected the emitted alias because the implicit
namespace remains semantic-only (`container-gnu-std-after-ctest.log`). The tuple
redeclaration control passed. The requested vector test name matched no test
and supplies no evidence.

Visible implicit namespaces now get a generated lexical declaration through
the existing typed namespace builder. Preprocessing indexes only source-spelled
fragments; generated fragments have no physical source interval. The alias,
tuple and vector emission/compile tests passed, 3/3
(`container-gnu-std-fragment-ctest.log`). No test expectation or diagnostic was
suppressed.

JSON validation first exposed namespace-symbol
serialization using a null declaration instead of the alias basis. That is
fixed, and reconstruction now constructs alias symbols from their typed basis
and validates metadata instead of overwriting it. Required-fields validation
passed; the alias checkpoint then reported unsupported
`SgNamespaceAliasDeclarationStatement`
(`container-namespace-alias-json-ctest.log`). Schema 111 now serializes the
declaration's typed targets and constructs it through the dependency worklist.
The original alias and an alias-chain checkpoint pass
(`container-namespace-alias-declaration-ctest.log`); the updated schema contract
and missing-field rejection checks pass
(`container-namespace-alias-contract-ctest.log`). Full acceptance is pending.

## Historical mawk code-storage report (AUD-993/1059)

AUD-1059 removes this dependency from the statement-attribute roundtrip while
preserving every emitted-output check and final compilation. The resulting
Memcheck passes with zero unsuppressed errors/lost bytes and three existing
suppressed reports in one context; evidence:
`build-audit/container-typed-statement-attributes-memcheck-raw-20261002.log`.
This does not fix mawk's internal allocation or resolve AUD-993's separate
anonymous-record reports.

The archived failure reports 408 possibly lost bytes in one awk block. The
allocation stack is `realloc` → `zrealloc` →
`code_shrink` → `set_code` → `parse` → `main`, separate from the builtin arena
classified in EXT-MAWK-BUILTIN-ARENA. Do not broaden that rule to cover this stack.

Running the exact unchanged awk output check independently reproduces the same
408-byte report, one error/context, no suppression, and zero definite/indirect
loss. The exact allocation ownership and termination lifetime still need source
proof and caller-owned negative boundary checks. No exception is accepted.
Packages: mawk 1.3.4.20260302-1 and Valgrind 3.27.1-0.2; executable SHA-256
`094cdcbd065029bf296201180c2ca69fa4ce61b06f61d09d2c70a01800d99a80`.
Evidence is preserved in
`build-audit/container-statement-attributes-focused-memcheck-raw-20261001.log`
and `build-audit/container-statement-attributes-stock-awk-memcheck-20261001.log`.

## Open dependence memory reports (AUD-1079)

Schema-158 Mem29502 fails `rex_global_initialization_dependence` with 1,596
errors in 1,000 contexts, reaching Valgrind's reporting cutoff. Of the captured
use stacks, 998 include Clang parsing and two were unnamed. These stacks
alone do not establish the originating object/write defect. No exception is
accepted. The cutoff prevents accepting later REX memory coverage, despite
zero lost bytes, 40 reachable bytes and zero
suppressions. Exact object/write and lifetime evidence is still required.

AUD-1251 subsequently resolves both unnamed leaves with the exact Debian Clang
debug file and a same-process symbol/load-bias anchor. They are integer constant
evaluation in ExprConstant.cpp:18304/18305. GDB and Valgrind recognize the matched
debug file; the latter verifies its CRC. The stock first-error diagnostic now
includes Expr.cpp:2417 and the parser stack origin at ParseDeclCXX.cpp:2634.
This improves symbol/origin evidence without classifying missing writes or
semantic effects. No suppression or dependency patch is introduced.

AUD-1260 traces this exact first path in stock Clang and the rebuilt current
REX executable, without watchpoints or writes to client state. The `Pair`
constructor defines its flexible-array bit 23 while unused bit 22 remains
undefined. `RecordDecl::completeDefinition` masks the byte's low two bits,
increments it and stores it. The mask guarantees no carry into either upper
bit; the actual flexible-array flag stays false. Memcheck nevertheless changes
the shadow mask from `0x40` to `0xc0` at that increment. The same record then
enters `numStructUnionElements` with a defined member count of two; extracting
the flag taints the count, which remains two, before `reserveInits` reports it.
This identifies instrumentation precision loss for the first dependence
report, rather than an omitted semantic initializer.

Both first-error diagnostics still exit 99. No suppression, memory annotation
or dependency patch is added: the remaining contexts, leaks and later REX
memory behavior are unclassified. Do not reset Clang flags or infer that every
initializer report has this cause. Recheck the exact instruction sequence and
object/count chain after LLVM, package or Valgrind changes. Source manifest:
`cmake-local-scope-cleanup-source-20261005.sha256`; logs:
`container-clang22-record-write-{unwatched,rex}-{paused,gdb}-20261005.log`.

Graph and required-fields Memchecks pass with zero errors or suppressions;
Fortran passes with zero unsuppressed errors and 95 suppressed reports in nine
existing contexts under unchanged policy. This does not establish a full memory
gate. Source:
`build-audit/typed-data-utility-tools-copy-roles-source-20261002.sha256`.
Evidence: `build-audit/container-typed-data-utility-tools-copy-roles-memcheck-20261002.log`
and `build-audit/container-typed-data-utility-tools-global-initialization-memcheck-raw-20261002.log`.

## Open closure-specialization initializer reports (AUD-1037)

The strengthened existing copy fixture constructs a closure inside a class
template and calls two function specializations. All copy ownership, family,
signature and unchanged-original assertions finish under Memcheck after
AUD-1036; the test still fails on 173 frontend errors in 155 contexts. There
are zero lost bytes and 40 reachable bytes. The first reported use is
`clang::InitListExpr::reserveInits`, before REX translation. Its recorded heap
origin runs through Clang's AST allocation and `ParmVarDecl::Create`; that
arena allocation does not identify the precise later object or missing write.

Stock `/usr/bin/clang++-22 -fsyntax-only -std=c++17` on the identical fixture,
with the same existing suppression file and origin tracking, reproduces the
same initializer-vector use/allocation path: 152 errors in 134 contexts, zero
definite/indirect loss and 96 possibly lost bytes. This establishes an external
reproduction for that path, not a classification of every REX report or an
accepted exception. Do not suppress the remaining contexts, remove the
aggregate specimen, zero Clang storage or alter AST types to avoid this path.
Recheck with dependency changes; an exact object/write/lifetime analysis is
still required to decide whether a supported integration fix exists.

Packages remain Clang 22.1.8-1+b2 and Valgrind 3.27.1-0.2. Raw evidence:
`build-audit/container-copy-signature-dependencies-final-contract-memcheck-raw-20261001.log`
and `build-audit/container-copy-signature-dependencies-stock-clang-memcheck-raw-20261001.log`.
The reporting-only `-s` flag repeats the same 155 contexts at shutdown; CTest
counts 310 report blocks. The actual process summary is 173 errors/155 contexts.
Both representations remain failed, with no framework change or exclusion.

The final fixture additionally checks retention of an unchanged constant
specialization. Its current Memcheck still completes the copy assertions and
fails with 1,210 reported errors, reaching the 1,000-context reporting limit,
zero lost bytes and 40 reachable bytes. Evidence:
`build-audit/container-copy-signature-dependencies-semantic-contract-memcheck-raw-20261001.log`.
The preceding stock control used the archived two-call input
`build-audit/rex_copy_signature_origin_fixture_20261001.cpp` (SHA-256
`381ec38f2b133fb57565a5cb4fa9c74a7581686c3a5b1d2c36cb3dd50b33f56d`),
not this expanded fixture. Preserve that distinction; all current contexts
remain unclassified. No additional suppression or exception is accepted.

Before further diagnostics, reuse the earlier packed-increment investigation
above: read-only traces already connect `RecordDecl::completeDefinition` to an
undefined reserve count and allocator-cursor shadow propagation in other
specimens. The reduced increment/OR/negative controls and all supported
precision-setting experiments are recorded under
`build-audit/bitfield-precision-options/`. Repeating those experiments does not
classify this fixture's remaining contexts; no configuration workaround was
established.

## LLVM 22 array-alias template reconstruction — unresolved semantic defect

Stock Clang 22.1.8 changes `new A{value, value, value}`, with `A = int[3]`,
from `int *` in the template pattern to `int (*)[3]` in its `int` specialization.
The instantiated initializer becomes `int[3][3]`; the array-bound literal is
shared with the original pattern. The equivalent non-template function retains
`int *` and an `int[3]` initializer. The expected-result assertion fails only
for the template. Evidence: `container-new-array-alias-stock-witness-20261002.log`
and `container-new-array-alias-stock-direct-control-20261002.log` in `build-audit`.
AUD-1076 must establish typed provenance before rejecting this malformed case
without rejecting valid array-of-alias allocations. No reconstruction hack,
dependency patch, suppression, or accepted exception has been added.
