# ROSE Compiler Fixes - October 18, 2025

This document records the issues encountered while getting `rose-compiler` to work with the Clang frontend for C compilation, along with solutions implemented.

## Summary

The REX compiler has transitioned from the proprietary EDG frontend to an experimental Clang/LLVM 20 frontend for C language analysis. This document tracks the compilation and runtime issues encountered and their resolutions.

---

## Issue #1: Missing Clang Builtin Headers in Staging Directory

### Problem Description
**Error**: `fatal error: 'clang-builtin-c.h' file not found`

When attempting to compile `test.c` with `rose-compiler test.c`, the Clang frontend failed immediately with:
```
<built-in>:391:10: fatal error: 'clang-builtin-c.h' file not found
    1 | #include "clang-builtin-c.h"
      |          ^~~~~~~~~~~~~~~~~~~
```

### Root Cause Analysis
1. The Clang frontend code (`clang-frontend.cpp:154`) adds `rose_include_path + "clang/"` to the system include directories
2. For build-tree execution, `rose_include_path` resolves to `build/include-staging/`
3. The builtin headers are required to be at `build/include-staging/clang/clang-builtin-c.h`
4. However, the `CMakeLists.txt` for the Clang frontend only had `install()` commands, not staging commands
5. The `build/include-staging/clang/` directory was never created during the build process

### Code Investigation
From `clang-frontend.cpp`:
```cpp
std::string rose_include_path;
bool in_install_tree = roseInstallPrefix(rose_include_path);
if (in_install_tree) {
    rose_include_path += "/include/";
}
else {
    rose_include_path = std::string(ROSE_AUTOMAKE_TOP_BUILDDIR) + "/include-staging/";
}
...
sys_dirs_list.push_back(rose_include_path + "clang/");
```

### Solution Implemented
Modified `src/frontend/CxxFrontend/Clang/CMakeLists.txt` to copy builtin headers to the staging directory during configuration:

```cmake
# Copy builtin headers to staging directory for build-tree usage
# (Install tree headers are handled by install() commands below)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include-staging/clang)
file(COPY
  clang-builtin-c.h
  clang-builtin-cpp.hpp
  clang-builtin-cuda.hpp
  clang-builtin-opencl.h
  DESTINATION ${CMAKE_BINARY_DIR}/include-staging/clang)
```

This ensures headers are available for both:
- **Build tree**: `build/include-staging/clang/`
- **Install tree**: `<install-prefix>/include/clang/` (via existing `install()` commands)

### Verification
After fix:
```bash
$ ls build/include-staging/clang/
clang-builtin-c.h  clang-builtin-cpp.hpp  clang-builtin-cuda.hpp  clang-builtin-opencl.h
```

The fatal error regarding missing headers was resolved.

---

## Issue #2: Symbol Table Insertion with NULL Project Pointer

### Problem Description
**Warnings**: Multiple occurrences of:
```
WARN : ROSETTA [.../Cxx_Grammar.C:114861, insert_symbol]: In SgScopeStatement::insert_symbol(): project == NULL
```

### Root Cause Analysis
1. During Clang AST parsing (`ParseAST()`), the `HandleTranslationUnit` callback is invoked
2. This creates the `SgGlobal` scope and begins traversing the Clang AST
3. As declarations are processed, symbols are inserted into scope symbol tables
4. Symbol insertion code (`Cxx_Grammar.C:114861`) tries to access the project via the scope hierarchy:
   ```
   SgGlobal -> SgSourceFile -> SgProject
   ```
5. However, the `SgGlobal` scope's parent (`SgSourceFile`) was not set until AFTER parsing completed
6. This left the parent pointer NULL during traversal, causing the warning

### Code Flow Problem
**Before fix**:
1. Create `ClangToSageTranslator` (line 306)
2. Call `ParseAST()` which triggers `HandleTranslationUnit` (line 325)
   - Inside: Create `SgGlobal` scope
   - Insert symbols → **WARNING: project == NULL**
3. After parsing: Set `global_scope->set_parent(&sageFile)` (line 340) ← **Too late!**

### Solution Implemented

Modified the translator to accept the `SgSourceFile` as a constructor parameter and set up the parent relationship immediately when creating the global scope.

**Step 1**: Update translator class (`clang-frontend-private.hpp`):
```cpp
class ClangToSageTranslator : public clang::ASTConsumer {
    ...
    SgSourceFile * p_sage_source_file; // New member

public:
    ClangToSageTranslator(clang::CompilerInstance * compiler_instance,
                          Language language_,
                          SgSourceFile * sage_source_file); // Updated signature
    ...
};
```

