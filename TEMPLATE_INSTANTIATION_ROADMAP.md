# Template Instantiation Implementation Roadmap

**Document Version**: 1.0
**Date**: 2025-10-23
**Author**: REX Development Team
**Estimated Effort**: 2-3 weeks (120-180 hours)

## Executive Summary

This document provides a detailed roadmap for implementing proper C++ template instantiation support in the REX/ROSE Clang frontend. Currently, template types like `std::array<double, 1024>` are handled using opaque types, which causes:
- Incorrect unparsing (`struct array` instead of `std::array<double, 1024>`)
- Symbol table failures (variables unparse as `42` placeholder)
- Incomplete type information in the AST

The solution is to implement ROSE's native template instantiation infrastructure (`SgTemplateInstantiationDecl`) for template types encountered in Clang's AST.

## Current Status

### What Works ✅
- Template type name extraction from Clang AST
- Template argument parsing (using LLVM 20 `template_arguments()` API)
- Template-dependent expressions (`UnresolvedLookupExpr`, `CXXDependentScopeMemberExpr`)
- Non-template code generation (100% correct for regular functions)

### What Doesn't Work ❌
- Template variable declarations unparse incorrectly
- Symbol table doesn't recognize template-typed variables
- Variable references use `42` placeholder instead of variable names
- Template instantiations treated as opaque types

### Root Cause
`buildOpaqueType("std::array<double, 1024>")` creates:
```cpp
typedef int std::array<double, 1024>;  // Hidden declaration
```
This typedef approach:
1. Uses `int` as base type (incorrect)
2. Doesn't create proper class structure
3. Doesn't populate symbol table for variables of this type
4. Unparsing extracts base type instead of full template name

## Architecture Overview

### ROSE Template Instantiation Model

ROSE represents template instantiations through a hierarchy:

```
SgTemplateClassDeclaration               // Template declaration (e.g., template<typename T, size_t N> class array)
    ├── SgTemplateParameterPtrList       // List of template parameters (T, N)
    │   ├── SgTemplateParameter (T)      // Type parameter
    │   └── SgTemplateParameter (N)      // Non-type parameter
    └── SgClassDeclaration               // Base class structure

SgTemplateInstantiationDecl             // Template instantiation (e.g., array<double, 1024>)
    ├── SgTemplateArgumentPtrList       // List of template arguments
    │   ├── SgTemplateArgument (double) // Type argument
    │   └── SgTemplateArgument (1024)   // Value argument
    ├── SgClassDeclaration*             // Points to template declaration
    └── SgClassDefinition               // Instantiated class body

SgClassType                             // Type for variables
    └── get_declaration() → SgTemplateInstantiationDecl
```

### Clang AST Mapping

Clang provides template information through:

```cpp
clang::TemplateSpecializationType
    ├── getTemplateName()              → Template name (std::array)
    └── template_arguments()           → Template arguments (double, 1024)

clang::ClassTemplateSpecializationDecl  // Full template specialization
    ├── getSpecializedTemplate()       → Template declaration
    └── getTemplateArgs()              → Argument list
```

## Implementation Plan

### Phase 1: Infrastructure Setup (Week 1, Days 1-2)

**Goal**: Create helper infrastructure for template management

#### Task 1.1: Template Cache/Registry
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-private.hpp`

Add to `ClangToSageTranslator` class:
```cpp
private:
    // Template declaration cache - maps template name to SgTemplateClassDeclaration
    // Key: mangled template name (e.g., "std::array")
    // Value: Template class declaration
    std::map<std::string, SgTemplateClassDeclaration*> p_template_decl_cache;

    // Template instantiation cache - maps instantiation signature to SgTemplateInstantiationDecl
    // Key: mangled instantiation name (e.g., "std::array<double, 1024>")
    // Value: Template instantiation declaration
    std::map<std::string, SgTemplateInstantiationDecl*> p_template_inst_cache;

    // Helper: Get or create template class declaration
    SgTemplateClassDeclaration* getOrCreateTemplateDeclaration(
        const std::string& template_name,
        const clang::TemplateSpecializationType* clang_type);

    // Helper: Get or create template instantiation
    SgTemplateInstantiationDecl* getOrCreateTemplateInstantiation(
        SgTemplateClassDeclaration* template_decl,
        const clang::TemplateSpecializationType* clang_type);

    // Helper: Build template arguments from Clang
    SgTemplateArgumentPtrList buildTemplateArguments(
        const clang::TemplateSpecializationType* clang_type);
