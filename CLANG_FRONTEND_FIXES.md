# REX Clang Frontend Critical Fixes

## Summary

This document details the critical bugs found and fixed in the REX Clang/LLVM 20 frontend implementation that were causing segmentation faults.

## Issues Fixed

### 1. **CRITICAL: Stack Object Ownership Bug** (clang-frontend.cpp:303-304)

**Problem**: Taking the address of a stack-allocated object and wrapping it in a `unique_ptr` causing double-free:

```cpp
// BEFORE - BROKEN:
ClangToSageTranslator translator(compiler_instance, language);
compiler_instance->setASTConsumer(std::move(std::unique_ptr<clang::ASTConsumer>(&translator)));
```

**Solution**: Allocate on heap using `make_unique` and maintain a raw pointer for access:

```cpp
// AFTER - FIXED:
auto translator_ptr = std::make_unique<ClangToSageTranslator>(compiler_instance, language);
ClangToSageTranslator* translator = translator_ptr.get();
compiler_instance->setASTConsumer(std::move(translator_ptr));
```

**Impact**: This was the PRIMARY cause of segfaults. The unique_ptr would try to delete the stack object, causing undefined behavior.

---

### 2. **File Manager Initialization Order** (clang-frontend.cpp:238-243, clang-to-dot.cpp)

**Problem**: Calling `getVirtualFileSystem()` before creating the file manager:

```cpp
// BEFORE - BROKEN:
clang::CompilerInstance * compiler_instance = new clang::CompilerInstance();
compiler_instance->createDiagnostics(compiler_instance->getVirtualFileSystem(), ...); // VFS NULL!
```

**Solution**: Create file manager first:

```cpp
// AFTER - FIXED:
clang::CompilerInstance * compiler_instance = new clang::CompilerInstance();
compiler_instance->createFileManager();  // Create file manager FIRST
compiler_instance->createDiagnostics(compiler_instance->getVirtualFileSystem(), ...);
```

**Impact**: Calling `getVirtualFileSystem()` on uninitialized CompilerInstance caused segfault.

---

### 3. **LLVM 20 API: createDiagnostics Signature** (clang-frontend.cpp:240, clang-to-dot.cpp:254)

**Problem**: LLVM 20 requires VFS parameter for `createDiagnostics`:

```cpp
// BEFORE - BROKEN (missing VFS):
compiler_instance->createDiagnostics(diag_printer, true);
```

**Solution**: Add VFS parameter:

```cpp
// AFTER - FIXED:
compiler_instance->createDiagnostics(compiler_instance->getVirtualFileSystem(), diag_printer, true);
```

---

### 4. **Missing Return Statement** (clang-frontend.cpp:650)

**Problem**: Function `NextPreprocessorToInsert::next()` creates result but doesn't return it:

```cpp
// BEFORE - BROKEN:
NextPreprocessorToInsert * NextPreprocessorToInsert::next() {
    ...
    NextPreprocessorToInsert * res = new NextPreprocessorToInsert(translator);
    ...
    // Missing: return res;
}
```

**Solution**: Add return statement:

```cpp
// AFTER - FIXED:
return res;
```

---

### 5. **Missing Return Statement** (clang-frontend.cpp:670)

**Problem**: Function `PreprocessorInserter::evaluateInheritedAttribute()` incomplete:

```cpp
// BEFORE - BROKEN:
NextPreprocessorToInsert * PreprocessorInserter::evaluateInheritedAttribute(...) {
    ...
    if (passed_cursor) {
        return inheritedValue->next();
    }
    // Missing: return inheritedValue;
}
```

**Solution**: Add return statement for non-passed case:

```cpp
// AFTER - FIXED:
return inheritedValue;
```

---

### 6. **EDG References Cleanup**

**Files Modified**:
- `clang-to-dot.cpp`: Changed "EDG AST" → "Clang AST", fixed typo `DEBUG_CLANG_DOT_GRAPH_SUPPPORT` → `DEBUG_CLANG_DOT_GRAPH_SUPPORT`
- `clang_graph.h`: Changed "EDG representation" → "Clang representation"
- `clang-frontend-decl.cpp`: Removed "Following EDG's implementation" comments
- `clang-to-rose-support.cpp`: Removed EDG reference comment
- `CMakeLists.txt`: Updated comment to remove EDG mention

---

## Build Instructions

**IMPORTANT**: Must use LLVM/Clang 20 (not 21) to match library versions:

```bash
# Clean rebuild with LLVM 20
rm -rf build && mkdir build && cd build

# Configure with explicit LLVM 20 paths
CC=/usr/lib/llvm-20/bin/clang \
CXX=/usr/lib/llvm-20/bin/clang++ \
cmake .. \
  -Denable-clang-frontend=ON \
  -DLLVM_DIR=/usr/lib/llvm-20/cmake \
  -DClang_DIR=/usr/lib/llvm-20/lib/cmake/clang \
  -DCMAKE_INSTALL_PREFIX=~/rex-install

# Build
make -j4
```

---

## Testing

Test with simple C program:

```c
// test.c
int main() {
    return 0;
}
```

```bash
build/bin/rose-compiler test.c -o test
```

---

## Known Remaining Issues

### Issue: Missing Clang Builtin Headers

**Symptom**:
```
fatal error: 'clang-builtin-c.h' file not found
```

**Cause**: The Clang frontend builtin headers (`clang-builtin-c.h`, `clang-builtin-cpp.hpp`, etc.) from `src/frontend/CxxFrontend/Clang/` are not being installed to the build's include-staging directory.

**Status**: Needs CMake configuration fix to install these headers properly.

---

## Files Modified

1. `src/frontend/CxxFrontend/Clang/clang-frontend.cpp` - Core fixes
2. `src/frontend/CxxFrontend/Clang/clang-to-dot.cpp` - Same API fixes
3. `src/frontend/CxxFrontend/Clang/clang-frontend-decl.cpp` - Comment cleanup
4. `src/frontend/CxxFrontend/Clang/clang-to-rose-support.cpp` - Comment cleanup
5. `src/frontend/CxxFrontend/Clang/clang_graph.h` - Comment cleanup
6. `src/frontend/CxxFrontend/Clang/CMakeLists.txt` - Comment cleanup

---

## Summary of Root Causes

1. **Memory Management Error**: Stack object wrapped in unique_ptr causing double-free
2. **Initialization Order**: Accessing VFS before file manager creation
3. **API Compatibility**: Missing LLVM 20 API parameter
4. **Code Completion**: Missing return statements in two functions
5. **Build Configuration**: Builtin headers not being staged/installed

**Status**: Major segfault issues RESOLVED. REX now successfully compiles with Clang frontend and begins parsing C files. Remaining issue is builtin header installation configuration.
