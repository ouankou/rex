# Sage AST JSON Refinement Plan

Last updated: 2026-06-30

This is the active plan for hardening the REX-side Sage AST JSON checkpoint
tool. It is intentionally separate from the older UPIR migration planning. The
scope here is the serializer/deserializer, Sage AST reconstruction, checkpoint
replacement, and regression coverage in REX.

## Goal

Keep the AST JSON checkpoint feature as a strict, text-file-based interchange
path for one complete `SgSourceFile` subtree. At any supported checkpoint:

1. serialize the active Sage AST to JSON;
2. read the JSON back;
3. reconstruct a fresh Sage AST;
4. replace the original source file in the active `SgProject`;
5. continue the normal compiler path with behavior matching the original AST.

The JSON contract must be complete for the supported surface. Missing required
state is a bug in the schema or serializer/deserializer, not something later
passes should repair.

## Non-Negotiable Rules

- Do not weaken assertions or validation checks to make tests pass.
- Do not add fallback AST repairs after JSON import.
- Do not serialize required Sage relationships as unstructured pointer text.
- Do not modify tests or expected output to hide compiler failures.
- Do not keep a non-tool source change without targeted `rex_test2026_*`
  coverage unless the change is only build/test wiring.
- New REX-originated C++ test inputs use `.cpp`.
- New targeted test names and CTest names include `rex_test2026_*`.

## Implementation Layout

The public API is:

- `sageAstJson.h`

The implementation is split into conventional compiled translation units with
one private, non-installed header:

- `sageAstJsonPrivate.h`: shared private records, RAII guards, marker
  attributes, constants, and internal function declarations.
- `sageAstJsonApi.cpp`: public entry points and command-line/environment
  option handling.
- `sageAstJsonFormat.cpp`: JSON parsing, string escaping, field writing, and
  file-id map serialization.
- `sageAstJsonSerializeSupport.cpp`: schema constants, internal attributes,
  type text preservation, source-position helpers, and symbol/reference
  serialization helpers.
- `sageAstJsonCollect.cpp`: deterministic node collection, auxiliary node
  collection, external boundary discovery, and structural validation helpers.
- `sageAstJsonSerializeExternal.cpp`: external declaration and identity record
  serialization.
- `sageAstJsonSerializeNodes.cpp`: node property writing, edge writing,
  metadata writing, and whole source-file JSON construction.
- `sageAstJsonProject.cpp`: project/file replacement, stale state purging,
  output-path handling, atomic writes, and round-trip comparison artifacts.
- `sageAstJsonDeserializeTypes.cpp`: type construction, source-position
  restoration, early type construction, external record reading, and external
  declaration validation.
- `sageAstJsonDeserializeNodes.cpp`: node allocation, property reading, edge
  restoration, and source-file reconstruction.
- `sageAstJsonDeserializeSymbols.cpp`: symbol-table reconstruction, duplicate
  lookup preference ordering, detached symbol reconstruction, and symbol
  validation.
- `sageAstJsonDeserializeFinalize.cpp`: final parent/scope/type/source-file
  restoration and round-trip validation.

There are no `.inc` implementation fragments or duplicate-`Json` source names.
The JSON formatting/parsing implementation lives in `sageAstJsonFormat.cpp`.
CMake compiles each `.cpp` above as a normal translation unit.

## Checkpoint Coverage

The supported checkpoint names are:

- `pre-omp-construction`
- `post-omp-construction`
- `post-omp-lowering`
- `all`

Coverage must prove that, for each checkpoint, original AST A and reconstructed
AST B can separately complete the remaining compiler path and produce matching
observable output. OpenMP GPU/offloading examples are part of the required
coverage because they exercise the long-term interchange surface most heavily.

## Targeted Coverage For Non-Tool Changes

Status: complete for the currently known non-tool source changes.

Each non-tool change below has targeted coverage so the behavior is tested even
when AST JSON is not enabled. Status is done unless stated otherwise.

- Optional OpenMP array-section bounds:
  `SgSubscriptExpression.C`.
  Coverage: `OMPTEST_rex_test2026_array_section_optional_bounds_cpp` and
  checkpoint variant.
