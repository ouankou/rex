# REX Memory Tooling (Sanitizers, Valgrind, Memcheck)

This document is the single source of guidance for using sanitizers and Valgrind memcheck in REX. It covers how to configure, run, and interpret results, plus what to keep in mind when developing or triaging memory issues.

## When to use which tool

- Sanitizers (ASan/LSan/UBSan): Fast feedback during development, great for catching UAF, OOB, and UB.
- Valgrind memcheck: Slower but precise leak reporting and call stacks; good for leak triage and hard-to-repro bugs.

Use separate build directories for normal, sanitizer, and Valgrind builds to avoid flag conflicts.

## Build and configure

### Normal build (no memory instrumentation)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

### Sanitizer build
Sanitizers require `libclang-cpp` (LLVM 20). Use `Debug` to keep `ROSE_ASSERT` active during memory checks. If `ROSE_SANITIZERS` is empty, the build defaults to `address;leak`.
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

## LSan and the embedded JVM (OFP)

When Fortran OFP support is enabled, the embedded JVM allocates process-lifetime
memory that LSan cannot attribute correctly. To keep sanitizer runs actionable,
REX ships `scripts/rose-suppressions-for-lsan` and CTest applies it automatically
when `ENABLE-SANITIZER` includes `leak`. If you run tests manually, set:
`LSAN_OPTIONS=suppressions=<path-to>/scripts/rose-suppressions-for-lsan`.

## How memcheck works in REX

When configured with Valgrind, CTest uses the build’s memcheck settings:
- `CTEST_MEMORYCHECK_COMMAND` is set to the Valgrind binary.
- Options include leak checking, all leak kinds, `--error-exitcode=1`, and `--trace-children=yes`.
- Suppressions live in `scripts/rose-suppressions-for-valgrind`.

Because child tracing is enabled, helper processes (Perl, Python, shell scripts) can be checked too. This can surface non-ROSE leaks; those should be handled via suppressions when they are clearly external and process-lifetime only.

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
  - External/runtime process-lifetime allocations (e.g., `/usr/bin/perl`, JVM, LLVM managed statics).

If “still reachable” comes from ROSE-owned structures, treat it as a real issue. If it is from external runtimes and cannot be cleaned up safely, suppress it explicitly in `scripts/rose-suppressions-for-valgrind`.

## Triage workflow in an actively developed tree

1. Run the tests normally (no memcheck). Fix functional failures first.
2. Run memcheck only on known-passing tests with `-R "<regex>"`.
3. Inspect memcheck logs to distinguish memory defects from functional failures.
4. Fix ROSE leaks at the root cause (prefer CFE fixes when the bug originates there).
5. Only add suppressions for third-party or process-lifetime allocations outside ROSE.

## Development guidelines for memory correctness

- Keep ownership explicit and use RAII (`std::unique_ptr`, `std::vector`, `std::string`) for non-AST objects.
- Avoid long-lived caches that survive AST teardown unless you also provide explicit cleanup.
- Do not hide memory issues by “fixing” tests or weakening assertions.
- Use sanitizers for rapid feedback during development and memcheck for leak triage.

## Ownership boundaries (AST, attributes, non-AST)

### AST nodes (Sage/ROSE IR)
- AST nodes are owned by the AST and its memory pools; do not `delete` them directly.
- When replacing or detaching a subtree, use ownership-aware helpers:
  - `SageInterface::replaceExpression`, `SageInterface::replaceStatement`, or `SageInterface::deepDelete/deleteAST` for detached nodes.
  - Avoid raw `set_*` on child pointers unless you also delete the old subtree.
- If you intentionally detach nodes (set pointers to `NULL`), you must delete the detached subtree or transfer ownership to a well-defined owner.
- AST teardown is the final owner boundary; do not rely on process exit to clean ROSE-owned nodes.

### AstAttributeMechanism (attached attributes)
- Always implement `AstAttribute::getOwnershipPolicy()` in custom attributes.
  - `CONTAINER_OWNERSHIP`: container owns and deletes on replace/clear.
  - `NO_OWNERSHIP`: attribute is intentionally leaked; do not delete.
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
