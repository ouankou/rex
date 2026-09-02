# REX Memory Tooling (Sanitizers, Valgrind, Memcheck)

This document is the single source of guidance for using sanitizers and Valgrind memcheck in REX. It covers how to configure, run, and interpret results, plus what to keep in mind when developing or triaging memory issues.

## When to use which tool

- Sanitizers (ASan/LSan/UBSan): Fast feedback during development, great for catching UAF, OOB, and UB.
- Valgrind memcheck: Slower but precise leak reporting and call stacks; good for leak triage and hard-to-repro bugs.

Use separate build directories for normal, sanitizer, and Valgrind builds to avoid flag conflicts.

`REX_ENABLE_UNINITIALIZED_FIELD_TESTS` defaults to `ON` only when CMake finds
both executable Valgrind and its development headers; otherwise it defaults to
`OFF`. `testUninitializedFields` uses Valgrind client requests as definedness
probes, which become no-ops outside Valgrind, but compiling the executable still
requires those headers. Explicitly selecting the suite with
`-DREX_ENABLE_UNINITIALIZED_FIELD_TESTS=ON` without complete Valgrind support is
a hard configuration error. Native source CI installs the dependency and
therefore keeps the availability-derived suite enabled. Standard nightly images
omit optional Valgrind tooling, so the suite remains disabled there without an
architecture switch. Finding Valgrind does not enable CTest MemCheck or its
extended timeouts; MemCheck is enabled only by an explicit `WITH-VALGRIND`
setting.

## Build and configure

### Normal build (no memory instrumentation)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

### Sanitizer build
Sanitizers require `libclang-cpp` (LLVM 22). Use `Debug` to keep `ROSE_ASSERT` active during memory checks. If `ROSE_SANITIZERS` is empty, the build defaults to `address;leak`.
```bash
cmake -S . -B build-sanitizer -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE-SANITIZER=ON -DROSE_SANITIZERS="address;leak;undefined"
cmake --build build-sanitizer -j"$(nproc)"
```

### Valgrind/memcheck build
Configure with Valgrind paths so CTest can drive memcheck. Use `Debug` so assertions stay enabled.
```bash
cmake -S . -B build-valgrind -DCMAKE_BUILD_TYPE=Debug -DWITH-VALGRIND=/usr
cmake --build build-valgrind -j"$(nproc)"
```

If Valgrind lives elsewhere, use:
- `-DWITH-VALGRIND=/path/to/prefix` or
- `-DWITH-VALGRIND-BIN=/path/to/bin -DWITH-VALGRIND-INCLUDE=/path/to/include -DWITH-VALGRIND-LIB=/path/to/lib`

## Running tests

### Standard tests
```bash
ctest --test-dir build --output-on-failure
```

### Sanitizer-labeled tests
```bash
ctest --test-dir build-sanitizer -L sanitizer -LE death -j"$(nproc)" --output-on-failure
```

### Valgrind memcheck (recommended subset first)
```bash
ctest --test-dir build-valgrind -T memcheck -R "<regex>" -LE death -j"$(nproc)" --output-on-failure
```

Use `ctest -N -R "<regex>"` to list tests before running memcheck.

## LSan and external runtime allocations

Some toolchain/runtime components allocate process-lifetime memory that LSan
cannot attribute cleanly. To keep sanitizer runs actionable, REX ships
`scripts/rex-suppressions-for-lsan`. Weekly sanitizer CI exports this via
`LSAN_OPTIONS`. If you run tests manually, set:
`LSAN_OPTIONS=suppressions=<path-to>/scripts/rex-suppressions-for-lsan`.

Weekly sanitizer CI scales both timeout layers by four. The
`ROSE_TEST_TIMEOUT_SCALE` environment variable controls REX's shell and Perl
test harnesses, while CTest's explicit 6,000-second limit covers direct test
executables that never enter those harnesses. Both boundaries remain hard
errors; the larger limit changes neither the selected tests nor their failure
handling.

The weekly complete Debug suite applies the same policy with a twofold scale:
1,800 seconds for REX harness commands and a 3,000-second CTest limit for
direct executables. This retains a finite failure boundary for every test while
covering the measured 14-minute outliner cases without depending on a
single-digit percentage timing margin.

## How memcheck works in REX

When configured with Valgrind, CTest uses the build’s memcheck settings:
- `CTEST_MEMORYCHECK_COMMAND` is set to the Valgrind binary.
- Options include leak checking, all leak kinds, `--error-exitcode=1`, and `--trace-children=yes`.
- Valgrind suppressions live in `scripts/rex-suppressions-for-valgrind`.

