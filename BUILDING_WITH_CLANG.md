# Building REX with Clang Frontend

This guide explains how to build REX (ROSE EXascale compiler) with the experimental Clang/LLVM frontend for C language analysis.

## Overview

REX has transitioned from the proprietary EDG frontend to an experimental Clang/LLVM-based frontend. This new frontend supports:
- **C language analysis** (primary focus)
- **LLVM 21.x** or later
- **CMake-only build system** (Autotools deprecated)

**Important**: The Clang frontend is **highly experimental** and may only successfully compile simple C programs like "hello world". C++ support is essentially non-existent at this stage.

## Prerequisites

### Required Dependencies

#### Core Build Tools

1. **CMake 3.15 or later**
   ```bash
   cmake --version  # Check your version
   ```

2. **C++17 compatible compiler**
   - GCC 7+ or Clang 5+
   - Comes with `build-essential` on Ubuntu/Debian

3. **Build essentials**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential git perl flex bison

   # Fedora/RHEL
   sudo dnf install gcc-c++ git perl flex bison

   # macOS
   xcode-select --install
   brew install flex bison
   ```

#### LLVM/Clang Frontend

4. **LLVM/Clang 21.x or later**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install llvm-21 clang-21 libclang-21-dev

   # Fedora/RHEL
   sudo dnf install llvm clang clang-devel

   # macOS (Homebrew)
   brew install llvm
   ```

#### Required Libraries

5. **Boost libraries (1.47.0 or later)**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libboost-all-dev

   # Fedora/RHEL
   sudo dnf install boost-devel

   # macOS
   brew install boost
   ```

6. **Compression libraries**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install zlib1g-dev libzstd-dev

   # Fedora/RHEL
   sudo dnf install zlib-devel libzstd-devel

   # macOS
   brew install zlib zstd
   ```

7. **XML library**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libxml2-dev

   # Fedora/RHEL
   sudo dnf install libxml2-devel

   # macOS
   brew install libxml2
   ```

8. **OpenCL (optional, for OpenCL analysis)**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install ocl-icd-opencl-dev

   # Fedora/RHEL
   sudo dnf install ocl-icd-devel
   ```

#### Complete Installation Command

For Ubuntu/Debian, install all dependencies at once:
```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential git cmake perl flex bison \
    llvm-21 clang-21 libclang-21-dev \
    libboost-all-dev \
    zlib1g-dev libzstd-dev \
    libxml2-dev \
    ocl-icd-opencl-dev
```

### Verify LLVM Installation

```bash
llvm-config --version  # Should show 21.x or later
clang --version        # Should match LLVM version
```

## Quick Start: Automated Build

The simplest way to build REX is using the provided automation script:

```bash
# Clone the repository
git clone https://github.com/passlab/rexompiler.git
cd rexompiler

# Run the build script (installs to ~/rex-install by default)
./build-rex.sh

# Or specify a custom install location
./build-rex.sh /path/to/install
```

The script will:
1. Initialize git submodules (ompparser, accparser)
2. Check for LLVM/Clang installation
3. Configure REX with CMake
4. Build with optimal parallelization
5. Install to the specified prefix

## Manual Build Instructions

If you prefer to build manually or need more control:

### 1. Clone and Initialize Submodules

```bash
git clone https://github.com/passlab/rexompiler.git
cd rexompiler
git submodule update --init --recursive
```

### 2. Create Build Directory

```bash
mkdir build
cd build
```

### 3. Configure with CMake

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/rex-install \
    -Denable-clang-frontend=ON \
    -Denable-c=ON \
    -Denable-fortran=OFF \
    -Denable-java=OFF \
    -DCMAKE_CXX_STANDARD=17
```

**Key CMake Options:**
- `-Denable-clang-frontend=ON` - **Required** to use Clang frontend
- `-Denable-c=ON` - Enable C language support
- `-Denable-fortran=OFF` - Disable Fortran (not supported with Clang frontend)
- `-DCMAKE_CXX_STANDARD=17` - Required by LLVM 21

### 4. Build

```bash
# Use all available cores
cmake --build . -j$(nproc)

