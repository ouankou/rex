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
