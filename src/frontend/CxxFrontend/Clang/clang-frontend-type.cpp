#include "clang-frontend-private.hpp"
#include "sage3basic.h"
#include "sageInterface.h"

#include <cctype>
#include "llvm/ADT/SmallString.h"

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

    } // namespace

    // Generate unique name for template instantiation
    // Note: Must not contain < > characters for ROSE mangling
    std::string ClangToSageTranslator::getTemplateQualifiedName(
        SgTemplateClassDeclaration *template_decl) {
      std::string template_base_name = template_decl->get_name().getString();
      std::string template_qualified_name = template_base_name;

      SgScopeStatement *decl_scope = template_decl->get_scope();
      while (decl_scope && !isSgGlobal(decl_scope)) {
        if (SgNamespaceDefinitionStatement *ns_def =
                isSgNamespaceDefinitionStatement(decl_scope)) {
          SgNamespaceDeclarationStatement *ns_decl =
              ns_def->get_namespaceDeclaration();
          template_qualified_name =
              ns_decl->get_name().getString() + "::" + template_qualified_name;
        } else if (SgClassDefinition *class_def =
                       isSgClassDefinition(decl_scope)) {
          SgClassDeclaration *class_decl = class_def->get_declaration();
          template_qualified_name = class_decl->get_name().getString() +
                                    "::" + template_qualified_name;
        }
        decl_scope = decl_scope->get_scope();
      }
      return template_qualified_name;
    }

    std::string ClangToSageTranslator::mangleTemplateInstantiation(
        const std::string &template_name,
        const clang::TemplateSpecializationType *spec_type) {
      std::string result = template_name + "_";
      auto args = spec_type->template_arguments();
      bool first = true;
      for (const clang::TemplateArgument &arg : args) {
        if (!first)
          result += "_";
        first = false;

        std::string arg_str;
        llvm::raw_string_ostream arg_stream(arg_str);
        arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream,
                  true);
        arg_stream.flush();

        // Replace special characters that can't be in mangled names
        for (char &c : arg_str) {
          if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' ||
              c == '*' || c == '&') {
            c = '_';
          }
        }
        result += arg_str;
      }
      return result;
    }

    std::string ClangToSageTranslator::mangleTemplateInstantiation(
        const std::string &template_name,
        const clang::TemplateArgumentList &args) {
      std::string result = template_name + "_";
      bool first = true;
      for (unsigned i = 0; i < args.size(); ++i) {
        const clang::TemplateArgument &arg = args.get(i);
        if (!first)
          result += "_";
        first = false;

        std::string arg_str;
        llvm::raw_string_ostream arg_stream(arg_str);
        arg.print(clang::PrintingPolicy(clang::LangOptions()), arg_stream,
                  true);
        arg_stream.flush();

        for (char &c : arg_str) {
          if (c == '<' || c == '>' || c == ',' || c == ' ' || c == ':' ||
              c == '*' || c == '&') {
            c = '_';
          }
        }
        result += arg_str;
      }
      return result;
    }

    namespace {

    void diagnose_null_scope(SgDeclarationStatement *decl,
                             const char *context) {
      if (decl == NULL || decl->get_scope() != NULL)
        return;
      MLOG_WARN_C(MLOG_FRONTEND,
                  "Declaration %s (%p) created with NULL scope in %s\n",
                  decl->class_name().c_str(), decl, context);
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

    // After translating the declaration, the symbol should now exist; refresh the lookup
    if (class_sym == NULL) {
        class_sym = isSgClassSymbol(GetSymbolFromSymbolTable(record_type->getDecl()));
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

            case clang::TemplateArgument::Template: {
                // Template template parameter
                param_kind = SgTemplateParameter::template_parameter;
                std::string param_name = "Template" + std::to_string(param_position);
                SgTemplateType* ttype = SageBuilder::buildTemplateType(SgName(param_name));

                // Create SgNonrealDecl
                SgScopeStatement* current_scope = SageBuilder::topScopeStack();
                ROSE_ASSERT(current_scope != NULL);
                SgDeclarationScope *decl_scope =
                    isSgDeclarationScope(current_scope);
                if (decl_scope == NULL) {
                  decl_scope = SageBuilder::buildDeclarationScope();
                  decl_scope->set_parent(current_scope);
                }

                SgNonrealDecl* nrdecl = SageBuilder::buildNonrealDecl(SgName(param_name), decl_scope);
                diagnose_null_scope(nrdecl, "TemplateTemplateArgument");
                // nrdecl->set_type(ttype); // Removed: SgNonrealDecl expects SgNonrealType

                // Translate inner parameters
                clang::TemplateName tname = arg.getAsTemplate();
                clang::TemplateDecl* tdecl = tname.getAsTemplateDecl();

                if (tdecl) {
                    SgTemplateParameterPtrList* inner_params = translateTemplateParameterList(tdecl->getTemplateParameters(), nrdecl);
                    if (inner_params) {
                        nrdecl->get_tpl_params() = *inner_params;
                        delete inner_params;
                    }
                }

                SgTemplateParameter* param = SageBuilder::buildTemplateParameter(SgTemplateParameter::template_parameter, ttype);
                param->set_templateDeclaration(nrdecl);

                param_list->push_back(param);
                param_position++;
                continue;
            }

            case clang::TemplateArgument::Pack:
                // Parameter pack (e.g., typename ... Args)
                // We infer it as a type parameter pack
                param_kind = SgTemplateParameter::type_parameter;
                param_type = SageBuilder::buildTemplateType(
                    SgName("... Args" + std::to_string(param_position)));
                break;

            case clang::TemplateArgument::Expression:
                // Expression argument (e.g., sizeof(T))
                // We infer it as a non-type parameter
                param_kind = SgTemplateParameter::nontype_parameter;
                if (clang::Expr* expr = arg.getAsExpr()) {
                    param_type = buildTypeFromQualifiedType(expr->getType());
                } else {
                    // Fallback if expression is null (shouldn't happen for Expression kind)
                    param_type = SageBuilder::buildIntType();
                }
                break;

            case clang::TemplateArgument::Declaration: {
                // Non-type parameter (e.g., void (*F)())
                param_kind = SgTemplateParameter::nontype_parameter;
                clang::ValueDecl* decl = arg.getAsDecl();
                if (decl) {
                    param_type = buildTypeFromQualifiedType(decl->getType());
                } else {
                    // Should not happen for Declaration kind
                    param_type = SageBuilder::buildVoidType(); 
                }
                break;
            }

            case clang::TemplateArgument::NullPtr:
                // Non-type parameter (e.g., nullptr)
                param_kind = SgTemplateParameter::nontype_parameter;
                param_type = buildTypeFromQualifiedType(arg.getNullPtrType());
                break;

            default:
                std::cerr << "Warning: Unsupported template parameter kind: "
                          << arg.getKind() << " (Pack=" << clang::TemplateArgument::Pack 
                          << ", Expression=" << clang::TemplateArgument::Expression
                          << ", Template=" << clang::TemplateArgument::Template
                          << ", Integral=" << clang::TemplateArgument::Integral
                          << ", Type=" << clang::TemplateArgument::Type
                          << ", Declaration=" << clang::TemplateArgument::Declaration
                          << ", NullPtr=" << clang::TemplateArgument::NullPtr
                          << ", Null=" << clang::TemplateArgument::Null
                          << ")" << std::endl;
                continue;
        }


        SgTemplateParameter* param = SageBuilder::buildTemplateParameter(
            param_kind, param_type);
        if (param) {
            param_list->push_back(param);
        }
        param_position++;
    }

    return param_list;
}

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

    // Resolve scope (handle namespaces like std::)
    SgScopeStatement* scope = getGlobalScope();
    if (last_colon != std::string::npos) {
        size_t pos = 0;
        while (pos < last_colon) {
            size_t next_colon = template_name.find("::", pos);
            if (next_colon == std::string::npos || next_colon > last_colon) {
                break;
            }
            std::string ns_name = template_name.substr(pos, next_colon - pos);
            
            // Find or create namespace
            SgNamespaceSymbol* ns_sym = scope->lookup_namespace_symbol(SgName(ns_name));
            if (ns_sym) {
                scope = ns_sym->get_declaration()->get_definition();
            } else {
                SgNamespaceDeclarationStatement* ns_decl = 
                    SageBuilder::buildNamespaceDeclaration(SgName(ns_name), scope);
                scope = ns_decl->get_definition();
            }
            
            pos = next_colon + 2;
        }
    }

    // Build template parameters
    SgTemplateParameterPtrList* params = buildTemplateParameters(clang_type);

    // Create empty template argument list for primary template
    SgTemplateArgumentPtrList* empty_args = new SgTemplateArgumentPtrList();

    // Create template class declaration
    SgTemplateClassDeclaration* template_decl =
        SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
            SgName(base_name),
            SgClassDeclaration::e_class,  // Assume class (could be struct)
            scope,
            params,
            empty_args  // No specialization arguments for primary template
        );

    // Mark as compiler generated and forward declaration
    template_decl->setForward();
    template_decl->set_isUnNamed(false);
    template_decl->get_file_info()->setCompilerGenerated();
    template_decl->get_file_info()->unsetOutputInCodeGeneration();

    // Insert into scope symbol table if not already present
    if (!scope->lookup_class_symbol(SgName(base_name))) {
        SgClassSymbol* template_symbol = new SgClassSymbol(template_decl);
        scope->insert_symbol(SgName(base_name), template_symbol);
    }

    // Cache it
    p_template_decl_cache[template_name] = template_decl;

    return template_decl;
}