- `uses_allocators` optional edges and per-entry isolation:
  `node.C`, `ompAstConstruction.cpp`.
  Coverage: `OMPTEST_rex_test2026_uses_allocators_optional_edges_c`,
  `OMPTEST_rex_test2026_uses_allocators_user_defined_isolation_c`, and
  checkpoint variants.
- Declare mapper typedef/template unparse:
  `unparseCxx_statements.C`.
  Coverage: `OMPTEST_rex_test2026_mapper_typedef_unparse_cpp`,
  `OMPTEST_rex_test2026_mapper_template_reachable_scope_cpp`, and checkpoint
  variants.
- Anonymous member name qualification:
  `clang-frontend-decl.cpp`, `nameQualificationSupport.C`,
  `sageAstJsonSerializeSupport.cpp`.
  Coverage: `Cxx_tests_rex_test2026_anonymous_member_no_qualification_unparse`.
- Template instantiation reachable scope:
  `clang-frontend-decl.cpp`, `clang-frontend-type.cpp`, `clang-frontend.cpp`.
  Coverage: `Cxx_tests_rex_test2026_template_instantiation_reachable_scope`,
  `Cxx_tests_rex_test2026_declaration_scope_structural_successor`, and
  existing issue-203 coverage.
- REX AST JSON option filtering before Clang cc1:
  `clang-frontend.cpp`.
  Coverage: `OMPTEST_rex_test2026_ast_json_clang_option_filter_cpp`.
- Detached source-file type-fixup guard:
  `fixupTypes.C`.
  Coverage: `ASTJSON_rex_test2026_detached_file_type_fixup`.
- OpenMP expression lookup current-file filtering:
  `omp_exprparser_parser.yy`.
  Coverage: `OMPTEST_rex_test2026_omp_expr_lookup_current_file_cpp`.
- Symbol-table rebuild completeness:
  `sageInterface.C`, `AstConsistencyTests.C`.
  Coverage: `astSymbolTable_rex_test2026_rebuild_symbol_table_coverage`.
- Deterministic outlining variable-symbol ordering:
  `ASTtools.hh`, `CollectVars.cc`, `VarSym.cc`, `omp_lowering.*`.
  Coverage: `omp_lowering_rex_test2026_deterministic_capture_order_cpp`.
- Deterministic OpenMP root lowering order:
  `omp_lowering.cpp`.
  Coverage: `omp_lowering_rex_test2026_deterministic_root_order_cpp` and
  `ASTJSON_omp_lowering_rex_test2026_deterministic_root_order_cpp`.
- Clang frontend Valgrind-defined reads:
  `clang-frontend-decl.cpp`, `clang-frontend-stmt.cpp`,
  `clang-frontend.cpp`, `clang-frontend-valgrind.cpp`.
  Coverage: `rex_test2026_*_valgrind_defined_reads*` tests and original
  Memcheck reproducers.
- Clang `FunctionDecl::getDefinition()` Valgrind-defined reads:
  `clang-frontend-decl.cpp`.
  Coverage: `Cxx11_tests_rex_test2026_function_definition_valgrind_defined_reads_cpp`
  and focused CTest Memcheck.
- Template qualifier and friend TypeLoc Valgrind-defined reads:
  `clang-frontend-decl.cpp`.
  Coverage:
  `Cxx11_tests_rex_test2026_template_qualifier_friend_valgrind_defined_reads_cpp`,
  focused CTest Memcheck, and original `stl_eval_cpp11_memory` reproducer.
- Compound-literal TypeLoc Valgrind-defined reads:
  `clang-frontend-stmt.cpp`.
  Coverage: `C_tests_rex_test2026_compound_literal_typeloc_valgrind_defined_reads_c`
  and original `C_tests_test2015_142_c` reproducer.
- Explicit leading/trailing token-stream output:
  `tokenStreamMapping.C`.
  Coverage:
  `tokenStreamMapping_rex_test2026_token_stream_explicit_leading_trailing_output_c`
  and companion entries.
- Template-parameter surface copying and split default arguments:
  `clang-frontend-decl.cpp`.
  Coverage: redeclaration/default-template-argument tests plus focused
  move-declaration Memcheck and sanitizer slices.
