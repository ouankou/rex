# AXPY End-to-End Runbook with Clang/Flang

This note captures the steps, roadblocks, and fixes required to get a pure C++
AXPY example working end-to-end with the experimental Clang frontend in REX.

## Toolchain & Build

- Configured a fresh `build-clang` tree with `clang-20`, `clang++-20`, and
  `flang-20` as the backend compilers:
  ```sh
  cmake -S . -B build-clang \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=/usr/lib/llvm-20/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/lib/llvm-20/bin/clang++ \
    -DCMAKE_Fortran_COMPILER=/usr/lib/llvm-20/bin/flang \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -Denable-c=ON -Denable-fortran=ON
  cmake --build build-clang -j$(nproc)
  ```
- All invocations of `rose-compiler` require `LD_LIBRARY_PATH` to point at the
  freshly built `build-clang/lib` directory.

## Source Example

- Added `tests/nonsmoke/functional/input_codes/axpy.cpp` as a C++ example that
  now exercises common standard headers (`<array>`, `<numeric>`, `<cmath>`) to
  validate language default initialisation.
- The program initialises two fixed-size buffers with `std::array`, performs the
  AXPY loop, and returns `0`/`1` based on a relative-error check (no runtime I/O
  needed).

## Frontend Issues & Fixes

1. **Missing language defaults for C++**  
   - *Symptom*: Including even `<cmath>` produced thousands of errors like
     `unknown type name 'bool'` and missing `__builtin_*` intrinsics.  
   - *Root cause*: We were never initialising `clang::LangOptions` with C++
     defaults; the invocation inherited C-like settings from
     `CompilerInvocation::CreateFromArgs`.  
   - *Fix*: Call `clang::LangOptions::setLangDefaults` with the host triple and
     `lang_gnucxx17` (see `src/frontend/CxxFrontend/Clang/clang-frontend.cpp:268`).
     We collect any implicit include paths returned by that helper and append
     them back into the active `PreprocessorOptions`.

2. **Unhandled `LinkageSpecDecl` (extern "C"/"C++")**  
   - *Symptom*: Parsing headers that contain `extern "C"` blocks aborted with
     `Unknown declaration kind: LinkageSpec !`.  
   - *Fix*: Added a traversal case for `clang::Decl::LinkageSpec` that recursively
     visits contained declarations and reuses the current scope when appending
     the resulting `SgDeclarationStatement` nodes
     (`src/frontend/CxxFrontend/Clang/clang-frontend-decl.cpp:666`).
   - *Limitation*: We currently do not emit dedicated `SgClinkage*` nodes, so
     linkage metadata is dropped, but this is sufficient for ingestion.

3. **Anonymous namespace handling**  
   - *Symptom*: The translator asserted inside
     `buildDefiningFunctionDeclaration_T` when processing a file that declared
     helper routines inside `namespace { ... }`.  
   - *Workaround*: Keep helper routines at global scope and mark them `static`.
     This sidesteps unimplemented namespace support in the current frontend.
4. **Duplicate scope symbols triggering name-qualification warnings**
   - *Symptom*: `nameQualificationSupport` warned on almost every reference to
     block-local variables (notably `for`-loop indices).
   - *Root cause*: The Clang bridge manually pushed a `SgVariableSymbol` into the
     current scope after calling `SageBuilder::buildVariableDeclaration_nfi`, but
     the builder already inserts the symbol. This double insertion inflated the
     scope's symbol table and fooled the unparser into thinking multiple
     declarations existed.
   - *Fix*: Drop the extra `insert_symbol` and rely on `SageBuilder` to manage the
     symbol table. The unparser now uses the scope counts directly with no
     custom filtering, so only real ambiguities surface.

## Validation Flow

1. Run the translator to generate `rose_axpy.cpp`:
   ```sh
   LD_LIBRARY_PATH=$PWD/build-clang/lib \
     ./build-clang/bin/rose-compiler tests/nonsmoke/functional/input_codes/axpy.cpp
   ```
   Output appears at the repository root.
2. Verify both the original and generated sources compile and execute cleanly:
   ```sh
   clang++ tests/nonsmoke/functional/input_codes/axpy.cpp -O2 -o /tmp/axpy_orig
   clang++ rose_axpy.cpp -O2 -o /tmp/axpy_rose
   /tmp/axpy_orig && /tmp/axpy_rose   # both exit 0
   ```

## Limitations & Next Steps

- **Future hardening**: it is still worthwhile to tighten the Clang bridge so loop induction variables are stored under the `SgForInitStatement` scope, but the immediate user-facing problem is gone.

## Known Gaps

- The frontend still struggles with large swaths of the C++ standard library
  (heavy template specialisations, allocator traits, etc.). The builtin/runtime
  mismatches are gone, but expect additional traversal failures until those
  visitors are implemented.

## May 2025 Status & Next Steps

- **Bug fixed**: the Clang driver now initialises `LangOptions` for C++ input,
  picking a GNU C++17 default, injecting implicit headers, and enabling builtin
  intrinsics. This unblocks `<cmath>`/`__builtin_*` usage and lines up with the
  intended Clang invocation (`clang-frontend.cpp:267`-`395`, `clang-to-dot.cpp:264`-`325`).
- **AXPY sample**: the smoke test in `tests/nonsmoke/functional/input_codes/axpy.cpp`
  now exercises `<array>`, `<numeric>`, `<cmath>`, and `<cstddef>`. It compiles and
  runs with stock `clang++`, but the current Clang→ROSE bridge still aborts when
  walking the resulting libstdc++ AST.
- **Translator gaps**: namespace traversal, template declarations/specialisations,
  and many type/statement visitors remain stubs. With real system headers enabled
  the translator quickly encounters `std::array` scaffolding and dies. Skipping or
  generating opaque stand-ins for those nodes is the next prerequisite before we
  can ingest richer C++.
- **Proposed short-term plan**
  1. Teach `VisitNamespaceDecl` to record namespace shells while continuing to walk
     child declarations safely (no early aborts).
  2. Add minimal handling for class/function/alias/var template decls: traverse the
     templated declaration, create an opaque `SgType`, and store symbols so later
     lookups succeed.
  3. Extend record/typedef/type visitors to fall back to `buildOpaqueType` and
     unblock `std::array` basics; do the same for critical statement/expression
     visitors (`DeclRefExpr`, `MemberExpr`, `CallExpr` into template instantiations).
  4. Re-run the AXPY sample end-to-end, then incrementally admit additional headers.
- **Risks**: without better symbol management we risk leaking thousands of opaque
  placeholders or emitting unusable ASTs. Each fallback should log once (guarded
  by a debug knob) so production runs stay quiet.
- Linkage specifications are flattened; follow-up work is needed if downstream
  analyses require exact `extern` metadata.
