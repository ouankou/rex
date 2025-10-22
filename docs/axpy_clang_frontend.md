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

- Added `tests/nonsmoke/functional/input_codes/axpy.cpp` as a minimal C++
  example that avoids STL usage and anonymous namespaces so that the current
  frontend can process it.
- The program initialises two fixed-size buffers, performs the AXPY loop, and
  returns `0`/`1` based on a relative-error check (no runtime I/O needed).

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

- **Resolved name-qualification noise**: the unparser used raw symbol-table counts and saw duplicate entries for block-local declarations (including `for`-loop indices), so every reference triggered a warning. `nameQualificationSupport.C` now counts distinct lexical declarations within the current scope before warning, which eliminates the flood while preserving real conflicts.
- **Future hardening**: it is still worthwhile to tighten the Clang bridge so loop induction variables are stored under the `SgForInitStatement` scope, but the immediate user-facing problem is gone.

## Known Gaps

- The frontend still struggles with standard C++ library headers, templates,
  and richer language constructs. Keep examples limited to plain C-style
  constructs for now.
- Linkage specifications are flattened; follow-up work is needed if downstream
  analyses require exact `extern` metadata.
