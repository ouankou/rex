#include "sage3basic.h"
#include "clang-frontend-private.hpp"

#include <cctype>

namespace {
    // Generate unique name for template declaration with full namespace qualification
    std::string mangleTemplateName(const clang::TemplateName& tname) {
        // Get fully qualified name from the underlying TemplateDecl
        if (clang::TemplateDecl* template_decl = tname.getAsTemplateDecl()) {
            // Get qualified name from the declaration (includes namespace)
            std::string result = template_decl->getQualifiedNameAsString();
            return result;
        }

        // Fallback: just use the template name without qualification
        std::string result;
        llvm::raw_string_ostream stream(result);
        clang::LangOptions opts;
        clang::PrintingPolicy policy(opts);
        tname.print(stream, policy);
        stream.flush();
        return result;
    }

    // Generate unique name for template instantiation
    // Note: Must not contain < > characters for ROSE mangling
    std::string mangleTemplateInstantiation(
        const std::string& template_name,
        const clang::TemplateSpecializationType* spec_type) {
        std::string result = template_name + "_";
        auto args = spec_type->template_arguments();
        bool first = true;
        for (const clang::TemplateArgument& arg : args) {
            if (!first) result += "_";
            first = false;

            std::string arg_str;
            llvm::raw_string_ostream arg_stream(arg_str);
            arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, true);
            arg_stream.flush();

            // Replace special characters that can't be in mangled names
            for (char& c : arg_str) {
                if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' || c == '*' || c == '&') {
                    c = '_';
                }
            }
            result += arg_str;
        }
        return result;
    }
} // anonymous namespace

SgType * ClangToSageTranslator::buildTypeFromQualifiedType(const clang::QualType & qual_type) {
    SgNode * tmp_type = Traverse(qual_type.getTypePtr());
    SgType * type = isSgType(tmp_type);

    ROSE_ASSERT(type != NULL); 

    if (qual_type.hasLocalQualifiers()) {
        SgModifierType * modified_type = new SgModifierType(type);
        SgTypeModifier & sg_modifer = modified_type->get_typeModifier();
        clang::Qualifiers qualifier = qual_type.getLocalQualifiers();

        if (qualifier.hasConst()) sg_modifer.get_constVolatileModifier().setConst();
        if (qualifier.hasVolatile()) sg_modifer.get_constVolatileModifier().setVolatile();
        if (qualifier.hasRestrict()) sg_modifer.setRestrict();
        
        if (qualifier.hasAddressSpace()) {
            clang::LangAS addrspace = qualifier.getAddressSpace();
            switch (addrspace) {
                case clang::LangAS::opencl_global:
                    sg_modifer.setOpenclGlobal();
                    break;
                case clang::LangAS::opencl_local:
                    sg_modifer.setOpenclLocal();
                    break;
                case clang::LangAS::opencl_constant:
                    sg_modifer.setOpenclConstant();
                    break;
                default:
                    sg_modifer.setAddressSpace();
                    sg_modifer.set_address_space_value(static_cast<unsigned int>(addrspace));
            }
        }
        modified_type = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

        return modified_type;
    }
    else {
        return type;
    }
}