Because child tracing is enabled, helper processes (Perl, Python, shell scripts) can be checked too. This can surface non-ROSE leaks; those should be handled via suppressions when they are clearly external and process-lifetime only.

The weekly workflow runs the reviewed `astInterface|testQuery|rex` MemCheck
selection in eight shards.  Before executing a shard,
`scripts/run_ctest_name_set.py` expands it to the transitive CTest `DEPENDS`
and fixture closure.  A producer can therefore never land in a different job
from its selected consumer.  Required support tests may execute in more than
one shard.  The runner derives numeric selectors from CTest's MemCheck-mode
registry, after `CTEST_CUSTOM_MEMCHECK_IGNORE` is applied; normal-registry
indices are not valid in that filtered dashboard registry.  A single hosted
job was measured at only 895 of 1,749 tests when
GitHub enforced its six-hour limit. Registration-index residues are not used:
regularly generated test families occupy equally spaced index blocks and can
therefore cluster their most expensive variants in a single job. Stable name
hashing makes shard membership independent of CMake registration order. The
eight-way split preserves all direct coverage while keeping each independent
job below that hard ceiling, including the measured parser-heavy tests that
need more than three hours under Valgrind.
Each shard is capped at eight build and MemCheck workers.  Hosted runners
currently provide fewer CPUs, while the cap prevents large local `act`
matrices from multiplying host concurrency once per container.

Full libstdc++ map and regex integration specimens remain in regular,
sanitizer, and local full CTest, but an exact, reviewed set of their individual
REX-tool variants is excluded from scheduled Memcheck.  The regex variants
crossed the five-and-a-half-hour per-test boundary.  The map variants each
spent multiple hours in the same frontend traversal, and the containing
ACT shard exceeded GitHub's six-hour job boundary before completing.  Each
such boundary has a bounded replacement
using the same REX executable and AST operation: the lazy-system-header fixture
is a real Clang system header, the normalization fixtures exercise both
transformation drivers, and the nested template-argument frontend fixture
avoids unrelated standard-library input.
The complete AST-interface `<regex>` query also remains in regular and
sanitizer CTest; Memcheck runs the same query executable against an additional
bounded `basic_regex` instantiation.
`rex_weekly_memory_workflow_contract` hard-checks both the exclusion list and
the registered replacements.  Adding an exclusion without an equivalent
bounded instrumented test is an error.

The same rule applies to two older integration families whose useful operation
is buried behind unrelated frontend volume. `interfaceFunctionCoverage` over
standard-library headers took more than four hours under Valgrind. Its bounded
replacement runs that exact executable over templates, inheritance, loops,
switches, calls, and mutable expressions without library headers. The two
anonymous-tag move-declaration checks each transit roughly 12,000 preprocessed
lines through three serialized tool modes. Their bounded replacement runs the
same three modes and hard-checks both generated-source syntax and the absence
of leaked anonymous-tag identities. The original inputs remain mandatory in
regular, sanitizer, and local full CTest.

CTest also contains compiler-only contract fixtures whose primary command is
the exact C, C++, or Fortran compiler selected at configure time. The memcheck
wrapper recognizes only those canonical compiler paths and executes them
without Valgrind because no REX process exists in those tests. Compiler child
processes and test-harness utilities are excluded through the child-trace list.
All other primary commands remain instrumented. Do not add a program to either
boundary merely to make a report disappear: first prove that the process is
external to REX, and keep the boundary tied to the configured tool or an exact
harness utility.

Executable-script tests are also kept inside the instrumentation boundary.
The wrapper resolves a valid shebang to its real interpreter before starting
Valgrind, then passes the script as the interpreter's first source argument.
This preserves Linux shebang semantics while avoiding multicall dispatch based
on Valgrind's synthetic primary-process name. An `env` shebang must name one
interpreter without options or assignments; malformed or ambiguous shebangs
are hard errors rather than uninstrumented fallbacks. With child tracing
enabled, both the interpreter and every REX process launched by the script are
checked.

Nested executable scripts cross the same boundary after the primary process
starts. CTest MemCheck therefore hard-requires the standalone GNU
`/usr/bin/env`: unlike a multicall executable, it does not dispatch from the
synthetic `argv[0]` supplied by Valgrind. Ubuntu 26.04 selects uutils by
default, so the weekly job atomically replaces `coreutils-from-uutils` with the
distribution's `coreutils-from-gnu` alternative before configuration and then
verifies the implementation. CMake rejects an incompatible interpreter at
configure time. No script test is excluded or allowed to escape child tracing.

## Interpreting memcheck results

