# Codebase audit

This is the execution record for the whole-codebase cleanup and typed-AST audit.
The initial revision is `eceaca2ef5`. **The audit is in progress; neither the
inventory nor a passing subset of tests establishes completion.**

Run heavy work only in the existing `rex-audit-bounded` container, with one
build or plain CTest job at a time (`-j1`). Read `build-audit/RESOURCE_STOP.json`
and verify the container limits before restarting it. Do not create containers
or custom audit runners. Keep source unchanged during a build/test run and
preserve command output and CTest logs. Historical inventories and partial
passes are not acceptance. Current blockers are in [remaining-work.md](remaining-work.md).

The bounded container mounts `/tmp` with `noexec`. For CTest controls that compile
and execute temporary programs, set `TMPDIR` to the existing executable build
directory `/mnt/current-release/Testing/Temporary`. An execution-permission
failure does not validate compiler behavior; retain it and rerun only the
affected controls after correcting the environment.

## Required outcome

- Remove proven redundant, stale, dead, and dropped-component code and comments.
- Construct compiler-required semantics as typed AST data in the frontend.
- Remove semantic string recovery, guessed values, silent failures, and repairs
  of malformed frontend output after construction.
- Simplify CMake using compiler/package metadata, target properties, and focused
  capability checks without weakening configuration errors.
- Preserve the complete regular CTest registry and run every eligible test under
  ASan/LSan/UBSan and Memcheck.

Public API and AST JSON schema changes are permitted when required. Update all
consumers and document migration; do not preserve a workaround in a compatibility
shim. Valid supported input must remain supported. An assertion alone is not a
replacement for implementing its semantics correctly.

Identifiers, literal spellings, comments, macro tokens, and opaque directives may
remain text. Required semantics must be represented independently in typed nodes,
fields, and identities, not generic string-keyed attributes or reparsed output.
Recursive construction and forward-reference resolution remain private to
construction. Published ASTs must satisfy their invariants.

## Coverage and evidence

`coverage.json` records review scope and disposition; `findings.md` records
individual findings. Pending groups have not been cleared by a keyword search.
Inventory every tracked file, including recursive submodules, and record content
hashes so evidence cannot silently carry across changed source.

Inspect frontends, ROSETTA inputs, AST support, analyses, transformations,
unparsers, serialization, utilities, build/configuration/install machinery, CI,
scripts, tools, examples, tutorials, tests, documentation, and browser support.
Include unbuilt files and disabled branches. Keep legal notices, useful design
rationale, historical upstream-sync records, and test provenance. Dropped language
frontends do not imply dropping active Python scripts or JavaScript browser code.

Deletion requires checking callers, generated references, macros, registration,
virtual dispatch, installed/public interfaces, tests, and supported alternate
configurations. Coverage or lack of textual references alone is insufficient.
Edit generation inputs and regenerate normally. Follow nested AGENTS.md files;
ignore legacy Makefile.am and configure.ac as directed by the root instructions.

## External behavior policy

Use stock LLVM 22 by default. Carrying or upstreaming a dependency fork is not a
requirement. Apply this decision order:

1. Fix REX misuse, initialization, ownership, lifetime, or API usage directly.
2. Prefer a correct integration through documented APIs and typed boundary
   conversions. It must preserve semantics with and without instrumentation.
3. Permit a narrow external-report suppression only when evidence establishes
   its origin and that REX semantic and memory correctness are unaffected.
4. Leave correctness-affecting issues unresolved when no sound correction exists.
   Listing an issue does not make it an accepted exception.

Do not modify dependency internals, fabricate semantic values, suppress REX-owned
defects, or interpret every stack containing an external frame as an external
defect. Process-lifetime allocations and verified padding reports can qualify;
semantic undefined reads, use-after-free, unbounded leaks, and incorrect ASTs do
not qualify merely because they originate externally.

Prefer report-specific suppressions to skipping tests, disabling reporting around
calls, or marking entire objects defined. Legitimate allocator annotations must
describe actual initialized storage and cannot hide undefined semantic fields.

Every external-specific adapter and suppression needs a stable register ID and
an inline comment linking to `external-exceptions.md`. Record affected versions,
platforms and tooling; an independent reproducer where feasible; diagnostics;
origin/correctness evidence; alternatives considered; exact scope; evidence that
REX defects remain detectable; verification date; and removal/recheck conditions.
An upstream reference is useful but not required.