SgNode * ClangToSageTranslator::Traverse(const clang::Type * type) {
    if (type == NULL)
        return NULL;

    std::map<const clang::Type *, SgNode *>::iterator it = p_type_translation_map.find(type);
#if DEBUG_TRAVERSE_TYPE
    std::cerr << "Traverse Type : " << type << " " << type->getTypeClassName ()<< std::endl;
#endif
    if (it != p_type_translation_map.end()) {
#if DEBUG_TRAVERSE_TYPE
      std::cerr << " already visited : node = " << it->second << std::endl;
#endif
      return it->second;
    }

    SgNode * result = NULL;
    bool ret_status = false;

    switch (type->getTypeClass()) {
        case clang::Type::Decayed:
            ret_status = VisitDecayedType((clang::DecayedType *)type, &result);
            break;
        case clang::Type::ConstantArray:
            ret_status = VisitConstantArrayType((clang::ConstantArrayType *)type, &result);
            break;
        case clang::Type::DependentSizedArray:
            ret_status = VisitDependentSizedArrayType((clang::DependentSizedArrayType *)type, &result);
            break;
       case clang::Type::IncompleteArray:
            ret_status = VisitIncompleteArrayType((clang::IncompleteArrayType *)type, &result);
            break;
        case clang::Type::VariableArray:
            ret_status = VisitVariableArrayType((clang::VariableArrayType *)type, &result);
            break;
        case clang::Type::Atomic:
            ret_status = VisitAtomicType((clang::AtomicType *)type, &result);
            break;
        case clang::Type::Attributed:
            ret_status = VisitAttributedType((clang::AttributedType *)type, &result);
            break;
        case clang::Type::BlockPointer:
            ret_status = VisitBlockPointerType((clang::BlockPointerType *)type, &result);
            break;
        case clang::Type::Builtin:
            ret_status = VisitBuiltinType((clang::BuiltinType *)type, &result);
            break;
        case clang::Type::Complex:
            ret_status = VisitComplexType((clang::ComplexType *)type, &result);
            break;
        case clang::Type::Decltype:
            ret_status = VisitDecltypeType((clang::DecltypeType *)type, &result);
            break;
     // case clang::Type::DependentDecltype:
     //     ret_status = VisitDependentDecltypeType((clang::DependentDecltypeType *)type, &result);
     //     break;
        case clang::Type::Auto:
            ret_status = VisitAutoType((clang::AutoType *)type, &result);
            break;
        case clang::Type::DeducedTemplateSpecialization:
            ret_status = VisitDeducedTemplateSpecializationType((clang::DeducedTemplateSpecializationType *)type, &result);
            break;
        case clang::Type::DependentSizedExtVector:
            ret_status = VisitDependentSizedExtVectorType((clang::DependentSizedExtVectorType *)type, &result);
            break;
        case clang::Type::DependentVector:
            ret_status = VisitDependentVectorType((clang::DependentVectorType *)type, &result);
            break;
        case clang::Type::FunctionNoProto:
            ret_status = VisitFunctionNoProtoType((clang::FunctionNoProtoType *)type, &result);
            break;
        case clang::Type::FunctionProto:
            ret_status = VisitFunctionProtoType((clang::FunctionProtoType *)type, &result);
            break;
        case clang::Type::InjectedClassName:
            ret_status = VisitInjectedClassNameType((clang::InjectedClassNameType *)type, &result);
            break;
     // case clang::Type::LocInfo:
     //     ret_status = VisitLocInfoType((clang::LocInfoType *)type, &result);
     //     break;
        case clang::Type::MacroQualified:
            ret_status = VisitMacroQualifiedType((clang::MacroQualifiedType *)type, &result);
            break;
        case clang::Type::MemberPointer:
            ret_status = VisitMemberPointerType((clang::MemberPointerType *)type, &result);
            break;
        case clang::Type::PackExpansion:
            ret_status = VisitPackExpansionType((clang::PackExpansionType *)type, &result);
            break;
        case clang::Type::Paren:
            ret_status = VisitParenType((clang::ParenType *)type, &result);
            break;
        case clang::Type::Pipe:
            ret_status = VisitPipeType((clang::PipeType *)type, &result);
            break;
        case clang::Type::Pointer:
            ret_status = VisitPointerType((clang::PointerType *)type, &result);
            break;
        case clang::Type::LValueReference:
            ret_status = VisitLValueReferenceType((clang::LValueReferenceType *)type, &result);
            break;
        case clang::Type::RValueReference:
            ret_status = VisitRValueReferenceType((clang::RValueReferenceType *)type, &result);
            break;
        case clang::Type::SubstTemplateTypeParmPack:
            ret_status = VisitSubstTemplateTypeParmPackType((clang::SubstTemplateTypeParmPackType *)type, &result);
            break;
        case clang::Type::SubstTemplateTypeParm:
            ret_status = VisitSubstTemplateTypeParmType((clang::SubstTemplateTypeParmType *)type, &result);
            break;
        case clang::Type::Enum:
            ret_status = VisitEnumType((clang::EnumType *)type, &result);
            break;
        case clang::Type::Record:
            ret_status = VisitRecordType((clang::RecordType *)type, &result);
            break;
        case clang::Type::TemplateSpecialization:
            ret_status = VisitTemplateSpecializationType((clang::TemplateSpecializationType *)type, &result);
            break;
        case clang::Type::TemplateTypeParm:
            ret_status = VisitTemplateTypeParmType((clang::TemplateTypeParmType *)type, &result);
            break;
        case clang::Type::Typedef:
            ret_status = VisitTypedefType((clang::TypedefType *)type, &result);
            break;
        case clang::Type::TypeOfExpr:
            ret_status = VisitTypeOfExprType((clang::TypeOfExprType *)type, &result);
            break;
    //  case clang::Type::DependentTypeOfExpr:
    //      ret_status = VisitDependentTypeOfExprType((clang::DependentTypeOfExprType *)type, &result);
    //      break;
        case clang::Type::TypeOf:
            ret_status = VisitTypeOfType((clang::TypeOfType *)type, &result);
            break;
        case clang::Type::DependentName:
            ret_status = VisitDependentNameType((clang::DependentNameType *)type, &result);
            break;
        case clang::Type::DependentTemplateSpecialization:
            ret_status = VisitDependentTemplateSpecializationType((clang::DependentTemplateSpecializationType *)type, &result);
            break;
        case clang::Type::Elaborated:
            ret_status = VisitElaboratedType((clang::ElaboratedType *)type, &result);
            break;
        case clang::Type::UnaryTransform:
            ret_status = VisitUnaryTransformType((clang::UnaryTransformType *)type, &result);
            break;
        case clang::Type::UnresolvedUsing:
            ret_status = VisitUnresolvedUsingType((clang::UnresolvedUsingType *)type, &result);
            break;
        case clang::Type::Vector:
            ret_status = VisitVectorType((clang::VectorType *)type, &result);
            break;
        case clang::Type::ExtVector:
            ret_status = VisitExtVectorType((clang::ExtVectorType *)type, &result);
            break;
        case clang::Type::Using:
            ret_status = VisitUsingType((clang::UsingType *)type, &result);
            break;

        default:
            std::cerr << "Warning: Unhandled clang::Type '" << type->getTypeClassName() << "'. Using opaque type." << std::endl;
            ret_status = true;
            break;
    }

    if (result == NULL) {
        result = SageBuilder::buildUnknownType();
    }

    p_type_translation_map.insert(std::pair<const clang::Type *, SgNode *>(type, result));

#if DEBUG_TRAVERSE_TYPE
    std::cerr << "Traverse(clang::Type : " << type << " ";
    std::cerr << " visit done : node = " << result << std::endl;
#endif
    return result;
}

/***************/
/* Visit Types */
/***************/

bool ClangToSageTranslator::VisitType(clang::Type * type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitType" << std::endl;
#endif

    if (*node == NULL) {
        std::cerr << "Runtime error: No Sage node associated with the type: " << type->getTypeClassName() << std::endl;
        return false;
    }
/*
    std::cerr << "Dump type " << type->getTypeClassName() << "(" << type << "): ";
    type->dump();
    std::cerr << std::endl;
*/
    // TODO

    return true;
}

bool ClangToSageTranslator::VisitAdjustedType(clang::AdjustedType * adjusted_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitAdjustedType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(adjusted_type, node) && res;
}

bool ClangToSageTranslator::VisitDecayedType(clang::DecayedType * decayed_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDecayedType" << std::endl;
#endif
    bool res = true;

//    SgType * decayType = buildTypeFromQualifiedType(decayed_type->getDecayedType ());
    SgType * pointeeType = buildTypeFromQualifiedType(decayed_type->getPointeeType ());

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

//    *node = pointeeType;
// Pei-Hung (04/08/2022) Building SgArrayyType to represent the DecayedType in Clang,
// in order to match the type of ParmVarDecl  in FunctionProtoType
// Might need to check the case when the pointeeType is a functionType
    if(decayed_type->getPointeeType()->getTypeClass() == clang::Type::VariableArray ||
       decayed_type->getPointeeType()->getTypeClass() == clang::Type::ConstantArray ||
       decayed_type->getPointeeType()->getTypeClass() == clang::Type::DependentSizedArray ||
       decayed_type->getPointeeType()->getTypeClass() == clang::Type::IncompleteArray 
       )
      *node = SageBuilder::buildArrayType(pointeeType);
    else 
      *node = pointeeType;

    return VisitAdjustedType(decayed_type, node) && res;
}

bool ClangToSageTranslator::VisitArrayType(clang::ArrayType * array_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitArrayType" << std::endl;
#endif
    bool res = true;

    // Array type handling is implemented in child visitor functions
    // (ConstantArrayType, VariableArrayType, DependentSizedArrayType, IncompleteArrayType)
    // which set *node before calling this base function
    // ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

 // DQ (11/28/2020): Added assertion.
    ROSE_ASSERT(*node != NULL);

    return VisitType(array_type, node) && res;
}

bool ClangToSageTranslator::VisitConstantArrayType(clang::ConstantArrayType * constant_array_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitConstantArrayType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(constant_array_type->getElementType());

    // TODO clang::ArrayType::ArraySizeModifier

    SgExpression * expr = SageBuilder::buildIntVal(constant_array_type->getSize().getSExtValue());

    *node = SageBuilder::buildArrayType(type, expr);

    return VisitArrayType(constant_array_type, node);
}