Memcheck failures can be either memory issues or functional failures:

- Functional failure: The test exits non-zero or hits `ROSE_ASSERT`, but Valgrind shows `ERROR SUMMARY: 0 errors`.
- Memory defect: Valgrind reports errors or leaks and CTest lists “defects.”

Always inspect `build-valgrind/Testing/Temporary/MemoryChecker.<#>.log` for the exact cause.

Death tests intentionally abort to validate hard invariants, which leaves “still reachable” memory behind. To keep memcheck and sanitizer runs actionable, exclude them with `-LE death` and keep them in the normal (non-memcheck) test suite.

### Leak kinds in Valgrind

- `definite`, `indirect`, `possible`: Almost always real leaks and must be fixed in ROSE code.
- `reachable` (aka “still reachable”): Memory that is still referenced at exit. This can be:
  - A real issue in ROSE (e.g., pools or caches not released after AST teardown).
  - External/runtime process-lifetime allocations (e.g., `/usr/bin/perl`, LLVM managed statics).

If “still reachable” comes from ROSE-owned structures, treat it as a real issue. If it is from external runtimes and cannot be cleaned up safely, suppress it explicitly in `scripts/rex-suppressions-for-valgrind`.

## Triage workflow in an actively developed tree

1. Run the tests normally (no memcheck). Fix functional failures first.
2. Run memcheck only on known-passing tests with `-R "<regex>"`.
3. Inspect memcheck logs to distinguish memory defects from functional failures.
4. Fix ROSE leaks at the root cause (prefer CFE fixes when the bug originates there).
5. Only add narrow suppressions for independently reproduced third-party or
   process-lifetime allocations outside ROSE. Anchor them to exact third-party
   frames and the REX integration boundary.
6. Keep all Valgrind suppressions in `scripts/rex-suppressions-for-valgrind` and require root-cause justification for each entry.

## Development guidelines for memory correctness

- Do not hide memory issues by “fixing” tests or weakening assertions.
- Use sanitizers for rapid feedback during development and memcheck for leak triage.

## Ownership boundaries (AST, attributes, non-AST)

### AST nodes (Sage/ROSE IR)
- AST nodes are owned by the AST and its memory pools; do not `delete` them directly.
- When replacing a subtree, use `SageInterface::replaceExpression` or `SageInterface::replaceStatement`.
  The original node becomes detached. For `replaceStatement`, you must delete the old node to avoid leaks.
  For `replaceExpression`, the old node is deleted by default; pass `keepOldExp=true` to take ownership and manage its deletion manually.
  To delete a detached subtree, use `SageInterface::deleteAST` (or its wrapper `SageInterface::deepDelete`).
  Note that this only deletes the AST nodes; you must handle any dangling symbols or types that result.
  Avoid raw `set_*` on child pointers unless you also delete the old subtree.
- If you intentionally detach nodes (set pointers to `NULL`), you must delete the detached subtree or transfer ownership to a well-defined owner.
- AST teardown is the final owner boundary; do not rely on process exit to clean ROSE-owned nodes.

### AstAttributeMechanism (attached attributes)
- Always implement `AstAttribute::getOwnershipPolicy()` in custom attributes.
  - `CONTAINER_OWNERSHIP`: container owns and deletes on replace/clear.
  - `NO_OWNERSHIP`: attribute is leaked and its memory is reclaimed only on process exit. This policy is not recommended.
  - `CUSTOM_OWNERSHIP`: attribute class must manage its own cleanup.
- Avoid `UNKNOWN_OWNERSHIP` (it is treated as a warning and often leaks).
- Do not store raw owning pointers inside attributes without RAII or explicit cleanup.

### Non-AST objects (utilities, frontends, tool glue)
- Prefer RAII (`std::unique_ptr`, `std::vector`, `std::string`) for owned objects.
- Raw pointers in APIs must be non-owning views; document who owns the lifetime.
- For C interop, keep allocation pairs explicit and matched (`malloc/free` or `new/delete`), and isolate the boundary in a small helper/RAII wrapper.
- Avoid global/process-lifetime caches unless you also provide explicit teardown.

### CI policy
- Sanitizer and memcheck runs are too heavy for per-commit gating; they run in weekly CI only.
- Use Debug builds for sanitizer/memcheck to keep `ROSE_ASSERT` active during memory triage.

## Common pitfalls

- Running memcheck in a non-Valgrind build does nothing (CTEST memorycheck is disabled).
- Mixing Valgrind and sanitizers in the same build is not supported; use separate build directories.
- Memcheck is slow; reduce the test set or run smaller subsets during iteration.