Existing exceptions receive the same audit as new ones. Re-run focused
reproducers without their exceptions at audit milestones and whenever dependency,
instrumentation, or integration versions change. Remove obsolete exceptions and
investigate changed signatures before changing scope. A diagnostic-only local
dependency patch does not authorize shipping a permanently patched toolchain.

## Validation

### Resource ceilings

Total audit RAM must remain below **48,000,000,000 bytes**. The existing
container must retain its 24 GiB memory limit, equal memory-plus-swap limit
(no swap), four-CPU limit, 512-process limit and disabled restart policy.
These limits do not authorize concurrent configurations or jobs.

The complete audit must stay below **200 GB (200,000,000,000 bytes)** of
task-owned storage, including build products, snapshots, caches, temporary
containers and toolchain images. Do not accumulate numbered build trees or
archives of superseded builds.

The resumed audit uses one existing LLVM 22 image and one container with a
read-only root filesystem. All builds, temporary compiler files, installed
consumers and detailed logs reside in a loop-mounted ext4 filesystem backed by
`build-audit/workspace.ext4`, whose fixed maximum size is 180,000,000,000 bytes.
The container's temporary mount is limited to 1,000,000,000 bytes and its Docker
log to 1 MB. The remaining allowance covers the existing 6.3 GiB host build,
the toolchain image and source/evidence metadata. No additional image or build
archive may be created without first reclaiming its space within this ceiling.
Monitor allocated bytes and filesystem free space before each build/test batch;
stop and reclaim obsolete products before the bounded filesystem fills. After
clean rebuilds, trim unused blocks in the mounted audit filesystem so deleted
objects also release physical backing-file storage.
Disable core dumps for audit commands, including deliberate assertion tests, so
the host crash collector cannot accumulate dumps outside the capped workspace.
An exhausted or interrupted run is a failed/incomplete attempt, never a pass.

Reuse one source snapshot and a fixed set of configuration directories. Before
replacing source contents, finish or stop every consumer, retain compact results,
and follow the clean-rebuild provenance requirement below. Retain manifests,
registries, final statuses and relevant raw diagnostics, rather than archiving
compiler objects, SDK copies or complete obsolete build trees. Unmount the audit
filesystem before removing its backing file.

Older guard listings containing only `: OK` lines are retained as `.log.gz`.
The archive verifies every decompressed SHA-256 before removing plaintext;
`build-audit/old-guard-archive-originals-20261012.sha256` retains original names
and digests. Current evidence and listings with other content remain plaintext.
Compression does not change any run's pass, failure or interrupted status.

On 2026-09-07 the user requested immediate cleanup after earlier runs accumulated
approximately 675 GB of artifacts. Those generated trees and archives were
deleted, together with 439 stopped audit containers and six disposable images.
Historical paths in earlier reports are no longer available unless their compact
results were retained in `build-audit/cleanup-results`. The interrupted runs are
not acceptance evidence for the current source.

### Build and test requirements

Use distinct Debug regular, sanitizer, and Memcheck builds, with C/C++ and Flang
enabled. Install executable Valgrind and its development headers and explicitly
enable `REX_ENABLE_UNINITIALIZED_FIELD_TESTS`. Capture toolchain versions, source
and submodule revisions, CMake configuration, exception hashes, and complete
CTest JSON registries before and after a batch.

A source snapshot transition requires a fresh build directory or an explicit
`cmake --build <dir> --clean-first` rebuild. Rebinding a frozen source at an
existing absolute path is insufficient: the previous build may have produced
objects after the new source's preserved mtime. Capture the clean build command
and completion status with each snapshot. A manifest proves source contents,
not that an incremental binary was compiled from those contents. Disable compiler
caching for the final acceptance rebuilds so their evidence is independent of
both timestamp scheduling and earlier compiler-cache artifacts.
Use plain commands inside the bounded container, recording their output and
actual exit status. For the final acceptance rebuild, using `build` as the
configured build directory:

```bash
CCACHE_DISABLE=1 cmake --build build --clean-first -j1
ctest --test-dir build --output-on-failure --no-tests=error -j1
```

