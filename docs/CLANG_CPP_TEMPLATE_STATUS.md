# C++ Template Support Status in Clang Frontend

**Last Updated**: January 2025
**Status**: Partial implementation, not production-ready

## Overview

This document describes the current state of C++ template support in the REX Clang frontend, including known issues, attempted fixes, and recommendations for future work.

## Current Implementation

### What Works ✅
1. **Basic class declarations** - Non-template C++ classes parse correctly
2. **Simple template detection** - Can identify when a class is described by a ClassTemplateDecl
3. **System header skipping** - System header template classes skip detailed member processing for performance

### What Doesn't Work ❌
1. **STL template processing** - Causes extreme slowness (hangs >60 seconds)
2. **Template instantiations** - Not properly tracked or translated
3. **Template specializations** - Cause variant type mismatches
4. **Dependent types** - Not resolved correctly
5. **Template metaprogramming** - Not supported

## Known Issues

### Issue 1: STL Template Performance
**Symptoms:**
- Tests using `<iostream>`, `<vector>`, `<string>` hang indefinitely
- Processing takes >60 seconds (tests must complete within 1 minute)
- Affects 3 tests: `interfaceFunctionCoverage`, `getDependentDecls`, `movePreprocessingInfo`

**Root Cause:**
- STL headers contain deeply nested template hierarchies
- Each template class triggers recursive processing of:
  - Template parameters
  - Base classes (often templated)
  - Member variables (often templated types)
  - Member functions (often templated)
  - Nested classes (often templates)
- This creates exponential traversal complexity

**Attempted Fixes:**
1. Skip system header templates in `VisitClassTemplateDecl` (line 1037-1044)
   - **Result**: Reduced some processing but still hangs
2. Skip member processing for system header templates in `VisitRecordDecl` (line 1462-1484)
   - **Result**: Helped but insufficient
3. Skip all system header RecordDecls
   - **Result**: Breaks type resolution, causes ROSE-1378 errors

**Status**: Partial mitigation implemented, full fix requires template instantiation tracking

### Issue 2: ROSE-1378 SgDeclarationScope
**Symptoms:**
- Assertion failure: `ROSE_ASSERT(!"ROSE-1378")` in `SgScopeStatement::getDeclarationList()`
- Occurs at `src/ROSETTA/Grammar/Statement.code:6292` and `6370`

**Root Cause:**
- `SgDeclarationScope` is used for function parameter scopes
- When template processing creates invalid scope hierarchies, code tries to call `getDeclarationList()` on `SgDeclarationScope`
- This method is not implemented for `SgDeclarationScope` (TODO since 2018)

**Implemented Fix:**
- Modified `Statement.code` lines 6287-6296 and 6366-6374
- Returns empty declaration list instead of asserting
- **Location**: `/home/ouankou/Projects/astinterface/rex/src/ROSETTA/Grammar/Statement.code`

```cpp
case V_SgDeclarationScope:
   {
   // TV (09/16/2018): (ROSE-1378) TODO
   // CLANG FRONTEND FIX: Return empty list for SgDeclarationScope instead of asserting
   // This is a temporary workaround for C++ template processing until full support is implemented
      static SgDeclarationStatementPtrList empty_list;
      return empty_list;
   }
```

**Status**: Workaround implemented, full fix requires proper SgDeclarationScope implementation

### Issue 3: Template vs Non-Template Variant Mismatch
**Symptoms:**
- Assertion failure: `this->variantT() == firstNondefiningDeclaration->variantT()`
- Error shows mixing `SgClassDeclaration` (variant 55) with `SgTemplateClassDeclaration` (variant 540)

**Root Cause:**
- Clang visits both `ClassTemplateDecl` AND the underlying `CXXRecordDecl`
- `VisitClassTemplateDecl` creates `SgTemplateClassDeclaration`
- `VisitRecordDecl` might also be called on same decl, creating `SgClassDeclaration`
- ROSE requires all forward/defining declarations use same variant type

**Attempted Fixes:**
1. Add template decl to translation map in `VisitClassTemplateDecl` (line 1111-1112)
   - Maps both `ClassTemplateDecl` and templated `CXXRecordDecl` to same `SgTemplateClassDeclaration`
   - **Result**: Should work but still had issues with complex templates
2. Check translation map early in `VisitRecordDecl` (line 1250-1259)
   - **Result**: Helps but not complete solution

**Status**: Partial fix, full solution requires comprehensive template traversal redesign

## Code Locations

### Key Files Modified
1. **`src/frontend/CxxFrontend/Clang/clang-frontend-decl.cpp`**
   - Line 1037-1044: Skip system header templates in `VisitClassTemplateDecl`
   - Line 1250-1259: Check translation map for templates in `VisitRecordDecl`
   - Line 1462-1484: Skip members of system header templates
   - Line 1586-1588: Set enum constant declptr (unrelated bug fix)

