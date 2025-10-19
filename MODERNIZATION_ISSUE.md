# Issue: Modernize Template Infrastructure to Remove `result_type` Dependency

## Summary

REX's AST query infrastructure (`src/midend/astQuery/`) uses legacy C++98/C++03 template patterns that require functors to provide a `result_type` typedef. This forces users to use deprecated `std::bind` instead of modern C++11+ lambdas, hindering code maintainability and modernization efforts.

## Background

### Current Implementation (C++98/C++03 Pattern)

The `querySubTree` template in `src/midend/astQuery/astQuery.h:267` uses:

```cpp
template<typename NodeFunctional>
typename NodeFunctional::result_type  // ← Requires result_type typedef!
querySubTree(SgNode* node, NodeFunctional nodeFunc,
             AstQueryNamespace::QueryDepth defineQueryType = AstQueryNamespace::AllNodes,
             t_traverseOrder treeTraversalOrder = preorder)
{
    // ...
}
```

This pattern comes from the C++98 era when:
- `std::unary_function` and `std::binary_function` base classes provided `result_type`
- Function adapters like `std::bind1st`, `std::bind2nd`, `std::ptr_fun` relied on these typedefs
- Template return type deduction required explicit `result_type` member

### Problem: Lambdas Don't Work

Modern C++11+ lambdas **do not provide `result_type`**, causing compilation failures:

```cpp
// ❌ FAILS - Lambda has no result_type
auto lambda = [](SgNode* node) { /* ... */ };
querySubTree(project, lambda);
// error: no type named 'result_type' in '(lambda at ...)'

// ✅ WORKS - std::bind provides result_type (but deprecated in C++17!)
auto bound = std::bind(functor, std::placeholders::_1, args...);
querySubTree(project, bound);
```

### Why This Matters

1. **Code Clarity**: Lambdas are more readable and easier to maintain than `std::bind`
2. **Modern C++ Standards**: `std::bind` is discouraged in modern C++ (prefer lambdas)
3. **Developer Experience**: Forces users to learn deprecated patterns
4. **Future-Proofing**: C++17 removed `std::unary_function`/`std::binary_function`

## Affected Files

### Core Template Infrastructure
- `src/midend/astQuery/astQuery.h` (lines 267, 330, 345)
  - `querySubTree()` template
  - `queryMemoryPool()` template
  - `queryNodeList()` template

### Test Files Requiring Workarounds
- `tests/nonsmoke/functional/roseTests/astQueryTests/testQuery2.C`
- `tests/nonsmoke/functional/roseTests/astQueryTests/testQuery3.C`

### Usage Throughout Codebase
Unknown - needs comprehensive grep/analysis to find all usages of these templates.

## Current Workaround: `rex_ptr_fun` Adapter (Implemented)

**Status:** ✅ **COMPLETED** - Temporary adapter implemented to eliminate C++17 deprecation warnings

### Why This Adapter is Needed

The deprecated `std::ptr_fun` provided critical typedefs that ROSE's legacy templates **explicitly require**:
- `result_type` - Return type of the callable
- `argument_type` - Argument type (for unary functions)
- `first_argument_type`, `second_argument_type` - Argument types (for binary functions)

**Key Insight:** ROSE templates use `typename NodeFunctional::result_type` **directly** (lines 178, 267, 359, 418 in astQuery.h), NOT type deduction. This means:
- ❌ Raw function pointers don't work (no typedef members)
- ❌ `std::function` doesn't work (lacks `result_type` and `argument_type`)
- ❌ Lambdas don't work (closure types have no typedefs)
- ✅ `rex_ptr_fun` adapter works (provides all required typedefs)

### Implementation Details

**Location:** `src/midend/astQuery/astQuery.h` (lines 88-175)

**Adapter Structs:**
```cpp
template<typename _Arg, typename _Result>
struct rex_unary_ptr_fun {
    using argument_type = _Arg;
    using result_type = _Result;
    explicit rex_unary_ptr_fun(_Result (*__pf)(_Arg)) : _M_ptr(__pf) {}
    _Result operator()(_Arg __x) const { return _M_ptr(__x); }
private:
    _Result (*_M_ptr)(_Arg);
};

template<typename _Arg1, typename _Arg2, typename _Result>
struct rex_binary_ptr_fun {
    using first_argument_type = _Arg1;
    using second_argument_type = _Arg2;
    using result_type = _Result;
    explicit rex_binary_ptr_fun(_Result (*__pf)(_Arg1, _Arg2)) : _M_ptr(__pf) {}
    _Result operator()(_Arg1 __x, _Arg2 __y) const { return _M_ptr(__x, __y); }
private:
    _Result (*_M_ptr)(_Arg1, _Arg2);
};
```

