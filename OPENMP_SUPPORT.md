# OpenMP Support in REX Clang Frontend

## Goal

Enable OpenMP pragma support in the REX Clang frontend with three operational modes:

1. **Pragma Passthrough** (no `-fopenmp`): Preserve OpenMP pragmas as-is in the output
2. **AST Construction** (`-fopenmp`): Convert pragmas to OpenMP-specific AST nodes (SgOmpParallelStatement, etc.)
3. **Lowering** (`-fopenmp -rose:openmp:lowering`): Full transformation to GOMP runtime calls

## Status

- ✅ **Case 1: Pragma Passthrough** - WORKING
- ✅ **Case 2: AST Construction** - WORKING
- ⏳ **Case 3: Lowering** - TODO (future work)

## Implementation

### Architecture Overview

The implementation follows ROSE's existing OpenMP infrastructure, integrating with the Clang frontend:

```
┌─────────────────────────────────────────────────────────────┐
│ Clang Preprocessing (PPCallbacks)                           │
│  - Capture #pragma omp directives during preprocessing      │
│  - Store pragma text with location info                     │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Clang AST Traversal (VisitCompoundStmt)                    │
│  - Create SgPragmaDeclaration for captured pragmas         │
│  - Insert pragma as first statement in body block          │
│  - Preserve whitespace for accurate passthrough            │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ ROSE AST Finalization                                       │
│  - Complete AST construction                                │
│  - Return to sage_support.cpp                              │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Automatic processOpenMP() Call                              │
│  - Called by sage_support.cpp:3119                         │
│  - Parse pragma text with ompparser                        │
│  - Convert SgPragmaDeclaration → SgOmpParallelStatement    │
│  - Replace pragma in-place with OpenMP AST node            │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Backend Unparsing                                           │
│  - Generate output code from AST                           │
│  - Pragmas unparsed from OpenMP nodes or passthrough       │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

#### 1. Pragma Capture (clang-frontend-stmt.cpp)

**Location**: Lines 1012-1077 in `VisitCompoundStmt()`

**Purpose**: Intercept OpenMP pragmas during Clang preprocessing and prepare them for ROSE AST

**Implementation**:
```cpp
// Check if there's a captured pragma for this location
if (pragma_map.count(pragma_location) > 0) {
    // Extract directive text (preserving whitespace)
    std::string directive = extractDirectiveFromPragmaText(pragma_text);

    // Create SgPragmaDeclaration with transformation file_info
    SgPragmaDeclaration* pragma_decl = SageBuilder::buildPragmaDeclaration(directive, block);

    // Set file info for source location tracking
    Sg_File_Info* start_fi = Sg_File_Info::generateDefaultFileInfoForTransformationNode();
    start_fi->set_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    start_fi->set_line(pragma_line);
    start_fi->set_col(1);
    pragma_decl->set_startOfConstruct(start_fi);

    // Insert pragma as first statement in block
    SageInterface::prependStatement(pragma_decl, block);
}
```

**Key Design Decisions**:
- **Prepend to existing block**: Insert pragma into the source-level compound block, not a wrapper
- **Transformation file_info**: Marks node as compiler-generated for processOpenMP() filtering
- **Whitespace preservation**: Store original pragma text to preserve formatting in passthrough mode

#### 2. Pragma Passthrough (clang-frontend-decl.cpp)

**Location**: Lines 100-164 in `VisitFunctionDecl()`

**Purpose**: When `-fopenmp` is NOT specified, attach pragma as preprocessing info for verbatim output

**Implementation**:
- Use `AttachedPreprocessingInfoType::before` to attach pragma before statement
- Set `RelativePositionType::inside` for block-level pragmas
- ROSE unparser outputs attached PreprocessingInfo verbatim

#### 3. OpenMP Flag Management (clang-frontend.cpp)

**Location**: Lines 455-469

**Purpose**: Override ROSE's default `parse_only` mode to use `ast_only` for Clang frontend

**Why**: ROSE defaults to parse-only mode, but Clang frontend should convert to AST nodes by default

```cpp
if (sageFile.get_openmp()) {
    bool has_explicit_processing_flag =
        sageFile.get_openmp_ast_only() ||
        sageFile.get_openmp_lowering() ||
        sageFile.get_openmp_analyzing();

    if (!has_explicit_processing_flag && sageFile.get_openmp_parse_only()) {
        sageFile.set_openmp_parse_only(false);
        sageFile.set_openmp_ast_only(true);
    }
}
```

#### 4. Integration with ROSE OpenMP Infrastructure

**Automatic Processing**: ROSE automatically calls `processOpenMP()` from `sage_support.cpp:3119`

**Critical**: Do NOT manually call `processOpenMP()` from the Clang frontend - this causes double processing!

**Processing Flow**:
1. `processOpenMP()` finds all `SgPragmaDeclaration` nodes
2. Filters by file and transformation status
3. Parses directive text using ompparser
4. Creates OpenMP-specific AST nodes (SgOmpParallelStatement, etc.)
5. Replaces pragma in-place with OpenMP node
6. Attaches body statement as child of OpenMP node

## Issues Encountered and Solutions

### Issue 1: Double Processing of Pragmas

**Symptom**:
```
[OMP DEBUG] CONVERTING pragma
[OpenMP] OpenMP AST construction complete
[OMP DEBUG] Processing pragma: 0xffff73e17010    <-- SAME PRAGMA AGAIN!
[FATAL] getNextStatement(): current statement is not found within its scope's statement list
```

**Root Cause**:
- Clang frontend manually called `OmpSupport::processOpenMP()` at line 552
- ROSE infrastructure also automatically calls `processOpenMP()` from sage_support.cpp:3119
- Second call tried to process already-converted pragma → crash

**Solution**:
Remove manual call from clang-frontend.cpp and rely on ROSE's automatic processing.

**Files Changed**:
- `clang-frontend.cpp`: Removed manual `OmpSupport::processOpenMP()` call
- Added comment explaining automatic processing

### Issue 2: Scope Mismatch for Pragma Nodes

**Symptom**:
Pragma's computed scope pointed to wrong block after AST construction

**Root Cause**:
- Initial approach created wrapper block to hold pragma and body as siblings
- AST fixup changed parent pointers during finalization
- `SgPragmaDeclaration` doesn't store explicit scope - it computes from parent chain

**Solution**:
Insert pragma directly into existing body block using `prependStatement()`, matching Fortran connector pattern

**Why This Works**:
- Pragma inserted into source-level block that already exists in AST
- Scope is stable throughout AST finalization
- Matches pattern used by OpenFortran Parser connector

### Issue 3: Whitespace Preservation

**Requirement**: Preserve exact whitespace in pragma for passthrough mode
- `#  pragma omp parallel` (two spaces after #)
- `#pragma  omp parallel` (two spaces between pragma and omp)