namespace {
// Build a literal expression from an APSInt while preserving sign and (as text) width.
SgExpression* buildIntegralTemplateArgExpr(const llvm::APSInt& value, SgType* int_type) {
    const bool is_signed = value.isSigned();
    const unsigned bitwidth = value.getBitWidth();

    SgExpression* expr = NULL;
    if (is_signed) {
        // Use the widest native builder we have; valueString keeps the full precision.
        long long v = (bitwidth <= 63) ? value.getSExtValue() : 0;
        expr = SageBuilder::buildLongLongIntVal(v);
    } else {
        unsigned long long v = (bitwidth <= 64) ? value.getZExtValue() : 0;
        expr = SageBuilder::buildUnsignedLongLongIntVal(v);
    }

    if (expr != NULL) {
        llvm::SmallString<64> buf;
        value.toString(buf, 10, value.isSigned());
        std::string text(buf.begin(), buf.end());

        if (SgLongLongIntVal* ll = isSgLongLongIntVal(expr)) {
            ll->set_valueString(text);
        } else if (SgUnsignedLongLongIntVal* ull = isSgUnsignedLongLongIntVal(expr)) {
            ull->set_valueString(text);
        }
    }

    return expr;
}
} // namespace

SgTemplateArgument *ClangToSageTranslator::translateTemplateArgument(
    const clang::TemplateArgument &arg, bool explicitlySpecified) {
  SgTemplateArgument *sg_arg = nullptr;

  switch (arg.getKind()) {
  case clang::TemplateArgument::Type: {
    SgType *arg_type = buildTypeFromQualifiedType(arg.getAsType());
    if (arg_type != NULL) {
      sg_arg = new SgTemplateArgument(arg_type, explicitlySpecified);
    }
    break;
  }

  case clang::TemplateArgument::Integral: {
    llvm::APSInt value = arg.getAsIntegral();
    SgType *int_type = buildTypeFromQualifiedType(arg.getIntegralType());

    SgExpression *value_expr = buildIntegralTemplateArgExpr(value, int_type);

    sg_arg =
        new SgTemplateArgument(SgTemplateArgument::nontype_argument, value_expr,
                               int_type, nullptr, nullptr, explicitlySpecified);
    break;
  }

  case clang::TemplateArgument::Template: {
    clang::TemplateDecl *template_decl =
        arg.getAsTemplate().getAsTemplateDecl();
    if (template_decl != nullptr) {
      SgDeclarationStatement *sg_decl =
          (SgDeclarationStatement *)Traverse(template_decl);

      if (sg_decl != nullptr) {
        if (SgTemplateClassDeclaration *class_tmpl =
                isSgTemplateClassDeclaration(sg_decl)) {
          SgName qual_name = class_tmpl->get_qualified_name();
          if (qual_name.getString().find("::") == std::string::npos &&
              class_tmpl->get_scope()) {
            if (SgNamespaceDefinitionStatement *ns_def =
                    isSgNamespaceDefinitionStatement(class_tmpl->get_scope())) {
              qual_name =
                  ns_def->get_namespaceDeclaration()->get_name().getString() +
                  "::" + class_tmpl->get_name().getString();
            }
          }
          SgType *type = SageBuilder::buildTemplateType(qual_name);
          sg_arg = new SgTemplateArgument(type, explicitlySpecified);
        } else {
          sg_arg = new SgTemplateArgument(
              SgTemplateArgument::template_template_argument, sg_decl);
        }
      } else {
        std::cerr << "Warning: Failed to translate template declaration "
                     "for template argument\n";
      }
    }
    break;
  }

  case clang::TemplateArgument::Expression: {
    clang::Expr *clang_expr = arg.getAsExpr();
    if (clang_expr != nullptr) {
      SgNode *node = Traverse(clang_expr);
      if (SgExpression *sg_expr = isSgExpression(node)) {
        sg_arg = new SgTemplateArgument(sg_expr, explicitlySpecified);
      }
    }
    break;
  }

  default:
    std::cerr << "Warning: Unsupported template argument kind: "
              << arg.getKind() << "\n";
    break;
  }

  return sg_arg;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateSpecializationType *clang_type) {

  SgTemplateArgumentPtrList arg_list;

  auto args = clang_type->template_arguments();
  for (const clang::TemplateArgument &arg : args) {
    if (SgTemplateArgument *sg_arg = translateTemplateArgument(arg, false)) {
      arg_list.push_back(sg_arg);
    }
  }

  return arg_list;
}