- C++11 STL Valgrind timeout isolation:
  `STL_tests/CMakeLists.txt`, `STL_tests/stl-eval.sh`.
  Coverage: `stl_eval_cpp11_locale` focused CTest Memcheck.
- Extended type-trait Memcheck timeout:
  `typeTraitTests/CMakeLists.txt`.
  Coverage: full Memcheck completed with only
  `type_trait_without_ret_roseHeader` timing out under the old 7200-second
  metadata; regenerated CTest metadata now sets the instrumented timeout to
  14400 seconds.

If another non-tool source change is needed while fixing validation failures,
add a targeted `rex_test2026_*` test in this section before considering the fix
complete.

## Current Validation Status

Regular build, sanitizer build, and Memcheck build reconfigure cleanly on the
current split implementation. JSON formatting lives in
`sageAstJsonFormat.cpp`; there are no duplicate-format implementation files or
`.inc` implementation fragments in the AST JSON build surface.

Current full regular CTest is green:

```text
ctest --test-dir build-ast-json --output-on-failure -j80
35360/35360 tests passed
Total Test time (real) = 2128.04 sec
```

Current broad OpenMP/OpenACC/OpenMP-lowering checkpoint coverage is green:

```text
REX_AST_JSON_CHECKPOINT=all \
REX_AST_JSON_DIR=$PWD/build-ast-json/test-output/ast-json-broad-after-template-qualifier-fix \
ctest --test-dir build-ast-json \
  -L 'OMPTEST|OMPACCTEST|OMPLOWERING|ASTJSON' \
  --output-on-failure -j80
1152/1152 tests passed
Total Test time (real) = 159.77 sec
```

Current full sanitizer CTest is green, with no ASan, LSan, or UBSan markers:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-ast-json-sanitizer --output-on-failure -j80
35360/35360 tests passed
Total Test time (real) = 5932.28 sec
```

Post-fix focused validation is green on the current tree:

```text
cmake --build build-ast-json -j80
cmake --build build-ast-json-memcheck -j80
cmake --build build-ast-json-sanitizer -j80

ctest --test-dir build-ast-json \
  -R '^Cxx11_tests_rex_test2026_(atomic_expr|function_definition|template_qualifier_friend)_valgrind_defined_reads_cpp$' \
  --output-on-failure -j3
3/3 tests passed
Total Test time (real) = 12.29 sec

ctest --test-dir build-ast-json-memcheck -T memcheck \
  -R 'rex_test2026_.*_valgrind_defined_reads_cpp$|^stl_eval_cpp11_memory$' \
  --output-on-failure -j4
4/4 MemCheck tests passed
Total Test time (real) = 1135.44 sec

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-ast-json-sanitizer \
  -R '^Cxx11_tests_rex_test2026_(atomic_expr|function_definition|template_qualifier_friend)_valgrind_defined_reads_cpp$' \
  --output-on-failure -j3
3/3 tests passed
Total Test time (real) = 17.76 sec
```

The previous full Memcheck run exposed two timeout failures in the aggregate
C++11 STL checks:

```text
ctest --test-dir build-ast-json-memcheck -T memcheck --output-on-failure -j80
stl_eval_cpp11       ***Timeout 21600.11 sec
stl_eval_no_cleanup  ***Timeout 21600.08 sec
```

The same run also exposed a Valgrind undefined-read report in
`clang::FunctionDecl::isDefined()` reached from
`isSystemOrBuiltinFunctionDecl()`. That boundary is now fixed by reading
`FunctionDecl::getDefinition()`, `getPrimaryTemplate()`,
`getTemplatedDecl()`, and related function-declaration locations through the
existing Clang API definedness wrapper.

The STL timeout root cause is addressed by splitting C++11 STL aggregate checks
into per-header tests only when `REX_EXTENDED_TEST_TIMEOUTS=ON` (the Memcheck
configuration). Regular and sanitizer configurations keep the aggregate
`stl_eval_cpp11` and `stl_eval_no_cleanup` tests. Memcheck now has 64
per-header C++11/no-cleanup STL tests. The heaviest known reproducer is green:

```text
ctest --test-dir build-ast-json-memcheck -T memcheck \
  -R '^stl_eval_cpp11_locale$' \
  --output-on-failure -j1