**Solution**:
Store complete pragma text from PPCallbacks, extract whitespace, and preserve in directive string

```cpp
// Extract spaces between # and pragma
size_t spaces_after_hash = pragma_pos - hash_pos - 1;
directive = std::string(spaces_after_hash, ' ') + pragma_text.substr(after_pragma);
```

## Testing

### Test Files

- `test_omp_normal.c` - Standard `#pragma omp parallel`
- `test_omp_space_after_hash.c` - Extra spaces: `#  pragma omp parallel`
- `test_omp_spaces.c` - Multiple spacing variations

### Test Commands

```bash
# Case 1: Pragma passthrough (no -fopenmp)
./build/bin/testTranslator -c test_omp_normal.c
# Expected: Pragma preserved exactly as written

# Case 2: AST construction (-fopenmp)
./build/bin/testTranslator -fopenmp -c test_omp_normal.c
# Expected: Clean exit, pragma converted to SgOmpParallelStatement, unparsed correctly

# Case 3: Lowering (not yet implemented)
./build/bin/testTranslator -fopenmp -rose:openmp:lowering -c test_omp_normal.c
# Expected: TODO - full GOMP transformation
```

### Expected Results

**Case 1 Output** (`rose_test_omp_normal.c`):
```c
#include <stdio.h>

int main()
{
{

#pragma omp parallel
    printf("Normal spacing\n");
  }
  return 0;
}
```