bool ClangToSageTranslator::VisitDependentSizedArrayType(clang::DependentSizedArrayType * dependent_sized_array_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDependentSizedArrayType" << std::endl;
#endif
    bool res = true;

    // Template-dependent array sizes (e.g., T arr[N] where N is a template parameter)
    // Create a placeholder array type with unknown size
    // ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    SgType * type = buildTypeFromQualifiedType(dependent_sized_array_type->getElementType());

    // Use buildArrayType without size expression to represent dependent-sized array
    *node = SageBuilder::buildArrayType(type);

    return VisitArrayType(dependent_sized_array_type, node) && res;
}

bool ClangToSageTranslator::VisitIncompleteArrayType(clang::IncompleteArrayType * incomplete_array_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitIncompleteArrayType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(incomplete_array_type->getElementType());

    // In LLVM 20, ArraySizeModifier moved from ArrayType:: to clang:: namespace

    clang::ArraySizeModifier sizeModifier = incomplete_array_type->getSizeModifier();

    if(sizeModifier == clang::ArraySizeModifier::Star)
    {
      SgExprListExp* exprListExp = SageBuilder::buildExprListExp(SageBuilder::buildNullExpression());
      *node = SageBuilder::buildArrayType(type, exprListExp);
    }
    else if(sizeModifier == clang::ArraySizeModifier::Static)
    {
      // TODO check how to handle Static
      *node = SageBuilder::buildArrayType(type);
    }
    else  // clang::ArraySizeModifier::Normal
    {
      *node = SageBuilder::buildArrayType(type);
    }



 // DQ (11/28/2020): Added assertion.
 // ROSE_ASSERT(*node != NULL);

    return VisitArrayType(incomplete_array_type, node);
}

bool ClangToSageTranslator::VisitVariableArrayType(clang::VariableArrayType * variable_array_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitVariableArrayType" << std::endl;
#endif
    bool res = true;

    SgType * type = buildTypeFromQualifiedType(variable_array_type->getElementType());

    SgNode* tmp_expr = Traverse(variable_array_type->getSizeExpr());
    SgExpression* array_size = isSgExpression(tmp_expr);

    SgArrayType* arrayType = SageBuilder::buildArrayType(type,array_size);
    arrayType->set_is_variable_length_array(true);
    *node = arrayType;

 // DQ (11/28/2020): Added assertion.
    ROSE_ASSERT(*node != NULL);

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitArrayType(variable_array_type, node) && res;
}

bool ClangToSageTranslator::VisitAtomicType(clang::AtomicType * atomic_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitAtomicType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(atomic_type, node) && res;
}

bool ClangToSageTranslator::VisitAttributedType(clang::AttributedType * attributed_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitAttributedType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(attributed_type->getModifiedType());

    SgModifierType * modified_type = SgModifierType::createType(type);
    SgTypeModifier & sg_modifer = modified_type->get_typeModifier();

//(01/29/2020) Pei-Hung needs to revisit this part.
/*
    switch (attributed_type->getAttrKind()) {
        case clang::AttributedType::attr_noreturn:             sg_modifer.setGnuAttributeNoReturn();      break;
        case clang::AttributedType::attr_cdecl:                sg_modifer.setGnuAttributeCdecl();         break;
        case clang::AttributedType::attr_stdcall:              sg_modifer.setGnuAttributeStdcall();       break;

        case clang::AttributedType::attr_address_space:
            std::cerr << "Unsupported attribute attr_address_space" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_regparm:
            std::cerr << "Unsupported attribute attr_regparm" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_vector_size:
            std::cerr << "Unsupported attribute attr_vector_size" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_neon_vector_type:
            std::cerr << "Unsupported attribute attr_neon_vector_type" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_neon_polyvector_type:
            std::cerr << "Unsupported attribute attr_neon_polyvector_type" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_objc_gc:
            std::cerr << "Unsupported attribute attr_objc_gc" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_objc_ownership:
            std::cerr << "Unsupported attribute attr_objc_ownership" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_pcs:
            std::cerr << "Unsupported attribute attr_pcs" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_fastcall:
            std::cerr << "Unsupported attribute attr_fastcall" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_thiscall:
            std::cerr << "Unsupported attribute attr_thiscall" << std::endl; ROSE_ASSERT(false);
        case clang::AttributedType::attr_pascal:
            std::cerr << "Unsupported attribute attr_pascal" << std::endl; ROSE_ASSERT(false);
        default:
            std::cerr << "Unknown attribute" << std::endl; ROSE_ASSERT(false);
    } 
*/
    *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);;

    return VisitType(attributed_type, node);
}