1/1 MemCheck tests passed
Total Test time (real) = 2233.57 sec
```

The full split Memcheck rerun completed. The only failure was a timeout in
`type_trait_without_ret_roseHeader`; no Valgrind memory error markers or other
compiler/test failures were reported:

```text
ctest --test-dir build-ast-json-memcheck -T memcheck \
  --output-on-failure -j80
35397/35398 MemCheck tests passed
30939 - type_trait_without_ret_roseHeader (Timeout)
astQuery_test3_cxx_grammar Passed 17470.46 sec
Total Test time (real) = 209867.87 sec
```

This was a stale test-metadata timeout, not a compiler failure. The timeout for
`type_trait_without_ret_roseHeader` is now 14400 seconds when
`REX_EXTENDED_TEST_TIMEOUTS=ON`; regular non-instrumented runs keep the
600-second timeout. After reconfiguring `build-ast-json-memcheck`, generated
CTest metadata contains:

```text
set_tests_properties(type_trait_without_ret_roseHeader PROPERTIES
  RUN_SERIAL "TRUE" TIMEOUT "14400")
```

Per user direction, the full Memcheck suite was not rerun after this
metadata-only change.

## Required Gates

Run these gates after source changes that affect AST shape, AST JSON, OpenMP
construction/lowering, frontend ownership, or unparse behavior:

```text
cmake --build build-ast-json -j$(nproc)
ctest --test-dir build-ast-json -R '^ASTJSON_' --output-on-failure -j$(nproc)
ctest --test-dir build-ast-json -R 'rex_test2026|^ASTJSON_' --output-on-failure -j$(nproc)
```

Run broad OpenMP/OpenACC/OpenMP-lowering checkpoint coverage:

```text
rm -rf build-ast-json/test-output/ast-json-broad
REX_AST_JSON_CHECKPOINT=all \
REX_AST_JSON_DIR=$PWD/build-ast-json/test-output/ast-json-broad \
ctest --test-dir build-ast-json \
  -L 'OMPTEST|OMPACCTEST|OMPLOWERING|ASTJSON' \
  --output-on-failure -j$(nproc)
```

Run full regular CTest:

```text
ctest --test-dir build-ast-json --output-on-failure -j$(nproc)
```

Run focused and full Memcheck:

```text
ctest --test-dir build-ast-json-memcheck \
  -T memcheck \
  -R '^ASTJSON_|rex_test2026' \
  --output-on-failure -j1

ctest --test-dir build-ast-json-memcheck \
  -T memcheck \
  --output-on-failure -j$(nproc)
```

Run focused and full sanitizer CTest:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-ast-json-sanitizer \
  -R '^ASTJSON_|rex_test2026' \
  --output-on-failure -j$(nproc)

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-ast-json-sanitizer \
  --output-on-failure -j$(nproc)
```

## Failure Triage

For every regular, Memcheck, or sanitizer failure:

1. Reproduce with the smallest CTest regex or direct command.
2. Preserve AST JSON mismatch artifacts before cleanup.
3. Classify the root cause as missing JSON contract state, wrong reconstructed
   ownership, pre-existing AST-shape bug, nondeterministic ordering, or memory
   lifetime bug.
4. Add or extend a targeted `rex_test2026_*` test unless one already fails.
5. Fix the root cause in compiler or AST JSON code.
6. Rerun the targeted test, the focused gate, and the relevant full gate.

Timeout-only failures still require root-cause analysis. If a test is valid but
too expensive under Valgrind, adjust the shared harness or shared test metadata
so the same behavior remains checked. Do not add local one-off skips or xfails.

## Definition Of Done

This refinement is complete only when:

- every non-tool source change has targeted `rex_test2026_*` coverage;
- the AST JSON implementation remains split into logical `.cpp` files;
- no `.inc` implementation fragments or odd duplicate names return;
- `README.md` documents the schema, failure mode, and ownership model;
- `git diff --check` is clean;
- focused AST JSON and `rex_test2026` gates pass;
- broad OpenMP checkpoint coverage passes;
- full regular CTest passes;
- focused and full Memcheck pass;
- focused and full sanitizer CTest pass;
- no tests are weakened, skipped, or rewritten to hide failures;
- all fixes are root-cause fixes with no workaround/fallback debt.
