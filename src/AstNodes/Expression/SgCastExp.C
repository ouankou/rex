#include "sage3basic.h"

#include "abiStuff.h"

void SgCastExp::post_construction_initialization() {}

namespace {

bool semanticConversionMayOwnBasePath(
    SgCastExp::semantic_conversion_kind_enum kind) {
  switch (kind) {
  case SgCastExp::e_semantic_conversion_BaseToDerived:
  case SgCastExp::e_semantic_conversion_DerivedToBase:
  case SgCastExp::e_semantic_conversion_UncheckedDerivedToBase:
  case SgCastExp::e_semantic_conversion_Dynamic:
  case SgCastExp::e_semantic_conversion_BaseToDerivedMemberPointer:
  case SgCastExp::e_semantic_conversion_DerivedToBaseMemberPointer:
    return true;
  default:
    return false;
  }
}

bool isExactCastType(SgType *type) {
  return type != nullptr && isSgTypeUnknown(type) == nullptr &&
         isSgTypeDefault(type) == nullptr &&
         !SageInterface::containsUnknownType(type);
}

} // namespace

void SgCastExp::validate_semantic_conversion() const {
  const bool valid_surface =
      get_cast_type() > e_unknown && get_cast_type() < e_last_cast;
  const bool valid_conversion =
      get_semantic_conversion_kind() > e_semantic_conversion_unclassified &&
      get_semantic_conversion_kind() < e_semantic_conversion_last;
  const bool valid_category =
      get_value_category() > e_value_category_unclassified &&
      get_value_category() < e_value_category_last;
  SgExpression *operand = get_operand();
  if (!valid_surface || !valid_conversion || !valid_category ||
      operand == nullptr || operand->get_parent() != this ||
      !isExactCastType(p_expression_type)) {
    fprintf(stderr, "REX_AST_INVARIANT[checked-cast]\n");
    fprintf(stderr,
            "REX_AST_DETAIL[checked-cast]: cast=%p surface=%d "
            "conversion=%d category=%d operand=%p operand-parent=%p "
            "result-type=%p does not describe one exact cast\n",
            static_cast<const void *>(this), static_cast<int>(get_cast_type()),
            static_cast<int>(get_semantic_conversion_kind()),
            static_cast<int>(get_value_category()),
            static_cast<void *>(operand),
            operand != nullptr ? static_cast<void *>(operand->get_parent())
                               : nullptr,
            static_cast<void *>(p_expression_type));
    ROSE_ABORT();
  }

  const SgTypePtrList &base_path = get_conversion_base_path();
  if (!base_path.empty() &&
      !semanticConversionMayOwnBasePath(get_semantic_conversion_kind())) {
    fprintf(stderr, "REX_AST_INVARIANT[cast-base-path]\n");
    fprintf(stderr,
            "REX_AST_DETAIL[cast-base-path]: cast=%p conversion=%d owns "
            "a base path for a conversion that cannot traverse bases\n",
            static_cast<const void *>(this),
            static_cast<int>(get_semantic_conversion_kind()));
    ROSE_ABORT();
  }
  for (SgType *base_type : base_path) {
    SgType *stripped =
        base_type != nullptr ? base_type->stripTypedefsAndModifiers() : nullptr;
    if (!isExactCastType(base_type) || isSgClassType(stripped) == nullptr) {
      fprintf(stderr, "REX_AST_INVARIANT[cast-base-path]\n");
      fprintf(stderr,
              "REX_AST_DETAIL[cast-base-path]: cast=%p conversion=%d "
              "contains non-class or inexact base type=%p/%s stripped=%p/%s\n",
              static_cast<const void *>(this),
              static_cast<int>(get_semantic_conversion_kind()),
              static_cast<void *>(base_type),
              base_type != nullptr ? base_type->class_name().c_str() : "<null>",
              static_cast<void *>(stripped),
              stripped != nullptr ? stripped->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
  }

  if (get_semantic_conversion_kind() ==
      e_semantic_conversion_BuiltinFnToFnPtr) {
    SgType *source = operand->get_type();
    SgType *stripped_source =
        source != nullptr ? source->stripTypedefsAndModifiers() : nullptr;
    SgType *stripped_result = p_expression_type->stripTypedefsAndModifiers();
    SgPointerType *pointer_result = isSgPointerType(stripped_result);
    const bool function_result = isSgFunctionType(stripped_result) != nullptr;
    const bool function_pointer_result =
        pointer_result != nullptr &&
        pointer_result->get_base_type() != nullptr &&
        isSgFunctionType(
            pointer_result->get_base_type()->stripTypedefsAndModifiers()) !=
            nullptr;
    if (isSgFunctionType(stripped_source) == nullptr ||
        function_result == function_pointer_result) {
      fprintf(stderr,
              "REX_AST_INVARIANT[builtin-function-decay-type]: cast=%p "
              "source=%p/%s result=%p/%s function-result=%d "
              "function-pointer-result=%d does not preserve one exact LLVM "
              "builtin callee conversion\n",
              static_cast<const void *>(this), static_cast<void *>(source),
              stripped_source != nullptr ? stripped_source->class_name().c_str()
                                         : "<null>",
              static_cast<void *>(p_expression_type),
              stripped_result != nullptr ? stripped_result->class_name().c_str()
                                         : "<null>",
              function_result ? 1 : 0, function_pointer_result ? 1 : 0);
      ROSE_ABORT();
    }
  }

  if (get_semantic_conversion_kind() ==
      e_semantic_conversion_FunctionToPointerDecay) {
    SgType *source = operand->get_type();
    SgType *decayed_source =
        source != nullptr ? source->stripTypedefsAndModifiers() : nullptr;
    if (SgReferenceType *reference = isSgReferenceType(decayed_source)) {
      decayed_source = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(decayed_source)) {
      decayed_source = reference->get_base_type();
    }
    SgPointerType *result =
        isSgPointerType(p_expression_type->stripTypedefsAndModifiers());
    if (source == nullptr || decayed_source == nullptr ||
        isSgFunctionType(decayed_source->stripTypedefsAndModifiers()) ==
            nullptr ||
        result == nullptr || result->get_base_type() != decayed_source) {
      SgFunctionRefExp *function_ref = isSgFunctionRefExp(operand);
      SgFunctionDeclaration *function_decl =
          function_ref != nullptr
              ? function_ref->getAssociatedFunctionDeclaration()
              : nullptr;
      Sg_File_Info *operand_start = operand->get_startOfConstruct();
      fprintf(
          stderr,
          "REX_AST_INVARIANT[function-to-pointer-decay-type]: cast=%p "
          "operand=%p/%s source=%p/%s result=%p/%s result-base=%p "
          "declaration=%p name=%s declaration-type=%p source-at=%s:%d:%d "
          "does not preserve the exact operand function type\n",
          static_cast<const void *>(this), static_cast<void *>(operand),
          operand->class_name().c_str(), static_cast<void *>(source),
          source != nullptr ? source->class_name().c_str() : "<null>",
          static_cast<void *>(p_expression_type),
          p_expression_type != nullptr ? p_expression_type->class_name().c_str()
                                       : "<null>",
          static_cast<void *>(result != nullptr ? result->get_base_type()
                                                : nullptr),
          static_cast<void *>(function_decl),
          function_decl != nullptr
              ? function_decl->get_qualified_name().getString().c_str()
              : "<not-a-function-reference>",
          static_cast<void *>(
              function_decl != nullptr ? function_decl->get_type() : nullptr),
          operand_start != nullptr ? operand_start->get_filenameString().c_str()
                                   : "<no-file>",
          operand_start != nullptr ? operand_start->get_line() : 0,
          operand_start != nullptr ? operand_start->get_col() : 0);
      ROSE_ABORT();
    }
  }

  if (get_cast_type() == e_dynamic_cast) {
    // CXXDynamicCastExpr records the semantic operation selected by Clang,
    // not merely the source keyword. Identity casts and statically known
    // upcasts are CK_NoOp and CK_DerivedToBase; only runtime-checked casts are
    // CK_Dynamic, and dependent casts retain CK_Dependent until instantiation.
    const semantic_conversion_kind_enum conversion =
        get_semantic_conversion_kind();
    if (conversion != e_semantic_conversion_Dependent &&
        conversion != e_semantic_conversion_NoOp &&
        conversion != e_semantic_conversion_DerivedToBase &&
        conversion != e_semantic_conversion_Dynamic) {
      fprintf(stderr,
              "REX_AST_INVARIANT[checked-cast]: dynamic_cast surface\n");
      fprintf(stderr,
              "REX_AST_DETAIL[checked-cast]: dynamic_cast surface has "
              "incompatible semantic conversion=%d\n",
              static_cast<int>(conversion));
      ROSE_ABORT();
    }
  }
  if (get_cast_type() == e_builtin_bit_cast &&
      get_semantic_conversion_kind() !=
          e_semantic_conversion_LValueToRValueBitCast) {
    fprintf(stderr,
            "REX_AST_INVARIANT[checked-cast]: __builtin_bit_cast surface\n");
    fprintf(stderr,
            "REX_AST_DETAIL[checked-cast]: __builtin_bit_cast surface "
            "has conversion=%d instead of LLVM 22 "
            "CK_LValueToRValueBitCast\n",
            static_cast<int>(get_semantic_conversion_kind()));
    ROSE_ABORT();
  }
  if (get_cast_type() == e_functional_cast ||
      get_cast_type() == e_functional_list_cast) {
    if (get_semantic_conversion_kind() == e_semantic_conversion_Dynamic) {
      fprintf(stderr,
              "REX_AST_INVARIANT[checked-cast]: functional cast surface\n");
      fprintf(stderr,
              "REX_AST_DETAIL[checked-cast]: functional cast surface cannot "
              "represent a runtime dynamic conversion\n");
      ROSE_ABORT();
    }
    SgExprListExp *arguments = nullptr;
    SgType *constructed_type = nullptr;
    if (SgConstructorInitializer *constructor =
            isSgConstructorInitializer(operand)) {
      arguments = constructor->get_args();
      constructed_type = constructor->get_type();
    } else if (SgAggregateInitializer *aggregate =
                   isSgAggregateInitializer(operand)) {
      arguments = aggregate->get_initializers();
      constructed_type = aggregate->get_type();
    } else if (isSgExprListExp(operand) != nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[functional-cast-operand]: functional cast "
              "owns an untyped raw expression list\n");
      ROSE_ABORT();
    }
    if (arguments != nullptr) {
      if (get_semantic_conversion_kind() != e_semantic_conversion_NoOp &&
          get_semantic_conversion_kind() != e_semantic_conversion_Dependent &&
          get_semantic_conversion_kind() !=
              e_semantic_conversion_ConstructorConversion) {
        fprintf(stderr,
                "REX_AST_INVARIANT[checked-cast]: functional cast surface\n");
        fprintf(stderr,
                "REX_AST_DETAIL[checked-cast]: functional construction "
                "surface has incompatible outer conversion=%d\n",
                static_cast<int>(get_semantic_conversion_kind()));
        ROSE_ABORT();
      }
      if (constructed_type == nullptr ||
          !SageInterface::isEquivalentType(constructed_type,
                                           p_expression_type) ||
          arguments->get_parent() != operand) {
        fprintf(stderr,
                "REX_AST_INVARIANT[functional-cast-operand]: functional "
                "cast construction has no exact compatible owned argument "
                "list (cast=%p operand=%p/%s arguments=%p parent=%p "
                "constructed-type=%p/%s result-type=%p/%s equivalent=%d)\n",
                static_cast<const void *>(this), static_cast<void *>(operand),
                operand->class_name().c_str(), static_cast<void *>(arguments),
                static_cast<void *>(arguments->get_parent()),
                static_cast<void *>(constructed_type),
                constructed_type != nullptr
                    ? constructed_type->class_name().c_str()
                    : "<null>",
                static_cast<void *>(p_expression_type),
                p_expression_type != nullptr
                    ? p_expression_type->class_name().c_str()
                    : "<null>",
                constructed_type != nullptr && p_expression_type != nullptr &&
                        SageInterface::isEquivalentType(constructed_type,
                                                        p_expression_type)
                    ? 1
                    : 0);
        ROSE_ABORT();
      }
      for (SgExpression *argument : arguments->get_expressions()) {
        if (argument == nullptr || argument->get_parent() != arguments) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[functional-cast-operand]: functional "
                  "cast construction has a non-owned argument\n");
          ROSE_ABORT();
        }
      }
    }
  }
}

