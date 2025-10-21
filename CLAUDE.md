# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ROSE is an open-source compiler infrastructure for building source-to-source program transformation and analysis tools for large-scale Fortran, C, C++, OpenMP, and UPC applications. It parses source code into a complete Abstract Syntax Tree (AST), provides APIs for sophisticated analysis and transformations, and can generate modified source code.

This is a fork/variant called "REX" (ROSE-based EXascale compiler) maintained in the repository at github.com/passlab.

## ⚠️ Important: Clang Frontend Migration (2025)

**REX has migrated from the EDG frontend to an experimental Clang/LLVM frontend for C language analysis.**

**Key Changes:**
- **Frontend**: Now uses Clang/LLVM 20.x (EDG removed due to licensing)
- **Language Support**: Currently **only C** (very experimental - may only compile "hello world")
- **C++ Support**: Not implemented (future goal)
- **Build System**: CMake only (Autotools deprecated and will be removed)
- **Goal**: Successful build first, then incremental C language support

**Quick Start with Clang Frontend:**
```bash
# Use the automation script
./build-rex.sh ~/rex-install

# Or manual build
mkdir build && cd build
cmake .. -Denable-clang-frontend=ON -Denable-c=ON -DCMAKE_INSTALL_PREFIX=~/rex-install
cmake --build . -j$(nproc)
cmake --install .
```

**See `BUILDING_WITH_CLANG.md` for complete Clang frontend build instructions.**

## Compiler Requirements

**REX prefers Clang/Clang++ but supports any modern C++17-capable compiler (GCC 7+, Clang 5+, etc.).**

The build system will automatically select compilers in this order:
1. **Environment variables** (`CC`, `CXX`, `FC`) - if set, these take precedence
2. **Preferred compilers** - `clang-20`/`clang++-20`/`flang-20` if available
3. **Fallback compilers** - `clang`/`clang++`/`flang-new`/`flang`
4. **System default** - whatever CMake finds on your system

```bash
# Use default (automatically prefers clang-20/clang++-20 if available)
./build-rex.sh ~/rex-install

# Explicitly use GCC/G++
CC=gcc CXX=g++ ./build-rex.sh ~/rex-install

# Use specific compiler versions
CC=gcc-11 CXX=g++-11 FC=gfortran-11 cmake .. -Denable-fortran=ON

# Use Clang (manually)
CC=clang-20 CXX=clang++-20 FC=flang-20 cmake .. -Denable-fortran=ON
```

**Note:** While any modern compiler works, Clang is preferred and better tested. The Clang/LLVM libraries for the frontend are linked separately and don't require building REX with Clang.

## Build System

### CMake Build (Primary)

The project uses CMake as the primary build system. Basic workflow:

```bash
# Run the build script to generate configure script (Autotools)
./build

# Create a separate build directory (out-of-source build required)
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/install \
         -DCMAKE_BUILD_TYPE=Release

# Build (use -j for parallel builds)
make -j8

# Run tests
ctest -j8
# Or: make check -j8

# Install
make install
```

### Important CMake Options

**Clang Frontend Options (New - 2025):**
- `-Denable-clang-frontend=ON/OFF` - **Enable Clang/LLVM frontend** (default: OFF, **Required for REX**)
- `-Denable-c=ON/OFF` - Enable C analysis with Clang (default: ON)
- `-DCMAKE_CXX_STANDARD=17` - C++17 required by LLVM 20 (default: 17)

**Legacy EDG Options (Deprecated):**
- `-DEDG_VERSION=5.0` - EDG frontend version (EDG removed from REX)

**Other Language Support:**
- `-Denable-fortran=ON/OFF` - Enable Fortran analysis (requires Java, default: OFF)
- `-Denable-java=ON/OFF` - Enable Java analysis (default: ON)
- `-Denable-cuda=ON/OFF` - Enable CUDA analysis (default: OFF)
- `-Denable-opencl=ON/OFF` - Enable OpenCL analysis (default: ON)

**General Options:**
- `-DBOOST_ROOT=/path/to/boost` - Specify Boost installation
- `-Ddisable-tests-directory=ON/OFF` - Control test compilation (default: OFF)