SgTemplateArgumentPtrList ClangToSageTranslator::buildTemplateArguments(
    const clang::TemplateArgumentListInfo &arg_info, bool explicitlySpecified) {
  SgTemplateArgumentPtrList arg_list;

  for (const clang::TemplateArgumentLoc &arg_loc : arg_info.arguments()) {
    if (SgTemplateArgument *sg_arg = translateTemplateArgument(
            arg_loc.getArgument(), explicitlySpecified)) {
      arg_list.push_back(sg_arg);
    }
  }

  return arg_list;
}

SgTemplateInstantiationDecl*
ClangToSageTranslator::getOrCreateTemplateInstantiation(
    SgTemplateClassDeclaration* template_decl,
    const clang::TemplateSpecializationType* clang_type) {

    // Extract both base name and qualified name for the template
    std::string template_base_name = template_decl->get_name().getString();

    // ROOT CAUSE FIX: Check if template declaration has namespace qualification stored
    // Use qualified name (e.g., "std::array") for instantiation name instead of just base name
    std::string template_qualified_name =
        getTemplateQualifiedName(template_decl);

    // Use qualified name in cache key to avoid namespace collisions
    // Example: "std::array<int>" and "my_ns::array<int>" must have different cache keys
    // Otherwise they would both mangle to "array_int" and collide
    std::string inst_name_full = mangleTemplateInstantiation(template_qualified_name, clang_type);

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
            SgName(inst_name_full),  // Use full mangled name for uniqueness
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
    
    // REX FIX: Always require global qualification for template instantiations
    // This ensures that the unparser prints "::" (e.g. "::std::vector" or "::tuple")
    // which prevents ambiguity when global templates are shadowed.
    inst_decl->set_global_qualification_required(true);

    if (inst_decl->get_templateDeclaration() == NULL) {
        std::cerr << "CRITICAL ERROR: inst_decl->get_templateDeclaration() is NULL immediately after creation! Setting it explicitly." << std::endl;
        inst_decl->set_templateDeclaration(template_decl);
    }

    // FIX P1: Handle nested namespaces correctly (e.g., "std::chrono::duration")
    // Split the qualified name by "::" and create/find nested namespace scopes
    SgScopeStatement* inst_scope = getGlobalScope();

    // Split qualified name into components
    std::vector<std::string> components;
    size_t start = 0;
    size_t colon_pos = template_qualified_name.find("::");
    while (colon_pos != std::string::npos) {
        components.push_back(template_qualified_name.substr(start, colon_pos - start));
        start = colon_pos + 2;  // Skip "::"
        colon_pos = template_qualified_name.find("::", start);
    }
    components.push_back(template_qualified_name.substr(start));  // Last component (class name)

    // Iterate through all namespace components (all except the last, which is the class name)
    // For "std::chrono::duration", iterate through ["std", "chrono"], not "duration"
    for (size_t i = 0; i + 1 < components.size(); ++i) {
        const std::string& ns_name = components[i];

        // Find or create namespace in current scope
        SgNamespaceDefinitionStatement* ns_def = nullptr;
        SgDeclarationStatementPtrList& decls = inst_scope->getDeclarationList();
        for (SgDeclarationStatement* decl : decls) {
            if (SgNamespaceDeclarationStatement* ns_decl = isSgNamespaceDeclarationStatement(decl)) {
                if (ns_decl->get_name().getString() == ns_name) {
                    ns_def = ns_decl->get_definition();
                    break;
                }
            }
        }

        if (ns_def == nullptr) {
            // Create namespace in current scope
            // buildNamespaceDeclaration automatically inserts the declaration into inst_scope
            SgNamespaceDeclarationStatement* ns_decl =
                SageBuilder::buildNamespaceDeclaration(SgName(ns_name), inst_scope);
            ns_decl->get_file_info()->setCompilerGenerated();
            ns_def = ns_decl->get_definition();
            ns_def->get_file_info()->setCompilerGenerated();
        }

        // Move into nested namespace for next iteration
        inst_scope = ns_def;
    }

    inst_decl->set_scope(inst_scope);

    // CRITICAL: Set template name before creating type
    // get_mangled_name() requires this to be set and will assert if it's null
    // Use ONLY base name for templateName (e.g., "array" not "std::array")
    // The qualified name is in the declaration name above
    // NameQualificationTraversal will add the necessary qualification (e.g. "std::")
    inst_decl->set_templateName(SgName(template_base_name));

    // Create class type pointing to this instantiation
    class_type = SgClassType::createType(inst_decl);
    inst_decl->set_type(class_type);

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

    // REX FIX: Handle dependent template specializations as opaque types
    // This avoids creating broken SgTemplateInstantiationDecl nodes for types like T::rebind<int>
    // which cause unparsing issues (e.g., "T__rebind_int_")
    if (template_specialization_type->isDependentType()) {
        clang::PrintingPolicy policy = p_compiler_instance->getASTContext().getPrintingPolicy();
        
        // Check if the template is a template parameter (e.g. C in UsePack<C, Args...>)
        // If so, do NOT use fully qualified name, as it would produce UsePack::C
        bool is_template_param = false;
        clang::TemplateName tname = template_specialization_type->getTemplateName();
        if (clang::TemplateDecl* decl = tname.getAsTemplateDecl()) {
            if (clang::isa<clang::TemplateTemplateParmDecl>(decl)) {
                is_template_param = true;
            }
        }
        
        if (!is_template_param) {
            policy.FullyQualifiedName = true;
        }
        policy.SuppressScope = false;
        
        std::string type_name = clang::QualType(template_specialization_type, 0).getAsString(policy);
        
        // Strip leading "::" if present to avoid double qualification by unparser
        if (type_name.size() >= 2 && type_name.substr(0, 2) == "::") {
            type_name = type_name.substr(2);
        }

        *node = SageBuilder::buildOpaqueType(type_name, getGlobalScope());
        return VisitType(template_specialization_type, node);
    }

    // Extract template name
    clang::TemplateName tname = template_specialization_type->getTemplateName();
    std::string template_name = mangleTemplateName(tname);

    // DEBUG: // std::cerr << "DEBUG VisitTemplateSpecializationType: template_name = '" << template_name << "'" << std::endl;

    // Get or create template class declaration
    SgTemplateClassDeclaration* template_decl = NULL;
    
    clang::TemplateDecl* clang_template_decl = tname.getAsTemplateDecl();
    if (clang_template_decl) {
        // std::cerr << "DEBUG: Found clang_template_decl for " << template_name << std::endl;
        SgNode* tmp_node = Traverse(clang_template_decl);
        template_decl = isSgTemplateClassDeclaration(tmp_node);
        if (template_decl) {
             // std::cerr << "DEBUG: Found existing SgTemplateClassDeclaration for " << template_name << std::endl;
        } else {
             // std::cerr << "DEBUG: Traverse returned NULL or non-template for " << template_name << std::endl;
        }
    } else {
        // std::cerr << "DEBUG: No clang_template_decl for " << template_name << std::endl;
    }

    if (template_decl == NULL) {
        // std::cerr << "DEBUG: Creating new template declaration for " << template_name << std::endl;
        template_decl = getOrCreateTemplateDeclaration(template_name, template_specialization_type);
    }
    
    if (template_decl == NULL) {
        std::cerr << "CRITICAL ERROR: template_decl is NULL for " << template_name << std::endl;
    }

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

    // CLANG FRONTEND FIX: Proper template type parameter support
    // Get the template parameter declaration to extract the name
    const clang::TemplateTypeParmDecl* param_decl = template_type_parm_type->getDecl();
    std::string param_name;

    if (param_decl && param_decl->getDeclName().isIdentifier()) {
        // Use the actual template parameter name (e.g., "T")
        param_name = param_decl->getNameAsString();
    } else {
        // Fallback to a generic name if we can't get the actual name
        param_name = "template_type_param";
    }

    // Create a proper template type with the actual parameter name
    *node = SageBuilder::buildTemplateType(SgName(param_name));

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
        if (SgProject::get_verbose() > 0) {
            std::cerr << "CFE: Missing typedef symbol for '" << typedef_type->getDecl()->getNameAsString() << "'"
                      << std::endl;
        }

        // Try to translate the typedef declaration on demand so the symbol becomes available.
        // This handles cases where a typedef type is referenced before its declaration is processed.
        auto it = p_decl_translation_map.find(typedef_type->getDecl());
        if (it == p_decl_translation_map.end()) {
            Traverse(const_cast<clang::TypedefNameDecl *>(typedef_type->getDecl()));
            sym = GetSymbolFromSymbolTable(typedef_type->getDecl());
            tdef_sym = isSgTypedefSymbol(sym);
        }
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
    // REX FIX: Do NOT sanitize! We want the raw template syntax for unparsing.
    std::string sanitized_name = type_name;
    /*
    for (size_t i = 0; i < sanitized_name.length(); ++i) {
        char c = sanitized_name[i];
        if (c == ':' || c == '<' || c == '>' || c == ',' || c == ' ' ||
            c == '*' || c == '&' || c == '(' || c == ')') {
            sanitized_name[i] = '_';
        }
    }
    */

    // Create a proper template type with the raw name
    // Note: buildOpaqueType creates SgClassType in global scope, which unparser qualifies with ::
    // SgTemplateType unparses as just the name, avoiding unwanted qualification.
    // REX FIX: Prepend "typename " and insert "template " for dependent types
    std::string final_name = sanitized_name;
    
    // Insert "template " after the last "::" if it exists
    size_t last_colon = final_name.find_last_of(':');
    if (last_colon != std::string::npos && last_colon + 1 < final_name.length()) {
        // Check if it's already there (unlikely)
        if (final_name.find("template ", last_colon + 1) != last_colon + 1) {
             final_name.insert(last_colon + 1, "template ");
        }
    }
    
    final_name = "typename " + final_name;
    *node = SageBuilder::buildTemplateType(SgName(final_name));

    return VisitTypeWithKeyword(dependent_template_specialization_type, node) && res;
}