**Step 2**: Update constructor implementation (`clang-frontend.cpp`):
```cpp
ClangToSageTranslator::ClangToSageTranslator(
    clang::CompilerInstance * compiler_instance,
    Language language_,
    SgSourceFile * sage_source_file) :
    ...
    p_sage_source_file(sage_source_file),
    language(language_)
{}
```

**Step 3**: Pass SgSourceFile when creating translator:
```cpp
auto translator_ptr = std::make_unique<ClangToSageTranslator>(
    compiler_instance, language, &sageFile);  // Pass sageFile here
```

**Step 4**: Set parent immediately when creating global scope (`clang-frontend-decl.cpp:2245`):
```cpp
*node = p_global_scope = new SgGlobal();

// Set up parent relationship immediately so symbol insertion can access the project
if (p_sage_source_file != NULL) {
    p_global_scope->set_parent(p_sage_source_file);
}
```

**Step 5**: Remove redundant parent setting after parsing (`clang-frontend.cpp:340`):
```cpp
sageFile.set_globalScope(global_scope);
// Parent relationship already set up during global scope creation
```

### Verification
After fix, the "project == NULL" warnings disappeared completely.

---

## Issue #3: Segmentation Fault in Backend Unparsing (UNRESOLVED)

### Problem Description
**Error**: Segmentation fault (exit code 139) during backend unparsing phase

```bash
$ build/bin/rose-compiler test.c
Exit code: 139
Segmentation fault (core dumped)
```

### Root Cause Analysis
GDB stack trace reveals crash in token-based unparsing:

```
#0  SgSourceFile::get_tokenSubsequenceMap()
#1  UnparseLanguageIndependentConstructs::unparseGlobalStmt(SgStatement*, SgUnparse_Info&)
#2  UnparseLanguageIndependentConstructs::unparseStatement(SgStatement*, SgUnparse_Info&)
#3  Unparser::unparseFile(SgSourceFile*, SgUnparse_Info&, SgScopeStatement*)
#4  unparseFile(SgFile*, UnparseFormatHelp*, UnparseDelegate*, SgScopeStatement*)
#5  unparseProject(SgProject*, UnparseFormatHelp*, UnparseDelegate*)
#6  backend(SgProject*, UnparseFormatHelp*, UnparseDelegate*)
```

**Specific failure point**: `SgSourceFile::get_tokenSubsequenceMap()` is accessing an uninitialized or NULL data structure.

### Analysis
1. ROSE's unparser can operate in two modes:
   - **Token-based unparsing**: Uses original source tokens for better fidelity
   - **AST-based unparsing**: Regenerates source from AST nodes

2. The Clang frontend does NOT populate token stream information:
   - No token subsequence maps
   - No token-to-AST mappings
   - Token data structures remain uninitialized

3. The unparser attempts to access `tokenSubsequenceMap` regardless, causing the crash

### Attempted Solutions
1. ✗ Adding `-c` flag to skip final compilation (still crashes during unparsing)
2. ✗ Checked for unparse skip flags (none applicable to token streams)

### Current Status: UNRESOLVED - TO BE ADDRESSED IN NEXT ROUND

The Clang frontend successfully:
- ✅ Finds and includes builtin headers
- ✅ Parses C source code
- ✅ Builds ROSE AST with proper parent relationships
- ✅ Inserts symbols into symbol tables without warnings
- ✅ Completes frontend processing with no errors

The Clang frontend FAILS at:
- ❌ Backend unparsing (segfault in tokenSubsequenceMap access)
- ❌ Generating output `rose_test.c` (file created but empty, 0 bytes)

### Root Cause Clarification

**IMPORTANT**: The backend unparsing code works perfectly with the EDG frontend. The issue is NOT in the backend - it's that the Clang frontend is not properly initializing the SgSourceFile object to match what the backend expects.

**Note**: EDG source code is NO LONGER AVAILABLE due to licensing restrictions being removed from REX. We cannot study EDG's initialization code directly.

The backend expects:
- `SgSourceFile::get_tokenSubsequenceMap()` to return a valid (possibly empty) map
- Proper initialization of token-related data structures
- Source file metadata to be populated correctly

### Required Fix (Next Round)

The Clang frontend needs to be updated to properly initialize SgSourceFile. Since EDG source is unavailable, we must:

1. **Analyze backend requirements**: Examine what the unparser actually needs from SgSourceFile
   - Check `src/backend/unparser/` for tokenSubsequenceMap usage
   - Identify minimum required initialization

2. **Study SgSourceFile class**: Examine the class definition and member variables
   - Check `build/src/frontend/SageIII/Cxx_Grammar.h` (generated)
   - Look for token-related members and initialization methods

