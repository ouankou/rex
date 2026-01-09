# Build

Preferred build:

```bash
./build-rex.sh $HOME/rex-install Release
```

Manual CMake workflow:

```bash
mkdir -p build
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$HOME/rex-install
cmake --build build -j"$(nproc)"
cmake --install build
```