Verify the configured source root and preserve the configuration, build log,
exit status and CTest results. A successful build is not a test pass.
Check the selected test count against the intended set; zero selected tests are
invalid evidence even when CTest returns success.

The sanitizer matrix must configure
`-DENABLE-SANITIZER=ON -DROSE_SANITIZERS="address;leak;undefined"` and
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`. Verify instrumentation in both the cache
and actual C/C++ compile commands. Runtime options such as `UBSAN_OPTIONS` do
not enable instrumentation. The default ASan/LSan profile alone does not satisfy
this matrix. Local logs and build products live under ignored `build-audit/`.

| Build | Required acceptance |
| --- | --- |
| Debug regular | Entire registry, including death and uninitialized-field tests |
| Debug ASan/LSan/UBSan | Entire eligible registry, reviewed death exclusions, registered external suppressions |
| Debug Memcheck | Entire eligible registry after reviewed exclusions; REX and child tracing remain instrumented |
| Optimized native | Full regular suite at final acceptance |
| Portability | Existing architecture and WebAssembly gates plus affected configuration contracts |

Use the registry of each actual configuration and verify that the complete
required set executed successfully. Preserve test dependencies and fixtures.
The narrow weekly memory regex is not audit acceptance.

Keep reviewed timeout and error policies. Do not add exclusions, weaken
assertions, change expected failures,
filter results, or reduce instrumentation to obtain passing reports. Existing
test exclusions must be reviewed and removed when obsolete. New *report*
suppressions require the external-exception evidence above.

For every semantic change, test the AST immediately after construction, unrelated
identifiers/equivalent spellings, applicable language edge cases, copying,
transformation, serialization, unparsing/recompilation, repeated parsing, failure
cleanup, and complete teardown. Include runtime checks when syntax cannot prove
correctness. Preserve specimens and expectations. New tests belong in nonsmoke
and use `rex_<area>_<behavior>.<ext>` names.

Replace implementation-string configuration assertions only with executable
contracts proving the same or stronger behavior, including invalid configuration
cases. Inspect raw memory reports, not only process exit codes. Acceptance means
no unexplained reports, no suppressed REX defects, and no unregistered or
unjustified external exceptions.

A zero command status alone is insufficient: every required test must have a
successful fresh result. Missing, partial, failed, or skipped required results
reject the run. Preserve raw CTest reports with CI artifacts; a stopped shell's
saved status cannot establish completion.

CTest's Memcheck defect table does not by itself make a successful test command
fail. Use the configured wrapper's continuous report stream and immediate
Valgrind error markers. The wrapper propagates child errors even when their
parent returns zero, and retains errors across exec with or without a final
summary. PID-based filenames alone are insufficient: exec reuses the PID and
truncates the earlier file. Preserve the complete CTest report; never infer a
clean child from a clean parent summary. AUD-038 records the counterexamples.
The post-Memcheck hook independently rejects dashboard defect counts, because
CTest's output-matching properties can override a command's nonzero exit.
Archive a completed dashboard before starting another Memcheck selection or
requesting a Memcheck-mode inventory in the same build: CTest can replace its
`DynamicAnalysis.xml` even for a show-only inventory. Failed and incomplete
attempts remain failed or incomplete evidence.

## Delivery and completion

Work in reviewable batches: baseline/inventory, trustworthy instrumentation and
external integration, typed frontend construction and lifecycle, repair-path
removal, CMake simplification, then remaining subsystem cleanup. Targeted tests
support iteration. Do not repeat full suites while known blockers remain.
Finish the root-fix batch and source review before freezing source for final
gates. Repeat a gate only for a relevant fix, a specific failure investigation,
or demonstrably invalid earlier evidence. Final acceptance requires all gates
on the same source state.

Every batch records finding IDs, root cause, behavior changes, removed paths,
API/schema migration, exception references, and validation. Completion requires
all coverage groups and findings resolved, no semantic workaround or concealed
REX defect, current evidence for every accepted external exception, no lost test
coverage, and passing clean final builds in the required matrix. Quantify removed
files/code/comments/APIs/probes and added semantic coverage. Pending correctness
findings or unavailable required validation keep the audit incomplete.