### Autotools Build (Legacy)

```bash
# Generate configure script
./build

# Create build directory and configure
mkdir build && cd build
../configure --prefix=/path/to/install \
             --enable-languages=c,c++ \
             --with-boost=/path/to/boost

# Build and test
make -j8
make check -j8
make install
```

## High-Level Architecture

### Three-Phase Compilation Model

ROSE follows a classic compiler architecture:

**Frontend (src/frontend/)**: Parses source code into AST
- **Clang/LLVM** (NEW): Experimental C parser using LLVM 20 (src/frontend/CxxFrontend/Clang/)
- **EDG (Edison Design Group)**: *REMOVED* - Commercial C/C++ parser (no longer available)
- **Open Fortran Parser**: Fortran support (src/frontend/OpenFortranParser_SAGE_Connection/)
- **SageIII**: The AST implementation (src/frontend/SageIII/)

**Midend (src/midend/)**: Analysis and transformation on AST
- AST traversal mechanisms (astProcessing/)
- Program analysis passes (programAnalysis/)
- Transformation operations (programTransformation/)

**Backend (src/backend/)**: Generates source code from AST
- Unparser converts AST back to source (backend/unparser/)

### Key Architectural Components

**ROSETTA (src/ROSETTA/)**: Meta-programming system that generates the AST node class definitions from high-level grammar specifications. Instead of manually writing ~670+ AST node classes, ROSETTA reads grammar files (*.code, *.macro) and automatically generates all the C++ code for the IR nodes.

**SageIII**: ROSE's Abstract Syntax Tree representation. All AST nodes start with `Sg` (e.g., `SgProject`, `SgFunctionDeclaration`, `SgExpression`). Key features:
- Memory pool allocation for efficient traversal
- Complete type system with type sharing
- Symbol tables for name resolution
- Source position tracking (Sg_File_Info)
- Preprocessing info preservation (comments, directives)

**SageInterface (src/frontend/SageIII/sageInterface/)**: High-level API for AST manipulation, queries, and traversals.

**SageBuilder (src/frontend/SageIII/sageInterface/sageBuilder.h)**: High-level API for AST construction with automatic symbol table management.

### How Translators Work

A "translator" is any program that parses code, optionally transforms it, and generates output:

```cpp
// Identity translator (no transformation)
int main(int argc, char* argv[]) {
    SgProject* project = frontend(argc, argv);  // Parse
    return backend(project);                     // Unparse
}

// Transformation translator
int main(int argc, char* argv[]) {
    SgProject* project = frontend(argc, argv);

    // Traverse and transform AST using SageInterface/SageBuilder
    MyTransformation transform;
    transform.traverse(project, preorder);

    return backend(project);  // Generate transformed code
}
```

### AST Traversal Patterns

ROSE provides sophisticated traversal templates in `src/midend/astProcessing/`:

- `AstSimpleProcessing`: Basic visitor pattern
- `AstTopDownProcessing`: Pre-order with inherited attributes
- `AstBottomUpProcessing`: Post-order with synthesized attributes
- `AstTopDownBottomUpProcessing`: Combined traversal

## Directory Structure