const SgTypePtrList &SgCastExp::get_conversion_base_path() const {
  return p_conversion_base_path;
}

void SgCastExp::set_conversion_base_path(const SgTypePtrList &path) {
  p_conversion_base_path = path;
}

// DQ (6/14/2005): Modified to make enum name consistant with elsewhere in ROSE
// (Sage III) SgCastExp::Sg_e_cast_type
SgCastExp::cast_type_enum SgCastExp::cast_type() const { return p_cast_type; }

// DQ (1/17/2008): Added set_type function since this is one of a few IR nodes
// that require the type to be held explicitly, for all other IR nodes the type
// is computed dynamicly.
void SgCastExp::set_type(SgType *type) { p_expression_type = type; }

SgType *SgCastExp::get_type() const {
  // DQ (1/16/2006): In this function we want to return the stored
  // p_expression_type. This IR node has to store the type explicitly since
  // there is no other way to recover what the cast is TO (since the operand
  // stored what the cast in FROM).

  ROSE_ASSERT(p_expression_type != NULL);
  return p_expression_type;
}

bool SgCastExp::cast_loses_precision() const {
  validate_semantic_conversion();

  struct IntegralDomain {
    bool is_unsigned = false;
    unsigned width = 0;
  };
  auto integral_domain = [this](SgType *type, IntegralDomain &domain) -> bool {
    type = type != nullptr ? type->stripTypedefsAndModifiers() : nullptr;
    if (type == nullptr)
      return false;
    switch (type->variantT()) {
    case V_SgTypeSignedChar:
    case V_SgTypeShort:
    case V_SgTypeSignedShort:
    case V_SgTypeInt:
    case V_SgTypeSignedInt:
    case V_SgTypeLong:
    case V_SgTypeSignedLong:
    case V_SgTypeLongLong:
    case V_SgTypeSignedLongLong:
      domain.is_unsigned = false;
      break;
    case V_SgTypeUnsignedChar:
    case V_SgTypeUnsignedShort:
    case V_SgTypeUnsignedInt:
    case V_SgTypeUnsignedLong:
    case V_SgTypeUnsignedLongLong:
      domain.is_unsigned = true;
      break;
    default:
      return false;
    }
    SgProject *project = SageInterface::getProject(this);
    if (project == nullptr)
      return false;
    StructLayoutInfo layout;
    if (project->get_mode_32_bit()) {
      I386PrimitiveTypeLayoutGenerator primitive(nullptr);
      NonpackedTypeLayoutGenerator generator(&primitive);
      layout = generator.layoutType(type);
    } else {
      X86_64PrimitiveTypeLayoutGenerator primitive(nullptr);
      NonpackedTypeLayoutGenerator generator(&primitive);
      layout = generator.layoutType(type);
    }
    if (layout.size == 0 || layout.size > 8)
      return false;
    domain.width = static_cast<unsigned>(layout.size * 8);
    return true;
  };
  auto floating_digits = [](SgType *type) -> unsigned {
    type = type != nullptr ? type->stripTypedefsAndModifiers() : nullptr;
    if (isSgTypeFloat(type) != nullptr)
      return 24;
    if (isSgTypeDouble(type) != nullptr)
      return 53;
    return 0;
  };
  auto unsupported = [this]() -> bool {
    fprintf(stderr,
            "REX_AST_INVARIANT[cast-precision-policy]: cast=%p conversion=%d "
            "source-type=%p target-type=%p has no exact numeric precision "
            "policy\n",
            static_cast<const void *>(this),
            static_cast<int>(get_semantic_conversion_kind()),
            static_cast<void *>(get_operand()->get_type()),
            static_cast<void *>(get_type()));
    ROSE_ABORT();
  };

  switch (get_semantic_conversion_kind()) {
  case e_semantic_conversion_NoOp:
  case e_semantic_conversion_LValueToRValue:
  case e_semantic_conversion_BooleanToSignedIntegral:
    return false;

  case e_semantic_conversion_IntegralToBoolean:
  case e_semantic_conversion_FloatingToIntegral:
  case e_semantic_conversion_FloatingToBoolean:
    return true;

  case e_semantic_conversion_IntegralCast: {
    IntegralDomain source;
    IntegralDomain target;
    if (!integral_domain(get_operand()->get_type(), source) ||
        !integral_domain(get_type(), target))
      return unsupported();
    if (source.is_unsigned == target.is_unsigned)
      return target.width < source.width;
    if (!source.is_unsigned && target.is_unsigned)
      return true;
    return target.width <= source.width;
  }

  case e_semantic_conversion_IntegralToFloating: {
    IntegralDomain source;
    const unsigned target_digits = floating_digits(get_type());
    if (!integral_domain(get_operand()->get_type(), source) ||
        target_digits == 0)
      return unsupported();
    const unsigned source_digits =
        source.width - static_cast<unsigned>(!source.is_unsigned);
    return target_digits < source_digits;
  }

  case e_semantic_conversion_FloatingCast: {
    const unsigned source_digits = floating_digits(get_operand()->get_type());
    const unsigned target_digits = floating_digits(get_type());
    if (source_digits == 0 || target_digits == 0)
      return unsupported();
    return target_digits < source_digits;
  }

  default:
    return unsupported();
  }
}