bool ClangToSageTranslator::VisitElaboratedType(clang::ElaboratedType * elaborated_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::VisitElaboratedType" << std::endl;
#endif

    SgType * type = buildTypeFromQualifiedType(elaborated_type->getNamedType());

    // CLANG FRONTEND NOTE: ElaboratedType contains namespace qualifiers (e.g., "std::" in "std::string")
    // and struct/class/enum keywords that provide "sugar" for the type reference.
    //
    // WARNING: Do NOT mutate the underlying SgTypedefDeclaration or other type declarations!
    // The same declaration is shared by all uses, so modifying it causes corruption:
    //   1st use: "string" → "std::string"
    //   2nd use: "std::string" → "std::std::string"
    //   3rd use: "std::std::string" → "std::std::std::string"
    //
    // TODO: ROSE needs a proper way to represent elaborated types with qualifiers.
    // Possible solutions:
    //   - Create SgQualifiedNameType or similar wrapper
    //   - Store qualifier info as attributes on the type
    //   - Handle qualification during unparsing only
    //
    // For now, we just desugar to the named type (same as EDG frontend behavior).

    *node = type;

    return VisitTypeWithKeyword(elaborated_type, node);
}

bool ClangToSageTranslator::VisitUnaryTransformType(clang::UnaryTransformType * unary_transform_type, SgNode ** node) {
#if DEBUG_VISIT_TYPE
    std::cerr << "ClangToSageTranslator::UnaryTransformType" << std::endl;
#endif
    bool res = true;

    // UnaryTransformType is sugar that wraps another type (e.g., __underlying_type).
    // Desugar it so downstream consumers see the real underlying type instead of an
    // unknown placeholder.
    clang::QualType underlying = unary_transform_type->desugar();
    if (underlying.isNull()) {
        underlying = unary_transform_type->getUnderlyingType();
    }
    if (underlying.isNull()) {
        underlying = unary_transform_type->getBaseType();
    }

    if (underlying.isNull()) {
        *node = SageBuilder::buildUnknownType();
    } else {
        *node = buildTypeFromQualifiedType(underlying);
    }

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

    // Preserve the type as an unresolved dependent name rather than leaving a null
    // node (which triggers runtime errors and forces an unknown type later).
    *node = SageBuilder::buildUnknownType();

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