# Or specify number of jobs
cmake --build . -j8
```

**Note**: Building REX takes significant time (30-60 minutes on modern hardware).

### 5. Install

```bash
cmake --install .
```

## Testing the Installation

### Set up Environment

```bash
export PATH=$HOME/rex-install/bin:$PATH
export LD_LIBRARY_PATH=$HOME/rex-install/lib:$LD_LIBRARY_PATH
```

### Simple C Example

Create a test file `hello.c`:

```c
#include <stdio.h>

int main() {
    printf("Hello from REX!\n");
    return 0;
}
```

Compile with REX:

```bash
rose-compiler -c hello.c
```

**Expected behavior:**
- REX should parse the C code using the Clang frontend
- Generate ROSE AST representation
- Output compilation diagnostics

**Note**: The Clang frontend is experimental. Complex C programs may fail to compile.

### Simple Test Cases That Should Work

1. **Minimal program:**
   ```c
   int main() { return 0; }
   ```

2. **Basic arithmetic:**
   ```c
   int add(int a, int b) { return a + b; }
   int main() { return add(2, 3); }
   ```

3. **Simple control flow:**
   ```c
   int factorial(int n) {
       if (n <= 1) return 1;
       return n * factorial(n - 1);
   }
   ```

## Troubleshooting

### LLVM Not Found

**Error**: `Could not find LLVM`

**Solution**:
```bash
# Specify LLVM installation directory
cmake .. -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
```

### Compilation Errors in Clang Frontend

**Error**: API compatibility errors in `clang-*.cpp` files

**Cause**: The Clang frontend code has been updated for LLVM 21 API. If you see errors, your LLVM version may be incompatible.

**Solution**: Ensure you're using LLVM 21.x:
```bash
llvm-config --version
```

### Missing zstd Library

**Error**: `The link interface of target "LLVMSupport" contains: zstd::libzstd_shared but the target was not found`

**Cause**: LLVM 21 requires the zstd compression library, but it's not installed.

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install libzstd-dev

# Fedora/RHEL
sudo dnf install libzstd-devel

# macOS
brew install zstd
```

### Submodule Errors

**Error**: Missing `ompparser` or `accparser`

**Solution**:
```bash
git submodule update --init --recursive
```

### Build Failures

**Error**: Build fails during ROSETTA code generation

**Solution**: This is a known issue in the development phase. Try:
```bash
# Clean build
rm -rf build
mkdir build
cd build
cmake .. [your options]
make -j1  # Build with single thread for clearer error messages
```

## Current Limitations

The Clang frontend in REX is **highly experimental**:

1. **Language Support:**
   - ✅ Basic C code (simple functions, control flow)
   - ⚠️ Complex C (advanced preprocessor, inline assembly) - **may fail**
   - ❌ C++ - **not supported**
   - ❌ C++11/14/17/20 - **not supported**
   - ❌ Fortran, Java, PHP, Python - **not applicable**

2. **Compilation Success:**
   - May only successfully compile trivial "hello world" style programs
   - Complex programs will likely fail during parsing or AST construction
   - This is expected for experimental software

3. **Build Goal:**
   - **Primary goal**: Achieve successful build compilation
   - **Secondary goal**: Parse simple C programs
   - **Future goal**: Full C language support

## Development Status

**Current Phase**: Migration from EDG to Clang frontend

- ✅ CMake build system configured
- ✅ LLVM 21 API compatibility implemented
- ✅ Basic Clang frontend integration
- ⚠️ Limited C language support (experimental)
- ❌ C++ support (not implemented)
- ❌ Production-ready (far future)

## Getting Help

- **Issues**: https://github.com/passlab/rexompiler/issues
- **Documentation**: See `CLAUDE.md` for architecture overview
- **ROSE Project**: http://rosecompiler.org

## Contributing

If you encounter build issues or want to contribute:

1. File detailed bug reports with:
   - LLVM/Clang version (`llvm-config --version`)
   - OS and distribution
   - Full error messages
   - Minimal reproducing example

2. Contributions welcome for:
   - Extending C language support
   - Fixing LLVM API compatibility
   - Improving build system
   - Documentation improvements

## References

- [LLVM Project](https://llvm.org/)
- [Clang Documentation](https://clang.llvm.org/docs/)
- [ROSE Compiler Framework](http://rosecompiler.org/)
- [CMake Documentation](https://cmake.org/documentation/)