// DQ (6/11/2015): Moved these six access functions, they should not be
// generated by ROSETTA so that we could avoid them setting the isModified flag
// which is a problem in the name qualification support for C++ (interfering
// with the token-based unparsing).
int SgCastExp::get_name_qualification_length() const {
  ROSE_ASSERT(this != NULL);
  return p_name_qualification_length;
}

void SgCastExp::set_name_qualification_length(int name_qualification_length) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_name_qualification_length = name_qualification_length;
}

bool SgCastExp::get_type_elaboration_required() const {
  ROSE_ASSERT(this != NULL);
  return p_type_elaboration_required;
}

void SgCastExp::set_type_elaboration_required(bool type_elaboration_required) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_type_elaboration_required = type_elaboration_required;
}

bool SgCastExp::get_global_qualification_required() const {
  ROSE_ASSERT(this != NULL);
  return p_global_qualification_required;
}

void SgCastExp::set_global_qualification_required(
    bool global_qualification_required) {
  ROSE_ASSERT(this != NULL);

  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_global_qualification_required = global_qualification_required;
}

std::string SgCastExp::cast_type_to_string(enum cast_type_enum cast_type) {
  std::string s;

  switch (cast_type) {
  case e_unknown:
    s = "e_unknown";
    break;
  case e_C_style_cast:
    s = "e_C_style_cast";
    break;
  case e_const_cast:
    s = "e_const_cast";
    break;
  case e_static_cast:
    s = "e_static_cast";
    break;
  case e_dynamic_cast:
    s = "e_dynamic_cast";
    break;
  case e_reinterpret_cast:
    s = "e_reinterpret_cast";
    break;
  case e_implicit_cast:
    s = "e_implicit_cast";
    break;
  case e_builtin_bit_cast:
    s = "e_builtin_bit_cast";
    break;
  case e_functional_cast:
    s = "e_functional_cast";
    break;
  case e_functional_list_cast:
    s = "e_functional_list_cast";
    break;
  case e_last_cast:
    s = "e_last_cast";
    break;

  default:
    printf("ERROR: default reached in switch: cast_type = %d \n", cast_type);
  }

  return s;
}