**Factory Functions:** Overloaded `rex_ptr_fun()` for automatic template type deduction

### Usage Across Codebase (21 Locations Fixed)

All `std::function<...>(__x)` wrappers replaced with `AstQueryNamespace::rex_ptr_fun(__x)`:

- **src/midend/astQuery/astQuery.h**: 2 locations (lines 473, 487)
- **src/midend/astQuery/numberQuery.C**: 8 locations (lines 301, 327, 346, 366, 384, 395, 425, 445)
- **src/midend/astQuery/nameQuery.C**: 8 locations (lines 910, 935, 954, 972, 989, 1000, 1032, 1052)
- **src/midend/astQuery/nodeQuery.C**: 3 locations (lines 119, 166 + typedef visibility fix at 1070-1071)

### Benefits of This Workaround

1. **Zero Deprecation Warnings**: Eliminates all C++17 `std::ptr_fun` deprecation warnings
2. **Backward Compatible**: Works with all existing code patterns
3. **Type Safe**: Provides compile-time type checking via templates
4. **Well Documented**: Comprehensive inline comments explain purpose and future modernization path
5. **Minimal Scope**: Contained to AST query infrastructure only

### Limitations (Why This is Temporary)

- Still requires function pointers (not lambdas or modern callables)
- Does not fix the root cause (legacy template design)
- Adds REX-specific code that duplicates standard library functionality
- Must be maintained until full template modernization is complete

## Long-Term Solution: Modernize with C++17 Features

### Option 1: Use `std::invoke_result` (C++17)

```cpp
template<typename NodeFunctional>
std::invoke_result_t<NodeFunctional, SgNode*>  // ← Modern C++17 approach
querySubTree(SgNode* node, NodeFunctional nodeFunc,
             AstQueryNamespace::QueryDepth defineQueryType = AstQueryNamespace::AllNodes,
             t_traverseOrder treeTraversalOrder = preorder)
{
    // ...
}
```

**Pros:**
- Works with lambdas, function pointers, functors, and `std::bind`
- Standard C++17 feature
- Type-safe

**Cons:**
- Requires C++17 (REX currently uses C++14/C++17 already for LLVM 20)

### Option 2: Use `decltype` with Expression SFINAE (C++14)

```cpp
template<typename NodeFunctional>
auto querySubTree(SgNode* node, NodeFunctional nodeFunc,
                  AstQueryNamespace::QueryDepth defineQueryType = AstQueryNamespace::AllNodes,
                  t_traverseOrder treeTraversalOrder = preorder)
    -> decltype(nodeFunc(node))  // ← Deduce return type
{
    // ...
}
```

**Pros:**
- Works with C++14 (current REX baseline)
- Clean syntax with trailing return type

**Cons:**
- Requires `nodeFunc` to be callable with `SgNode*` (may need SFINAE for overloads)

### Option 3: Use `auto` Return Type Deduction (C++14)

```cpp
template<typename NodeFunctional>
auto querySubTree(SgNode* node, NodeFunctional nodeFunc,
                  AstQueryNamespace::QueryDepth defineQueryType = AstQueryNamespace::AllNodes,
                  t_traverseOrder treeTraversalOrder = preorder)
{
    // ... existing implementation ...
    // Compiler automatically deduces return type
}
```

**Pros:**
- Simplest change
- Works with C++14

**Cons:**
- May require implementation in header file (if not already)
- Recursive calls need careful handling

## Recommended Approach

**Use Option 1 (`std::invoke_result_t`) for the following reasons:**

1. REX already requires C++17 for LLVM 20 compatibility
2. Most explicit and type-safe
3. Industry standard for modern C++ template programming
4. Works with all callable types (lambdas, functors, function pointers, `std::bind`)

## Implementation Plan

### Phase 1: Research & Analysis (Estimated: 1-2 days)
- [ ] Grep entire codebase for all uses of `querySubTree`, `queryMemoryPool`, `queryNodeList`
- [ ] Identify all callsites and functor types used
- [ ] Document edge cases and potential compatibility issues
- [ ] Create comprehensive test suite covering all usage patterns

### Phase 2: Template Modernization (Estimated: 2-3 days)
- [ ] Update `src/midend/astQuery/astQuery.h`:
  - [ ] Replace `typename NodeFunctional::result_type` with `std::invoke_result_t<...>`
  - [ ] Update all template function signatures
  - [ ] Update documentation/comments