2. **`src/ROSETTA/Grammar/Statement.code`**
   - Line 6287-6296: ROSE-1378 workaround for `getDeclarationList()`
   - Line 6366-6374: ROSE-1378 workaround for const version

### Test Files Affected
- `tests/nonsmoke/functional/roseTests/astInterfaceTests/inputinterfaceFunctionCoverage.C`
- `tests/nonsmoke/functional/roseTests/astInterfaceTests/inputgetDependentDecls.C`
- `tests/nonsmoke/functional/roseTests/astInterfaceTests/inputMovePreprocessingInfo.C`

All three include STL headers requiring template support.

## Recommendations for Future Work

### Short-Term (Performance Improvement)
1. **Implement selective template instantiation**
   - Only process templates actually used in user code
   - Skip unused STL template instantiations

2. **Add template instantiation cache**
   - Cache commonly used template instantiations (e.g., `std::vector<int>`)
   - Avoid re-processing same templates

3. **Limit template recursion depth**
   - Set maximum depth for template hierarchy traversal
   - Prevent runaway recursion in complex templates

### Medium-Term (Correctness)
1. **Complete SgDeclarationScope implementation**
   - Add `get_declarations()` member to `SgDeclarationScope`
   - Update ROSETTA grammar specification
   - Remove workaround from `Statement.code`

2. **Template translation map redesign**
   - Ensure all template-related decls properly cached
   - Prevent duplicate visits to same decl
   - Handle template specializations correctly

3. **Variant type consistency**
   - Ensure template and non-template decls never mixed
   - Add assertions to catch mixing early
   - Design clear policy for which variant to use

### Long-Term (Full Support)
1. **Template instantiation tracking**
   - Track which templates are instantiated where
   - Maintain instantiation context
   - Support explicit and implicit instantiations

2. **Template specialization support**
   - Handle partial specializations
   - Handle full specializations
   - Resolve correct specialization for given arguments

3. **Dependent type resolution**
   - Resolve dependent types when possible
   - Handle type-dependent expressions
   - Support template metaprogramming patterns

4. **Template argument deduction**
   - Implement function template argument deduction
   - Handle complex deduction scenarios
   - Support SFINAE (Substitution Failure Is Not An Error)

## Testing Strategy

### Current Test Status
- **Total tests**: 60
- **Passing**: 57 (95%)
- **Failing**: 3 (all template-related)

### Recommended Test Additions
1. **Unit tests for template basics**
   - Simple class template: `template<typename T> class Foo { T member; };`
   - Simple function template: `template<typename T> T max(T a, T b) { ... }`
   - Test without STL dependencies

2. **Incremental STL tests**
   - Test individual STL headers separately
   - Start with simpler headers (e.g., `<utility>`)
   - Progress to complex headers (e.g., `<iostream>`)

3. **Performance benchmarks**
   - Measure parsing time for various template depths
   - Set performance regression tests
   - Target: <1 second for simple template classes

## Debugging Tips

### Enable Debug Output
Add to `clang-frontend-decl.cpp`:
```cpp
#define DEBUG_VISIT_DECL 1  // Enable visitor debug output
#define DEBUG_TRAVERSE_DECL 1  // Enable traverse debug output
```

### Useful GDB Breakpoints
```bash
# Break when template class is visited
b ClangToSageTranslator::VisitClassTemplateDecl

# Break when record decl is visited
b ClangToSageTranslator::VisitRecordDecl

# Break on ROSE-1378 error
b Statement.code:6292
b Statement.code:6370
```

### Check Translation Map
Add debug code:
```cpp
std::cerr << "Translation map size: " << p_decl_translation_map.size() << std::endl;
if (p_decl_translation_map.find(decl) != p_decl_translation_map.end()) {
    std::cerr << "Decl already in map: " << decl->getNameAsString() << std::endl;
}
```

## References

### ROSE Documentation
- ROSE IR nodes: `src/frontend/SageIII/Cxx_Grammar.h`
- AST construction: `src/frontend/SageIII/sageInterface/sageBuilder.h`
- Template documentation: (TODO - needs to be written)

### Clang Documentation
- Clang AST: https://clang.llvm.org/docs/IntroductionToTheClangAST.html
- Template handling: https://clang.llvm.org/docs/LibASTMatchers.html
- ClassTemplateDecl: https://clang.llvm.org/doxygen/classclang_1_1ClassTemplateDecl.html

### Related ROSE Issues
- ROSE-1378: SgDeclarationScope not implemented (2018)
- Template support has been a known gap since EDG frontend removal

## Conclusion

C++ template support in the Clang frontend is **partially implemented** but **not production-ready**. Basic C++ class support works well, but any code using STL templates will fail or hang. Significant engineering effort (estimated weeks to months) would be required to implement full template support.

**Recommendation**: For production use, REX should be limited to C programs or C++ programs that avoid templates and STL. Users requiring C++ template support should wait for future releases or consider contributing to template implementation efforts.
