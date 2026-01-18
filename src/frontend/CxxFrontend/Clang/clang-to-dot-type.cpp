#include "clang-to-dot-private.hpp"
#include "sage3basic.h"

std::string ClangToDotTranslator::Traverse(const clang::Type *type) {
  if (type == NULL)
    return "";

  // Look for previous translation
  std::map<const clang::Type *, std::string>::iterator it =
      p_type_translation_map.find(type);
  if (it != p_type_translation_map.end())
    return it->second;

  // SgNode * result = NULL;
  // bool ret_status = false;

  // If first time, create a new entry
  std::string node_ident = genNextIdent();
  p_type_translation_map.insert(
      std::pair<const clang::Type *, std::string>(type, node_ident));
  NodeDescriptor &node_desc =
      p_node_desc
          .insert(std::pair<std::string, NodeDescriptor>(
              node_ident, NodeDescriptor(node_ident)))
          .first->second;

  bool ret_status = false;

  // CLANG_ROSE_Graph::graph (type);

  switch (type->getTypeClass()) {
  case clang::Type::Decayed:
    ret_status = VisitDecayedType((clang::DecayedType *)type, node_desc);
    break;
  case clang::Type::ConstantArray:
    ret_status =
        VisitConstantArrayType((clang::ConstantArrayType *)type, node_desc);
    break;
  case clang::Type::DependentSizedArray:
    ret_status = VisitDependentSizedArrayType(
        (clang::DependentSizedArrayType *)type, node_desc);
    break;
  case clang::Type::IncompleteArray:
    ret_status =
        VisitIncompleteArrayType((clang::IncompleteArrayType *)type, node_desc);
    break;
  case clang::Type::VariableArray:
    ret_status =
        VisitVariableArrayType((clang::VariableArrayType *)type, node_desc);
    break;
  case clang::Type::Atomic:
    ret_status = VisitAtomicType((clang::AtomicType *)type, node_desc);
    break;
  case clang::Type::Attributed:
    ret_status = VisitAttributedType((clang::AttributedType *)type, node_desc);
    break;
  case clang::Type::BlockPointer:
    ret_status =
        VisitBlockPointerType((clang::BlockPointerType *)type, node_desc);
    break;
  case clang::Type::Builtin:
    ret_status = VisitBuiltinType((clang::BuiltinType *)type, node_desc);
    break;
  case clang::Type::Complex:
    ret_status = VisitComplexType((clang::ComplexType *)type, node_desc);
    break;
  case clang::Type::Decltype:
    ret_status = VisitDecltypeType((clang::DecltypeType *)type, node_desc);
    break;
    // case clang::Type::DependentDecltype:
    //     ret_status = VisitDependentDecltypeType((clang::DependentDecltypeType
    //     *)type, node_desc); break;
  case clang::Type::Auto:
    ret_status = VisitAutoType((clang::AutoType *)type, node_desc);
    break;
  case clang::Type::DeducedTemplateSpecialization:
    ret_status = VisitDeducedTemplateSpecializationType(
        (clang::DeducedTemplateSpecializationType *)type, node_desc);
    break;
  case clang::Type::DependentSizedExtVector:
    ret_status = VisitDependentSizedExtVectorType(
        (clang::DependentSizedExtVectorType *)type, node_desc);
    break;
  case clang::Type::DependentVector:
    ret_status =
        VisitDependentVectorType((clang::DependentVectorType *)type, node_desc);
    break;
  case clang::Type::FunctionNoProto:
    ret_status =
        VisitFunctionNoProtoType((clang::FunctionNoProtoType *)type, node_desc);
    break;
  case clang::Type::FunctionProto:
    ret_status =
        VisitFunctionProtoType((clang::FunctionProtoType *)type, node_desc);
    break;
  case clang::Type::InjectedClassName:
    ret_status = VisitInjectedClassNameType(
        (clang::InjectedClassNameType *)type, node_desc);
    break;
    // case clang::Type::LocInfo:
    //     ret_status = VisitLocInfoType((clang::LocInfoType *)type, node_desc);
    //     break;
  case clang::Type::MacroQualified:
    ret_status =
        VisitMacroQualifiedType((clang::MacroQualifiedType *)type, node_desc);
    break;
  case clang::Type::MemberPointer:
    ret_status =
        VisitMemberPointerType((clang::MemberPointerType *)type, node_desc);
    break;
  case clang::Type::PackExpansion:
    ret_status =
        VisitPackExpansionType((clang::PackExpansionType *)type, node_desc);
    break;
  case clang::Type::Paren:
    ret_status = VisitParenType((clang::ParenType *)type, node_desc);
    break;
  case clang::Type::Pipe:
    ret_status = VisitPipeType((clang::PipeType *)type, node_desc);
    break;
  case clang::Type::Pointer:
    ret_status = VisitPointerType((clang::PointerType *)type, node_desc);
    break;
  case clang::Type::LValueReference:
    ret_status =
        VisitLValueReferenceType((clang::LValueReferenceType *)type, node_desc);
    break;
  case clang::Type::RValueReference:
    ret_status =
        VisitRValueReferenceType((clang::RValueReferenceType *)type, node_desc);
    break;
  case clang::Type::SubstTemplateTypeParmPack:
    ret_status = VisitSubstTemplateTypeParmPackType(
        (clang::SubstTemplateTypeParmPackType *)type, node_desc);
    break;
  case clang::Type::SubstTemplateTypeParm:
    ret_status = VisitSubstTemplateTypeParmType(
        (clang::SubstTemplateTypeParmType *)type, node_desc);
    break;
  case clang::Type::Enum:
    ret_status = VisitEnumType((clang::EnumType *)type, node_desc);
    break;
  case clang::Type::Record:
    ret_status = VisitRecordType((clang::RecordType *)type, node_desc);
    break;
  case clang::Type::TemplateSpecialization:
    ret_status = VisitTemplateSpecializationType(
        (clang::TemplateSpecializationType *)type, node_desc);
    break;
  case clang::Type::TemplateTypeParm:
    ret_status = VisitTemplateTypeParmType((clang::TemplateTypeParmType *)type,
                                           node_desc);
    break;
  case clang::Type::Typedef:
    ret_status = VisitTypedefType((clang::TypedefType *)type, node_desc);
    break;
  case clang::Type::TypeOfExpr:
    ret_status = VisitTypeOfExprType((clang::TypeOfExprType *)type, node_desc);
    break;
    //  case clang::Type::DependentTypeOfExpr:
    //      ret_status =
    //      VisitDependentTypeOfExprType((clang::DependentTypeOfExprType *)type,
    //      node_desc); break;
  case clang::Type::TypeOf:
    ret_status = VisitTypeOfType((clang::TypeOfType *)type, node_desc);
    break;
  case clang::Type::DependentName:
    ret_status =
        VisitDependentNameType((clang::DependentNameType *)type, node_desc);
    break;
  case clang::Type::DependentTemplateSpecialization:
    ret_status = VisitDependentTemplateSpecializationType(
        (clang::DependentTemplateSpecializationType *)type, node_desc);
    break;
  case clang::Type::Elaborated:
    ret_status = VisitElaboratedType((clang::ElaboratedType *)type, node_desc);
    break;
  case clang::Type::UnaryTransform:
    ret_status =
        VisitUnaryTransformType((clang::UnaryTransformType *)type, node_desc);
    break;
  case clang::Type::UnresolvedUsing:
    ret_status =
        VisitUnresolvedUsingType((clang::UnresolvedUsingType *)type, node_desc);
    break;
  case clang::Type::Vector:
    ret_status = VisitVectorType((clang::VectorType *)type, node_desc);
    break;
  case clang::Type::ExtVector:
    ret_status = VisitExtVectorType((clang::ExtVectorType *)type, node_desc);
    break;
  default:
    std::cerr << "Unhandled type" << std::endl;
    ROSE_ABORT();
  }

  // ROSE_ASSERT(result != NULL);
  // p_type_translation_map.insert(std::pair<const clang::Type *, SgNode
  // *>(type, result)); return result;

  assert(ret_status != false);

  return node_ident;
}

