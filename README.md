# REX (ROSE Compiler Modernization)

REX is a modernization of the ROSE compiler infrastructure that uses a Clang/LLVM frontend (LLVM 23) for C/C++ analysis.

## Platform support

REX targets Linux only.

## Build

Preferred (default build type is RelWithDebInfo):
```bash
./build-rex.sh $HOME/rex-install
```

The helper reuses the existing build directory and builds with one job by default.

Manual:
```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$HOME/rex-install
cmake --build build --parallel 1
cmake --install build
```

## Tests

```bash
ctest --test-dir build -j1 --output-on-failure
```

## Sanitizers and Valgrind memcheck

Use separate build directories for normal, sanitizer, and Valgrind builds. Sanitizer builds require `libclang-cpp` (LLVM 23). Use `Debug` for sanitizer and memcheck builds so `ROSE_ASSERT` stays enabled.

Sanitizers (ASan/LSan/UBSan):
```bash
cmake -S . -B build-sanitizer -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE-SANITIZER=ON -DROSE_SANITIZERS="address;leak;undefined"
cmake --build build-sanitizer --parallel 1
```

Valgrind/memcheck:
```bash
cmake -S . -B build-valgrind -DCMAKE_BUILD_TYPE=Debug -DWITH-VALGRIND=/usr
cmake --build build-valgrind --parallel 1
ctest --test-dir build-valgrind -T memcheck -R "<regex>" -j1 --output-on-failure
```

## Docs

REX uses MrDocs for API documentation and Zensical for the site.
Use the helper script to set up a Python venv, fetch MrDocs, and build the site.

```bash
# Build the docs site (API reference + Zensical)
scripts/build-docs
```

The docs build will generate required headers in the build tree (for example,
`rosePublicConfig.h` and `Cxx_Grammar.h`) if they are missing.

Output is written to `_site` and `docs/reference` by default. To keep all
generated files under the build tree, set `DOCS_SCRATCH_DIR`, for example:

```bash
DOCS_SCRATCH_DIR=build/docs-scratch scripts/build-docs
```
