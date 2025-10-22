# Fix Explanation: Clang Frontend Header Search Paths

## Overview

This document explains the fix for the issue where REX built with Clang could not find system headers like `stdio.h`.

## The Problem

### Symptoms
```bash
$ CC=clang-20 CXX=clang++-20 ./build-rex.sh ~/install
$ ~/install/bin/rose-compiler test.c
test.c:1:10: fatal error: 'stdio.h' file not found
#include <stdio.h>
         ^~~~~~~~~
```

### Why GCC Worked But Clang Didn't

The issue was in `CMakeLists.txt` around line 917:

```cmake
if(shell AND CMAKE_COMPILER_IS_GNUCXX)
```

This condition (`CMAKE_COMPILER_IS_GNUCXX`) is a CMake built-in variable that is **only set to TRUE when building with GNU G++**. When building with Clang, this variable is FALSE, so the entire block of code that discovers system header paths is skipped.

## The Code Flow

### What Happens During CMake Configuration

1. **Choose Backend Compiler** (`cmake/modules/roseChooseBackendCompiler.cmake`):
   - Determines what compiler will be used by the generated `rose-compiler`
   - Sets `BACKEND_C_COMPILER` and `BACKEND_CXX_COMPILER`
   - Extracts compiler name: `BACKEND_C_COMPILER_NAME_WITHOUT_PATH` (e.g., "clang-20", "gcc")

2. **Discover System Header Paths** (`CMakeLists.txt` lines 911-980):
   - **BEFORE FIX**: Only executed if `CMAKE_COMPILER_IS_GNUCXX` was true
   - Uses `config/get_compiler_header_dirs` script to query the compiler for its include paths
   - Populates `C_INCLUDE_STRING` and `CXX_INCLUDE_STRING` macros

3. **Generate rose_config.h**:
   - CMake substitutes the `C_INCLUDE_STRING` and `CXX_INCLUDE_STRING` values
   - These become C preprocessor macros used by `clang-frontend.cpp`

4. **REX Frontend Uses These Paths** (`src/frontend/CxxFrontend/Clang/clang-frontend.cpp`):
   ```cpp
   const char * c_config_include_dirs_array[] = C_INCLUDE_STRING;
   ```
   - Lines 125-126 use these macros to get system header paths
   - Lines 147-158 add these paths to the Clang CompilerInstance

## The Fix in Detail

### Before (Lines 917-935)

```cmake
if(shell AND CMAKE_COMPILER_IS_GNUCXX)  # ← PROBLEM: Clang builds skip this!
    # ... directory includes setup ...

    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/get_compiler_header_dirs gcc c gnu
      OUTPUT_VARIABLE C_backend_includes)  # ← PROBLEM: Hardcoded 'gcc'

    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/get_compiler_header_dirs g++ c++ gnu
      OUTPUT_VARIABLE CXX_backend_includes)  # ← PROBLEM: Hardcoded 'g++'
```

**Three Problems:**
1. Condition only works for GCC
2. Hardcoded `gcc` instead of `${BACKEND_C_COMPILER}`
3. Hardcoded vendor `gnu` instead of actual compiler vendor

### After (Lines 918-963)

```cmake
# Map CMake compiler IDs to vendor names
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    set(C_COMPILER_VENDOR "gnu")
elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    set(C_COMPILER_VENDOR "clang")  # ← NEW: Support Clang
# ... similar for CXX_COMPILER_VENDOR ...

# Extended condition to include Clang
if(shell AND (CMAKE_COMPILER_IS_GNUCXX OR
              "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
              "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"))  # ← FIX #1

    # ... directory includes setup ...

    # Use actual backend compiler instead of hardcoded names
    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/get_compiler_header_dirs
              ${BACKEND_C_COMPILER} c ${C_COMPILER_VENDOR}  # ← FIX #2 and #3
      OUTPUT_VARIABLE C_backend_includes)

    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/get_compiler_header_dirs
              ${BACKEND_CXX_COMPILER} c++ ${CXX_COMPILER_VENDOR}  # ← FIX #2 and #3
      OUTPUT_VARIABLE CXX_backend_includes)
```

## How get_compiler_header_dirs Works

The script `config/get_compiler_header_dirs` is clever:

```bash
#!/bin/bash
compilerName="$1"       # e.g., clang-20, gcc
language="$2"           # e.g., c, c++
compilerVendorName="$3" # e.g., clang, gnu

case "$compilerVendorName" in
  gnu|intel|clang)
    # Ask the compiler where it looks for headers
    $1 -v -E -x $language /dev/null 2>&1 | \
      sed -n '/^#include </,/^End/p' | \
      sed '/^#include </d; /^End/d; /\/usr\/include$/d; ...'
    ;;
esac
```

**Example output when running with Clang:**
```bash
$ clang-20 -v -E -x c /dev/null 2>&1 | sed -n '/^#include </,/^End/p'
#include <...> search starts here:
 /usr/lib/llvm-20/lib/clang/20/include
 /usr/local/include
 /usr/include/x86_64-linux-gnu
End of search list.
```

These paths are what get stored in `C_INCLUDE_STRING`.

## Result in rose_config.h

### Before Fix (Building with Clang)
```c
#define C_INCLUDE_STRING {"clang-20_HEADERS"}
```
Only has the REX-specific headers directory, **missing all system paths**.

### After Fix (Building with Clang)
```c
#define C_INCLUDE_STRING {"clang-20_HEADERS", \
  "/usr/lib/llvm-20/lib/clang/20/include", \
  "/usr/local/include", \
  "/usr/include/x86_64-linux-gnu", \
  "/usr/include"}
```
Contains the complete set of include paths that Clang uses.

## Why This Matters for the Clang Frontend

In `src/frontend/CxxFrontend/Clang/clang-frontend.cpp`:

```cpp
// Lines 125-135: Load the configuration
const char * c_config_include_dirs_array[] = C_INCLUDE_STRING;
std::vector<std::string> c_config_include_dirs(
    c_config_include_dirs_array,
    c_config_include_dirs_array + sizeof(c_config_include_dirs_array) / sizeof(const char*)
);

// Lines 157-159: Add as system directories
sys_dirs_list.insert(sys_dirs_list.begin(),
                     c_config_include_dirs.begin(),
                     c_config_include_dirs.end());

// Lines 212-221: Pass to Clang CompilerInstance
for (it_str = sys_dirs_list.begin(); it_str != sys_dirs_list.end(); it_str++) {
    args[i] = new char[it_str->size() + 9];
    // Creates "-isystem/usr/lib/llvm-20/lib/clang/20/include" etc.
    args[i][0] = '-'; args[i][1] = 'i'; args[i][2] = 's';
    args[i][3] = 'y'; args[i][4] = 's'; args[i][5] = 't';
    args[i][6] = 'e'; args[i][7] = 'm';
    strcpy(&(args[i][8]), it_str->c_str());
}
```

Without the system paths in `C_INCLUDE_STRING`, the Clang frontend gets no `-isystem` paths and cannot find `stdio.h`.

## Impact

This fix ensures that REX can be built with **any modern C++ compiler** (GCC, Clang, AppleClang, Intel) and the resulting `rose-compiler` will correctly find system headers for C code analysis.

This is critical for the Clang frontend migration, as documented in `CLAUDE.md`:
> REX has migrated from the EDG frontend to an experimental Clang/LLVM frontend for C language analysis.