```

**Estimated Time**: 4 hours

#### Task 1.2: Template Name Mangling
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-type.cpp`

Create mangling utilities:
```cpp
namespace {
    // Generate unique name for template declaration
    std::string mangleTemplateName(const clang::TemplateName& tname) {
        std::string result;
        llvm::raw_string_ostream stream(result);
        tname.print(stream, clang::PrintingPolicy(clang::LangOptions()));
        stream.flush();
        return result;
    }

    // Generate unique name for template instantiation
    std::string mangleTemplateInstantiation(
        const std::string& template_name,
        const clang::TemplateSpecializationType* spec_type) {
        std::string result = template_name + "<";
        auto args = spec_type->template_arguments();
        bool first = true;
        for (const clang::TemplateArgument& arg : args) {
            if (!first) result += ", ";
            first = false;

            std::string arg_str;
            llvm::raw_string_ostream arg_stream(arg_str);
            arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
            arg_stream.flush();
            result += arg_str;
        }
        result += ">";
        return result;
    }
}
```

**Estimated Time**: 2 hours

### Phase 2: Template Declaration Creation (Week 1, Days 3-4)

**Goal**: Create `SgTemplateClassDeclaration` for template types

#### Task 2.1: Template Parameter Extraction
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-type.cpp`

```cpp
SgTemplateParameterPtrList*
ClangToSageTranslator::buildTemplateParameters(
    const clang::TemplateSpecializationType* clang_type) {

    // For Clang frontend, we don't have access to the original template parameter
    // declarations since they're in standard library headers. We need to infer
    // parameters from the instantiation arguments.

    SgTemplateParameterPtrList* param_list = new SgTemplateParameterPtrList();

    auto args = clang_type->template_arguments();
    int param_position = 0;

    for (const clang::TemplateArgument& arg : args) {
        SgType* param_type = nullptr;
        SgTemplateParameter::template_parameter_enum param_kind;

        switch (arg.getKind()) {
            case clang::TemplateArgument::Type:
                // Type parameter (e.g., typename T)
                param_kind = SgTemplateParameter::parameter_template_type;
                param_type = SageBuilder::buildTemplateType(
                    SgName("T" + std::to_string(param_position)));
                break;

            case clang::TemplateArgument::Integral:
                // Non-type parameter (e.g., size_t N)
                param_kind = SgTemplateParameter::parameter_nontype;
                param_type = buildTypeFromQualifiedType(arg.getIntegralType());
                break;

            case clang::TemplateArgument::Template:
                // Template template parameter
                param_kind = SgTemplateParameter::parameter_template;
                param_type = SageBuilder::buildTemplateType(
                    SgName("Template" + std::to_string(param_position)));
                break;

            default:
                std::cerr << "Warning: Unsupported template parameter kind: "
                          << arg.getKind() << std::endl;
                continue;
        }

        SgTemplateParameter* param = SageBuilder::buildTemplateParameter(
            param_kind, param_type);
        param_list->push_back(param);
        param_position++;
    }

    return param_list;
}
```

**Estimated Time**: 8 hours (complex logic, needs careful handling)

#### Task 2.2: Template Class Declaration Creation
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-type.cpp`

```cpp
SgTemplateClassDeclaration*
ClangToSageTranslator::getOrCreateTemplateDeclaration(
    const std::string& template_name,
    const clang::TemplateSpecializationType* clang_type) {

    // Check cache first
    auto it = p_template_decl_cache.find(template_name);
    if (it != p_template_decl_cache.end()) {
        return it->second;
    }

    // Extract just the base name (e.g., "array" from "std::array")
    size_t last_colon = template_name.find_last_of(':');
    std::string base_name = (last_colon != std::string::npos)
                            ? template_name.substr(last_colon + 1)
                            : template_name;

    // Build template parameters
    SgTemplateParameterPtrList* params = buildTemplateParameters(clang_type);

    // Create template class declaration
    SgTemplateClassDeclaration* template_decl =
        SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
            SgName(base_name),
            SgClassDeclaration::e_class,  // Assume class (could be struct)
            getGlobalScope(),
            params,
            nullptr  // No specialization arguments for primary template
        );

    // Mark as compiler generated and forward declaration
    template_decl->setForward();
    template_decl->set_isUnNamed(false);
    template_decl->get_file_info()->setCompilerGenerated();
    template_decl->get_file_info()->unsetOutputInCodeGeneration();

    // Insert into global scope symbol table
    SgTemplateSymbol* template_symbol = new SgTemplateSymbol(template_decl);
    getGlobalScope()->insert_symbol(SgName(base_name), template_symbol);

    // Cache it
    p_template_decl_cache[template_name] = template_decl;

    return template_decl;
}
```