3. **Initialize tokenSubsequenceMap**: Ensure the map is created/initialized (even if empty)
   - May need to call initialization methods in `clang_main()`
   - Check if SgSourceFile constructor needs specific parameters

4. **Check existing initialization code**: Look at how SgSourceFile is created in other contexts
   - Search for `new SgSourceFile` in codebase
   - Check tutorial examples and tests

**BACKEND MODIFICATION IS NOT AN OPTION** - The backend works correctly and should not be changed. The fix must be in the Clang frontend code or CMake configuration.

### Investigation Strategy (Next Round)
1. Examine `SgSourceFile` class definition for all token-related members
2. Find what `get_tokenSubsequenceMap()` returns and how it should be initialized
3. Check if there are initialization methods that must be called
4. Look for similar initialization in tests or tutorial code
5. Add proper initialization in `clang_main()` before or after parsing
6. Test and verify unparsing works

---

## Summary of Changes

### Files Modified

1. **src/frontend/CxxFrontend/Clang/CMakeLists.txt**
   - Added staging directory creation and file copying for builtin headers

2. **src/frontend/CxxFrontend/Clang/clang-frontend-private.hpp**
   - Added `p_sage_source_file` member to `ClangToSageTranslator`
   - Updated constructor signature to accept `SgSourceFile*`

3. **src/frontend/CxxFrontend/Clang/clang-frontend.cpp**
   - Updated constructor implementation to store `p_sage_source_file`
   - Modified translator instantiation to pass `&sageFile`
   - Removed redundant parent setting after parsing

4. **src/frontend/CxxFrontend/Clang/clang-frontend-decl.cpp**
   - Added immediate parent relationship setup when creating `SgGlobal`

### Test Results

**Before fixes**:
```bash
$ build/bin/rose-compiler test.c
fatal error: 'clang-builtin-c.h' file not found
WARN: project == NULL (repeated 11 times)
Segmentation fault
```

**After fixes**:
```bash
$ build/bin/rose-compiler test.c
(no warnings about project == NULL)
Segmentation fault  ← Still occurs in unparsing
```

### Progress Summary
- ✅ **Issue #1 RESOLVED**: Builtin headers now found
- ✅ **Issue #2 RESOLVED**: Symbol table operations work correctly
- ❌ **Issue #3 UNRESOLVED**: Backend unparsing still crashes

---

## Testing Instructions

### Test Case
Simple C program (`test.c`):
```c
int main() {
    return 0;
}
```

### Expected Behavior (when fully working)
```bash
$ build/bin/rose-compiler test.c
$ ls rose_test.c
-rw-rw-r-- 1 user user 28 Oct 18 18:00 rose_test.c
$ cat rose_test.c

int main()
{
  return 0;
}
```

### Current Behavior
```bash
$ build/bin/rose-compiler test.c
Segmentation fault (core dumped)
$ ls -la rose_test.c
-rw-rw-r-- 1 user user 0 Oct 18 18:00 rose_test.c  # Empty file
```

---

## Environment

- **Date**: October 18, 2025
- **System**: Linux 6.8.0-85-generic
- **Compiler**: Clang/LLVM 20.1.8
- **REX Version**: 0.11.96.11
- **Build System**: CMake
- **Git Branch**: main
- **Latest Commit**: bd305f9508 ("Fix critical segfaults in Clang frontend for C compilation #2")

---

## References

- [BUILDING_WITH_CLANG.md](BUILDING_WITH_CLANG.md) - Clang frontend build instructions
- [CLAUDE.md](CLAUDE.md) - Project overview and architecture
- Issue #1: https://github.com/ouankou/rexompiler/pull/1
- Issue #2: https://github.com/ouankou/rexompiler/pull/2

---

## Notes for Future Developers

1. **SgSourceFile Initialization**: The Clang frontend must properly initialize all SgSourceFile members to match backend expectations
   - **DO NOT modify the backend** - it works correctly with EDG
   - Focus on proper initialization in Clang frontend (`clang_main()`)
   - Study SgSourceFile class to understand required initialization

2. **Token Map Initialization**: Ensure `tokenSubsequenceMap` is at minimum initialized (even if empty)
   - Check if SgSourceFile constructor handles this automatically
   - May need explicit initialization call after SgSourceFile creation
   - Backend expects a valid map reference, not NULL

3. **Testing**: Add unit tests for:
   - Staged header installation
   - Basic C program compilation end-to-end
   - Verify unparsing produces non-empty output

4. **Documentation**: Update BUILDING_WITH_CLANG.md with:
   - Current working status
   - Known limitations
   - Workarounds if any

---

*Document created: October 18, 2025*
*Last updated: October 18, 2025*