/***************/
/* Visit Types */
/***************/

bool ClangToDotTranslator::VisitType(clang::Type *type,
                                     NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("Type");

  // In LLVM 20, Linkage became a scoped enum
  switch (type->getLinkage()) {
  case clang::Linkage::None:
    break;
  case clang::Linkage::Internal:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("linkage", "internal"));
    break;
  case clang::Linkage::UniqueExternal:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("linkage", "unique external"));
    break;
  case clang::Linkage::External:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("linkage", "external"));
    break;
  default:
    break;
  }

  switch (type->getVisibility()) {
  case clang::HiddenVisibility:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("visibility", "hidden"));
    break;
  case clang::ProtectedVisibility:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("visibility", "protected"));
    break;
  case clang::DefaultVisibility:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("visibility", "default"));
    break;
  }

  // DQ (11/27/2020): Comment from original code that generated the ROSE AST.
  // TODO

  return true;
}

bool ClangToDotTranslator::VisitAdjustedType(clang::AdjustedType *adjusted_type,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitAdjustedType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AdjustedType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(adjusted_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDecayedType(clang::DecayedType *decayed_type,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDecayedType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DecayedType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitAdjustedType(decayed_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitArrayType(clang::ArrayType *array_type,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitArrayType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ArrayType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(array_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitConstantArrayType(
    clang::ConstantArrayType *constant_array_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitConstantArrayType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ConstantArrayType");

  return VisitArrayType(constant_array_type, node_desc);
}

bool ClangToDotTranslator::VisitDependentSizedArrayType(
    clang::DependentSizedArrayType *dependent_sized_array_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDependentSizedArrayType"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentSizedArrayType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitArrayType(dependent_sized_array_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitIncompleteArrayType(
    clang::IncompleteArrayType *incomplete_array_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitIncompleteArrayType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("IncompleteArrayType");

  return VisitArrayType(incomplete_array_type, node_desc);
}

bool ClangToDotTranslator::VisitVariableArrayType(
    clang::VariableArrayType *variable_array_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitVariableArrayType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("VariableArrayType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitArrayType(variable_array_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitAtomicType(clang::AtomicType *atomic_type,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitAtomicType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AtomicArrayType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(atomic_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitAttributedType(
    clang::AttributedType *attributed_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitAttributedType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("AttributedType");

  return VisitType(attributed_type, node_desc);
}

bool ClangToDotTranslator::VisitBlockPointerType(
    clang::BlockPointerType *block_pointer_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitBlockPointerType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BlockPointerType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(block_pointer_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitBuiltinType(clang::BuiltinType *builtin_type,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitBuiltinType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("BuiltinType");

  switch (builtin_type->getKind()) {
  case clang::BuiltinType::Void:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "void"));
    break;
  case clang::BuiltinType::Bool:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "bool"));
    break;
  case clang::BuiltinType::Short:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "short"));
    break;
  case clang::BuiltinType::Int:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "int"));
    break;
  case clang::BuiltinType::Long:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "long"));
    break;
  case clang::BuiltinType::LongLong:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "long long"));
    break;
  case clang::BuiltinType::Float:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "float"));
    break;
  case clang::BuiltinType::Double:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "double"));
    break;
  case clang::BuiltinType::LongDouble:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "long double"));
    break;
  case clang::BuiltinType::Char_S:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "char_s"));
    break;
  case clang::BuiltinType::UInt:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "unsigned int"));
    break;
  case clang::BuiltinType::UChar:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "unsigned char"));
    break;
  case clang::BuiltinType::SChar:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "signed char"));
    break;
  case clang::BuiltinType::UShort:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "unsigned short"));
    break;
  case clang::BuiltinType::ULong:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "unsigned long"));
    break;
  case clang::BuiltinType::ULongLong:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "unsigned long long"));
    break;
  case clang::BuiltinType::NullPtr:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "null pointer"));
    break;
  case clang::BuiltinType::UInt128:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "uint_128"));
    break;
  case clang::BuiltinType::Int128:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "int_128"));
    break;
  case clang::BuiltinType::Char_U:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "char_u"));
    break;
  case clang::BuiltinType::WChar_U:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "wchar_u"));
    break;
  case clang::BuiltinType::Char16:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "char_16"));
    break;
  case clang::BuiltinType::Char32:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "char_32"));
    break;
  case clang::BuiltinType::WChar_S:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "wchar_s"));
    break;
  case clang::BuiltinType::ObjCId:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "ObjCId"));
    break;
  case clang::BuiltinType::ObjCClass:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "ObjCClass"));
    break;
  case clang::BuiltinType::ObjCSel:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "ObjCSel"));
    break;
  case clang::BuiltinType::Dependent:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "Dependent"));
    break;
  case clang::BuiltinType::Overload:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "Overload"));
    break;
  case clang::BuiltinType::BoundMember:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "BoundMember"));
    break;
  case clang::BuiltinType::UnknownAny:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("type", "UnknownAny"));
    break;
  default:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "type",
        builtin_type->getName(p_compiler_instance->getLangOpts()).str()));
    break;
  }

  return VisitType(builtin_type, node_desc);
}