bool ClangToSageTranslator::VisitBlockPointerType(clang::BlockPointerType * block_pointer_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitBlockPointerType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(block_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitBuiltinType(clang::BuiltinType * builtin_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitBuiltinType" << std::endl;
#endif

    switch (builtin_type->getKind()) {
        case clang::BuiltinType::Void:       *node = SageBuilder::buildVoidType();             break;
        case clang::BuiltinType::Bool:       *node = SageBuilder::buildBoolType();             break;
        case clang::BuiltinType::Short:      *node = SageBuilder::buildShortType();            break;
        case clang::BuiltinType::Int:        *node = SageBuilder::buildIntType();              break;
        case clang::BuiltinType::Long:       *node = SageBuilder::buildLongType();             break;
        case clang::BuiltinType::LongLong:   *node = SageBuilder::buildLongLongType();         break;
        case clang::BuiltinType::Float:      *node = SageBuilder::buildFloatType();            break;
        case clang::BuiltinType::Double:     *node = SageBuilder::buildDoubleType();           break;
        case clang::BuiltinType::LongDouble: *node = SageBuilder::buildLongDoubleType();       break;

        case clang::BuiltinType::Char_S:     *node = SageBuilder::buildCharType();             break;

        case clang::BuiltinType::UInt:       *node = SageBuilder::buildUnsignedIntType();      break;
        case clang::BuiltinType::UChar:      *node = SageBuilder::buildUnsignedCharType();     break;
        case clang::BuiltinType::SChar:      *node = SageBuilder::buildSignedCharType();       break;
        case clang::BuiltinType::UShort:     *node = SageBuilder::buildUnsignedShortType();    break;
        case clang::BuiltinType::ULong:      *node = SageBuilder::buildUnsignedLongType();     break;
        case clang::BuiltinType::ULongLong:  *node = SageBuilder::buildUnsignedLongLongType(); break;
/*
        case clang::BuiltinType::NullPtr:    *node = SageBuilder::build(); break;
*/
        // TODO ROSE type ?
        case clang::BuiltinType::UInt128:    *node = SageBuilder::buildUnsignedLongLongType(); break;
        case clang::BuiltinType::Int128:     *node = SageBuilder::buildLongLongType();         break;
 
        // Wide character and Unicode types - use wchar for wide chars, int/long for char16/32
        case clang::BuiltinType::Char_U:    *node = SageBuilder::buildUnsignedCharType();   break;
        case clang::BuiltinType::WChar_U:   *node = SageBuilder::buildWcharType();          break;
        case clang::BuiltinType::WChar_S:   *node = SageBuilder::buildWcharType();          break;
        case clang::BuiltinType::Char16:    *node = SageBuilder::buildUnsignedShortType();  break; // char16_t is typically 16-bit
        case clang::BuiltinType::Char32:    *node = SageBuilder::buildUnsignedIntType();    break; // char32_t is typically 32-bit


        case clang::BuiltinType::ObjCId:
        case clang::BuiltinType::ObjCClass:
        case clang::BuiltinType::ObjCSel:
        case clang::BuiltinType::Dependent:
        case clang::BuiltinType::Overload:
        case clang::BuiltinType::BoundMember:
        case clang::BuiltinType::UnknownAny:
        default: {
            // Fallback for unknown builtin types (e.g., ARM SVE types, vendor extensions)
            std::string type_name = builtin_type->getName(p_compiler_instance->getLangOpts()).str();
            // Using fallback type for unknown builtin (suppressed)

            // Check if scope stack is initialized before building opaque type
            SgScopeStatement* scope = SageBuilder::topScopeStack();
            if (scope != nullptr) {
                // Build opaque type if we have a valid scope
                *node = SageBuilder::buildOpaqueType(type_name, scope);
            } else {
                // Fall back to int type if scope not yet initialized (early header processing)
                // Scope not initialized, using int type (suppressed)
                *node = SageBuilder::buildIntType();
            }
            break;
        }
    }

    return VisitType(builtin_type, node);
}

bool ClangToSageTranslator::VisitComplexType(clang::ComplexType * complex_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitComplexType" << std::endl;
#endif

    bool res = true;

    SgType * type = buildTypeFromQualifiedType(complex_type->getElementType());

    *node = SageBuilder::buildComplexType(type);

    return VisitType(complex_type, node) && res;
}

bool ClangToSageTranslator::VisitDecltypeType(clang::DecltypeType * decltype_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDecltypeType" << std::endl;
#endif
    bool res = true;

    // TODO: Full support for decltype not yet implemented
    // decltype(expr) deduces the type of an expression
    // For now, use a generic unknown type scoped to global scope to avoid ROSE-1378
    *node = SageBuilder::buildOpaqueType("decltype", getGlobalScope());

    return VisitType(decltype_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentDecltypeType(clang::DependentDecltypeType * dependent_decltype_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDependentDecltypeType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitDecltypeType(dependent_decltype_type, node) && res;
}

bool ClangToSageTranslator::VisitDeducedType(clang::DeducedType * deduced_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDeducedType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(deduced_type, node) && res;
}

bool ClangToSageTranslator::VisitAutoType(clang::AutoType * auto_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitAutoType" << std::endl;
#endif
    bool res = true;

    // TODO: Full support for auto type deduction not yet implemented
    // auto keyword (C++11) allows type to be deduced from initializer
    // For now, use a generic unknown type scoped to global scope
    *node = SageBuilder::buildOpaqueType("auto", getGlobalScope());

    return VisitDeducedType(auto_type, node) && res;
}

bool ClangToSageTranslator::VisitDeducedTemplateSpecializationType(clang::DeducedTemplateSpecializationType * deduced_template_specialization_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDeducedTemplateSpecializationType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitDeducedType(deduced_template_specialization_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentAddressSpaceType(clang::DependentAddressSpaceType * dependent_address_space_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitDependentAddressSpaceType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(dependent_address_space_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentSizedExtVectorType(clang::DependentSizedExtVectorType * dependent_sized_ext_vector_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentSizedExtVectorType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(dependent_sized_ext_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentVectorType(clang::DependentVectorType * dependent_vector_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentVectorType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(dependent_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionType(clang::FunctionType * function_type, SgNode ** node)  {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitFunctionType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitType(function_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionNoProtoType(clang::FunctionNoProtoType * function_no_proto_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitFunctionNoProtoType" << std::endl;
#endif

    bool res = true;

    SgFunctionParameterTypeList * param_type_list = new SgFunctionParameterTypeList();

    SgType * ret_type = buildTypeFromQualifiedType(function_no_proto_type->getReturnType()); 

    *node = SageBuilder::buildFunctionType(ret_type, param_type_list);

    return VisitType(function_no_proto_type, node) && res;
}

bool ClangToSageTranslator::VisitFunctionProtoType(clang::FunctionProtoType * function_proto_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitFunctionProtoType" << std::endl;
#endif

    bool res = true;
    SgFunctionParameterTypeList * param_type_list = new SgFunctionParameterTypeList();
    for (unsigned i = 0; i < function_proto_type->getNumParams(); i++) {
#if DEBUG_VISIT_TYPE
        std::cerr << "funcProtoType: " << i << " th param" << std::endl;
#endif
        SgType * param_type = buildTypeFromQualifiedType(function_proto_type->getParamType(i));

        param_type_list->append_argument(param_type);
    }

    if (function_proto_type->isVariadic()) {
        param_type_list->append_argument(SgTypeEllipse::createType());
    }

    SgType * ret_type = buildTypeFromQualifiedType(function_proto_type->getReturnType());

    SgFunctionType * func_type = SageBuilder::buildFunctionType(ret_type, param_type_list);
    if (function_proto_type->isVariadic()) func_type->set_has_ellipses(1);

    *node = func_type;

    return VisitType(function_proto_type, node) && res;
}

bool ClangToSageTranslator::VisitInjectedClassNameType(clang::InjectedClassNameType * injected_class_name_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::InjectedClassNameType" << std::endl;
#endif
    bool res = true;

    // InjectedClassName represents a class referring to itself within its own definition (e.g., in member functions)
    // Desugar to get the actual instantiated type
    *node = Traverse(injected_class_name_type->getInjectedSpecializationType().getTypePtr());

    return VisitType(injected_class_name_type, node) && res;
}

// LocInfoType was removed in LLVM 20
/*
bool ClangToSageTranslator::VisitLocInfoType(clang::LocInfoType * loc_info_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::LocInfoType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitType(loc_info_type, node) && res;
}
*/

bool ClangToSageTranslator::VisitMacroQualifiedType(clang::MacroQualifiedType * macro_qualified_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::MacroQualifiedType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(macro_qualified_type, node) && res;
}

bool ClangToSageTranslator::VisitMemberPointerType(clang::MemberPointerType * member_pointer_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::MemberPointerType" << std::endl;
#endif
    bool res = true;

    // TODO: Full support for member pointers not yet implemented
    // Member pointers (e.g., int Class::*) point to class members
    // For now, use a generic unknown type scoped to global scope
    *node = SageBuilder::buildOpaqueType("member_pointer", getGlobalScope());

    return VisitType(member_pointer_type, node) && res;
}

bool ClangToSageTranslator::VisitPackExpansionType(clang::PackExpansionType * pack_expansion_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::PackExpansionType" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: Pack expansion types (e.g., Args... in variadic templates)
    // represent template parameter packs that are expanded
    // Try to get the pattern type (the type being expanded)
    clang::QualType pattern = pack_expansion_type->getPattern();
    SgType* pattern_type = buildTypeFromQualifiedType(pattern);

    if (pattern_type != NULL) {
        // Use the pattern type directly - the pack expansion is handled at a higher level
        *node = pattern_type;
    } else {
        // Fallback: use opaque type if pattern translation fails
        *node = SageBuilder::buildOpaqueType("pack_expansion", getGlobalScope());
    }

    return VisitType(pack_expansion_type, node) && res;
}

bool ClangToSageTranslator::VisitParenType(clang::ParenType * paren_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitParenType" << std::endl;
#endif

    *node = buildTypeFromQualifiedType(paren_type->getInnerType());

    return VisitType(paren_type, node);
}

bool ClangToSageTranslator::VisitPipeType(clang::PipeType * pipe_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::PipeType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(pipe_type, node) && res;
}

bool ClangToSageTranslator::VisitPointerType(clang::PointerType * pointer_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitPointerType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(pointer_type->getPointeeType());

    *node = SageBuilder::buildPointerType(type);

    return VisitType(pointer_type, node);
}

bool ClangToSageTranslator::VisitReferenceType(clang::ReferenceType * reference_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::ReferenceType" << std::endl;
#endif

    SgType * pointee_type = buildTypeFromQualifiedType(reference_type->getPointeeType());
    if (pointee_type == nullptr) {
        return false;
    }

    if (clang::isa<clang::RValueReferenceType>(reference_type)) {
        *node = SageBuilder::buildRvalueReferenceType(pointee_type);
    } else {
        *node = SageBuilder::buildReferenceType(pointee_type);
    }

    return VisitType(reference_type, node);
}

bool ClangToSageTranslator::VisitLValueReferenceType(clang::LValueReferenceType * lvalue_reference_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::LValueReferenceType" << std::endl;
#endif
    return VisitReferenceType(lvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitRValueReferenceType(clang::RValueReferenceType * rvalue_reference_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::RValueReferenceType" << std::endl;
#endif
    return VisitReferenceType(rvalue_reference_type, node);
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmPackType(clang::SubstTemplateTypeParmPackType * subst_template_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::SubstTemplateTypeParmPackType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(subst_template_type, node) && res;
}

bool ClangToSageTranslator::VisitSubstTemplateTypeParmType(clang::SubstTemplateTypeParmType * subst_template_type_parm_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::SubstTemplateTypeParmType" << std::endl;
#endif

    // SubstTemplateTypeParmType represents a type where a template parameter has been
    // substituted with a concrete type. We simply traverse to the replacement type.
    clang::QualType replacement_type = subst_template_type_parm_type->getReplacementType();
    *node = Traverse(replacement_type.getTypePtr());

    return VisitType(subst_template_type_parm_type, node);
}

bool ClangToSageTranslator::VisitTagType(clang::TagType * tag_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitTagType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitType(tag_type, node) && res;
}

bool ClangToSageTranslator::VisitEnumType(clang::EnumType * enum_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitEnumType" << std::endl;
#endif

    SgSymbol * sym = GetSymbolFromSymbolTable(enum_type->getDecl());

    SgEnumSymbol * enum_sym = isSgEnumSymbol(sym);

    if (enum_sym == NULL) {
        SgNode * tmp_decl = Traverse(enum_type->getDecl());
        SgEnumDeclaration * sg_decl = isSgEnumDeclaration(tmp_decl);

        ROSE_ASSERT(sg_decl != NULL);
        *node = sg_decl->get_type();
    }
    else {
        *node = enum_sym->get_type();
    }

    if (isSgEnumType(*node) != NULL) {
        if (enum_sym == NULL) {
            p_enum_type_decl_first_see_in_type.insert(std::pair<SgEnumType *, bool>(isSgEnumType(*node), true));
        }
        else
            p_enum_type_decl_first_see_in_type.insert(std::pair<SgEnumType *, bool>(isSgEnumType(*node), false));
    }

    return VisitType(enum_type, node);
}

bool ClangToSageTranslator::VisitRecordType(clang::RecordType * record_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitRecordType" << std::endl;
#endif

    SgSymbol * sym = GetSymbolFromSymbolTable(record_type->getDecl());

    SgClassSymbol * class_sym = isSgClassSymbol(sym);

    if (class_sym == NULL) {
        clang::RecordDecl *record_decl = record_type->getDecl();
        SgNode * tmp_decl = Traverse(record_decl);
        SgClassDeclaration * sg_decl = isSgClassDeclaration(tmp_decl);

        if (sg_decl != NULL) {
            // Ensure firstNondefiningDeclaration is set before calling get_type()
            // which internally calls createType() and asserts on this pointer
            if (sg_decl->get_firstNondefiningDeclaration() == NULL) {
                // For template specializations and forward declarations without separate non-defining decl
                // use the declaration itself as the first non-defining declaration
                sg_decl->set_firstNondefiningDeclaration(sg_decl);
            }
            *node = sg_decl->get_type();
        } else {
            std::string qualified_name = record_decl->getQualifiedNameAsString();
            if (qualified_name.empty()) {
                qualified_name = "__anonymous_record";
            }
            // std::isalnum expects values representable as unsigned char; cast to avoid UB for negative char.
            for (char &ch : qualified_name) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
                    ch = '_';
                }
            }
            SgScopeStatement *scope = SageBuilder::topScopeStack();
            if (scope == NULL) {
                scope = p_global_scope;
            }
            *node = SageBuilder::buildOpaqueType(qualified_name, scope);
        }
    }
    else {
        *node = class_sym->get_type();
    }

    if (isSgClassType(*node) != NULL) {
        if (class_sym == NULL) {
            p_class_type_decl_first_see_in_type.insert(std::pair<SgClassType *, bool>(isSgClassType(*node), true));
            isSgNamedType(*node)->set_autonomous_declaration(true);
        }
        else
            p_class_type_decl_first_see_in_type.insert(std::pair<SgClassType *, bool>(isSgClassType(*node), false));
    }

    return VisitType(record_type, node);
}

// Build template parameters by inferring from instantiation arguments
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
                param_kind = SgTemplateParameter::type_parameter;
                param_type = SageBuilder::buildTemplateType(
                    SgName("T" + std::to_string(param_position)));
                break;

            case clang::TemplateArgument::Integral:
                // Non-type parameter (e.g., size_t N)
                param_kind = SgTemplateParameter::nontype_parameter;
                param_type = buildTypeFromQualifiedType(arg.getIntegralType());
                break;

            case clang::TemplateArgument::Template:
                // Template template parameter
                param_kind = SgTemplateParameter::template_parameter;
                param_type = SageBuilder::buildTemplateType(
                    SgName("Template" + std::to_string(param_position)));
                break;

            case clang::TemplateArgument::Pack:
                // Parameter pack - skip for now as ROSE doesn't fully support variadic templates
                // Skipping parameter pack (variadic templates not fully supported)
                continue;

            case clang::TemplateArgument::Expression:
            case clang::TemplateArgument::NullPtr:
            case clang::TemplateArgument::Declaration:
                // These are less common - treat as nontype parameters
                param_kind = SgTemplateParameter::nontype_parameter;
                param_type = SageBuilder::buildIntType();
                break;

            default:
                // Unsupported template parameter kind (suppressed)
                continue;
        }

        SgTemplateParameter* param = SageBuilder::buildTemplateParameter(
            param_kind, param_type);
        param_list->push_back(param);
        param_position++;
    }

    return param_list;
}

// Get or create template class declaration
SgTemplateClassDeclaration*
ClangToSageTranslator::getOrCreateTemplateDeclaration(
    const std::string& template_name,
    const clang::TemplateSpecializationType* clang_type) {

    // Check cache first
    auto it = p_template_decl_cache.find(template_name);
    if (it != p_template_decl_cache.end()) {
        // DEBUG: // std::cerr << "DEBUG: CACHE HIT for template_name = '" << template_name << "'" << std::endl;
        return it->second;
    }
    // DEBUG: // std::cerr << "DEBUG: CACHE MISS for template_name = '" << template_name << "' - creating new" << std::endl;

    // Extract namespace prefix and base name (e.g., "std" and "array" from "std::array")
    std::string namespace_prefix;
    std::string base_name;

    // DEBUG: // std::cerr << "DEBUG: getOrCreateTemplateDeclaration: template_name = '" << template_name << "'" << std::endl;

    size_t last_colon = template_name.find_last_of(':');
    if (last_colon != std::string::npos && last_colon > 0 && template_name[last_colon-1] == ':') {
        // Has namespace prefix
        namespace_prefix = template_name.substr(0, last_colon - 1);
        base_name = template_name.substr(last_colon + 1);
    } else {
        // No namespace prefix
        base_name = template_name;
    }

    // DEBUG: // std::cerr << "DEBUG: namespace_prefix = '" << namespace_prefix << "', base_name = '" << base_name << "'" << std::endl;

    // Build template parameters
    SgTemplateParameterPtrList* params = buildTemplateParameters(clang_type);

    // For primary template, use empty template argument list (not nullptr)
    SgTemplateArgumentPtrList* empty_args = new SgTemplateArgumentPtrList();

    // WORKAROUND: Create template in global scope to avoid SageBuilder assertion issues
    // When passing namespace scope to buildNondefiningTemplateClassDeclaration_nfi, it creates
    // internal declarations with mismatched variant types causing assertion failures.
    // Instead, we'll store the namespace prefix in the type's globalQualifiedNameMapForTypes
    // which the unparser uses for name qualification.
    SgTemplateClassDeclaration* template_decl =
        SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
            SgName(base_name),
            SgClassDeclaration::e_class,  // Assume class (could be struct)
            getGlobalScope(),  // Use global scope to avoid assertion failures
            params,
            empty_args  // Empty list for primary template (not a specialization)
        );

    // Set file info and mark as compiler generated
    Sg_File_Info* file_info = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    template_decl->set_file_info(file_info);
    template_decl->setForward();
    template_decl->set_isUnNamed(false);
    template_decl->set_definingDeclaration(nullptr);
    template_decl->set_firstNondefiningDeclaration(template_decl);

    // Store qualified name for unparser (includes namespace prefix)
    if (!namespace_prefix.empty()) {
        SgClassType* class_type = template_decl->get_type();
        if (class_type != nullptr) {
            // Store qualified name in global map for unparser to find
            std::string qualified_name = namespace_prefix + "::" + base_name;

            // Add to global qualified name map
            std::map<SgNode*,std::string>& typeMap = SgNode::get_globalQualifiedNameMapForTypes();
            typeMap[class_type] = qualified_name;

            // DEBUG: // std::cerr << "DEBUG: Set qualified name '" << qualified_name << "' for type" << std::endl;
        }
    }

    // Note: We don't insert a template symbol for the primary template declaration
    // because it's not a SgTemplateDeclaration type and we're just creating a
    // synthetic representation of standard library templates.

    // Cache it
    p_template_decl_cache[template_name] = template_decl;

    return template_decl;
}

// Build template arguments from Clang template instantiation
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
                sg_arg = new SgTemplateArgument(arg_type, false);
                break;
            }

            case clang::TemplateArgument::Integral: {
                // Non-type argument (e.g., 1024)
                llvm::APSInt value = arg.getAsIntegral();

                // Create integer literal expression
                SgExpression* value_expr = SageBuilder::buildIntVal(
                    value.getLimitedValue());

                sg_arg = new SgTemplateArgument(value_expr, false);
                break;
            }

            case clang::TemplateArgument::Template: {
                // Template template argument
                // This is complex - may need separate implementation
                // Template template arguments not yet supported
                continue;
            }

            case clang::TemplateArgument::Pack:
                // Parameter pack - skip
                continue;

            case clang::TemplateArgument::Expression: {
                // Expression argument (e.g., constexpr values, integer literals)
                clang::Expr* expr = arg.getAsExpr();
                if (expr) {
                    SgNode* sg_expr_node = Traverse(expr);
                    SgExpression* sg_expr = isSgExpression(sg_expr_node);
                    if (sg_expr) {
                        sg_arg = new SgTemplateArgument(sg_expr, false);
                    }
                }
                break;
            }
            case clang::TemplateArgument::Declaration:
            case clang::TemplateArgument::NullPtr:
                // These types are less common - skip for now
                continue;

            default:
                // Unknown template argument kind
                continue;
        }

        if (sg_arg) {
            arg_list.push_back(sg_arg);
        }
    }

    return arg_list;
}

// Get or create template instantiation declaration
SgTemplateInstantiationDecl*
ClangToSageTranslator::getOrCreateTemplateInstantiation(
    SgTemplateClassDeclaration* template_decl,
    const clang::TemplateSpecializationType* clang_type) {

    // Extract both base name and qualified name for the template
    std::string template_base_name = template_decl->get_name().getString();

    // ROOT CAUSE FIX: Check if template declaration has namespace qualification stored
    // Use qualified name (e.g., "std::array") for instantiation name instead of just base name
    std::string template_qualified_name = template_base_name;
    SgClassType* template_type = template_decl->get_type();
    if (template_type != nullptr) {
        std::map<SgNode*,std::string>& typeMap = SgNode::get_globalQualifiedNameMapForTypes();
        auto it = typeMap.find(template_type);
        if (it != typeMap.end()) {
            template_qualified_name = it->second;  // Use "std::array" instead of "array"
        }
    }

    std::string inst_name_full = mangleTemplateInstantiation(template_base_name, clang_type);

    // Check cache
    auto it = p_template_inst_cache.find(inst_name_full);
    if (it != p_template_inst_cache.end()) {
        return it->second;
    }

    // Build template arguments
    SgTemplateArgumentPtrList args = buildTemplateArguments(clang_type);

    // Create class type first (will be set on instantiation)
    SgClassType* class_type = nullptr;

    // Create template instantiation declaration with all parameters
    // ROOT CAUSE FIX: Use qualified name (e.g., "std::array") not just base name ("array")
    // This ensures the unparser outputs the correct namespace qualification
    SgTemplateInstantiationDecl* inst_decl =
        new SgTemplateInstantiationDecl(
            SgName(template_qualified_name),  // Use qualified name like "std::array"
            SgClassDeclaration::e_class,
            class_type,   // type (initially nullptr, will be set)
            nullptr,      // definition
            template_decl,
            args
        );

    // Set file info and mark as compiler generated
    // Create synthetic file info since this is a compiler-generated node
    Sg_File_Info* file_info = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    inst_decl->set_file_info(file_info);
    inst_decl->setForward();
    inst_decl->set_definingDeclaration(nullptr);
    inst_decl->set_firstNondefiningDeclaration(inst_decl);

    // ROOT CAUSE FIX: Set scope to namespace if qualified name has namespace prefix
    // Extract namespace from qualified name and find/create namespace scope
    SgScopeStatement* inst_scope = getGlobalScope();

    // Check if we have namespace prefix (e.g., "std" from "std::array")
    size_t colon_pos = template_qualified_name.find("::");
    if (colon_pos != std::string::npos) {
        std::string ns_name = template_qualified_name.substr(0, colon_pos);

        // Find or create namespace
        SgNamespaceDefinitionStatement* ns_def = nullptr;
        SgDeclarationStatementPtrList& decls = getGlobalScope()->getDeclarationList();
        for (SgDeclarationStatement* decl : decls) {
            if (SgNamespaceDeclarationStatement* ns_decl = isSgNamespaceDeclarationStatement(decl)) {
                if (ns_decl->get_name().getString() == ns_name) {
                    ns_def = ns_decl->get_definition();
                    break;
                }
            }
        }

        if (ns_def == nullptr) {
            // Create namespace
            SgNamespaceDeclarationStatement* ns_decl =
                SageBuilder::buildNamespaceDeclaration(SgName(ns_name), getGlobalScope());
            ns_decl->get_file_info()->setCompilerGenerated();
            ns_def = ns_decl->get_definition();
            ns_def->get_file_info()->setCompilerGenerated();
            getGlobalScope()->append_declaration(ns_decl);
        }

        inst_scope = ns_def;
    }

    inst_decl->set_scope(inst_scope);

    // CRITICAL: Set template name before creating type
    // get_mangled_name() requires this to be set and will assert if it's null
    // Use ONLY base name for templateName (e.g., "array" not "std::array")
    // The qualified name is in the declaration name above
    inst_decl->set_templateName(SgName(template_base_name));

    // Create class type pointing to this instantiation
    class_type = SgClassType::createType(inst_decl);
    inst_decl->set_type(class_type);

    // TODO: Namespace qualification for template instantiations
    // The unparser currently outputs "class ::array" instead of "std::array"
    // This is a limitation of ROSE's unparser name qualification system which was
    // designed for the EDG frontend. The globalQualifiedNameMapForTypes mechanism
    // doesn't work for template instantiations in this context, and SgTemplateInstantiationDecl
    // doesn't have a set_qualified_name_prefix() method.
    //
    // WORKAROUND ATTEMPTED: Tried setting qualified names via globalQualifiedNameMapForTypes
    // but the unparser uses a different code path for variable type unparsing.
    //
    // ROOT CAUSE: Need to either:
    // 1. Create template declarations in proper namespace scope (causes SageBuilder assertions)
    // 2. Extend SgTemplateInstantiationDecl to support namespace qualification
    // 3. Modify unparser to check additional qualification mechanisms
    //
    // For now, the AST is correct, template instantiation works, but unparsed output
    // has incorrect namespace qualification.

    // Create symbol and insert into symbol table
    // ROOT CAUSE FIX: Insert symbol into the same scope as the declaration (inst_scope)
    // not getGlobalScope(). This fixes ROSETTA warnings:
    // "SgScopeStatement::insert_symbol(): class_declaration->get_scope() != this"
    // The declaration's scope (set on line 1322) must match the scope where we insert the symbol.
    // Use full mangled name for symbol table to avoid conflicts
    SgClassSymbol* class_symbol = new SgClassSymbol(inst_decl);
    inst_scope->insert_symbol(SgName(inst_name_full), class_symbol);

    // Cache it with full name
    p_template_inst_cache[inst_name_full] = inst_decl;

    return inst_decl;
}

bool ClangToSageTranslator::VisitTemplateSpecializationType(clang::TemplateSpecializationType * template_specialization_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::TemplateSpecializationType" << std::endl;
#endif

    // Don't desugar or use canonical type for template specializations
    // We want to create proper SgTemplateInstantiationDecl nodes with template arguments
    // Desugaring would lose the template argument information

    // Extract template name
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    std::string template_name = mangleTemplateName(tname);

    // DEBUG: // std::cerr << "DEBUG VisitTemplateSpecializationType: template_name = '" << template_name << "'" << std::endl;

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

bool ClangToSageTranslator::VisitTemplateTypeParmType(clang::TemplateTypeParmType * template_type_parm_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitTemplateTypeParmType" << std::endl;
#endif
    bool res = true;

    // TODO: Full support for template type parameters not yet implemented
    // Template type parameters (e.g., typename T) are placeholders for types
    // For now, use a generic unknown type scoped to global scope to avoid ROSE-1378
    *node = SageBuilder::buildOpaqueType("template_type_param", getGlobalScope());

    return VisitType(template_type_parm_type, node) && res;
}

bool ClangToSageTranslator::VisitTypedefType(clang::TypedefType * typedef_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitTypedefType" << std::endl;
#endif

    bool res = true;

    SgSymbol * sym = GetSymbolFromSymbolTable(typedef_type->getDecl());
    SgTypedefSymbol * tdef_sym = isSgTypedefSymbol(sym);

    if (tdef_sym == NULL) {
        // Some typedefs (especially template-dependent ones) may not have symbols yet
        // Use unknown type as fallback - this is acceptable for incomplete C++ support
        // Cannot find a typedef symbol for the TypedefType, using unknown type
    }

    *node = (tdef_sym != NULL) ? tdef_sym->get_type()
                               : SageBuilder::buildUnknownType();

   return VisitType(typedef_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeOfExprType(clang::TypeOfExprType * type_of_expr_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::TypeOfExprType" << std::endl;
#endif
    bool res = true;

    SgNode* tmp_expr = Traverse(type_of_expr_type->getUnderlyingExpr());

 // printf ("In VisitTypeOfExprType(): tmp_expr = %p = %s \n",tmp_expr,tmp_expr->class_name().c_str());

    SgExpression* expr = isSgExpression(tmp_expr);
    SgType* type = SageBuilder::buildTypeOfType(expr,NULL);

    *node = type;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(type_of_expr_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentTypeOfExprType(clang::DependentTypeOfExprType * dependent_type_of_expr_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentTypeOfExprType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitTypeOfExprType(dependent_type_of_expr_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeOfType(clang::TypeOfType * type_of_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::TypeOfType" << std::endl;
#endif
    bool res = true;

    // In LLVM 20, getUnderlyingType() was renamed to getUnmodifiedType()
    SgType* underlyinigType = buildTypeFromQualifiedType(type_of_type->getUnmodifiedType());

    SgType* type = SageBuilder::buildTypeOfType(NULL,underlyinigType);

    *node = type;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(type_of_type, node) && res;
}

bool ClangToSageTranslator::VisitTypeWithKeyword(clang::TypeWithKeyword * type_with_keyword, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitTypeWithKeyword" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitType(type_with_keyword, node) && res;
}

bool ClangToSageTranslator::VisitDependentNameType(clang::DependentNameType * dependent_name_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentNameType" << std::endl;
#endif
    bool res = true;

    // TODO: Full support for dependent names not yet implemented
    // Dependent names (e.g., T::value_type) depend on template parameters
    // For now, use a generic unknown type scoped to global scope to avoid ROSE-1378
    *node = SageBuilder::buildOpaqueType("dependent_name", getGlobalScope());

    return VisitTypeWithKeyword(dependent_name_type, node) && res;
}

bool ClangToSageTranslator::VisitDependentTemplateSpecializationType(clang::DependentTemplateSpecializationType * dependent_template_specialization_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentTemplateSpecializationType" << std::endl;
#endif
    bool res = true;

    // REX: Build a meaningful name for dependent template specializations
    // Extract the template name and arguments even though they're dependent
    std::string type_name;

    // Get the qualifier (e.g., "std" in "std::array")
    if (dependent_template_specialization_type->getQualifier() != NULL) {
        llvm::raw_string_ostream qualifier_stream(type_name);
        dependent_template_specialization_type->getQualifier()->print(qualifier_stream,
                                                                       clang::PrintingPolicy(clang::LangOptions()));
        qualifier_stream.flush();
    }

    // Get the template name (e.g., "array")
    // Note: getIdentifier() returns nullptr for operator/literal templates like T::template operator+<U>
    // In those cases, fall back to a generic name
    if (dependent_template_specialization_type->getIdentifier() != NULL) {
        type_name += dependent_template_specialization_type->getIdentifier()->getName().str();
    } else {
        // Handle operator or literal template names
        type_name += "dependent_template_specialization";
    }

    // Build template arguments string
    // In LLVM 20, use template_arguments() iterator instead of getNumArgs()/getArg()
    auto template_args = dependent_template_specialization_type->template_arguments();
    if (!template_args.empty()) {
        type_name += "<";
        bool first = true;
        for (const clang::TemplateArgument &arg : template_args) {
            if (!first) type_name += ", ";
            first = false;
            std::string arg_str;
            llvm::raw_string_ostream arg_stream(arg_str);
            arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream, /*IncludeType=*/true);
            arg_stream.flush();
            type_name += arg_str;
        }
        type_name += ">";
    }

    // Sanitize the type name for use as a C++ identifier
    // Replace invalid characters (::, <, >, comma, space, *, &) with underscores
    // This produces a valid typedef name instead of using raw template syntax
    std::string sanitized_name = type_name;
    for (size_t i = 0; i < sanitized_name.length(); ++i) {
        char c = sanitized_name[i];
        if (c == ':' || c == '<' || c == '>' || c == ',' || c == ' ' ||
            c == '*' || c == '&' || c == '(' || c == ')') {
            sanitized_name[i] = '_';
        }
    }

    // Create an opaque type with the sanitized identifier
    // Note: Full template type support requires SgTemplateType/SgTemplateInstantiationType
    *node = SageBuilder::buildOpaqueType(sanitized_name, getGlobalScope());

    return VisitTypeWithKeyword(dependent_template_specialization_type, node) && res;
}

bool ClangToSageTranslator::VisitElaboratedType(clang::ElaboratedType * elaborated_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitElaboratedType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(elaborated_type->getNamedType());

    // FIXME clang::ElaboratedType contains the "sugar" of a type reference (eg, "struct A" or "M::N::A"), it should be pass down to ROSE

    *node = type;

    return VisitTypeWithKeyword(elaborated_type, node);
}

bool ClangToSageTranslator::VisitUnaryTransformType(clang::UnaryTransformType * unary_transform_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::UnaryTransformType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(unary_transform_type, node) && res;
}

// DependentUnaryTransformType was removed/renamed in LLVM 20
/*
bool ClangToSageTranslator::VisitDependentUnaryTransformType(clang::DependentUnaryTransformType * dependent_unary_transform_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::DependentUnaryTransformType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

    return VisitUnaryTransformType(dependent_unary_transform_type, node) && res;
}
*/

bool ClangToSageTranslator::VisitUnresolvedUsingType(clang::UnresolvedUsingType * unresolved_using_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::UnresolvedUsingType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME 

    return VisitType(unresolved_using_type, node) && res;
}

bool ClangToSageTranslator::VisitVectorType(clang::VectorType * vector_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitVectorType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(vector_type->getElementType());

    SgModifierType * modified_type = new SgModifierType(type);
    SgTypeModifier & sg_modifer = modified_type->get_typeModifier();

    sg_modifer.setVectorType();
    sg_modifer.set_vector_size(vector_type->getNumElements());

    *node = SgModifierType::insertModifierTypeIntoTypeTable(modified_type);

    return VisitType(vector_type, node);
}

bool ClangToSageTranslator::VisitExtVectorType(clang::ExtVectorType * ext_vector_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitExtVectorType" << std::endl;
#endif
    bool res = true;

    ROSE_ASSERT(FAIL_FIXME == 0); // FIXME Is it anything to be done here?

    return VisitVectorType(ext_vector_type, node) && res;
}

bool ClangToSageTranslator::VisitUsingType(clang::UsingType * using_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitUsingType" << std::endl;
#endif
    bool res = true;

    // ROOT CAUSE FIX: UsingType is a type alias from a using declaration
    // Desugar it to get the underlying type
    clang::QualType underlying = using_type->desugar();
    *node = buildTypeFromQualifiedType(underlying);

    return res;
}