**Case 2 Output** (`rose_test_omp_normal.c`):
```c
#include <stdio.h>

int main()
{
{
#pragma omp parallel
    printf("Normal spacing\n");
  }
  return 0;
}
```

Note: Output is identical to source, but internally the pragma was converted to `SgOmpParallelStatement` and back.

## Files Modified

### Core Implementation

1. **src/frontend/Clang/clang-frontend-stmt.cpp**
   - Added pragma capture in `VisitCompoundStmt()` (lines 1012-1077)
   - Insert pragma into body block using `prependStatement()`

2. **src/frontend/Clang/clang-frontend.cpp**
   - Added OpenMP flag override logic (lines 455-469)
   - Removed manual `processOpenMP()` call
   - Added explanatory comments

3. **src/frontend/Clang/clang-frontend-decl.cpp**
   - Added pragma passthrough for non-OpenMP mode (lines 100-164)
   - Attach pragmas as PreprocessingInfo for verbatim unparsing

### No Changes Required

- **src/frontend/SageIII/ompAstConstruction.cpp** - No modifications needed
- **src/frontend/SageIII/sage_support/sage_support.cpp** - Automatic call already present

## Future Work

### Case 3: OpenMP Lowering

Implement full transformation to GOMP runtime when `-rose:openmp:lowering` is specified:

1. Call `lower_omp()` after `processOpenMP()`
2. Transform OpenMP constructs to outlined functions
3. Insert GOMP runtime calls (e.g., `GOMP_parallel_start()`)
4. Handle data sharing clauses
5. Manage thread team creation

**Reference**: See existing lowering implementation in `src/midend/programTransformation/ompLowering/`

### Additional Directives

Current implementation handles `#pragma omp parallel`. Future work should add:

- Loop constructs (`for`, `do`, `simd`)
- Tasking (`task`, `taskwait`, `taskloop`)
- Data environment (`shared`, `private`, `firstprivate`, etc.)
- Synchronization (`barrier`, `critical`, `atomic`)
- SIMD constructs
- Device directives (`target`, `teams`, `distribute`)

### Fortran Support

Extend to Fortran when Clang Fortran (flang) frontend is integrated.

## References

- ROSE Documentation: http://rosecompiler.org
- OpenMP Specification: https://www.openmp.org/specifications/
- Clang PPCallbacks: clang/Lex/PPCallbacks.h
- ROSE ompAstConstruction.cpp: OpenMP AST construction implementation
- Open Fortran Parser connector: Reference implementation for pragma handling

## Maintenance Notes

### Important Invariants

1. **Single processOpenMP() Call**: Never call `processOpenMP()` manually from frontend
2. **Pragma Scope**: Insert pragmas into existing source blocks, not wrapper blocks
3. **Transformation File Info**: Pragmas must be marked as transformation for filtering
4. **Whitespace Preservation**: Store complete pragma text for passthrough mode

### Debugging Tips

If OpenMP processing fails:

1. **Check pragma capture**: Verify `PragmaDirectiveCallback::PragmaDirective()` is called
2. **Verify pragma in AST**: Use NodeQuery to find `SgPragmaDeclaration` nodes
3. **Check scope**: Ensure pragma's `get_scope()` returns correct block
4. **Trace processOpenMP()**: Add debug to see if pragma is being processed
5. **Check OpenMP flags**: Verify `get_openmp_ast_only()` is set correctly

### Common Pitfalls

- **Do not** create wrapper blocks - insert into existing blocks
- **Do not** call `processOpenMP()` manually - it's called automatically
- **Do not** forget to set transformation file_info on pragmas
- **Do not** lose whitespace when extracting directive text