// DQ (4/15/2019): These six access functions should not be generated by ROSETTA
// so that we could avoid them setting the isModified flag which is a problem in
// the name qualification support for C++ (interfering with the token-based
// unparsing).
int SgCastExp::get_name_qualification_for_pointer_to_member_class_length()
    const {
  ROSE_ASSERT(this != NULL);
  return p_name_qualification_for_pointer_to_member_class_length;
}

void SgCastExp::set_name_qualification_for_pointer_to_member_class_length(
    int name_qualification_length) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_name_qualification_for_pointer_to_member_class_length =
      name_qualification_length;
}

bool SgCastExp::get_type_elaboration_for_pointer_to_member_class_required()
    const {
  ROSE_ASSERT(this != NULL);
  return p_type_elaboration_for_pointer_to_member_class_required;
}

void SgCastExp::set_type_elaboration_for_pointer_to_member_class_required(
    bool type_elaboration_required) {
  ROSE_ASSERT(this != NULL);
  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_type_elaboration_for_pointer_to_member_class_required =
      type_elaboration_required;
}

bool SgCastExp::get_global_qualification_for_pointer_to_member_class_required()
    const {
  ROSE_ASSERT(this != NULL);
  return p_global_qualification_for_pointer_to_member_class_required;
}

void SgCastExp::set_global_qualification_for_pointer_to_member_class_required(
    bool global_qualification_required) {
  ROSE_ASSERT(this != NULL);

  // This can't be called by the name qualification API (see test2015_26.C).
  // set_isModified(true);

  p_global_qualification_for_pointer_to_member_class_required =
      global_qualification_required;
}