bool ClangToDotTranslator::VisitComplexType(clang::ComplexType *complex_type,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitComplexType" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("ComplexType");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "element_type", Traverse(complex_type->getElementType().getTypePtr())));

  return VisitType(complex_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDecltypeType(clang::DecltypeType *decltype_type,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDecltypeType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DecltypeType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(decltype_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentDecltypeType(
    clang::DependentDecltypeType *dependent_decltype_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDependentDecltypeType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentDecltypeType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDecltypeType(dependent_decltype_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDeducedType(clang::DeducedType *deduced_type,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDeducedType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DeducedType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(deduced_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitAutoType(clang::AutoType *auto_type,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitAutoType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AutoType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDeducedType(auto_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDeducedTemplateSpecializationType(
    clang::DeducedTemplateSpecializationType
        *deduced_template_specialization_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDeducedTemplateSpecializationType"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DeducedTemplateSpecializationType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDeducedType(deduced_template_specialization_type, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitDependentAddressSpaceType(
    clang::DependentAddressSpaceType *dependent_address_space_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitDependentAddressSpaceType"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentAddressSpaceType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(dependent_address_space_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentSizedExtVectorType(
    clang::DependentSizedExtVectorType *dependent_sized_ext_vector_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::DependentSizedExtVectorType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentSizedExtVectorType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(dependent_sized_ext_vector_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentVectorType(
    clang::DependentVectorType *dependent_vector_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::DependentVectorType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentVectorType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(dependent_vector_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitFunctionType(clang::FunctionType *function_type,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitFunctionType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FunctionType");

  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("result_type",
  // Traverse(function_type->getResultType().getTypePtr())));
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "result_type", Traverse(function_type->getReturnType().getTypePtr())));

  // TODO some attr

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(function_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitFunctionNoProtoType(
    clang::FunctionNoProtoType *function_no_proto_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitFunctionNoProtoType" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("FunctionNoProtoType");

  return VisitType(function_no_proto_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitFunctionProtoType(
    clang::FunctionProtoType *function_proto_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitFunctionProtoType" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("FunctionProtoType");

  // DQ (11/27/2020): Updated to Clang 10.
  // for (unsigned i = 0; i < function_proto_type->getNumArgs(); i++)
  for (unsigned i = 0; i < function_proto_type->getNumParams(); i++) {
    std::ostringstream oss;
    oss << "arg_type[" << i << "]";

    // DQ (11/27/2020): Updated to Clang 10.
    // node_desc.successors.push_back(std::pair<std::string,
    // std::string>(oss.str(),
    // Traverse(function_proto_type->getArgType(i).getTypePtr())));
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        oss.str(),
        Traverse(function_proto_type->getParamType(i).getTypePtr())));
  }

  if (function_proto_type->isVariadic())
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("have", "variadic"));

  // Using the return from Tristan's dot generator code.
  // return VisitType(function_proto_type, node_desc) && res;
  return VisitFunctionType(function_proto_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitInjectedClassNameType(
    clang::InjectedClassNameType *injected_class_name_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::InjectedClassNameType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("InjectedClassNameType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "injected_specialization_type",
      Traverse(injected_class_name_type->getInjectedSpecializationType()
                   .getTypePtr())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "injected_template_specialization_type",
      Traverse(injected_class_name_type->getInjectedTST())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "declaration", Traverse(injected_class_name_type->getDecl())));

  return VisitType(injected_class_name_type, node_desc) && res;
}

// bool ClangToDotTranslator::VisitLocInfoType(clang::LocInfoType *
// loc_info_type, NodeDescriptor & node_desc) { #if DEBUG_VISIT_TYPE
//     std::cerr << "ClangToDotTranslator::LocInfoType" << std::endl;
// #endif
//     bool res = true;
//
//      node_desc.kind_hierarchy.push_back("LocInfoType");
//
//     ROSE_ASSERT(FAIL_FIXME == 0); // FIXME
//
//     return VisitType(loc_info_type, node_desc) && res;
// }

bool ClangToDotTranslator::VisitMacroQualifiedType(
    clang::MacroQualifiedType *macro_qualified_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::MacroQualifiedType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MacroQualifiedType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(macro_qualified_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitMemberPointerType(
    clang::MemberPointerType *member_pointer_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::MemberPointerType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MemberPointerType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(member_pointer_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitPackExpansionType(
    clang::PackExpansionType *pack_expansion_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::PackExpansionType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("PackExpansionType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(pack_expansion_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitParenType(clang::ParenType *paren_type,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitParenType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ParenType");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "inner_type", Traverse(paren_type->getInnerType().getTypePtr())));

  return VisitType(paren_type, node_desc);
}

bool ClangToDotTranslator::VisitPipeType(clang::PipeType *pipe_type,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::PipeType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("PipeType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(pipe_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitPointerType(clang::PointerType *pointer_type,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitPointerType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("PointerType");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "pointee_type", Traverse(pointer_type->getPointeeType().getTypePtr())));

  return VisitType(pointer_type, node_desc);
}

bool ClangToDotTranslator::VisitReferenceType(
    clang::ReferenceType *reference_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::ReferenceType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ReferenceType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "pointee_type", Traverse(reference_type->getPointeeType().getTypePtr())));

  return VisitType(reference_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitLValueReferenceType(
    clang::LValueReferenceType *lvalue_reference_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::LValueReferenceType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("LValueReferenceType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitReferenceType(lvalue_reference_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitRValueReferenceType(
    clang::RValueReferenceType *rvalue_reference_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::RValueReferenceType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("RValueReferenceType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitReferenceType(rvalue_reference_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitSubstTemplateTypeParmPackType(
    clang::SubstTemplateTypeParmPackType *subst_template_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::SubstTemplateTypeParmPackType"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SubstTemplateTypeParmPackType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(subst_template_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitSubstTemplateTypeParmType(
    clang::SubstTemplateTypeParmType *subst_template_type_parm_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::SubstTemplateTypeParmType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("SubstTemplateTypeParmType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "replacement_decl",
      Traverse(subst_template_type_parm_type->getAssociatedDecl())));

  clang::TemplateTypeParmDecl *parmDecl =
      const_cast<clang::TemplateTypeParmDecl *>(
          subst_template_type_parm_type->getReplacedParameter());
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "replaced_parameter", Traverse(parmDecl)));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "replacement_type",
      Traverse(
          subst_template_type_parm_type->getReplacementType().getTypePtr())));

  return VisitType(subst_template_type_parm_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitTagType(clang::TagType *tag_type,
                                        NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitTagType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TagType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "declaration", Traverse(tag_type->getDecl())));

  return VisitType(tag_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitEnumType(clang::EnumType *enum_type,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitEnumType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("EnumType");

  return VisitType(enum_type, node_desc);
}

bool ClangToDotTranslator::VisitRecordType(clang::RecordType *record_type,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitRecordType" << std::endl;
#endif

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "decl", Traverse(record_type->getDecl())));
  node_desc.kind_hierarchy.push_back("RecordType");

  return VisitType(record_type, node_desc);
}

bool ClangToDotTranslator::VisitTemplateSpecializationType(
    clang::TemplateSpecializationType *template_specialization_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::TemplateSpecializationType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TemplateSpecializationType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  if (template_specialization_type->isTypeAlias())
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "aliased_type",
        Traverse(template_specialization_type->getAliasedType().getTypePtr())));

  const clang::TemplateName &template_name =
      template_specialization_type->getTemplateName();
  VisitTemplateName(template_name, node_desc, "template_name");

  // In LLVM 20, iterator API was removed, use template_arguments() instead
  unsigned cnt = 0;
  for (const clang::TemplateArgument &arg :
       template_specialization_type->template_arguments()) {
    std::ostringstream oss;
    oss << "template_argument[" << cnt++ << "]";
    VisitTemplateArgument(arg, node_desc, oss.str());
  }

  return VisitType(template_specialization_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitTemplateTypeParmType(
    clang::TemplateTypeParmType *template_type_parm_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::TemplateTypeParmType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TemplateTypeParmType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "declaration", Traverse(template_type_parm_type->getDecl())));

  clang::IdentifierInfo *indent_info = template_type_parm_type->getIdentifier();

  assert(indent_info != NULL); // I am not sure of it let try

  node_desc.attributes.push_back(std::pair<std::string, std::string>(
      "identifier_name", indent_info->getName().data()));

  return VisitType(template_type_parm_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypedefType(clang::TypedefType *typedef_type,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitTypedefType" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("TypedefType");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "declaration", Traverse(typedef_type->getDecl())));

  return VisitType(typedef_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypeOfExprType(
    clang::TypeOfExprType *type_of_expr_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::TypeOfExprType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TypeOfExprType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(type_of_expr_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentTypeOfExprType(
    clang::DependentTypeOfExprType *dependent_type_of_expr_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::DependentTypeOfExprType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentTypeOfExprType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeOfExprType(dependent_type_of_expr_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypeOfType(clang::TypeOfType *type_of_type,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::TypeOfType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TypeOfType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(type_of_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypeWithKeyword(
    clang::TypeWithKeyword *type_with_keyword, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitTypeWithKeyword" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TypeWithKeyword");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  // In LLVM 20, use ElaboratedTypeKeyword enum
  switch (type_with_keyword->getKeyword()) {
  case clang::ElaboratedTypeKeyword::Struct:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "elaborated_type_keyword", "struct"));
    break;
  case clang::ElaboratedTypeKeyword::Union:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "elaborated_type_keyword", "union"));
    break;
  case clang::ElaboratedTypeKeyword::Class:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "elaborated_type_keyword", "class"));
    break;
  case clang::ElaboratedTypeKeyword::Enum:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("elaborated_type_keyword", "enum"));
    break;
  case clang::ElaboratedTypeKeyword::Typename:
    node_desc.attributes.push_back(std::pair<std::string, std::string>(
        "elaborated_type_keyword", "typename"));
    break;
  case clang::ElaboratedTypeKeyword::None:
    break;
  default:
    break;
  }

  return VisitType(type_with_keyword, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentNameType(
    clang::DependentNameType *dependent_name_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::DependentNameType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentNameType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  VisitNestedNameSpecifier(dependent_name_type->getQualifier(), node_desc,
                           "nested_name_qualifier");

  const clang::IdentifierInfo *identifier =
      dependent_name_type->getIdentifier();
  assert(identifier != NULL);
  node_desc.attributes.push_back(std::pair<std::string, std::string>(
      "identifier", identifier->getName().data()));

  return VisitTypeWithKeyword(dependent_name_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitDependentTemplateSpecializationType(
    clang::DependentTemplateSpecializationType
        *ependent_template_specialization_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::DependentTemplateSpecializationType"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DependentTemplateSpecializationType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeWithKeyword(ependent_template_specialization_type,
                              node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitElaboratedType(
    clang::ElaboratedType *elaborated_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitElaboratedType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("ElaboratedType");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "named_type", Traverse(elaborated_type->getNamedType().getTypePtr())));

  return VisitTypeWithKeyword(elaborated_type, node_desc);
}

bool ClangToDotTranslator::VisitUnaryTransformType(
    clang::UnaryTransformType *unary_transform_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::UnaryTransformType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UnaryTransformType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(unary_transform_type, node_desc) && res;
}

// bool
// ClangToDotTranslator::VisitDependentUnaryTransformType(clang::DependentUnaryTransformType
// * dependent_unary_transform_type, NodeDescriptor & node_desc) { #if
// DEBUG_VISIT_TYPE
//     std::cerr << "ClangToDotTranslator::DependentUnaryTransformType" <<
//     std::endl;
// #endif
//     bool res = true;
//
//      node_desc.kind_hierarchy.push_back("DependentUnaryTransformType");
//
//     ROSE_ASSERT(FAIL_FIXME == 0); // FIXME
//
//     return VisitUnaryTransformType(dependent_unary_transform_type, node_desc)
//     && res;
// }

bool ClangToDotTranslator::VisitUnresolvedUsingType(
    clang::UnresolvedUsingType *unresolved_using_type,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::UnresolvedUsingType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UnresolvedUsingType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitType(unresolved_using_type, node_desc) && res;
}

bool ClangToDotTranslator::VisitVectorType(clang::VectorType *vector_type,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitVectorType" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("VectorType");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "element_type", Traverse(vector_type->getElementType().getTypePtr())));

  std::ostringstream oss;
  oss << vector_type->getNumElements();
  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("number_element", oss.str()));

  return VisitType(vector_type, node_desc);
}

bool ClangToDotTranslator::VisitExtVectorType(
    clang::ExtVectorType *ext_vector_type, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_TYPE
  std::cerr << "ClangToDotTranslator::VisitExtVectorType" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ExtVectorType");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME Is it anything to be done here?

  return VisitVectorType(ext_vector_type, node_desc) && res;
}