**Estimated Time**: 12 hours (complex, needs testing)

### Phase 3: Template Instantiation Creation (Week 2, Days 1-3)

**Goal**: Create `SgTemplateInstantiationDecl` for each template use

#### Task 3.1: Template Argument Building
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-type.cpp`

```cpp
SgTemplateArgumentPtrList
ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateSpecializationType* clang_type) {

    SgTemplateArgumentPtrList arg_list;

    auto args = clang_type->template_arguments();
    for (const clang::TemplateArgument& arg : args) {
        SgTemplateArgument* sg_arg = nullptr;

        switch (arg.getKind()) {
            case clang::TemplateArgument::Type: {
                // Type argument (e.g., double)
                SgType* arg_type = buildTypeFromQualifiedType(arg.getAsType());
                sg_arg = new SgTemplateArgument(
                    SgTemplateArgument::template_type_argument,
                    nullptr,  // expression
                    arg_type,
                    nullptr,  // template decl
                    nullptr,  // template name
                    false     // is explicit
                );
                break;
            }

            case clang::TemplateArgument::Integral: {
                // Non-type argument (e.g., 1024)
                llvm::APSInt value = arg.getAsIntegral();
                SgType* int_type = buildTypeFromQualifiedType(arg.getIntegralType());

                // Create integer literal expression
                SgExpression* value_expr = SageBuilder::buildIntVal(
                    value.getLimitedValue());

                sg_arg = new SgTemplateArgument(
                    SgTemplateArgument::nontype_argument,
                    value_expr,
                    int_type,
                    nullptr, nullptr, false
                );
                break;
            }

            case clang::TemplateArgument::Template: {
                // Template template argument
                // This is complex - may need separate implementation
                std::cerr << "Warning: Template template arguments not yet supported\n";
                continue;
            }

            default:
                std::cerr << "Warning: Unsupported template argument kind\n";
                continue;
        }

        if (sg_arg) {
            arg_list.push_back(sg_arg);
        }
    }

    return arg_list;
}
```

**Estimated Time**: 10 hours

#### Task 3.2: Template Instantiation Creation
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-type.cpp`

```cpp
SgTemplateInstantiationDecl*
ClangToSageTranslator::getOrCreateTemplateInstantiation(
    SgTemplateClassDeclaration* template_decl,
    const clang::TemplateSpecializationType* clang_type) {

    // Generate mangled name for cache lookup
    std::string template_name = template_decl->get_name().getString();
    std::string inst_name = mangleTemplateInstantiation(template_name, clang_type);

    // Check cache
    auto it = p_template_inst_cache.find(inst_name);
    if (it != p_template_inst_cache.end()) {
        return it->second;
    }

    // Build template arguments
    SgTemplateArgumentPtrList args = buildTemplateArguments(clang_type);

    // Create template instantiation declaration
    // Note: This is a forward declaration of the instantiated class
    SgTemplateInstantiationDecl* inst_decl =
        new SgTemplateInstantiationDecl(
            SgName(inst_name),
            SgClassDeclaration::e_class,
            nullptr,  // type (will be set later)
            nullptr   // previousDeclaration
        );

    // Set template-specific information
    inst_decl->set_templateDeclaration(template_decl);
    inst_decl->set_templateArguments(args);

    // Mark as compiler generated
    inst_decl->setForward();
    inst_decl->get_file_info()->setCompilerGenerated();
    inst_decl->get_file_info()->unsetOutputInCodeGeneration();

    // Set scope
    inst_decl->set_scope(getGlobalScope());

    // Create class type pointing to this instantiation
    SgClassType* class_type = SgClassType::createType(inst_decl);
    inst_decl->set_type(class_type);

    // Create symbol and insert into symbol table
    SgClassSymbol* class_symbol = new SgClassSymbol(inst_decl);
    getGlobalScope()->insert_symbol(SgName(inst_name), class_symbol);

    // Cache it
    p_template_inst_cache[inst_name] = inst_decl;

    return inst_decl;
}
```