- [ ] Verify all template specializations still compile

### Phase 3: Test Code Modernization (Estimated: 1 day)
- [ ] Convert test files to use lambdas instead of `std::bind`:
  - [ ] `testQuery2.C`
  - [ ] `testQuery3.C`
- [ ] Remove deprecated `std::binary_function`/`std::unary_function` usage
- [ ] Remove manual `result_type` typedefs from test functors (no longer needed)

### Phase 4: Validation (Estimated: 1 day)
- [ ] Full clean rebuild with zero warnings
- [ ] Run all AST query tests
- [ ] Verify Fortran/C compilation still works
- [ ] Performance testing (ensure no regression)

### Phase 5: Documentation (Estimated: 1 day)
- [ ] Update developer documentation
- [ ] Add examples showing lambda usage with `querySubTree`
- [ ] Update CLAUDE.md with modernization status
- [ ] Create migration guide for external users

**Total Estimated Effort:** 6-8 days

## Testing Strategy

### Compile-Time Tests
- Verify templates work with:
  - [ ] C++11 lambdas (capture-by-value, capture-by-reference)
  - [ ] C++14 generic lambdas
  - [ ] Function pointers
  - [ ] Functor classes
  - [ ] `std::function` wrappers
  - [ ] `std::bind` (for backward compatibility)

### Runtime Tests
- [ ] All existing AST query tests pass
- [ ] Performance benchmarks show no regression
- [ ] Memory usage remains stable

## Benefits of Modernization

### Developer Experience
- **Cleaner Code**: Use modern C++ idioms
- **Better Readability**: Lambdas are self-documenting
- **Reduced Complexity**: No need to understand deprecated `std::bind` patterns

### Code Quality
- **Type Safety**: `std::invoke_result_t` provides compile-time guarantees
- **Maintainability**: Align with C++17 best practices
- **Future-Proof**: Remove dependency on deprecated STL features

### Community Impact
- **Attracts Contributors**: Modern C++ is more accessible to new developers
- **Educational Value**: REX becomes a showcase for modern C++ compiler development
- **Industry Relevance**: Demonstrates commitment to current standards

## Example: Before vs After

### Before (Current - Using deprecated std::bind)

```cpp
// test.cpp
NodesInSubTree nodesInTree;
int count1 = 0, count2 = 0;

// Must use std::bind (deprecated in C++17)
auto bound = std::bind(nodesInTree,
                       std::placeholders::_1,
                       std::pair<int*, int*>(&count1, &count2));
AstQueryNamespace::querySubTree(project, bound);
```

### After (Modernized - Using lambdas)

```cpp
// test.cpp
NodesInSubTree nodesInTree;
int count1 = 0, count2 = 0;

// Clean, modern C++11 lambda
AstQueryNamespace::querySubTree(project, [&](SgNode* node) {
    return nodesInTree(node, std::pair<int*, int*>(&count1, &count2));
});
```

**Result:** More readable, more maintainable, uses modern C++ standards.

## Related Issues

- Deprecation warnings removal (completed in previous PR)
- C++17 migration for LLVM 20 compatibility (completed)
- General codebase modernization efforts

## Priority

**Priority: Medium-High**

**Rationale:**
- Not blocking current functionality (workaround exists with `std::bind`)
- Important for long-term maintainability and developer experience
- Aligns with REX modernization goals
- Good "first large refactoring" project to establish modernization patterns

## References

### C++ Standards Documentation
- [std::invoke_result (C++17)](https://en.cppreference.com/w/cpp/types/result_of)
- [Lambda expressions (C++11)](https://en.cppreference.com/w/cpp/language/lambda)
- [Deprecated std::unary_function/std::binary_function](https://en.cppreference.com/w/cpp/utility/functional/unary_function)

### ROSE/REX Documentation
- `CLAUDE.md` - Project overview and architecture
- `BUILDING_WITH_CLANG.md` - Build system requirements
- `src/midend/astQuery/astQuery.h` - Current template implementation

### Related Work
- Similar modernization efforts in LLVM/Clang codebase
- Boost library migration from `result_type` to `result_of`/`invoke_result`

## Action Items

- [ ] Create GitHub issue with this content
- [ ] Label as: `enhancement`, `modernization`, `technical-debt`, `good-first-large-project`
- [ ] Assign milestone: `REX Modernization Phase 1`
- [ ] Add to project board: `REX Improvements`

---

**Created:** 2025-01-XX
**Last Updated:** 2025-01-XX
**Status:** Proposed
**Estimated Effort:** 6-8 developer days
**Blocking:** None (workaround available)
