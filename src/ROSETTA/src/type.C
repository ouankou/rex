
#include "AstNodeClass.h"

#include "ROSETTA_macros.h"

#include "grammar.h"

void Grammar::setUpTypes() {
  NEW_TERMINAL_MACRO(TypeUnknown, "TypeUnknown", "T_UNKNOWN");
  NEW_TERMINAL_MACRO(TypeDependent, "TypeDependent", "T_DEPENDENT");
  NEW_TERMINAL_MACRO(TypeChar, "TypeChar", "T_CHAR");
  NEW_TERMINAL_MACRO(TypeSignedChar, "TypeSignedChar", "T_SIGNED_CHAR");
  NEW_TERMINAL_MACRO(TypeUnsignedChar, "TypeUnsignedChar", "T_UNSIGNED_CHAR");
  NEW_TERMINAL_MACRO(TypeShort, "TypeShort", "T_SHORT");
  NEW_TERMINAL_MACRO(TypeSignedShort, "TypeSignedShort", "T_SIGNED_SHORT");
  NEW_TERMINAL_MACRO(TypeUnsignedShort, "TypeUnsignedShort",
                     "T_UNSIGNED_SHORT");
  NEW_TERMINAL_MACRO(TypeInt, "TypeInt", "T_INT");
  NEW_TERMINAL_MACRO(TypeSignedInt, "TypeSignedInt", "T_SIGNED_INT");
  NEW_TERMINAL_MACRO(TypeUnsignedInt, "TypeUnsignedInt", "T_UNSIGNED_INT");
  NEW_TERMINAL_MACRO(TypeLong, "TypeLong", "T_LONG");
  NEW_TERMINAL_MACRO(TypeSignedLong, "TypeSignedLong", "T_SIGNED_LONG");
  NEW_TERMINAL_MACRO(TypeUnsignedLong, "TypeUnsignedLong", "T_UNSIGNED_LONG");
  NEW_TERMINAL_MACRO(TypeVoid, "TypeVoid", "T_VOID");
  NEW_TERMINAL_MACRO(TypeGlobalVoid, "TypeGlobalVoid", "T_GLOBAL_VOID");
  NEW_TERMINAL_MACRO(TypeWchar, "TypeWchar", "T_WCHAR");
  NEW_TERMINAL_MACRO(TypeFloat, "TypeFloat", "T_FLOAT");
  NEW_TERMINAL_MACRO(TypeDouble, "TypeDouble", "T_DOUBLE");
  NEW_TERMINAL_MACRO(TypeLongLong, "TypeLongLong", "T_LONG_LONG");
  NEW_TERMINAL_MACRO(TypeSignedLongLong, "TypeSignedLongLong",
                     "T_SIGNED_LONG_LONG");
  NEW_TERMINAL_MACRO(TypeUnsignedLongLong, "TypeUnsignedLongLong",
                     "T_UNSIGNED_LONG_LONG");
  NEW_TERMINAL_MACRO(TypeLongDouble, "TypeLongDouble", "T_LONG_DOUBLE");
  NEW_TERMINAL_MACRO(TypeFloat80, "TypeFloat80", "T_FLOAT80");
  NEW_TERMINAL_MACRO(TypeFloat128, "TypeFloat128", "T_FLOAT128");
  NEW_TERMINAL_MACRO(TypeString, "TypeString", "T_STRING");
  NEW_TERMINAL_MACRO(TypeBool, "TypeBool", "T_BOOL");
  NEW_TERMINAL_MACRO(TypeFloat16, "TypeFloat16", "T_FLOAT16");
  NEW_TERMINAL_MACRO(TypeFp16, "TypeFp16", "T_FP16");
  NEW_TERMINAL_MACRO(TypeBFloat16, "TypeBFloat16", "T_BFLOAT16");
  NEW_TERMINAL_MACRO(TypeFloat32x, "TypeFloat32x", "T_FLOAT32X");
  NEW_TERMINAL_MACRO(TypeFloat64x, "TypeFloat64x", "T_FLOAT64X");
  NEW_TERMINAL_MACRO(TypeFloat32, "TypeFloat32", "T_FLOAT32");
  NEW_TERMINAL_MACRO(TypeFloat64, "TypeFloat64", "T_FLOAT64");

  NEW_TERMINAL_MACRO(TypeNullptr, "TypeNullptr", "T_NULLPTR");
  NEW_TERMINAL_MACRO(TypeFortranBOZ, "TypeFortranBOZ", "T_FORTRAN_BOZ");

  NEW_TERMINAL_MACRO(TypeComplex, "TypeComplex", "T_COMPLEX");
  NEW_TERMINAL_MACRO(TypeImaginary, "TypeImaginary", "T_IMAGINARY");
  NEW_TERMINAL_MACRO(TypeDefault, "TypeDefault", "T_DEFAULT");
  NEW_TERMINAL_MACRO(TypeTargetBuiltin, "TypeTargetBuiltin",
                     "T_TARGET_BUILTIN");
  NEW_TERMINAL_MACRO(TypeFortranAssumed, "TypeFortranAssumed",
                     "T_FORTRAN_ASSUMED");
  NEW_TERMINAL_MACRO(TypeFortranUnlimitedPolymorphic,
                     "TypeFortranUnlimitedPolymorphic",
                     "T_FORTRAN_UNLIMITED_POLYMORPHIC");
  NEW_TERMINAL_MACRO(PointerMemberType, "PointerMemberType",
                     "T_MEMBER_POINTER");
  NEW_TERMINAL_MACRO(ReferenceType, "ReferenceType", "T_REFERENCE");

  NEW_TERMINAL_MACRO(RvalueReferenceType, "RvalueReferenceType",
                     "T_RVALUE_REFERENCE");

  NEW_TERMINAL_MACRO(DeclType, "DeclType", "T_DECLTYPE");

  NEW_TERMINAL_MACRO(TypeOfType, "TypeOfType", "T_TYPEOF_TYPE");
  NEW_TERMINAL_MACRO(UnaryTransformType, "UnaryTransformType",
                     "T_UNARY_TRANSFORM");

  NEW_TERMINAL_MACRO(TypeUnsigned128bitInteger, "TypeUnsigned128bitInteger",
                     "T_UNSIGNED_128BIT_INTEGER");
  NEW_TERMINAL_MACRO(TypeSigned128bitInteger, "TypeSigned128bitInteger",
                     "T_SIGNED_128BIT_INTEGER");

  NEW_TERMINAL_MACRO(TypeLabel, "TypeLabel", "T_LABEL");

  NEW_TERMINAL_MACRO(TemplateType, "TemplateType", "T_TEMPLATE");
  NEW_TERMINAL_MACRO(EnumType, "EnumType", "T_ENUM");
  NEW_TERMINAL_MACRO(TypedefType, "TypedefType", "T_TYPEDEF");

  NEW_TERMINAL_MACRO(NonrealType, "NonrealType", "T_NONREAL");

  NEW_TERMINAL_MACRO(AutoType, "AutoType", "T_AUTO");

  NEW_TERMINAL_MACRO(ModifierType, "ModifierType", "T_MODIFIER");

  NEW_TERMINAL_MACRO(ArrayType, "ArrayType", "T_ARRAY");
  NEW_TERMINAL_MACRO(TypeEllipse, "TypeEllipse", "T_ELLIPSE");

  NEW_TERMINAL_MACRO(TypeCrayPointer, "TypeCrayPointer", "T_CRAY_POINTER");

  NEW_TERMINAL_MACRO(MemberFunctionType, "MemberFunctionType",
                     "T_MEMBERFUNCTION");
  NEW_NONTERMINAL_MACRO(FunctionType, MemberFunctionType, "FunctionType",
                        "T_FUNCTION", true);
  NEW_NONTERMINAL_MACRO(PointerType, PointerMemberType, "PointerType",
                        "T_POINTER", true);

  NEW_TERMINAL_MACRO(ClassType, "ClassType", "T_CLASS");
  NEW_NONTERMINAL_MACRO(NamedType,
                        ClassType | EnumType | TypedefType | NonrealType,
                        "NamedType", "T_NAME", false);
  // The UTF character types are distinct fundamental types.  Keep separate
  // ROSE nodes for them instead of folding them into their underlying integer
  // representations.
  NEW_TERMINAL_MACRO(TypeChar8, "TypeChar8", "T_CHAR8");
  NEW_TERMINAL_MACRO(TypeChar16, "TypeChar16", "T_CHAR16");
  NEW_TERMINAL_MACRO(TypeChar32, "TypeChar32", "T_CHAR32");

  NEW_NONTERMINAL_MACRO(SignedIntegralType,
                        TypeInt | TypeLong | TypeLongLong | TypeShort |
                            TypeSigned128bitInteger | TypeSignedChar |
                            TypeSignedInt | TypeSignedLong |
                            TypeSignedLongLong | TypeSignedShort,
                        "SignedIntegralType", "SignedIntegralTypeTag", false);

  NEW_NONTERMINAL_MACRO(
      UnsignedIntegralType,
      TypeUnsigned128bitInteger | TypeUnsignedChar | TypeUnsignedInt |
          TypeUnsignedLong | TypeUnsignedLongLong | TypeUnsignedShort,
      "UnsignedIntegralType", "UnsignedIntegralTypeTag", false);

  NEW_NONTERMINAL_MACRO(IntegralType,
                        SignedIntegralType | UnsignedIntegralType | TypeChar |
                            TypeWchar | TypeChar8 | TypeChar16 | TypeChar32 |
                            TypeBool,
                        "IntegralType", "IntegralTypeTag", false);

  NEW_NONTERMINAL_MACRO(FloatingPointType,
                        TypeFloat | TypeFloat128 | TypeFloat80 | TypeDouble |
                            TypeLongDouble | TypeFloat16 | TypeBFloat16 |
                            TypeFp16 | TypeFloat32x | TypeFloat64x |
                            TypeFloat32 | TypeFloat64,
                        "FloatingPointType", "FloatingPointTypeTag", false);

  NEW_NONTERMINAL_MACRO(
      Type,
      TypeUnknown | TypeDependent | TypeVoid | TypeGlobalVoid | TypeString |
          PointerType | ReferenceType | NamedType | ModifierType |
          FunctionType | ArrayType | TypeEllipse | TemplateType | TypeComplex |
          TypeImaginary | TypeDefault | TypeTargetBuiltin | TypeCrayPointer |
          TypeLabel | TypeFortranAssumed | TypeFortranUnlimitedPolymorphic |
          RvalueReferenceType | TypeNullptr | TypeFortranBOZ | DeclType |
          TypeOfType | UnaryTransformType | AutoType | IntegralType |
          FloatingPointType,
      "Type", "TypeTag", false);

  Type.setFunctionPrototype("HEADER", "../Grammar/Type.code");
  Type.setFunctionSource("SOURCE_MAIN_TYPE", "../Grammar/Type.code");
  Type.setAutomaticGenerationOfConstructor(false);
  Type.setDestructorPrologue(R"(
    SgNode *parent = get_parent();
    if (parent != nullptr && SgNode::isLiveNode(parent)) {
      if (auto *index = isSgFunctionTypeSymbol(parent)) {
        ROSE_ASSERT(index->get_type() == this);
        SgNode *owner = index->get_parent();
        if (owner != nullptr) {
          ROSE_ASSERT(SgNode::isLiveNode(owner));
          auto *table = isSgSymbolTable(owner);
          ROSE_ASSERT(table != nullptr && table->exists(index));
          table->remove(index);
        }
        set_parent(nullptr);
        delete index;
      }
    }
)");

  Type.setSubTreeFunctionPrototype("HEADER_GET_MANGLED",
                                   "../Grammar/Type.code");
  Type.excludeFunctionPrototype("HEADER_GET_MANGLED", "../Grammar/Type.code");

  Type.setDataPrototype("bool", "isCoArray", "= 0", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Reverse caches belong to one type identity. Copy construction leaves them
  // empty; copying source entries and clearing them after publication can erase
  // wrappers already created for the new type.
  Type.setDataPrototype("SgReferenceType*", "ref_to", "= NULL",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  Type.setDataPrototype("SgPointerType*", "ptr_to", "= NULL",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  Type.setDataPrototype("SgModifierNodes*", "modifiers", "= NULL",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, DEF_DELETE, NO_COPY_DATA);
  Type.setDataPrototype("SgRvalueReferenceType*", "rvalue_ref_to", "= NULL",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_FLAG_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  Type.setDataPrototype("std::vector<SgUnaryTransformType*>",
                        "unary_transform_types", "", NO_CONSTRUCTOR_PARAMETER,
                        NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                        NO_COPY_DATA);

  Type.setDataPrototype("SgExpression*", "type_kind", "= nullptr",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                        OPTIONAL_TRAVERSAL_MEMBER);
  Type.setDataPrototype("bool", "hasTypeKindStar", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // DOUBLE PRECISION and DOUBLE COMPLEX have no source selector expression,
  // but their implied KIND is target-configured Flang semantic state. Keep
  // that value distinct from type_kind so the exact fixed source spelling is
  // emitted unchanged while consumers can still validate its resolved type.
  Type.setDataPrototype("bool", "fortran_fixed_kind_value_is_available",
                        "= false", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  Type.setDataPrototype("int64_t", "fortran_fixed_kind_value", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);
  // Source-spelled Fortran intrinsic types are structural syntax records, not
  // semantic canonical types. This marker keeps reconstruction and copying
  // from interning them into the global semantic type tables even when every
  // selector is omitted or represented by a nonconstant sentinel.
  Type.setDataPrototype("bool", "fortran_source_syntax", "= false",
                        NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE);

  // Embedded checkpoint type identities belong to one object lifetime.
  Type.setDataPrototype("uint64_t", "ast_json_type_identity", "= 0",
                        NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                        NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);

  Type.setDataPrototype("AstAttributeMechanism*", "attributeMechanism",
                        "= NULL", NO_CONSTRUCTOR_PARAMETER,
                        BUILD_FLAG_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
                        CLONE_PTR);
  Type.setFunctionPrototype("HEADER_ATTRIBUTE_SUPPORT",
                            "../Grammar/Support.code");
  Type.setFunctionSource("SOURCE_ATTRIBUTE_SUPPORT", "../Grammar/Support.code");

  NamedType.excludeFunctionPrototype("HEADER_GET_MANGLED",
                                     "../Grammar/Type.code");

  Type.setSubTreeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                   "../Grammar/Type.code");
  Type.excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                "../Grammar/Type.code");

  Type.setSubTreeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                "../Grammar/Type.code");
  Type.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                             "../Grammar/Type.code");

  FloatingPointType.setSubTreeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                             "../Grammar/Type.code");
  FloatingPointType.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                          "../Grammar/Type.code");

  IntegralType.setSubTreeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                        "../Grammar/Type.code");
  IntegralType.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                     "../Grammar/Type.code");

  SignedIntegralType.setSubTreeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                              "../Grammar/Type.code");
  SignedIntegralType.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                           "../Grammar/Type.code");

  UnsignedIntegralType.setSubTreeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                                "../Grammar/Type.code");
  UnsignedIntegralType.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                             "../Grammar/Type.code");

  TypeComplex.excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                       "../Grammar/Type.code");
  TypeComplex.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                    "../Grammar/Type.code");

  TypeImaginary.excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                         "../Grammar/Type.code");
  TypeImaginary.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                      "../Grammar/Type.code");

  TypeTargetBuiltin.excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                             "../Grammar/Type.code");
  TypeTargetBuiltin.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                          "../Grammar/Type.code");
  UnaryTransformType.excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                              "../Grammar/Type.code");
  UnaryTransformType.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                           "../Grammar/Type.code");
  UnaryTransformType.setFunctionPrototype("HEADER_UNARY_TRANSFORM_TYPE",
                                          "../Grammar/Type.code");
  UnaryTransformType.setDataPrototype(
      "SgType*", "operand_type", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);
  UnaryTransformType.setMemberDestructor("operand_type", R"(
    if (SgNode::isLiveNode(p_operand_type)) {
      auto &types = p_operand_type->p_unary_transform_types;
      types.erase(std::remove(types.begin(), types.end(), this), types.end());
    }
)");
  UnaryTransformType.setDataPrototype(
      "SgUnaryTransformType::operation_enum", "operation",
      "= SgUnaryTransformType::e_invalid", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  TypeString.excludeFunctionPrototype("HEADER_CREATE_TYPE_WITH_PARAMETER",
                                      "../Grammar/Type.code");

  // String types depend on their length expression.

  TypeString.setDataPrototype("SgExpression*", "lengthExpression", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);
  // A nonconstant semantic CHARACTER length is resolved against the exact
  // typed source expression during the same declaration transaction. Such a
  // type may be used for semantic name publication while pending, but it must
  // never cross an AST finalization, JSON, copy, or unparse boundary.
  TypeString.setDataPrototype("bool", "fortran_dynamic_length_pending",
                              "= false", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  // A semantic expression can have a runtime CHARACTER length even when it is
  // neither an assumed-length (*) nor deferred-length (:) declaration. Keep
  // that valid semantic category explicit instead of collapsing it to ':'.
  TypeString.setDataPrototype("bool", "fortran_dynamic_result_length",
                              "= false", NO_CONSTRUCTOR_PARAMETER,
                              BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

#define CUSTOM_CREATE_TYPE_MACRO(typeObjectName, sourceCodeName,               \
                                 parameterString)                              \
  typeObjectName.excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",         \
                                          "../Grammar/Type.code");             \
  typeObjectName.excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",            \
                                       "../Grammar/Type.code");                \
  typeObjectName.setFunctionPrototype("HEADER_CREATE_TYPE_WITH_PARAMETER",     \
                                      "../Grammar/Type.code");                 \
  typeObjectName.editSubstitute("CREATE_TYPE_PARAMETER", parameterString);     \
  typeObjectName.setFunctionSource(sourceCodeName, "../Grammar/Type.code");

  CUSTOM_CREATE_TYPE_MACRO(TypeInt, "SOURCE_CREATE_TYPE_FOR_TYPE_INT_TYPE",
                           "int sz = 0");

  CUSTOM_CREATE_TYPE_MACRO(PointerType, "SOURCE_CREATE_TYPE_FOR_POINTER_TYPE",
                           "SgType* type = NULL");
  CUSTOM_CREATE_TYPE_MACRO(PointerMemberType,
                           "SOURCE_CREATE_TYPE_FOR_POINTER_MEMBER_TYPE",
                           "SgType* type = NULL, SgType* class_type = NULL");
  CUSTOM_CREATE_TYPE_MACRO(ReferenceType,
                           "SOURCE_CREATE_TYPE_FOR_REFERENCE_TYPE",
                           "SgType* type = NULL");

  CUSTOM_CREATE_TYPE_MACRO(RvalueReferenceType,
                           "SOURCE_CREATE_TYPE_FOR_RVALUE_REFERENCE_TYPE",
                           "SgType* type = NULL");

  CUSTOM_CREATE_TYPE_MACRO(DeclType, "SOURCE_CREATE_TYPE_FOR_DECL_TYPE",
                           "SgExpression* expr, SgType* base_type");

  CUSTOM_CREATE_TYPE_MACRO(TypeOfType, "SOURCE_CREATE_TYPE_FOR_TYPEOF_TYPE",
                           "SgExpression* expr = NULL");

  CUSTOM_CREATE_TYPE_MACRO(ClassType, "SOURCE_CREATE_TYPE_FOR_CLASS_TYPE",
                           "SgDeclarationStatement* decl = NULL");

  for (auto *type : {&TemplateType, &NonrealType, &AutoType, &ModifierType,
                     &FunctionType, &MemberFunctionType, &ArrayType}) {
    type->excludeFunctionPrototype("HEADER_COMMON_CREATE_TYPE",
                                   "../Grammar/Type.code");
    type->excludeFunctionSource("SOURCE_COMMON_CREATE_TYPE",
                                "../Grammar/Type.code");
  }

  CUSTOM_CREATE_TYPE_MACRO(EnumType, "SOURCE_CREATE_TYPE_FOR_ENUM_TYPE",
                           "SgEnumDeclaration* decl = NULL");
  CUSTOM_CREATE_TYPE_MACRO(TypedefType, "SOURCE_CREATE_TYPE_FOR_TYPEDEF_TYPE",
                           "SgTypedefDeclaration* decl = NULL");
  ModifierType.setFunctionSource("SOURCE_MODIFIER_TYPE_INTERNING",
                                 "../Grammar/Type.code");

  CUSTOM_CREATE_TYPE_MACRO(TypeString, "SOURCE_CREATE_TYPE_FOR_STRING_TYPE",
                           "SgExpression* expr = NULL, size_t length = 0");

  TypeComplex.setFunctionSource("SOURCE_CREATE_TYPE_FOR_COMPLEX_TYPE",
                                "../Grammar/Type.code");
  TypeImaginary.setFunctionSource("SOURCE_CREATE_TYPE_FOR_COMPLEX_TYPE",
                                  "../Grammar/Type.code");

  TypeInt.setFunctionPrototype("HEADER_TYPE_INT_TYPE", "../Grammar/Type.code");

  TypeInt.setDataPrototype("int", "field_size", "= 0", CONSTRUCTOR_PARAMETER,
                           BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  // Traverse the pointee type so type transformations can reach it.
  PointerType.setDataPrototype("SgType*", "base_type", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               DEF_TRAVERSAL, NO_DELETE, CLONE_TREE,
                               OPTIONAL_TRAVERSAL_MEMBER);
  PointerType.setMemberDestructor("base_type", R"(
    // Bulk teardown can retire the borrowed base before its wrappers.
    if (SgNode::isLiveNode(p_base_type) && p_base_type->get_ptr_to() == this)
      p_base_type->set_ptr_to(nullptr);
)");

  ReferenceType.setDataPrototype("SgType*", "base_type", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE, CLONE_TREE);
  ReferenceType.setMemberDestructor("base_type", R"(
    if (SgNode::isLiveNode(p_base_type) && p_base_type->get_ref_to() == this)
      p_base_type->set_ref_to(nullptr);
)");

  RvalueReferenceType.setDataPrototype(
      "SgType*", "base_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, CLONE_TREE);
  RvalueReferenceType.setMemberDestructor("base_type", R"(
    if (SgNode::isLiveNode(p_base_type) && p_base_type->get_rvalue_ref_to() == this)
      p_base_type->set_rvalue_ref_to(nullptr);
)");

  DeclType.setFunctionPrototype("HEADER_DECL_TYPE", "../Grammar/Type.code");
  DeclType.setDataPrototype("SgExpression*", "base_expression", "= NULL",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                            OPTIONAL_TRAVERSAL_MEMBER);

  DeclType.setDataPrototype("SgType*", "base_type", "= NULL",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  DeclType.setDataPrototype("bool", "is_gnu_decltype", "= false",
                            CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);

  DeclType.setFunctionPrototype("HEADER_GET_BASE_TYPE", "../Grammar/Type.code");

  TypeOfType.setFunctionPrototype("HEADER_TYPEOF_TYPE", "../Grammar/Type.code");
  TypeOfType.setDataPrototype("SgExpression*", "base_expression", "= NULL",
                              CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                              DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                              OPTIONAL_TRAVERSAL_MEMBER);
  TypeOfType.setDataPrototype("SgType*", "base_type", "= NULL",
                              CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE);
  // A copied expression owns a new lexical typeof occurrence.
  TypeOfType.setDataPrototype("uint64_t", "expression_occurrence_id", "= 0",
                              NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                              NO_TRAVERSAL, NO_DELETE, NO_COPY_DATA);
  TypeOfType.setFunctionPrototype("HEADER_GET_BASE_TYPE",
                                  "../Grammar/Type.code");

  PointerMemberType.setFunctionPrototype("HEADER_POINTER_MEMBER_TYPE",
                                         "../Grammar/Type.code");
  PointerMemberType.setDataPrototype(
      "SgType*", "class_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  PointerMemberType.setMemberDestructor("class_type",
                                        "forgetCanonicalSemanticType(this);");
  // An unqualified injected class name in a member-pointer declarator denotes
  // the current instantiation semantically, but its exact source surface is the
  // bare class name. Preserve that use-site role on the noncanonical TypeLoc
  // wrapper rather than deriving spelling from the canonical class type.
  PointerMemberType.setDataPrototype(
      "bool", "source_class_type_is_unqualified_injected_name", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PointerMemberType.setDataPrototype(
      "bool", "source_base_type_qualification_present", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PointerMemberType.setDataPrototype(
      "bool", "source_base_type_global_qualification", "= false",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);
  PointerMemberType.setDataPrototype(
      "SgStringList", "source_base_type_qualification_tokens", "",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  NamedType.setFunctionPrototype("HEADER_VIRTUAL_GET_NAME",
                                 "../Grammar/Type.code");
  NamedType.setFunctionPrototype("HEADER_GET_QUALIFIED_NAME",
                                 "../Grammar/Type.code");

  // The declaration owns the named type; this edge refers back to that owner.
  NamedType.setDataPrototype("SgDeclarationStatement*", "declaration", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);

  ClassType.setFunctionPrototype("HEADER_CLASS_TYPE", "../Grammar/Type.code");
  ClassType.setFunctionPrototype("HEADER_GET_NAME", "../Grammar/Type.code");

  NonrealType.setFunctionPrototype("HEADER_NONREAL_TYPE",
                                   "../Grammar/Type.code");
  NonrealType.setFunctionPrototype("HEADER_GET_NAME", "../Grammar/Type.code");
  NonrealType.setDataPrototype(
      "SgType*", "aliased_type", "= nullptr", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_CONSTRUCTOR_REFERENCE);
  NonrealType.setDataPrototype("SgTypePtrList", "owned_aliased_types", "= {}",
                               NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, DEF_DELETE, NO_COPY_DATA);

  AutoType.setFunctionPrototype("HEADER_AUTO_TYPE", "../Grammar/Type.code");
  AutoType.setDataPrototype("bool", "is_constrained", "= false",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            NO_TRAVERSAL, NO_DELETE);
  AutoType.setDataPrototype("SgNonrealRefExp*", "type_constraint", "= nullptr",
                            CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                            DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                            OPTIONAL_TRAVERSAL_MEMBER);

  TemplateType.setFunctionPrototype("HEADER_TEMPLATE_TYPE",
                                    "../Grammar/Type.code");

  TemplateType.setDataPrototype("SgName", "name", "= \"\"",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  TemplateType.setDataPrototype(
      "int", "template_parameter_position", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateType.setDataPrototype(
      "int", "template_parameter_depth", "= -1", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateType.setDataPrototype("std::optional<canonical_source_identity>",
                                "canonical_source_identity", "= std::nullopt",
                                NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE, COPY_DATA);

  TemplateType.setDataPrototype("SgTemplateParameter *", "template_parameter",
                                "= NULL", NO_CONSTRUCTOR_PARAMETER,
                                BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                                COPY_REFERENCE);
  TemplateType.setDataPrototype(
      "SgTemplateArgumentPtrList", "tpl_args", "= SgTemplateArgumentPtrList()",
      NO_CONSTRUCTOR_PARAMETER, BUILD_LIST_ACCESS_FUNCTIONS, DEF_TRAVERSAL,
      DEF_DELETE, CLONE_TREE);

  TemplateType.setDataPrototype(
      "bool", "packed", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TemplateType.setDataPrototype(
      "bool", "is_pack_substitution", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  EnumType.setFunctionPrototype("HEADER_ENUM_TYPE", "../Grammar/Type.code");
  EnumType.setFunctionPrototype("HEADER_GET_NAME", "../Grammar/Type.code");

  TypedefType.setFunctionPrototype("HEADER_GET_NAME", "../Grammar/Type.code");

  // This get_base_type function is not associated with a base_type data filed
  // in this object
  TypedefType.setFunctionPrototype("HEADER_GET_BASE_TYPE",
                                   "../Grammar/Type.code");
  TypedefType.setFunctionPrototype("HEADER_TYPEDEF_TYPE",
                                   "../Grammar/Type.code");
  // Exclude the get_mangled function since we include it in the
  // HEADER_MODIFIER_TYPE string
  ModifierType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                     "../Grammar/Type.code");
  ModifierType.setFunctionPrototype("HEADER_MODIFIER_TYPE",
                                    "../Grammar/Type.code");

  ModifierType.setDataPrototype(
      "SgType*", "base_type", "", CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
      DEF_TRAVERSAL, NO_DELETE, CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER);
  ModifierType.setMemberDestructor("base_type", R"(
    if (SgNode::isLiveNode(p_base_type)) {
      SgNode *owner = p_base_type;
      auto *bucket = p_base_type->get_modifiers();
      while (bucket != nullptr) {
        ROSE_ASSERT(SgNode::isLiveNode(bucket) && bucket->get_parent() == owner);
        ROSE_ASSERT(bucket->p_nodes.size() == 4);
        auto *next = bucket->get_next();
        if (next != nullptr)
          ROSE_ASSERT(SgNode::isLiveNode(next) && next->get_parent() == bucket);
        bool retired = false;
        for (auto &entry : bucket->p_nodes) {
          if (entry == this) {
            entry = nullptr;
            retired = true;
          }
        }
        if (retired &&
            std::all_of(bucket->p_nodes.begin(), bucket->p_nodes.end(),
                        [](SgModifierType *entry) { return entry == nullptr; })) {
          bucket->p_next = nullptr;
          if (auto *previous = isSgModifierNodes(owner))
            previous->p_next = next;
          else
            p_base_type->set_modifiers(next);
          if (next != nullptr)
            next->set_parent(owner);
          bucket->set_parent(nullptr);
          delete bucket;
        } else {
          owner = bucket;
        }
        bucket = next;
      }
    }
)");

  ModifierType.setDataPrototype("SgTypeModifier", "typeModifier", "",
                                NO_CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  FunctionType.setFunctionPrototype("HEADER_FUNCTION_TYPE",
                                    "../Grammar/Type.code");
  FunctionType.setDataPrototype("SgFunctionTypeTable*", "function_type_table",
                                "= nullptr", NO_CONSTRUCTOR_PARAMETER,
                                NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                                NO_COPY_DATA);
  FunctionType.setDestructorPrologue(R"(
    if (p_function_type_table != nullptr) {
      ROSE_ASSERT(SgNode::isLiveNode(p_function_type_table));
      p_function_type_table->remove_function_type(this);
    }
)");
  FunctionType.setDataPrototype("SgType*", "return_type", "",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                DEF_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE,
                                OPTIONAL_TRAVERSAL_MEMBER);
  FunctionType.setDataPrototype("bool", "has_ellipses", "",
                                CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  FunctionType.setDataPrototype(
      "SgFunctionParameterTypeList*", "argument_list", "",
      CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, DEF_DELETE,
      CLONE_TREE, std::nullopt, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  FunctionType.setDataPrototype(
      "SgFunctionType::exception_specification_kind_enum",
      "exception_specification_kind", "= e_exception_none",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionType.setDataPrototype(
      "SgExpression*", "exception_specification_expression", "= nullptr",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, DEF_TRAVERSAL, DEF_DELETE,
      CLONE_TREE, OPTIONAL_TRAVERSAL_MEMBER, DIRECT_TRAVERSAL_ACCESS,
      COPY_BEFORE_CONSTRUCTOR_DEPENDENCIES);
  FunctionType.setDataPrototype("SgTypePtrList",
                                "exception_specification_types", "= {}",
                                CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, DELETE_IF_OWNED, CLONE_TREE);
  FunctionType.setDataPrototype("std::vector<bool>",
                                "exception_specification_type_pack_expansions",
                                "= {}", CONSTRUCTOR_PARAMETER,
                                NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionType.setDataPrototype("SgFunctionDeclaration*",
                                "exception_specification_source", "= nullptr",
                                CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  FunctionType.setDataPrototype("SgFunctionDeclaration*",
                                "exception_specification_pattern", "= nullptr",
                                CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  FunctionType.setDataPrototype(
      "bool", "exception_specification_source_is_self", "= false",
      CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionType.setDataPrototype("bool", "exception_specification_is_written",
                                "= false", CONSTRUCTOR_PARAMETER,
                                NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionType.setDataPrototype("SgFunctionType*", "semantic_signature",
                                "= nullptr", CONSTRUCTOR_PARAMETER,
                                NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE,
                                COPY_REFERENCE);
  FunctionType.setDataPrototype(
      "bool", "source_trailing_return", "= false", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  FunctionType.setDataPrototype("bool", "has_prototype", "= true",
                                CONSTRUCTOR_PARAMETER, NO_ACCESS_FUNCTIONS,
                                NO_TRAVERSAL, NO_DELETE);
  MemberFunctionType.setFunctionPrototype("HEADER_MEMBER_FUNCTION_TYPE",
                                          "../Grammar/Type.code");
  MemberFunctionType.setDataPrototype(
      "SgType*", "class_type", "= NULL", CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE, COPY_REFERENCE);
  MemberFunctionType.setDataPrototype(
      "unsigned int", "mfunc_specifier", "= 0", CONSTRUCTOR_PARAMETER,
      NO_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);

  ArrayType.setDataPrototype("SgType*", "base_type", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE, CLONE_TREE);

  // The array type owns its index expression.
  ArrayType.setDataPrototype("SgExpression*", "index", "= NULL",
                             CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);

  ArrayType.setDataPrototype("SgExprListExp*", "dim_info", "= NULL",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             DEF_TRAVERSAL, DEF_DELETE, COPY_DATA,
                             OPTIONAL_TRAVERSAL_MEMBER);
  ArrayType.setDataPrototype("int", "rank", "= 0", NO_CONSTRUCTOR_PARAMETER,
                             BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  ArrayType.setDataPrototype("int", "number_of_elements", "= 0",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  ArrayType.setDataPrototype("bool", "is_variable_length_array", "= false",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  TypeComplex.setFunctionPrototype("HEADER_TYPE_COMPLEX_TYPE",
                                   "../Grammar/Type.code");
  TypeComplex.setDataPrototype("SgType*", "base_type", "= NULL",
                               CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                               NO_TRAVERSAL, NO_DELETE);
  TypeComplex.setMemberDestructor("base_type",
                                  "forgetCanonicalSemanticType(this);");

  TypeImaginary.setFunctionPrototype("HEADER_TYPE_COMPLEX_TYPE",
                                     "../Grammar/Type.code");
  TypeImaginary.setDataPrototype("SgType*", "base_type", "= NULL",
                                 CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                                 NO_TRAVERSAL, NO_DELETE);
  TypeImaginary.setMemberDestructor("base_type",
                                    "forgetCanonicalSemanticType(this);");

  TypeString.setFunctionPrototype("HEADER_TYPE_STRING_TYPE",
                                  "../Grammar/Type.code");

  // Target-defined Clang builtin types (SVE, RVV, PPC MMA, WebAssembly,
  // AMDGPU, OpenCL, and HLSL) are semantic types in their own right.  They
  // must not be collapsed to a scalar, vector, or unknown-type surrogate.
  TypeTargetBuiltin.setFunctionPrototype("HEADER_TYPE_TARGET_BUILTIN_TYPE",
                                         "../Grammar/Type.code");
  TypeTargetBuiltin.setDataPrototype(
      "SgName", "spelling", "= \"\"", NO_CONSTRUCTOR_PARAMETER,
      BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL, NO_DELETE);
  TypeTargetBuiltin.setDataPrototype(
      "SgTypeTargetBuiltin::target_family_enum", "target_family",
      "= SgTypeTargetBuiltin::e_target_builtin_aarch64",
      NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS, NO_TRAVERSAL,
      NO_DELETE);

  TypeLabel.setFunctionPrototype("HEADER_TYPE_LABEL_TYPE",
                                 "../Grammar/Type.code");
  TypeLabel.setDataPrototype("SgName", "name", "= \"\"",
                             NO_CONSTRUCTOR_PARAMETER, BUILD_ACCESS_FUNCTIONS,
                             NO_TRAVERSAL, NO_DELETE);

  Type.setSubTreeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");
  Type.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");

  PointerType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                    "../Grammar/Type.code");
  PointerType.setFunctionSource("SOURCE_GET_MANGLED_BASE_TYPE",
                                "../Grammar/Type.code");

  ReferenceType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                      "../Grammar/Type.code");
  ReferenceType.setFunctionSource("SOURCE_GET_MANGLED_BASE_TYPE",
                                  "../Grammar/Type.code");

  RvalueReferenceType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                            "../Grammar/Type.code");
  RvalueReferenceType.setFunctionSource("SOURCE_GET_MANGLED_BASE_TYPE",
                                        "../Grammar/Type.code");

  TypeComplex.excludeFunctionSource("SOURCE_GET_MANGLED",
                                    "../Grammar/Type.code");
  TypeComplex.setFunctionSource("SOURCE_GET_MANGLED_BASE_TYPE",
                                "../Grammar/Type.code");

  DeclType.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");

  TypeOfType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                   "../Grammar/Type.code");

  ArrayType.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");

  FunctionType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                     "../Grammar/Type.code");

  TypedefType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                    "../Grammar/Type.code");
  NamedType.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");

  MemberFunctionType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                           "../Grammar/Type.code");
  ModifierType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                     "../Grammar/Type.code");

  ClassType.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");

  NonrealType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                    "../Grammar/Type.code");
  NonrealType.setFunctionSource("SOURCE_NONREAL_TYPE", "../Grammar/Type.code");

  AutoType.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");
  AutoType.setFunctionSource("SOURCE_AUTO_TYPE", "../Grammar/Type.code");

  EnumType.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");

  PointerMemberType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                          "../Grammar/Type.code");
  PointerMemberType.setFunctionSource("SOURCE_POINTER_MEMBER_GET_MANGLED",
                                      "../Grammar/Type.code");

  TemplateType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                     "../Grammar/Type.code");

  TypeUnknown.editSubstitute("MANGLED_ID_STRING", "Unknown");
  TypeDependent.editSubstitute("MANGLED_ID_STRING", "Dependent");
  TypeChar.editSubstitute("MANGLED_ID_STRING", "c");
  TypeSignedChar.editSubstitute("MANGLED_ID_STRING", "si");
  TypeUnsignedChar.editSubstitute("MANGLED_ID_STRING", "Uc");
  TypeShort.editSubstitute("MANGLED_ID_STRING", "s");
  TypeSignedShort.editSubstitute("MANGLED_ID_STRING", "Ss");
  TypeUnsignedShort.editSubstitute("MANGLED_ID_STRING", "Us");
  TypeInt.editSubstitute("MANGLED_ID_STRING", "i");
  TypeSignedInt.editSubstitute("MANGLED_ID_STRING", "Si");
  TypeUnsignedInt.editSubstitute("MANGLED_ID_STRING", "Ui");
  TypeLong.editSubstitute("MANGLED_ID_STRING", "l");
  TypeSignedLong.editSubstitute("MANGLED_ID_STRING", "Sl");
  TypeUnsignedLong.editSubstitute("MANGLED_ID_STRING", "Ul");
  TypeVoid.editSubstitute("MANGLED_ID_STRING", "v");
  TypeGlobalVoid.editSubstitute("MANGLED_ID_STRING", "gv");
  TypeWchar.editSubstitute("MANGLED_ID_STRING", "wc");
  TypeFloat.editSubstitute("MANGLED_ID_STRING", "f");
  TypeDouble.editSubstitute("MANGLED_ID_STRING", "d");
  TypeLongLong.editSubstitute("MANGLED_ID_STRING", "L");
  TypeSignedLongLong.editSubstitute("MANGLED_ID_STRING", "SL");
  TypeUnsignedLongLong.editSubstitute("MANGLED_ID_STRING", "UL");

  TypeFloat80.editSubstitute("MANGLED_ID_STRING", "w");
  TypeFloat128.editSubstitute("MANGLED_ID_STRING", "q");

  TypeFloat16.editSubstitute("MANGLED_ID_STRING", "f16");
  TypeFp16.editSubstitute("MANGLED_ID_STRING", "fp16");
  TypeBFloat16.editSubstitute("MANGLED_ID_STRING", "bf");
  TypeFloat32x.editSubstitute("MANGLED_ID_STRING", "f32x");
  TypeFloat64x.editSubstitute("MANGLED_ID_STRING", "f64x");
  TypeFloat32.editSubstitute("MANGLED_ID_STRING", "f32");
  TypeFloat64.editSubstitute("MANGLED_ID_STRING", "f64");

  TypeSigned128bitInteger.editSubstitute("MANGLED_ID_STRING", "SL128");
  TypeUnsigned128bitInteger.editSubstitute("MANGLED_ID_STRING", "UL128");

  TypeChar8.editSubstitute("MANGLED_ID_STRING", "c8");
  TypeChar16.editSubstitute("MANGLED_ID_STRING", "c16");
  TypeChar32.editSubstitute("MANGLED_ID_STRING", "c32");

  TypeLongDouble.editSubstitute("MANGLED_ID_STRING", "ld");
  TypeString.editSubstitute("MANGLED_ID_STRING", "str");
  TypeBool.editSubstitute("MANGLED_ID_STRING", "b");

  TypeNullptr.editSubstitute("MANGLED_ID_STRING", "nullptr_t");
  TypeFortranBOZ.editSubstitute("MANGLED_ID_STRING", "fortran_boz");

  TypeComplex.editSubstitute("MANGLED_ID_STRING", "Complex");
  TypeImaginary.editSubstitute("MANGLED_ID_STRING", "Imaginary");
  TypeFortranAssumed.editSubstitute("MANGLED_ID_STRING", "FortranAssumed");
  TypeFortranUnlimitedPolymorphic.editSubstitute("MANGLED_ID_STRING",
                                                 "FortranUnlimitedPolymorphic");
  PointerType.editSubstitute("MANGLED_ID_STRING", "P");
  ReferenceType.editSubstitute("MANGLED_ID_STRING", "R");
  RvalueReferenceType.editSubstitute("MANGLED_ID_STRING", "Rvalue");

  DeclType.editSubstitute("MANGLED_ID_STRING", "decltype");

  TypeOfType.editSubstitute("MANGLED_ID_STRING", "typeOftype");

  TypeEllipse.editSubstitute("MANGLED_ID_STRING", "e");

  TypedefType.setFunctionSource("SOURCE_TYPEDEF_TYPE", "../Grammar/Type.code");
  MemberFunctionType.setFunctionSource("SOURCE_MEMBER_FUNCTION_TYPE",
                                       "../Grammar/Type.code");
  PointerMemberType.setFunctionSource("SOURCE_POINTER_MEMBER_TYPE",
                                      "../Grammar/Type.code");

  NamedType.setFunctionSource("SOURCE_NAMED_TYPE", "../Grammar/Type.code");
  EnumType.setFunctionSource("SOURCE_ENUM_TYPE", "../Grammar/Type.code");
  FunctionType.setFunctionSource("SOURCE_FUNCTION_TYPE",
                                 "../Grammar/Type.code");

  ClassType.setFunctionSource("SOURCE_CLASS_TYPE", "../Grammar/Type.code");

  TemplateType.setFunctionSource("SOURCE_TEMPLATE_TYPE",
                                 "../Grammar/Type.code");

  DeclType.setFunctionSource("SOURCE_DECL_TYPE", "../Grammar/Type.code");

  TypeOfType.setFunctionSource("SOURCE_TYPEOF_TYPE", "../Grammar/Type.code");

  ArrayType.setFunctionSource("SOURCE_ARRAY_TYPE", "../Grammar/Type.code");
  ModifierType.setFunctionSource("SOURCE_MODIFIER_TYPE",
                                 "../Grammar/Type.code");
  TypeCrayPointer.editSubstitute("MANGLED_ID_STRING", "CrayPointer");

  TypeString.excludeFunctionSource("SOURCE_GET_MANGLED",
                                   "../Grammar/Type.code");
  TypeString.setFunctionSource("SOURCE_GET_MANGLED_STRING_TYPE",
                               "../Grammar/Type.code");

  TypeDefault.excludeFunctionSource("SOURCE_GET_MANGLED",
                                    "../Grammar/Type.code");
  TypeDefault.setFunctionSource("SOURCE_TYPE_DEFAULT_TYPE",
                                "../Grammar/Type.code");

  TypeTargetBuiltin.excludeFunctionSource("SOURCE_GET_MANGLED",
                                          "../Grammar/Type.code");
  TypeTargetBuiltin.setFunctionSource("SOURCE_TYPE_TARGET_BUILTIN_TYPE",
                                      "../Grammar/Type.code");
  UnaryTransformType.excludeFunctionSource("SOURCE_GET_MANGLED",
                                           "../Grammar/Type.code");
  UnaryTransformType.setFunctionSource("SOURCE_UNARY_TRANSFORM_TYPE",
                                       "../Grammar/Type.code");

  TypeLabel.excludeFunctionSource("SOURCE_GET_MANGLED", "../Grammar/Type.code");
  TypeLabel.setFunctionSource("SOURCE_TYPE_LABEL_TYPE", "../Grammar/Type.code");
}