**Estimated Time**: 12 hours

### Phase 4: Integration with Type System (Week 2, Days 4-5)

**Goal**: Replace opaque type creation with template instantiation

#### Task 4.1: Modify VisitTemplateSpecializationType
**File**: `src/frontend/CxxFrontend/Clang/clang-frontend-type.cpp` (lines 941-996)

Replace current implementation:
```cpp
bool ClangToSageTranslator::VisitTemplateSpecializationType(
    clang::TemplateSpecializationType * template_specialization_type,
    SgNode ** node) {

#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::TemplateSpecializationType" << std::endl;
#endif

    // Try to desugar first (might give us RecordType)
    clang::QualType desugared = template_specialization_type->desugar();
    if (!desugared.isNull() &&
        desugared.getTypePtr() != template_specialization_type) {
        SgNode *desugared_node = Traverse(desugared.getTypePtr());
        if (desugared_node != NULL) {
            *node = desugared_node;
            return VisitType(template_specialization_type, node);
        }
    }

    // Extract template name
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    std::string template_name = mangleTemplateName(tname);

    // Get or create template class declaration
    SgTemplateClassDeclaration* template_decl =
        getOrCreateTemplateDeclaration(template_name, template_specialization_type);

    // Get or create template instantiation
    SgTemplateInstantiationDecl* inst_decl =
        getOrCreateTemplateInstantiation(template_decl, template_specialization_type);

    // Return the class type
    *node = inst_decl->get_type();
    ROSE_ASSERT(*node != nullptr);

    return VisitType(template_specialization_type, node);
}
```

**Estimated Time**: 6 hours (modification + testing)

### Phase 5: Testing and Refinement (Week 3)

**Goal**: Comprehensive testing and bug fixes

#### Task 5.1: Unit Tests
Create test cases in `tests/nonsmoke/functional/CompileTests/Cxx_tests/`:

```cpp
// test_template_01.cpp - Basic template instantiation
#include <vector>
int main() {
    std::vector<int> v;
    return 0;
}

// test_template_02.cpp - Multiple template arguments
#include <array>
int main() {
    std::array<double, 10> arr;
    return 0;
}

// test_template_03.cpp - Nested templates
#include <vector>
int main() {
    std::vector<std::vector<int>> matrix;
    return 0;
}

// test_template_04.cpp - Template with non-type parameters
template<typename T, int N>
class MyArray {
    T data[N];
};

int main() {
    MyArray<double, 5> arr;
    return 0;
}
```

**Estimated Time**: 16 hours (create tests, fix bugs)

#### Task 5.2: Symbol Table Verification
**File**: `tests/template_symbol_test.cpp`

Verify symbol table contains proper entries:
```cpp
// Test that symbols are created correctly
void testSymbolTable(SgProject* project) {
    // Find variable declaration
    SgVariableDeclaration* var_decl = /* ... */;
    SgInitializedName* init_name = var_decl->get_variables()[0];

    // Check type is SgClassType pointing to template instantiation
    SgClassType* class_type = isSgClassType(init_name->get_type());
    ASSERT(class_type != nullptr);

    SgTemplateInstantiationDecl* inst =
        isSgTemplateInstantiationDecl(class_type->get_declaration());
    ASSERT(inst != nullptr);

    // Check symbol exists
    SgVariableSymbol* var_symbol = init_name->search_for_symbol_from_symbol_table();
    ASSERT(var_symbol != nullptr);

    // Verify variable reference can find symbol
    SgVarRefExp* var_ref = /* ... */;
    ASSERT(var_ref->get_symbol() == var_symbol);
}
```

**Estimated Time**: 8 hours

#### Task 5.3: Unparsing Verification
Verify generated code is correct:
```cpp
// Input:
std::array<double, 1024> x;

// Expected output:
std::array<double, 1024> x;

// NOT:
struct array x;  // Current buggy output
```

**Estimated Time**: 12 hours (may need unparsing fixes)

## Expected Challenges

### Challenge 1: Standard Library Headers
**Problem**: Template declarations are in standard library headers not included in AST
**Solution**: Create synthetic template declarations based on instantiation information