- **src/**: All source code for ROSE
  - **frontend/**: Parsing and AST construction
    - **CxxFrontend/Clang/**: Clang/LLVM frontend integration (NEW - experimental)
    - **CxxFrontend/EDG/**: EDG C/C++ parser (REMOVED - no license)
    - **SageIII/**: Core AST implementation
      - **sageInterface/**: High-level AST manipulation API
      - **astFixup/**: Post-parse AST corrections
      - **virtualCFG/**: Control flow graph support
      - **ompparser/**: OpenMP directive parsing (git submodule)
      - **accparser/**: OpenACC directive parsing (git submodule)
  - **midend/**: Analysis and transformation
    - **astProcessing/**: Traversal mechanisms
    - **programAnalysis/**: CFG, dataflow, pointer analysis, etc.
    - **programTransformation/**: Inlining, outlining, loop processing
  - **backend/unparser/**: Code generation from AST
  - **ROSETTA/**: AST generator meta-program
  - **util/**: General utilities (graphs, strings, command line)
- **tests/**: Test suites
- **tools/**: Complete, usable tools
- **tutorial/**: Examples demonstrating ROSE features
- **exampleTranslators/**: Example translator implementations
- **docs/**: Documentation source files

## Development Workflows

### Running a Single Test

```bash
# In your build directory
cd tests/nonsmoke/functional/CompileTests/C_tests
make test2025_01.o  # Build specific test
./test2025_01       # Run if executable

# Or use ctest with filter
ctest -R test2025_01 -V
```

### Building Only Part of ROSE

```bash
# Build specific subdirectory
cd build/src/frontend/SageIII
make -j8

# Build specific tool
cd build/tools/compilationDatabase2Makefile
make -j8
```

### Viewing Generated Code

ROSETTA-generated files are in: `build/src/frontend/SageIII/` with prefix `Cxx_Grammar*`

### Working with AST Nodes

All AST node types inherit from `SgNode` and use a variant system for efficient type identification:

```cpp
// Type checking with casting functions
if (SgFunctionDeclaration* func = isSgFunctionDeclaration(node)) {
    // Work with function declaration
}

// Query by variant type
if (node->variantT() == V_SgForStatement) {
    // Process for loop
}
```

### Common Analysis Tasks

**Find all function calls:**
```cpp
Rose_STL_Container<SgNode*> calls =
    NodeQuery::querySubTree(project, V_SgFunctionCallExp);
```

**Build new AST nodes:**
```cpp
SgExprStatement* stmt =
    SageBuilder::buildExprStatement(
        SageBuilder::buildFunctionCallExp(...));
```

### Debugging Tips

- Set `ROSE_DEBUG` environment variable for verbose output
- Use `project->get_verbose()` to control ROSE verbosity
- AST visualization: Use `generateDOT()` or `generatePDF()` methods
- Binary AST dump: Use `-rose:binary` for faster reloading

## Common Issues

**Boost version conflicts**: ROSE requires Boost >= 1.47.0. Specify with `-DBOOST_ROOT=` or `--with-boost=`.

**LLVM/Clang requirement**: REX requires LLVM/Clang 20.x or later libraries for the frontend (libclang, not the compiler itself). Install with `sudo apt-get install llvm-20 libclang-20-dev` (Ubuntu/Debian). The `clang-20` compiler package is optional but recommended.

**Clang frontend limitations**: The Clang frontend is highly experimental. It may only successfully compile simple C programs. C++ is not supported.

**C++ standard**: ROSE uses C++14 by default. Backend compiler must support it.

**Memory usage**: Building ROSE is memory-intensive. Use fewer parallel jobs (`-j4` instead of `-j8`) on machines with limited RAM.

**Test failures**: Some tests require specific backend compilers. Check test prerequisites in `tests/` Makefile.am files.

## Code Style

- C++14 standard
- AST node classes: Prefix with `Sg` (e.g., `SgExpression`)
- ROSETTA grammar files: Use `.code` extension
- Indentation: 2 spaces (for CMake files)
- No tabs for indentation in CMake

## Important Files

- `ROSE_VERSION`: Version string (updated during release)
- `rose_config.h.in.cmake`: CMake configuration template
- `CMakeLists.txt`: Top-level CMake configuration
- `configure.ac`: Autotools configuration (legacy)
- `build`: Script to generate autotools configure script

## Testing

Tests are organized in `tests/` with multiple categories:
- `tests/nonsmoke/functional/CompileTests/`: Compilation tests by language
- `tests/nonsmoke/functional/RunTests/`: Runtime execution tests
- Individual test directories contain their own Makefile.am with test definitions

Run all tests: `make check` or `ctest`
Run specific test suite: `cd tests/path/to/suite && make check`

## Documentation

- Online: http://rosecompiler.org
- GitHub Wiki: https://github.com/rose-compiler/rose/wiki
- Doxygen API: Build with `make doxygen_docs` in `docs/Rose/`
- Tutorial: See `tutorial/` directory for code examples
