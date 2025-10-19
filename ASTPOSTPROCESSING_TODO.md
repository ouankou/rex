# AstPostProcessing Issue for Clang Frontend

## Background

The `AstPostProcessing()` function performs critical AST fixup operations after parsing:
- `markLhsValues()`: Marks expressions that appear on the left-hand side of assignments
- Fortran array/function disambiguation
- Other language-specific fixups

Historically, this was always enabled for the EDG frontend but was disabled for the Clang frontend due to unresolved issues.

## Current Status

**Problem**: When `AstPostProcessing()` is enabled for the Clang frontend, compilation fails with:

```
FAIL : ASSERTION:require: [Cxx_Grammar.C:16887, get_file_id]:
p_file_id == NULL_FILE_ID || p_file_id == COMPILER_GENERATED_FILE_ID || p_fileidtoname_map.count(p_file_id) > 0
```

**Root Cause**: Unknown. Investigation suggests:
1. All Clang frontend code creates `Sg_File_Info` objects using the string constructor
2. The string constructor properly registers filenames in the static maps via `set_filenameString()`
3. File registration should be automatic and persistent
4. Yet somehow, during `AstPostProcessing()`, a file ID is encountered that is not in the map

**Possible Theories**:
- Copy constructor might be copying file IDs without re-registration (but maps are static, so should persist)
- Integer constructor being used somewhere (but grep found no usage in Clang frontend)
- Map corruption or clearing somewhere during processing
- Thread-safety issue if maps are accessed concurrently (unlikely in single-threaded parse)
- Clang frontend creating Sg_File_Info objects with special initialization that bypasses registration

## Temporary Workaround

For the EDG cleanup PR, we apply a targeted workaround:
- Skip `AstPostProcessing()` only for Clang-based frontends (C, C++, CUDA, OpenCL)
- Keep it enabled for Fortran and binary analysis frontends
- Document the issue for future resolution

**Implementation** (in `src/frontend/SageIII/sage_support/sage_support.cpp`):

```cpp
// REX: Temporarily disable AstPostProcessing for Clang frontend until file ID issue is resolved.
// Clang triggers assertion in Sg_File_Info::get_file_id() about unregistered file IDs.
// See ASTPOSTPROCESSING_TODO.md for details.
bool isClangFrontend = get_C_only() || get_Cxx_only() || get_UPC_only() ||
                       get_Cuda_only() || get_OpenCL_only();

if ( (get_fileList().empty() == false) && (get_useBackendOnly() == false) && !isClangFrontend )
{
    AstPostProcessing(this);
}
```

## TODO: Complete Fix Required

**Goal**: Enable `AstPostProcessing()` for Clang frontend with proper file ID handling.

**Investigation Needed**:
1. Add comprehensive debug logging to identify the exact file ID that fails
2. Trace the creation path of the failing Sg_File_Info object
3. Check if Clang-specific code paths bypass registration
4. Compare with Fortran frontend's file ID handling
5. Verify thread safety of static map access
6. Check for any map clearing or reset operations

**Potential Fixes**:
1. **If registration is missing**: Add explicit registration in Clang frontend initialization
2. **If copy constructor is the issue**: Override copy constructor to ensure file ID validity
3. **If special file types need handling**: Add Clang-specific file ID types (like `CLANG_BUILTIN_FILE_ID`)
4. **If maps are corrupted**: Add defensive checks and re-registration logic

**Testing Strategy**:
1. Enable debug build (`-DCMAKE_BUILD_TYPE=Debug`)
2. Run with GDB breakpoint at the assertion
3. Examine `p_file_id`, `p_fileidtoname_map`, and call stack
4. Identify which Clang frontend operation creates the problematic Sg_File_Info
5. Fix the root cause in the Clang frontend code

**Files to Modify** (likely):
- `src/frontend/CxxFrontend/Clang/clang-frontend.cpp`: Main Clang frontend entry
- `src/frontend/CxxFrontend/Clang/clang-frontend-decl.cpp`: Declaration handling
- `src/frontend/CxxFrontend/Clang/clang-frontend-stmt.cpp`: Statement handling
- `src/frontend/CxxFrontend/Clang/clang-frontend-expr.cpp`: Expression handling
- `src/ROSETTA/Grammar/Support.code`: If assertion needs refinement (ONLY as last resort)

**Priority**: Medium - The Clang frontend currently works for simple programs without `AstPostProcessing`, but may have subtle bugs in complex cases where LHS marking matters.

**Related Issues**:
- ROSE-1499: Assertion about file ID validity
- REX issue: Clang frontend file ID registration

## References

- **Support.code** (line 14309-14321): The assertion that fails
- **sage_support.cpp** (line 1751-1756): Workaround implementation
- **clang-frontend.cpp** (line 356-357): Example of Sg_File_Info creation
- **Investigation summary**: All 92+ instances of Sg_File_Info creation in Clang frontend use string constructor

## Date

Created: 2025-10-19 (during EDG cleanup PR)