### Challenge 2: Template Argument Deduction
**Problem**: Some template arguments may be deduced/defaulted
**Solution**: Use Clang's `getTemplateArgs()` which provides complete argument list

### Challenge 3: Nested Templates
**Problem**: `std::vector<std::vector<int>>` requires recursive instantiation
**Solution**: Process template arguments recursively, creating instantiations as needed

### Challenge 4: Template Specializations
**Problem**: Partial/full specializations have different structure
**Solution**: Phase 1 focuses on primary templates only; specializations are future work

### Challenge 5: Symbol Table Scope
**Problem**: Where should template instantiations be inserted?
**Solution**: Use global scope for standard library templates; use namespace scope for user templates

## Success Criteria

### Minimum Viable Product (MVP)
- ✅ `std::array<T, N>` instantiations create proper `SgTemplateInstantiationDecl`
- ✅ Variable declarations with template types unparse correctly
- ✅ Symbol table contains entries for template-typed variables
- ✅ Variable references resolve to correct symbols (no more "42")

### Full Success
- ✅ Standard library containers work (`vector`, `map`, `array`, etc.)
- ✅ Nested template instantiations work
- ✅ Non-type template parameters work (e.g., `array<T, N>`)
- ✅ Template-dependent member access works
- ✅ All axpy.cpp test generates 100% correct code

## Timeline

### Week 1: Foundation (40 hours)
- Days 1-2: Infrastructure setup (cache, mangling)
- Days 3-4: Template declaration creation
- Day 5: Initial testing and debugging

### Week 2: Core Implementation (40 hours)
- Days 1-3: Template instantiation creation
- Days 4-5: Type system integration

### Week 3: Testing and Refinement (40-60 hours)
- Days 1-2: Unit test development
- Days 3-4: Bug fixing and refinement
- Day 5: Documentation and code review

**Total: 120-140 hours (3 weeks at 40 hrs/week)**

## Code Organization

### New Files
```
src/frontend/CxxFrontend/Clang/
    clang-frontend-template.cpp      // Template-specific helper functions
    clang-frontend-template.hpp      // Template helper declarations
```

### Modified Files
```
src/frontend/CxxFrontend/Clang/
    clang-frontend-private.hpp       // Add template cache members
    clang-frontend-type.cpp          // Modify VisitTemplateSpecializationType
    clang-frontend.cpp               // Initialize template caches
```

## Dependencies

### ROSE Components Used
- `SgTemplateClassDeclaration`
- `SgTemplateInstantiationDecl`
- `SgTemplateParameter`
- `SgTemplateArgument`
- `SgTemplateSymbol`
- `SgClassType`
- `SageBuilder::buildTemplateParameter()`
- `SageBuilder::buildNondefiningTemplateClassDeclaration_nfi()`

### Clang Components Used
- `clang::TemplateSpecializationType`
- `clang::ClassTemplateSpecializationDecl`
- `clang::TemplateName`
- `clang::TemplateArgument`

## References

### ROSE Documentation
- Template handling: `docs/Rose/Tutorial/templateSupport.pdf`
- AST structure: `docs/Rose/ROSE_UserManual/ROSE-UserManual.pdf`

### ROSE Source Code Examples
- EDG frontend: `src/frontend/CxxFrontend/EDG/edgRose/edgRoseTemplate.C`
- Template post-processing: `src/frontend/SageIII/astPostProcessing/fixupTemplateInstantiations.C`

### Clang Documentation
- Template AST: https://clang.llvm.org/doxygen/classclang_1_1TemplateSpecializationType.html

## Next Steps After Completion

Once template instantiation support is complete:

1. **Template Functions**: Extend to handle function template instantiations
2. **Template Specializations**: Add support for partial/full specializations
3. **SFINAE**: Implement substitution failure handling
4. **Concepts** (C++20): Add concept checking support
5. **Template Metaprogramming**: Handle complex compile-time computations

## Conclusion

Implementing template instantiation support is a significant undertaking but will dramatically improve REX's C++ support. The phased approach outlined here provides a clear path from current opaque types to proper template infrastructure, with testable milestones at each phase.

**Key Insight**: Rather than trying to replicate EDG's full template analysis, we leverage Clang's instantiated templates and create just enough ROSE infrastructure to represent them correctly in the AST and symbol table.
