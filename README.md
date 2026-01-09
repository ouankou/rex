# REX (ROSE Compiler Modernization)

REX is a modernization of the ROSE compiler infrastructure that uses a Clang/LLVM frontend (LLVM 20) for C/C++ analysis.

## Platform support

REX targets Linux only.

## Build

Preferred:
```bash
./build-rex.sh $HOME/rex-install Release
```

Manual:
```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/rex-install
cmake --build build -j"$(nproc)"
cmake --install build
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

## Docs

REX uses MrDocs for API documentation and MkDocs Material for the site.
Use the helper script below to set up a Python venv, fetch the latest MrDocs,
and build the site.

```bash
# Ensure a build tree exists with compile_commands.json
./build-rex.sh $HOME/rex-install Release

# Build the docs site (API reference + MkDocs)
scripts/build-docs
```

Output is written to `_site`.
