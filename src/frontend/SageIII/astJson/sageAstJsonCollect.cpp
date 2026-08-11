#include "AstNodes/Expression/OpenMPModifierValidation.h"
#include "ompAstConstruction.h"
#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

SgSourceFile *collectionBoundaryFile = nullptr;
SgNode *collectionBoundaryRoot = nullptr;

namespace {
thread_local bool pointerMemberTypeSerializationIdentityActive = false;
thread_local std::unordered_map<const SgPointerMemberType *, uint64_t>
    pointerMemberTypeSerializationIdentities;
thread_local std::unordered_map<uint64_t, const SgPointerMemberType *>
    pointerMemberTypeSerializationReverseIdentities;
thread_local bool arrayTypeSerializationIdentityActive = false;
thread_local std::unordered_map<const SgArrayType *, uint64_t>
    arrayTypeSerializationIdentities;
thread_local std::unordered_map<uint64_t, const SgArrayType *>
    arrayTypeSerializationReverseIdentities;

uint64_t
pointerMemberTypeSerializationIdentity(const SgPointerMemberType *type) {
  if (!pointerMemberTypeSerializationIdentityActive || type == nullptr) {
    throw std::runtime_error(
        "AST JSON pointer-member serialization identity has no active exact "
        "type graph");
  }
  auto found = pointerMemberTypeSerializationIdentities.find(type);
  if (found != pointerMemberTypeSerializationIdentities.end()) {
    return found->second;
  }
  const uint64_t identity =
      pointerMemberJsonIdentity(const_cast<SgPointerMemberType *>(type));
  if (!pointerMemberTypeSerializationIdentities.emplace(type, identity)
           .second ||
      !pointerMemberTypeSerializationReverseIdentities.emplace(identity, type)
           .second) {
    throw std::runtime_error(
        "AST JSON pointer-member serialization identity names distinct exact "
        "types");
  }
  return identity;
}

uint64_t arrayTypeSerializationIdentity(const SgArrayType *type) {
  if (!arrayTypeSerializationIdentityActive || type == nullptr ||
      type->get_fortran_source_syntax()) {
    throw std::runtime_error(
        "AST JSON semantic array serialization identity has no active exact "
        "array type graph");
  }
  auto found = arrayTypeSerializationIdentities.find(type);
  if (found != arrayTypeSerializationIdentities.end()) {
    return found->second;
  }
  const uint64_t identity =
      semanticArrayJsonIdentity(const_cast<SgArrayType *>(type));
  if (!arrayTypeSerializationIdentities.emplace(type, identity).second ||
      !arrayTypeSerializationReverseIdentities.emplace(identity, type).second) {
    throw std::runtime_error(
        "AST JSON semantic array serialization identity names distinct exact "
        "types");
  }
  return identity;
}

bool supportsFortranSourceSyntaxIdentity(const SgType *type) {
  return isSgTypeBool(type) != nullptr || isSgTypeChar(type) != nullptr ||
         isSgTypeInt(type) != nullptr || isSgTypeUnsignedInt(type) != nullptr ||
         isSgTypeFloat(type) != nullptr || isSgTypeDouble(type) != nullptr ||
         isSgTypeString(type) != nullptr || isSgTypeComplex(type) != nullptr ||
         isSgTypeFortranAssumed(type) != nullptr ||
         isSgTypeFortranUnlimitedPolymorphic(type) != nullptr ||
         isSgTypeCrayPointer(type) != nullptr ||
         isSgPointerType(type) != nullptr || isSgArrayType(type) != nullptr ||
         isSgModifierType(type) != nullptr || isSgFunctionType(type) != nullptr;
}

bool hasFortranFoldedSelectorValue(const SgExpression *expression) {
  return expression != nullptr &&
         expression->get_fortran_integer_constant_value_is_available();
}

void validateFortranSourceSyntaxStructure(const SgType *type) {
  if (type == nullptr || !type->get_fortran_source_syntax()) {
    return;
  }
  auto require_source_base = [](const SgType *base, const char *owner) {
    if (base == nullptr || (!base->get_fortran_source_syntax() &&
                            isSgNamedType(base) == nullptr)) {
      throw std::runtime_error(std::string("AST JSON source ") + owner +
                               " has a semantic base type");
    }
  };

  if (isSgTypeBool(type) != nullptr || isSgTypeInt(type) != nullptr ||
      isSgTypeUnsignedInt(type) != nullptr || isSgTypeFloat(type) != nullptr) {
    if (type->get_type_kind() != nullptr &&
        !hasFortranFoldedSelectorValue(type->get_type_kind())) {
      throw std::runtime_error(
          "AST JSON source intrinsic KIND selector has no folded value");
    }
  } else if (const SgTypeString *string_type = isSgTypeString(type)) {
    const SgExpression *length = string_type->get_lengthExpression();
    if (string_type->get_fortran_dynamic_length_pending()) {
      throw std::runtime_error(
          "AST JSON source CHARACTER type has an unresolved semantic length");
    }
    if (string_type->get_type_kind() != nullptr &&
        !hasFortranFoldedSelectorValue(string_type->get_type_kind())) {
      throw std::runtime_error(
          "AST JSON source CHARACTER KIND selector has no folded value");
    }
    if (length != nullptr && isSgAsteriskShapeExp(length) == nullptr &&
        isSgColonShapeExp(length) == nullptr) {
      const bool has_folded_value = hasFortranFoldedSelectorValue(length);
      if ((!has_folded_value &&
           length->get_fortran_integer_constant_value() != 0) ||
          (isSgValueExp(length) != nullptr && !has_folded_value)) {
        throw std::runtime_error(
            "AST JSON source CHARACTER LEN selector has invalid exact-value "
            "metadata");
      }
    }
  } else if (const SgTypeComplex *complex = isSgTypeComplex(type)) {
    const SgType *base = complex->get_base_type();
    require_source_base(base, "COMPLEX type");
    if (complex->get_type_kind() != nullptr &&
        !hasFortranFoldedSelectorValue(complex->get_type_kind())) {
      throw std::runtime_error(
          "AST JSON source COMPLEX KIND selector has no folded value");
    }
    if (complex->get_fortran_fixed_kind_value_is_available() &&
        (isSgTypeDouble(base) == nullptr ||
         !base->get_fortran_fixed_kind_value_is_available() ||
         base->get_fortran_fixed_kind_value() !=
             complex->get_fortran_fixed_kind_value())) {
      throw std::runtime_error(
          "AST JSON fixed source COMPLEX type has a contradictory component "
          "KIND");
    }
  } else if (isSgTypeFortranAssumed(type) != nullptr ||
             isSgTypeFortranUnlimitedPolymorphic(type) != nullptr) {
    if (type->get_type_kind() != nullptr || type->get_hasTypeKindStar() ||
        type->get_fortran_fixed_kind_value_is_available() ||
        type->get_fortran_fixed_kind_value() != 0) {
      throw std::runtime_error(
          "AST JSON Fortran assumed type owns an intrinsic selector");
    }
  } else if (isSgTypeCrayPointer(type) != nullptr) {
    if (type->get_type_kind() != nullptr || type->get_hasTypeKindStar() ||
        type->get_fortran_fixed_kind_value_is_available() ||
        type->get_fortran_fixed_kind_value() != 0) {
      throw std::runtime_error(
          "AST JSON source Cray pointer type owns an intrinsic selector");
    }
  } else if (const SgPointerType *pointer = isSgPointerType(type)) {
    require_source_base(pointer->get_base_type(), "pointer");
  } else if (const SgArrayType *array = isSgArrayType(type)) {
    require_source_base(array->get_base_type(), "array");
  } else if (const SgModifierType *modifier = isSgModifierType(type)) {
    require_source_base(modifier->get_base_type(), "modifier");
  } else if (const SgFunctionType *function = isSgFunctionType(type)) {
    require_source_base(function->get_return_type(), "function type");
  }
}
} // namespace

PointerMemberTypeSerializationIdentityGuard::
    PointerMemberTypeSerializationIdentityGuard() {
  if (pointerMemberTypeSerializationIdentityActive ||
      !pointerMemberTypeSerializationIdentities.empty() ||
      !pointerMemberTypeSerializationReverseIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON pointer-member serialization graph is already active");
  }
  pointerMemberTypeSerializationIdentityActive = true;
}

PointerMemberTypeSerializationIdentityGuard::
    ~PointerMemberTypeSerializationIdentityGuard() {
  pointerMemberTypeSerializationIdentities.clear();
  pointerMemberTypeSerializationReverseIdentities.clear();
  pointerMemberTypeSerializationIdentityActive = false;
}

ArrayTypeSerializationIdentityGuard::ArrayTypeSerializationIdentityGuard() {
  if (arrayTypeSerializationIdentityActive ||
      !arrayTypeSerializationIdentities.empty() ||
      !arrayTypeSerializationReverseIdentities.empty()) {
    throw std::runtime_error(
        "AST JSON semantic array serialization graph is already active");
  }
  arrayTypeSerializationIdentityActive = true;
}

ArrayTypeSerializationIdentityGuard::~ArrayTypeSerializationIdentityGuard() {
  arrayTypeSerializationIdentities.clear();
  arrayTypeSerializationReverseIdentities.clear();
  arrayTypeSerializationIdentityActive = false;
}

void validateTemplateParameterContract(const SgTemplateParameter *parameter,
                                       const std::string &context) {
  if (parameter == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " has a null SgTemplateParameter");
  }

  auto fail = [&](const std::string &detail) {
    throw std::runtime_error("AST JSON " + context + " " + detail);
  };
  auto require_no_non_type_fields = [&]() {
    if (parameter->get_initializedName() != nullptr ||
        parameter->get_expression() != nullptr ||
        parameter->get_defaultExpressionParameter() != nullptr) {
      fail("template parameter kind owns non-type parameter fields");
    }
  };
  auto require_no_source_template_declaration = [&]() {
    if (parameter->get_sourceSpelledTemplateDeclaration() != nullptr) {
      fail("non-template parameter owns a source-spelled template "
           "declaration");
    }
  };
  auto require_source_keyword = [&]() {
    if (parameter->get_templateParameterKeyword() !=
            SgTemplateParameter::keyword_class &&
        parameter->get_templateParameterKeyword() !=
            SgTemplateParameter::keyword_typename) {
      fail("template parameter has no exact source keyword");
    }
  };
  auto validate_coordinate_identity = [&](SgType *type) {
    SgTemplateType *template_type = isSgTemplateType(type);
    if (template_type == nullptr) {
      return;
    }
    const int depth = template_type->get_template_parameter_depth();
    const int position = template_type->get_template_parameter_position();
    if ((depth < 0) != (position < 0)) {
      fail("template type has a partial depth/position coordinate");
    }
    if (depth >= 0 &&
        !template_type->get_canonical_source_identity().has_value()) {
      fail("coordinate template type has no canonical source identity");
    }
  };

  switch (parameter->get_parameterType()) {
  case SgTemplateParameter::type_parameter:
    require_source_keyword();
    if (isSgTemplateType(parameter->get_type()) == nullptr &&
        isSgNonrealType(parameter->get_type()) == nullptr) {
      fail("type parameter has no exact template-parameter type");
    }
    if (parameter->get_templateDeclaration() != nullptr) {
      fail("type parameter owns a template declaration");
    }
    require_no_source_template_declaration();
    require_no_non_type_fields();
    if (parameter->get_defaultTemplateDeclarationParameter() != nullptr) {
      fail("type parameter owns a template-template default");
    }
    if ((parameter->get_typeConstraint() == nullptr) !=
        (parameter->get_sourceTypeConstraint() == nullptr)) {
      fail("type parameter has mismatched semantic/source constraints");
    }
    validate_coordinate_identity(parameter->get_type());
    break;

  case SgTemplateParameter::nontype_parameter: {
    if (parameter->get_templateParameterKeyword() !=
        SgTemplateParameter::keyword_unspecified) {
      fail("non-type parameter owns a type-parameter keyword");
    }
    if (parameter->get_type() == nullptr) {
      fail("non-type parameter has no semantic type");
    }
    if (parameter->get_templateDeclaration() != nullptr) {
      fail("non-type parameter owns a template declaration");
    }
    require_no_source_template_declaration();
    const bool has_initialized_name =
        parameter->get_initializedName() != nullptr;
    const bool has_expression = parameter->get_expression() != nullptr;
    if (has_initialized_name == has_expression) {
      fail("non-type parameter must own exactly one declaration or expression");
    }
    if (has_initialized_name &&
        parameter->get_initializedName()->get_type() != parameter->get_type()) {
      fail("non-type parameter declaration has a different semantic type");
    }
    if (parameter->get_defaultTypeParameter() != nullptr ||
        parameter->get_defaultTemplateDeclarationParameter() != nullptr) {
      fail("non-type parameter owns a default for another parameter kind");
    }
    if (parameter->get_sourceTypeConstraint() != nullptr) {
      fail("non-type parameter owns a separate source constraint instead of "
           "spelling it through its declared placeholder type");
    }
    break;
  }

  case SgTemplateParameter::template_parameter:
    require_source_keyword();
    if (isSgTemplateType(parameter->get_type()) == nullptr) {
      fail("template-template parameter has no exact SgTemplateType");
    }
    if (isSgTemplateDeclaration(parameter->get_templateDeclaration()) ==
        nullptr) {
      fail("template-template parameter has no exact SgTemplateDeclaration");
    }
    if (SgTemplateDeclaration *source_declaration =
            parameter->get_sourceSpelledTemplateDeclaration()) {
      if (source_declaration == parameter->get_templateDeclaration() ||
          source_declaration->get_parent() != parameter ||
          source_declaration->get_scope() == nullptr) {
        fail("template-template parameter has a malformed source-spelled "
             "template declaration");
      }
    }
    require_no_non_type_fields();
    if (parameter->get_defaultTypeParameter() != nullptr) {
      fail("template-template parameter owns a type default");
    }
    if (SgDeclarationStatement *default_declaration =
            parameter->get_defaultTemplateDeclarationParameter()) {
      if (isSgTemplateDeclaration(default_declaration) == nullptr) {
        fail("template-template default is not an exact SgTemplateDeclaration");
      }
    }
    validate_coordinate_identity(parameter->get_type());
    break;

  default:
    fail("has an unknown parameter kind");
  }
}

uint64_t varRefSymbolDeclarationId(
    SgVarRefExp *ref, const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (ref == nullptr) {
    return 0;
  }
  SgVariableSymbol *symbol = ref->get_symbol();
  if (symbol == nullptr) {
    throw std::runtime_error("AST JSON SgVarRefExp has no variable symbol");
  }
  const uint64_t id = idFor(ids, symbol->get_declaration());
  if (id == 0) {
    std::ostringstream message;
    message << "AST JSON SgVarRefExp symbol declaration was not collected: "
            << symbol->get_name().getString();
    auto append_source_file = [&](const char *label, SgNode *node) {
      SgSourceFile *source = SageInterface::getEnclosingSourceFile(node, true);
      message << ' ' << label << '=';
      if (source == nullptr) {
        message << "<null>";
      } else {
        message << source->getFileName() << '@' << source;
        if (SgNode *parent = source->get_parent()) {
          message << " parent=" << parent->sage_class_name() << '@' << parent;
        } else {
          message << " parent=<null>";
        }
      }
    };
    append_source_file("boundary_file", collectionBoundaryFile);
    if (SgInitializedName *declaration = symbol->get_declaration()) {
      message << " declaration=" << declaration->sage_class_name();
      append_source_file("declaration_file", declaration);
      if (SgNode *decl_parent = declaration->get_parent()) {
        message << " declaration_parent=" << decl_parent->sage_class_name();
      }
      if (SgScopeStatement *scope = declaration->get_scope()) {
        message << " declaration_scope=" << scope->sage_class_name();
      }
      message << " declaration_inside_boundary="
              << (insideCollectionBoundary(declaration) ? "true" : "false");
      message << " declaration_ancestors=";
      for (SgNode *ancestor = declaration; ancestor != nullptr;
           ancestor = ancestor->get_parent()) {
        if (ancestor != declaration) {
          message << "/";
        }
        message << ancestor->sage_class_name();
      }
    }
    if (SgNode *parent = ref->get_parent()) {
      message << " ref_parent=" << parent->sage_class_name();
    }
    append_source_file("ref_file", ref);
    message << " ref_inside_boundary="
            << (insideCollectionBoundary(ref) ? "true" : "false");
    throw std::runtime_error(message.str());
  }
  return id;
}

std::string
rawTypeJson(SgType *type,
            const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::vector<std::string> fields;
  if (type == nullptr) {
    fields.push_back(rawBoolField("present", false));
  } else {
    fields.push_back(rawBoolField("present", true));
    fields.push_back(rawStringField("kind", type->sage_class_name()));
    if (type->get_fortran_source_syntax() &&
        !supportsFortranSourceSyntaxIdentity(type)) {
      throw std::runtime_error(
          "AST JSON type has an invalid Fortran source-syntax identity");
    }
    if (const SgTypeString *string_type = isSgTypeString(type);
        string_type != nullptr &&
        string_type->get_fortran_dynamic_length_pending()) {
      throw std::runtime_error(
          "AST JSON CHARACTER type has an unresolved semantic length");
    }
    if (const SgTypeString *string_type = isSgTypeString(type);
        string_type != nullptr &&
        string_type->get_fortran_dynamic_result_length() &&
        (type->get_fortran_source_syntax() ||
         string_type->get_lengthExpression() != nullptr)) {
      throw std::runtime_error(
          "AST JSON dynamic CHARACTER result type has contradictory source "
          "or selector state");
    }
    if ((isSgTypeFortranAssumed(type) != nullptr ||
         isSgTypeFortranUnlimitedPolymorphic(type) != nullptr) &&
        (type->get_type_kind() != nullptr || type->get_hasTypeKindStar() ||
         type->get_fortran_fixed_kind_value_is_available() ||
         type->get_fortran_fixed_kind_value() != 0)) {
      throw std::runtime_error(
          "AST JSON Fortran assumed type owns an intrinsic selector");
    }
    const bool semanticSelectorMetadata =
        !type->get_fortran_source_syntax() &&
        ((type->get_type_kind() != nullptr &&
          type->get_type_kind()
              ->get_fortran_integer_constant_value_is_available()) ||
         (isSgTypeString(type) != nullptr &&
          isSgTypeString(type)->get_lengthExpression() != nullptr &&
          isSgTypeString(type)
              ->get_lengthExpression()
              ->get_fortran_integer_constant_value_is_available()));
    if (semanticSelectorMetadata) {
      throw std::runtime_error(
          "AST JSON semantic type owns source selector metadata");
    }
    const bool ownsDirectKind =
        isSgTypeBool(type) != nullptr || isSgTypeInt(type) != nullptr ||
        isSgTypeUnsignedInt(type) != nullptr ||
        isSgTypeFloat(type) != nullptr || isSgTypeString(type) != nullptr;
    if (type->get_type_kind() != nullptr && !ownsDirectKind &&
        isSgTypeComplex(type) == nullptr) {
      throw std::runtime_error(
          "AST JSON type owns a KIND selector outside an eligible type");
    }
    validateFortranSourceSyntaxStructure(type);
    fields.push_back(rawBoolField("fortran_source_syntax",
                                  type->get_fortran_source_syntax()));
    if (SgArrayType *semantic_array = isSgArrayType(type);
        semantic_array != nullptr &&
        !semantic_array->get_fortran_source_syntax()) {
      fields.push_back(
          rawIntegerField("semantic_array_identity",
                          static_cast<int64_t>(
                              arrayTypeSerializationIdentity(semantic_array))));
    }

    const bool fixedFortranKindEligible =
        isSgTypeDouble(type) != nullptr || isSgTypeComplex(type) != nullptr;
    const bool fixedFortranKindAvailable =
        type->get_fortran_fixed_kind_value_is_available();
    const std::int64_t fixedFortranKind = type->get_fortran_fixed_kind_value();
    if ((!fixedFortranKindAvailable && fixedFortranKind != 0) ||
        (fixedFortranKindAvailable && !type->get_fortran_source_syntax()) ||
        (!fixedFortranKindEligible &&
         (fixedFortranKindAvailable || fixedFortranKind != 0))) {
      throw std::runtime_error(
          "AST JSON type has invalid fixed Fortran KIND metadata");
    }
    if (fixedFortranKindEligible) {
      fields.push_back(rawBoolField("fortran_fixed_kind_value_is_available",
                                    fixedFortranKindAvailable));
      fields.push_back(
          rawIntegerField("fortran_fixed_kind_value", fixedFortranKind));
    }

    const bool hasPrimitiveFortranKind =
        isSgTypeBool(type) != nullptr || isSgTypeInt(type) != nullptr ||
        isSgTypeUnsignedInt(type) != nullptr || isSgTypeFloat(type) != nullptr;
    const bool starKindEligible = hasPrimitiveFortranKind ||
                                  isSgTypeString(type) != nullptr ||
                                  isSgTypeComplex(type) != nullptr;
    if (type->get_hasTypeKindStar() &&
        (!type->get_fortran_source_syntax() || !starKindEligible)) {
      throw std::runtime_error(
          "AST JSON type has source-only star KIND metadata without an "
          "eligible Fortran source type");
    }
    if (hasPrimitiveFortranKind) {
      fields.push_back(jsonString("type_kind") + ": " +
                       rawTypeOwnedExpressionRef(type->get_type_kind(), ids));
      fields.push_back(
          rawBoolField("has_type_kind_star", type->get_hasTypeKindStar()));
    }

    if (SgTypeTargetBuiltin *target_builtin = isSgTypeTargetBuiltin(type)) {
      if (target_builtin->get_spelling().is_null() ||
          target_builtin->get_target_family() <
              SgTypeTargetBuiltin::e_target_builtin_aarch64 ||
          target_builtin->get_target_family() >
              SgTypeTargetBuiltin::e_target_builtin_hlsl) {
        throw std::runtime_error(
            "AST JSON target builtin type has invalid exact identity");
      }
      fields.push_back(rawStringField(
          "spelling", target_builtin->get_spelling().getString()));
      fields.push_back(rawIntegerField(
          "target_family",
          static_cast<int64_t>(target_builtin->get_target_family())));
    } else if (SgAutoType *auto_type = isSgAutoType(type)) {
      const bool is_constrained = auto_type->get_is_constrained();
      const std::string &source_constraint_spelling =
          auto_type->get_source_constraint_spelling();
      if (is_constrained != !source_constraint_spelling.empty()) {
        throw std::runtime_error(
            "AST JSON SgAutoType constraint state has no exact source "
            "spelling");
      }
      fields.push_back(rawBoolField("is_constrained", is_constrained));
      fields.push_back(rawStringField("source_constraint_spelling",
                                      source_constraint_spelling));
    } else if (SgPointerMemberType *member_pointer =
                   isSgPointerMemberType(type)) {
      fields.push_back(rawIntegerField(
          "pointer_member_identity",
          static_cast<int64_t>(
              pointerMemberTypeSerializationIdentity(member_pointer))));
      fields.push_back(rawBoolField(
          "semantic_canonical",
          SgPointerMemberType::isCanonicalSemanticType(member_pointer)));
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(member_pointer->get_base_type(), ids));
      fields.push_back(jsonString("class_type") + ": " +
                       rawTypeJson(member_pointer->get_class_type(), ids));
      fields.push_back(rawBoolField(
          "source_class_type_is_unqualified_injected_name",
          member_pointer
              ->get_source_class_type_is_unqualified_injected_name()));
      fields.push_back(rawBoolField(
          "source_base_type_qualification_present",
          member_pointer->get_source_base_type_qualification_present()));
      fields.push_back(rawBoolField(
          "source_base_type_global_qualification",
          member_pointer->get_source_base_type_global_qualification()));
      fields.push_back(
          jsonString("source_base_type_qualification_tokens") + ": " +
          rawStringListJson(
              member_pointer->get_source_base_type_qualification_tokens()));
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(pointer->get_base_type(), ids));
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(reference->get_base_type(), ids));
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(reference->get_base_type(), ids));
    } else if (SgTypeString *string_type = isSgTypeString(type)) {
      fields.push_back(
          jsonString("length_expression") + ": " +
          rawTypeOwnedExpressionRef(string_type->get_lengthExpression(), ids));
      fields.push_back(
          jsonString("type_kind") + ": " +
          rawTypeOwnedExpressionRef(string_type->get_type_kind(), ids));
      fields.push_back(rawBoolField("has_type_kind_star",
                                    string_type->get_hasTypeKindStar()));
      fields.push_back(
          rawBoolField("fortran_dynamic_result_length",
                       string_type->get_fortran_dynamic_result_length()));
    } else if (SgTypeComplex *complex_type = isSgTypeComplex(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(complex_type->get_base_type(), ids));
      fields.push_back(rawBoolField("has_type_kind_star",
                                    complex_type->get_hasTypeKindStar()));
    } else if (SgArrayType *array = isSgArrayType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(array->get_base_type(), ids));
      fields.push_back(jsonString("index") + ": " +
                       rawTypeOwnedExpressionRef(array->get_index(), ids));
      fields.push_back(jsonString("dim_info") + ": " +
                       rawTypeOwnedExprListExpJson(array->get_dim_info(), ids));
      fields.push_back(rawIntegerField("rank", array->get_rank()));
      fields.push_back(rawIntegerField("number_of_elements",
                                       array->get_number_of_elements()));
      fields.push_back(rawBoolField("is_coarray", array->get_isCoArray()));
      fields.push_back(rawBoolField("is_variable_length_array",
                                    array->get_is_variable_length_array()));
    } else if (SgModifierType *modifier = isSgModifierType(type)) {
      fields.push_back(jsonString("base") + ": " +
                       rawTypeJson(modifier->get_base_type(), ids));
      SgTypeModifier &type_modifier = modifier->get_typeModifier();
      SgConstVolatileModifier &cv = type_modifier.get_constVolatileModifier();
      fields.push_back(rawBoolField("modifier_const", cv.isConst()));
      fields.push_back(rawBoolField("modifier_volatile", cv.isVolatile()));
      fields.push_back(
          rawBoolField("modifier_restrict", type_modifier.isRestrict()));
    } else if (SgTypeLabel *label_type = isSgTypeLabel(type)) {
      fields.push_back(
          rawStringField("name", label_type->get_name().getString()));
    } else if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      SgTypedefDeclaration *typedef_declaration =
          isSgTypedefDeclaration(typedef_type->get_declaration());
      const uint64_t declaration_id =
          canonicalTypedefDeclarationId(ids, typedef_declaration);
      if (declaration_id == 0) {
        std::ostringstream message;
        message << "AST JSON SgTypedefType declaration was not collected";
        if (typedef_declaration != nullptr) {
          message << " declaration=" << typedef_declaration->get_name();
          if (SgNode *parent = typedef_declaration->get_parent()) {
            message << " declaration_parent=" << parent->sage_class_name();
          }
        }
        if (currentTypeSerializationNode != nullptr) {
          message << " while serializing "
                  << currentTypeSerializationNode->sage_class_name();
          const auto current_id = ids.find(currentTypeSerializationNode);
          if (current_id != ids.end()) {
            message << " id=" << current_id->second;
          }
          if (const SgVarRefExp *ref =
                  isSgVarRefExp(currentTypeSerializationNode)) {
            const SgVariableSymbol *symbol = ref->get_symbol();
            const SgInitializedName *declaration =
                symbol != nullptr ? symbol->get_declaration() : nullptr;
            message << " var_symbol="
                    << (symbol != nullptr ? symbol->get_name().getString()
                                          : "<null>");
            if (declaration != nullptr) {
              const auto declaration_id = ids.find(declaration);
              if (declaration_id != ids.end()) {
                message << " var_declaration_id=" << declaration_id->second;
              } else {
                message << " var_declaration_uncollected";
              }
              message << " var_declaration=" << declaration->get_name();
              if (SgType *declaration_type = declaration->get_type()) {
                message << " var_declaration_type="
                        << declaration_type->sage_class_name();
              }
            }
          }
          const SgNode *parent = currentTypeSerializationNode->get_parent();
          int depth = 0;
          while (parent != nullptr && depth < 8) {
            message << " parent" << depth << "=" << parent->sage_class_name();
            const auto parent_id = ids.find(parent);
            if (parent_id != ids.end()) {
              message << "#" << parent_id->second;
            }
            parent = parent->get_parent();
            ++depth;
          }
        }
        throw std::runtime_error(message.str());
      }
      fields.push_back(rawIntegerField("declaration", declaration_id));
      fields.push_back(
          rawBoolField("autonomous_declaration",
                       typedef_type->get_autonomous_declaration()));
    } else if (SgClassType *class_type = isSgClassType(type)) {
      SgClassDeclaration *decl =
          isSgClassDeclaration(class_type->get_declaration());
      uint64_t declaration_id = idFor(ids, decl);
      if (declaration_id == 0 && decl != nullptr) {
        declaration_id =
            idFor(ids, isSgClassDeclaration(decl->get_definingDeclaration()));
      }
      if (declaration_id == 0 && decl != nullptr) {
        declaration_id = idFor(
            ids, isSgClassDeclaration(decl->get_firstNondefiningDeclaration()));
      }
      const bool external_declaration =
          declaration_id == 0 && decl != nullptr &&
          (isAstJsonExternalClassDeclaration(decl) ||
           !insideCollectionBoundary(decl));
      if (declaration_id == 0) {
        if (external_declaration) {
          fields.push_back(jsonString("external_declaration") + ": " +
                           rawExternalClassDeclarationJson(decl));
        } else {
          std::ostringstream message;
          message << "AST JSON SgClassType declaration was not collected";
          if (decl != nullptr) {
            message << " declaration=" << decl->get_name();
            if (SgNode *parent = decl->get_parent()) {
              message << " declaration_parent=" << parent->sage_class_name();
            } else {
              message << " declaration_parent=<null>";
            }
            if (SgScopeStatement *scope = decl->get_scope()) {
              message << " declaration_scope=" << scope->sage_class_name();
            } else {
              message << " declaration_scope=<null>";
            }
            const SgNode *decl_parent = decl->get_parent();
            int decl_depth = 0;
            while (decl_parent != nullptr && decl_depth < 8) {
              message << " declaration_parent" << decl_depth << "="
                      << decl_parent->sage_class_name();
              const auto parent_id = ids.find(decl_parent);
              if (parent_id != ids.end()) {
                message << "#" << parent_id->second;
              }
              decl_parent = decl_parent->get_parent();
              ++decl_depth;
            }
          }
          if (currentTypeSerializationNode != nullptr) {
            message << " while serializing "
                    << currentTypeSerializationNode->sage_class_name();
            const auto current_id = ids.find(currentTypeSerializationNode);
            if (current_id != ids.end()) {
              message << " id=" << current_id->second;
            }
            message << " text="
                    << safeNodeText(
                           const_cast<SgNode *>(currentTypeSerializationNode));
            const SgNode *parent = currentTypeSerializationNode->get_parent();
            int depth = 0;
            while (parent != nullptr && depth < 8) {
              message << " parent" << depth << "=" << parent->sage_class_name();
              const auto parent_id = ids.find(parent);
              if (parent_id != ids.end()) {
                message << "#" << parent_id->second;
              }
              parent = parent->get_parent();
              ++depth;
            }
          }
          throw std::runtime_error(message.str());
        }
      } else {
        fields.push_back(rawIntegerField("declaration", declaration_id));
      }
      fields.push_back(rawBoolField("autonomous_declaration",
                                    class_type->get_autonomous_declaration()));
    } else if (SgEnumType *enum_type = isSgEnumType(type)) {
      SgEnumDeclaration *decl =
          isSgEnumDeclaration(enum_type->get_declaration());
      uint64_t declaration_id = idFor(ids, decl);
      if (decl != nullptr) {
        if (SgEnumDeclaration *first_nondef =
                isSgEnumDeclaration(decl->get_firstNondefiningDeclaration())) {
          if (uint64_t first_nondef_id = idFor(ids, first_nondef)) {
            declaration_id = first_nondef_id;
          }
        }
      }
      if (declaration_id == 0) {
        throw std::runtime_error(
            "AST JSON SgEnumType declaration was not collected");
      }
      fields.push_back(rawIntegerField("declaration", declaration_id));
      fields.push_back(rawBoolField("autonomous_declaration",
                                    enum_type->get_autonomous_declaration()));
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
      const uint64_t declaration_id =
          idFor(ids, nonreal_type->get_declaration());
      if (declaration_id == 0) {
        throw std::runtime_error(
            "AST JSON SgNonrealType declaration was not collected");
      }
      fields.push_back(rawIntegerField("declaration", declaration_id));
      fields.push_back(
          rawBoolField("autonomous_declaration",
                       nonreal_type->get_autonomous_declaration()));
    } else if (SgDeclType *decl_type = isSgDeclType(type)) {
      fields.push_back(
          jsonString("base_expression") + ": " +
          rawTypeOwnedExpressionRef(decl_type->get_base_expression(), ids));
      fields.push_back(
          rawBoolField("is_gnu_decltype", decl_type->get_is_gnu_decltype()));
      fields.push_back(jsonString("base_type") + ": " +
                       rawTypeJson(decl_type->get_base_type(), ids));
    } else if (SgTypeOfType *typeof_type = isSgTypeOfType(type)) {
      SgExpression *base_expression = typeof_type->get_base_expression();
      SgType *base_type = typeof_type->get_base_type();
      if (base_type == nullptr ||
          (base_expression != nullptr &&
           base_expression->get_parent() != typeof_type)) {
        throw std::runtime_error(
            "AST JSON SgTypeOfType has no exact typed operand ownership");
      }
      fields.push_back(jsonString("base_expression") + ": " +
                       rawTypeOwnedExpressionRef(base_expression, ids));
      fields.push_back(jsonString("base_type") + ": " +
                       rawTypeJson(base_type, ids));
    } else if (SgTemplateType *template_type = isSgTemplateType(type)) {
      const auto &source_identity =
          template_type->get_canonical_source_identity();
      if (source_identity.has_value()) {
        filenameForFileId(source_identity->expansion_file_id,
                          "SgTemplateType expansion identity");
        filenameForFileId(source_identity->spelling_file_id,
                          "SgTemplateType spelling identity");
        fields.push_back(
            jsonString("canonical_source_identity") + ": {" +
            rawIntegerField("expansion_file_id",
                            source_identity->expansion_file_id) +
            ", " +
            rawIntegerField("expansion_file_offset",
                            source_identity->expansion_file_offset) +
            ", " +
            rawIntegerField("spelling_file_id",
                            source_identity->spelling_file_id) +
            ", " +
            rawIntegerField("spelling_file_offset",
                            source_identity->spelling_file_offset) +
            "}");
      } else {
        fields.push_back(jsonString("canonical_source_identity") + ": null");
      }
      fields.push_back(
          rawStringField("name", template_type->get_name().getString()));
      fields.push_back(
          rawIntegerField("template_parameter_position",
                          template_type->get_template_parameter_position()));
      fields.push_back(
          rawIntegerField("template_parameter_depth",
                          template_type->get_template_parameter_depth()));
      const uint64_t template_parameter_id =
          idFor(ids, template_type->get_template_parameter());
      if (template_type->get_template_parameter() != nullptr &&
          template_parameter_id == 0) {
        throw std::runtime_error(
            "AST JSON SgTemplateType parameter was not collected");
      }
      fields.push_back(
          rawIntegerField("template_parameter", template_parameter_id));
      fields.push_back(rawBoolField("packed", template_type->get_packed()));
      if (!template_type->get_tpl_args().empty() ||
          !template_type->get_part_spec_tpl_args().empty()) {
        throw std::runtime_error(
            "AST JSON SgTemplateType argument identity is not represented");
      }
      fields.push_back(jsonString("class_type") + ": " +
                       rawTypeJson(template_type->get_class_type(), ids));
      fields.push_back(
          jsonString("parent_class_type") + ": " +
          rawTypeJson(template_type->get_parent_class_type(), ids));
    } else if (SgMemberFunctionType *member_type =
                   isSgMemberFunctionType(type)) {
      fields.push_back(jsonString("return_type") + ": " +
                       rawTypeJson(member_type->get_return_type(), ids));
      fields.push_back(jsonString("class_type") + ": " +
                       rawTypeJson(member_type->get_class_type(), ids));
      fields.push_back(rawIntegerField("mfunc_specifier",
                                       member_type->get_mfunc_specifier()));
      fields.push_back(
          rawBoolField("has_ellipses", member_type->get_has_ellipses()));
      std::ostringstream args;
      args << jsonString("arguments") << ": [";
      const SgTypePtrList &argument_types =
          member_type->get_argument_list() != nullptr
              ? member_type->get_argument_list()->get_arguments()
              : member_type->get_arguments();
      if (!argument_types.empty()) {
        args << '\n';
        for (size_t i = 0; i < argument_types.size(); ++i) {
          indent(args, 8);
          args << rawTypeJson(argument_types[i], ids);
          if (i + 1 != argument_types.size()) {
            args << ',';
          }
          args << '\n';
        }
        indent(args, 6);
      }
      args << "]";
      fields.push_back(args.str());
    } else if (SgFunctionType *function_type = isSgFunctionType(type)) {
      fields.push_back(jsonString("return_type") + ": " +
                       rawTypeJson(function_type->get_return_type(), ids));
      fields.push_back(
          rawBoolField("has_ellipses", function_type->get_has_ellipses()));
      std::ostringstream args;
      args << jsonString("arguments") << ": [";
      const SgTypePtrList &argument_types =
          function_type->get_argument_list() != nullptr
              ? function_type->get_argument_list()->get_arguments()
              : function_type->get_arguments();
      if (!argument_types.empty()) {
        args << '\n';
        for (size_t i = 0; i < argument_types.size(); ++i) {
          indent(args, 8);
          args << rawTypeJson(argument_types[i], ids);
          if (i + 1 != argument_types.size()) {
            args << ',';
          }
          args << '\n';
        }
        indent(args, 6);
      }
      args << "]";
      fields.push_back(args.str());
    }
  }

  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  std::string result = out.str();
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

std::string rawTemplateArgumentListJson(
    const SgTemplateArgumentPtrList &arguments,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!arguments.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgTemplateArgument *argument : arguments) {
      const uint64_t id = idFor(ids, argument);
      if (argument == nullptr || id == 0) {
        throw std::runtime_error(
            "AST JSON template argument list has no exact collected typed "
            "argument");
      }
      indent(out, 6);
      out << id;
      if (++index != arguments.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawOmpUsesAllocatorsDefinitionsJson(
    const SgOmpUsesAllocatorsDefinationPtrList &definitions,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  std::ostringstream out;
  out << "[";
  if (!definitions.empty()) {
    out << '\n';
    size_t index = 0;
    for (SgOmpUsesAllocatorsDefination *definition : definitions) {
      const uint64_t id = idFor(ids, definition);
      if (definition != nullptr && id == 0) {
        throw std::runtime_error(
            "AST JSON OMP uses_allocators definition target was not "
            "collected");
      }
      indent(out, 6);
      out << id;
      if (++index != definitions.size()) {
        out << ',';
      }
      out << '\n';
    }
    indent(out, 4);
  }
  out << "]";
  return out.str();
}

std::string rawAstAttributesJson(SgNode *node) {
  std::ostringstream out;
  out << "[";
  AstAttributeMechanism *attributes =
      node != nullptr ? node->get_attributeMechanism() : nullptr;
  if (attributes != nullptr && attributes->size() != 0) {
    std::vector<std::string> entries;
    for (const std::string &name : attributes->getAttributeIdentifiers()) {
      if (name == kAstJsonExternalFunctionAttribute ||
          name == kAstJsonExternalModuleAttribute ||
          name == kAstJsonExternalClassDeclarationAttribute ||
          name == kAstJsonExternalSourceFileAttribute) {
        continue;
      }
      AstAttribute *attribute = node->getAttribute(name);
      if (AstIntAttribute *int_attribute =
              dynamic_cast<AstIntAttribute *>(attribute)) {
        std::vector<std::string> fields;
        fields.push_back(rawStringField("name", name));
        fields.push_back(rawStringField("type", "AstIntAttribute"));
        fields.push_back(rawIntegerField("value", int_attribute->getValue()));
        std::ostringstream item;
        writeRawObject(item, 0, fields, false);
        entries.push_back(item.str());
      } else if (AstValueAttribute<std::string> *string_attribute =
                     dynamic_cast<AstValueAttribute<std::string> *>(
                         attribute)) {
        std::vector<std::string> fields;
        fields.push_back(rawStringField("name", name));
        fields.push_back(rawStringField("type", "AstStringAttribute"));
        fields.push_back(rawStringField("value", string_attribute->get()));
        std::ostringstream item;
        writeRawObject(item, 0, fields, false);
        entries.push_back(item.str());
      } else {
        throw std::runtime_error("AST JSON cannot preserve attribute '" + name +
                                 "' on " + node->class_name());
      }
    }
    if (!entries.empty()) {
      std::sort(entries.begin(), entries.end());
      out << '\n';
      for (size_t i = 0; i < entries.size(); ++i) {
        indent(out, 6);
        out << entries[i];
        if (i + 1 != entries.size()) {
          out << ',';
        }
        out << '\n';
      }
      indent(out, 4);
    }
  }
  out << "]";
  return out.str();
}

void addExpressionType(
    std::vector<std::string> &fields, SgExpression *expr,
    const std::unordered_map<const SgNode *, uint64_t> &ids) {
  if (expressionCarriesSemanticType(expr)) {
    SgType *type = expr->get_type();
    validateExactSemanticExpressionType(expr, type, "serialization");
    fields.push_back(jsonString("type") + ": " + rawTypeJson(type, ids));
  }
}

template <typename T>
void addReferenceQualificationFields(std::vector<std::string> &fields, T *ref) {
  fields.push_back(rawIntegerField("name_qualification_length",
                                   ref->get_name_qualification_length()));
  fields.push_back(rawBoolField("type_elaboration_required",
                                ref->get_type_elaboration_required()));
  fields.push_back(rawBoolField("global_qualification_required",
                                ref->get_global_qualification_required()));
  fields.push_back(
      rawIntegerField("explicit_name_qualification_length",
                      ref->get_explicit_name_qualification_length()));
  fields.push_back(rawBoolField("explicit_global_qualification",
                                ref->get_explicit_global_qualification()));
  fields.push_back(
      jsonString("explicit_name_qualification_tokens") + ": " +
      rawStringListJson(ref->get_explicit_name_qualification_tokens()));
}

void addExpressionQualificationFields(std::vector<std::string> &fields,
                                      SgExpression *expr) {
  if (SgPseudoDestructorRefExp *pseudo = isSgPseudoDestructorRefExp(expr)) {
    fields.push_back(rawIntegerField("name_qualification_length",
                                     pseudo->get_name_qualification_length()));
    fields.push_back(rawBoolField("type_elaboration_required",
                                  pseudo->get_type_elaboration_required()));
    fields.push_back(rawBoolField("global_qualification_required",
                                  pseudo->get_global_qualification_required()));
  } else if (SgTypeExpression *typeExpression = isSgTypeExpression(expr)) {
    fields.push_back(
        rawIntegerField("name_qualification_length",
                        typeExpression->get_name_qualification_length()));
    fields.push_back(
        rawBoolField("type_elaboration_required",
                     typeExpression->get_type_elaboration_required()));
    fields.push_back(
        rawBoolField("global_qualification_required",
                     typeExpression->get_global_qualification_required()));
  } else if (SgNewExp *newExpression = isSgNewExp(expr)) {
    fields.push_back(
        rawIntegerField("name_qualification_length",
                        newExpression->get_name_qualification_length()));
    fields.push_back(
        rawBoolField("type_elaboration_required",
                     newExpression->get_type_elaboration_required()));
    fields.push_back(
        rawBoolField("global_qualification_required",
                     newExpression->get_global_qualification_required()));
    fields.push_back(
        rawBoolField("explicit_name_qualification_present",
                     newExpression->get_explicit_name_qualification_present()));
    fields.push_back(
        rawBoolField("explicit_global_qualification",
                     newExpression->get_explicit_global_qualification()));
    fields.push_back(
        jsonString("explicit_name_qualification_tokens") + ": " +
        rawStringListJson(
            newExpression->get_explicit_name_qualification_tokens()));
  } else if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    validateAnonymousDataMemberReferenceQualification(ref);
    addReferenceQualificationFields(fields, ref);
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    addReferenceQualificationFields(fields, ref);
  } else if (SgEnumVal *ref = isSgEnumVal(expr)) {
    addReferenceQualificationFields(fields, ref);
  }
}

template <typename T>
void restoreReferenceQualificationFields(T *ref, const JsonValue &properties) {
  ref->set_name_qualification_length(
      static_cast<int>(properties.at("name_qualification_length").asInt()));
  ref->set_type_elaboration_required(
      properties.at("type_elaboration_required").asBool());
  ref->set_global_qualification_required(
      properties.at("global_qualification_required").asBool());
  ref->set_explicit_name_qualification_length(static_cast<int>(
      properties.at("explicit_name_qualification_length").asInt()));
  ref->set_explicit_global_qualification(
      properties.at("explicit_global_qualification").asBool());
  ref->set_explicit_name_qualification_tokens(
      stringListFromJson(properties.at("explicit_name_qualification_tokens"),
                         "explicit_name_qualification_tokens"));
}

void restoreExpressionQualificationFields(SgExpression *expr,
                                          const JsonValue &properties) {
  if (SgPseudoDestructorRefExp *pseudo = isSgPseudoDestructorRefExp(expr)) {
    pseudo->set_name_qualification_length(
        static_cast<int>(properties.requiredInt("name_qualification_length")));
    pseudo->set_type_elaboration_required(
        properties.requiredBool("type_elaboration_required"));
    pseudo->set_global_qualification_required(
        properties.requiredBool("global_qualification_required"));
  } else if (SgTypeExpression *typeExpression = isSgTypeExpression(expr)) {
    typeExpression->set_name_qualification_length(
        static_cast<int>(properties.requiredInt("name_qualification_length")));
    typeExpression->set_type_elaboration_required(
        properties.requiredBool("type_elaboration_required"));
    typeExpression->set_global_qualification_required(
        properties.requiredBool("global_qualification_required"));
  } else if (SgNewExp *newExpression = isSgNewExp(expr)) {
    newExpression->set_name_qualification_length(
        static_cast<int>(properties.requiredInt("name_qualification_length")));
    newExpression->set_type_elaboration_required(
        properties.requiredBool("type_elaboration_required"));
    newExpression->set_global_qualification_required(
        properties.requiredBool("global_qualification_required"));
    newExpression->set_explicit_name_qualification_present(
        properties.requiredBool("explicit_name_qualification_present"));
    newExpression->set_explicit_global_qualification(
        properties.requiredBool("explicit_global_qualification"));
    newExpression->set_explicit_name_qualification_tokens(
        stringListFromJson(properties.at("explicit_name_qualification_tokens"),
                           "explicit_name_qualification_tokens"));
  } else if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  } else if (SgEnumVal *ref = isSgEnumVal(expr)) {
    restoreReferenceQualificationFields(ref, properties);
  }
}

void addLocatedPreprocessing(std::vector<std::string> &fields,
                             const SgLocatedNode *node) {
  const AttachedPreprocessingInfoType *infos =
      node != nullptr
          ? const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo()
          : nullptr;
  std::ostringstream out;
  out << jsonString("preprocessing") << ": [";
  if (infos != nullptr && !infos->empty()) {
    std::vector<std::vector<std::string>> preprocessing_entries;
    for (size_t i = 0; i < infos->size(); ++i) {
      const PreprocessingInfo *info = (*infos)[i];
      if (info == nullptr) {
        std::ostringstream message;
        message << "AST JSON cannot serialize null PreprocessingInfo attached "
                << "to " << node->class_name();
        throw std::runtime_error(message.str());
      }
      if (isSgInitializedName(const_cast<SgLocatedNode *>(node)) != nullptr &&
          (info->getTypeOfDirective() ==
               PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           info->getTypeOfDirective() ==
               PreprocessingInfo::CpreprocessorIncludeNextDeclaration)) {
        SgNode *parent = const_cast<SgLocatedNode *>(node)->get_parent();
        SgLocatedNode *parent_located = isSgLocatedNode(parent);
        AttachedPreprocessingInfoType *parent_infos =
            parent_located != nullptr
                ? parent_located->getAttachedPreprocessingInfo()
                : nullptr;
        bool parent_has_same_include = false;
        if (parent_infos != nullptr) {
          for (const PreprocessingInfo *parent_info : *parent_infos) {
            if (parent_info != nullptr &&
                parent_info->getTypeOfDirective() ==
                    info->getTypeOfDirective() &&
                parent_info->getString() == info->getString()) {
              parent_has_same_include = true;
              break;
            }
          }
        }
        if (parent_has_same_include) {
          continue;
        }
      }
      std::vector<std::string> entry;
      entry.push_back(rawIntegerField(
          "directive", static_cast<int>(info->getTypeOfDirective())));
      entry.push_back(rawIntegerField(
          "relative", static_cast<int>(info->getRelativePosition())));
      entry.push_back(rawStringField("text", info->getString()));
      entry.push_back(jsonString("file_info") + ": " +
                      rawFileInfoJson(info->get_file_info()));
      entry.push_back(rawIntegerField("lines", info->getNumberOfLines()));
      entry.push_back(rawBoolField("transformation", info->isTransformation()));
      entry.push_back(rawIntegerField(
          "output_placement", static_cast<int>(info->getOutputPlacement())));
      preprocessing_entries.push_back(std::move(entry));
    }
    if (!preprocessing_entries.empty()) {
      out << '\n';
      for (size_t i = 0; i < preprocessing_entries.size(); ++i) {
        writeRawObject(out, 8, preprocessing_entries[i],
                       i + 1 != preprocessing_entries.size());
      }
      indent(out, 6);
    }
  }
  out << "]";
  fields.push_back(out.str());
}

void writeFileInfoJson(std::ostream &out, int level, const Sg_File_Info *info,
                       bool comma) {
  indent(out, level);
  out << "{\n";
  if (info == nullptr) {
    writeBoolField(out, level + 2, "present", false, false);
  } else {
    writeBoolField(out, level + 2, "present", true);
    writeStringField(out, level + 2, "filename", info->get_filenameString());
    writeStringField(out, level + 2, "raw_filename", info->get_raw_filename());
    writeStringField(out, level + 2, "physical_filename",
                     info->get_physical_filename());
    writeIntegerField(out, level + 2, "file_id", info->get_file_id());
    const int physical_file_id = info->get_physical_file_id();
    std::string physical_raw_filename = info->get_physical_filename();
    if (physical_file_id >= 0) {
      physical_raw_filename = Sg_File_Info::getFilenameFromID(physical_file_id);
    }
    writeStringField(out, level + 2, "physical_raw_filename",
                     physical_raw_filename);
    writeIntegerField(out, level + 2, "physical_file_id", physical_file_id);
    writeIntegerField(out, level + 2, "physical_internal_file_id",
                      info->get_raw_physical_file_id());
    writeIntegerField(out, level + 2, "line", info->get_line());
    writeIntegerField(out, level + 2, "column", info->get_col());
    writeIntegerField(out, level + 2, "raw_line", info->get_raw_line());
    writeIntegerField(out, level + 2, "raw_column", info->get_raw_col());
    writeIntegerField(out, level + 2, "physical_line",
                      info->get_physical_line());
    writeIntegerField(out, level + 2, "source_sequence",
                      info->get_source_sequence_number());
    writeBoolField(out, level + 2, "compiler_generated",
                   info->isCompilerGenerated());
    writeBoolField(out, level + 2, "transformation", info->isTransformation());
    writeBoolField(out, level + 2, "frontend_specific",
                   info->isFrontendSpecific());
    writeBoolField(out, level + 2, "output_in_code_generation",
                   info->isOutputInCodeGeneration());
    writeBoolField(out, level + 2, "shared", info->isShared());
    writeBoolField(out, level + 2, "source_position_unavailable_in_frontend",
                   info->isSourcePositionUnavailableInFrontend());
    writeBoolField(out, level + 2, "comment_or_directive",
                   info->isCommentOrDirective());
    writeBoolField(out, level + 2, "token", info->isToken());
    writeBoolField(out, level + 2, "default_argument",
                   info->isDefaultArgument());
    writeBoolField(out, level + 2, "implicit_cast", info->isImplicitCast(),
                   false);
  }
  indent(out, level);
  out << '}';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

SgProject *currentDeserializationProject = nullptr;
SgNode *currentAuxiliaryOwner = nullptr;
std::vector<std::string> currentAuxiliaryTypeStack;
thread_local std::unordered_set<SgNode *> *currentExpandedSubtrees = nullptr;
std::unordered_map<SgSourceFile *, std::vector<SgClassDeclaration *>>
    currentDeserializationClassDeclarationCache;

std::string safeNodeText(SgNode *node);

bool insideCollectionBoundary(SgNode *node) {
  if (node == nullptr || collectionBoundaryFile == nullptr) {
    return true;
  }
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (current == collectionBoundaryFile) {
      return true;
    }
    if (isSgType(current) != nullptr) {
      return true;
    }
    if (isSgSourceFile(current) != nullptr ||
        isSgFileList(current) != nullptr || isSgProject(current) != nullptr) {
      return false;
    }
  }
  return true;
}

CollectionBoundaryGuard::CollectionBoundaryGuard(SgSourceFile *file)
    : previous_(collectionBoundaryFile) {
  collectionBoundaryFile = file;
}

CollectionBoundaryGuard::~CollectionBoundaryGuard() {
  collectionBoundaryFile = previous_;
}

SubtreeBoundaryGuard::SubtreeBoundaryGuard(SgNode *root)
    : previous_(collectionBoundaryRoot) {
  collectionBoundaryRoot = root;
}

SubtreeBoundaryGuard::~SubtreeBoundaryGuard() {
  collectionBoundaryRoot = previous_;
}

DeserializationProjectGuard::DeserializationProjectGuard(SgProject *project)
    : previous_(currentDeserializationProject) {
  currentDeserializationProject = project;
  currentDeserializationClassDeclarationCache.clear();
}

DeserializationProjectGuard::~DeserializationProjectGuard() {
  currentDeserializationClassDeclarationCache.clear();
  currentDeserializationProject = previous_;
}

class AuxiliaryOwnerGuard {
public:
  explicit AuxiliaryOwnerGuard(SgNode *node)
      : previous_(currentAuxiliaryOwner) {
    currentAuxiliaryOwner = node;
  }
  ~AuxiliaryOwnerGuard() { currentAuxiliaryOwner = previous_; }

private:
  SgNode *previous_;
};

class SubtreeExpansionGuard {
public:
  explicit SubtreeExpansionGuard(std::unordered_set<SgNode *> &expanded) {
    if (currentExpandedSubtrees != nullptr || !expanded.empty()) {
      throw std::runtime_error("AST JSON subtree collection is already active");
    }
    currentExpandedSubtrees = &expanded;
  }

  ~SubtreeExpansionGuard() { currentExpandedSubtrees = nullptr; }

  SubtreeExpansionGuard(const SubtreeExpansionGuard &) = delete;
  SubtreeExpansionGuard &operator=(const SubtreeExpansionGuard &) = delete;
};

class AuxiliaryTypeGuard {
public:
  explicit AuxiliaryTypeGuard(SgType *type) {
    currentAuxiliaryTypeStack.push_back(
        type != nullptr ? type->sage_class_name() : "<null>");
  }
  ~AuxiliaryTypeGuard() { currentAuxiliaryTypeStack.pop_back(); }
};

bool isAstJsonExternalMarker(SgNode *node) {
  return isAstJsonExternalFunction(isSgFunctionDeclaration(node)) ||
         isAstJsonExternalModule(isSgModuleStatement(node)) ||
         isAstJsonExternalClassDeclaration(isSgClassDeclaration(node));
}

bool isStructuralAstChildOfParent(SgNode *node) {
  SgNode *parent = node != nullptr ? node->get_parent() : nullptr;
  if (parent == nullptr || isAstJsonExternalMarker(parent)) {
    return false;
  }
  for (const std::pair<SgNode *, std::string> &entry :
       parent->returnDataMemberPointers()) {
    if (entry.first != node) {
      continue;
    }
    if (entry.second == "parent" || entry.second == "scope" ||
        entry.second == "symbol_table" || entry.second == "type_table" ||
        entry.second == "firstNondefiningDeclaration" ||
        entry.second == "definingDeclaration") {
      continue;
    }
    return true;
  }
  return false;
}

bool hasExternalMarkerAncestor(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (isAstJsonExternalMarker(current)) {
      return true;
    }
  }
  return false;
}

void addSingleNode(SgNode *node, std::vector<SgNode *> &nodes,
                   std::unordered_set<SgNode *> &seen) {
  if (hasExternalMarkerAncestor(node)) {
    return;
  }
  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    if (collectionBoundaryFile != nullptr &&
        isSgType(ref->get_parent()) != nullptr) {
      const SgNode *basis = symbolBasis(ref->get_symbol());
      if (basis != nullptr &&
          !insideCollectionBoundary(const_cast<SgNode *>(basis))) {
        std::ostringstream message;
        message << "AST JSON type-owned SgVarRefExp references a symbol "
                   "outside the collection boundary: "
                << symbolName(ref->get_symbol());
        if (currentAuxiliaryOwner != nullptr) {
          message << " owner=" << currentAuxiliaryOwner->sage_class_name()
                  << " owner_text=" << safeNodeText(currentAuxiliaryOwner);
        }
        if (!currentAuxiliaryTypeStack.empty()) {
          message << " type_stack=";
          for (size_t i = 0; i < currentAuxiliaryTypeStack.size(); ++i) {
            if (i != 0) {
              message << "/";
            }
            message << currentAuxiliaryTypeStack[i];
          }
        }
        if (const SgNode *parent = ref->get_parent()) {
          message << " parent=" << parent->sage_class_name();
        }
        throw std::runtime_error(message.str());
      }
    }
  }
  if (node != nullptr && insideCollectionBoundary(node) &&
      seen.insert(node).second) {
    nodes.push_back(node);
  }
}

void addSubtreeNodes(SgNode *root, std::vector<SgNode *> &nodes,
                     std::unordered_set<SgNode *> &seen) {
  if (root == nullptr || !insideCollectionBoundary(root)) {
    return;
  }
  if (currentExpandedSubtrees == nullptr) {
    throw std::runtime_error(
        "AST JSON subtree collection has no active expansion graph");
  }
  // RoseAst requests each successor by index. Generated nodes with large
  // child vectors rebuild their complete traversal-successor container for
  // every index, making auxiliary declaration lists quadratic. Snapshot each
  // node's structural successors exactly once across the complete collection
  // and walk that explicit graph.
  std::vector<SgNode *> pending{root};
  while (!pending.empty()) {
    SgNode *current = pending.back();
    pending.pop_back();
    if (current == nullptr || !insideCollectionBoundary(current) ||
        !currentExpandedSubtrees->insert(current).second) {
      continue;
    }
    addSingleNode(current, nodes, seen);
    if (isSgBaseClass(current) != nullptr) {
      continue;
    }
    const std::vector<SgNode *> successors =
        current->get_traversalSuccessorContainer();
    for (auto successor = successors.rbegin(); successor != successors.rend();
         ++successor) {
      if (*successor != nullptr) {
        pending.push_back(*successor);
      }
    }
  }
}

void addNodeAncestors(SgNode *node, std::vector<SgNode *> &nodes,
                      std::unordered_set<SgNode *> &seen) {
  for (SgNode *current = node != nullptr ? node->get_parent() : nullptr;
       current != nullptr; current = current->get_parent()) {
    if (!insideCollectionBoundary(current) ||
        isSgSourceFile(current) != nullptr ||
        isSgFileList(current) != nullptr || isSgProject(current) != nullptr) {
      return;
    }
    addSingleNode(current, nodes, seen);
  }
}

void addReferencedSymbolBasis(const SgSymbol *symbol,
                              std::vector<SgNode *> &nodes,
                              std::unordered_set<SgNode *> &seen) {
  SgNode *basis = const_cast<SgNode *>(symbolBasis(symbol));
  if (hasExternalMarkerAncestor(basis)) {
    return;
  }
  addSubtreeNodes(basis, nodes, seen);
  addNodeAncestors(basis, nodes, seen);
}

void addExactBoundSymbolDependency(const SgSymbol *symbol,
                                   std::vector<SgNode *> &nodes,
                                   std::unordered_set<SgNode *> &seen) {
  if (symbol == nullptr) {
    return;
  }
  SgSymbolTable *table = isSgSymbolTable(symbol->get_parent());
  SgScopeStatement *scope =
      table != nullptr ? isSgScopeStatement(table->get_parent()) : nullptr;
  size_t occurrences = 0;
  if (table != nullptr && table->get_table() != nullptr) {
    for (const auto &entry : *table->get_table()) {
      occurrences += entry.second == symbol ? 1 : 0;
    }
  }
  if (scope == nullptr || occurrences != 1 ||
      !insideCollectionBoundary(scope)) {
    std::ostringstream message;
    message << "AST JSON exact symbol dependency has no unique in-file "
               "visibility owner";
    message << " symbol=" << symbol->class_name();
    message << " name=" << symbol->get_name().getString();
    message << " table=" << table;
    message << " scope=" << scope;
    message << " occurrences=" << occurrences;
    throw std::runtime_error(message.str());
  }
  addReferencedSymbolBasis(symbol, nodes, seen);
  addSingleNode(scope, nodes, seen);
  addNodeAncestors(scope, nodes, seen);
}

void addExpressionSymbolDependencies(SgNode *node, std::vector<SgNode *> &nodes,
                                     std::unordered_set<SgNode *> &seen);

void addExpressionSubtreeSymbolDependencies(
    SgExpression *root, std::vector<SgNode *> &nodes,
    std::unordered_set<SgNode *> &seen) {
  if (root == nullptr || !insideCollectionBoundary(root)) {
    return;
  }
  addExpressionSymbolDependencies(root, nodes, seen);
  RoseAst ast(root);
  for (RoseAst::iterator it = ast.begin().withoutNullValues(); it != ast.end();
       ++it) {
    if (!insideCollectionBoundary(*it)) {
      continue;
    }
    addExpressionSymbolDependencies(*it, nodes, seen);
    if (isSgBaseClass(*it) != nullptr) {
      it.skipChildrenOnForward();
    }
  }
}

void addOmpAuxiliaryNodes(SgNode *node, std::vector<SgNode *> &nodes,
                          std::unordered_set<SgNode *> &seen) {
  AuxiliaryOwnerGuard owner_guard(node);
  if (SgNode *parent = node != nullptr ? node->get_parent() : nullptr) {
    if (node != collectionBoundaryRoot && isSgType(parent) == nullptr &&
        isSgFileList(parent) == nullptr && isSgProject(parent) == nullptr) {
      addSubtreeNodes(parent, nodes, seen);
    }
  }
  if (SgInitializedName *name = isSgInitializedName(node)) {
    addExactBoundSymbolDependency(
        name->get_fortran_source_derived_type_symbol(), nodes, seen);
  }
  if (SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(node)) {
    addExactBoundSymbolDependency(
        procedure->get_fortran_source_derived_type_symbol(), nodes, seen);
  }
  if (SgAggregateInitializer *aggregate = isSgAggregateInitializer(node)) {
    addExactBoundSymbolDependency(
        aggregate->get_fortran_source_derived_type_symbol(), nodes, seen);
  }

  auto add_expression_with_owner = [&](SgExpression *expr) {
    if (expr == nullptr) {
      return;
    }
    SgExpression *owner = expr;
    while (SgExpression *parent = isSgExpression(owner->get_parent())) {
      owner = parent;
    }
    SgNode *container = owner->get_parent();
    if (isSgExprStatement(container) != nullptr ||
        isSgInitializer(container) != nullptr) {
      addSubtreeNodes(container, nodes, seen);
      addExpressionSubtreeSymbolDependencies(owner, nodes, seen);
    } else {
      addSubtreeNodes(owner, nodes, seen);
      addExpressionSubtreeSymbolDependencies(owner, nodes, seen);
    }
  };

  std::function<void(SgType *)> add_type;
  auto add_template_argument = [&](SgTemplateArgument *argument) {
    if (argument == nullptr) {
      throw std::runtime_error(
          "AST JSON template argument list contains a null typed argument");
    }
    addSubtreeNodes(argument, nodes, seen);
    add_type(argument->get_type());
    add_type(argument->get_sourceSpelledType());
    add_expression_with_owner(argument->get_expression());
    addSubtreeNodes(argument->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(argument->get_initializedName(), nodes, seen);
  };
  auto add_template_arguments =
      [&](const SgTemplateArgumentPtrList &arguments) {
        for (SgTemplateArgument *argument : arguments) {
          add_template_argument(argument);
        }
      };
  auto add_type_owned_expression_dependencies = [&](SgExpression *expr) {
    if (expr == nullptr) {
      return;
    }
    addExpressionSubtreeSymbolDependencies(expr, nodes, seen);

    auto add_expression_type = [&](SgExpression *current) {
      if (current == nullptr) {
        return;
      }
      if (SgFortranCommonBlockRefExp *common =
              isSgFortranCommonBlockRefExp(current)) {
        SageInterface::validateFortranCommonBlockRef(common);
      } else if (SgNewExp *new_expr = isSgNewExp(current)) {
        add_type(new_expr->get_specified_type());
      } else if (SgPseudoDestructorRefExp *pseudo =
                     isSgPseudoDestructorRefExp(current)) {
        add_type(pseudo->get_object_type());
        add_type(pseudo->get_type());
      } else if (SgSizeOfOp *size_of = isSgSizeOfOp(current)) {
        add_type(size_of->get_operand_type());
        add_type(size_of->get_type());
      } else if (SgAlignOfOp *align_of = isSgAlignOfOp(current)) {
        add_type(align_of->get_operand_type());
        add_type(align_of->get_type());
      } else if (expressionCarriesSemanticType(current)) {
        add_type(current->get_type());
      }
      if (SgTypeExpression *type_expr = isSgTypeExpression(current)) {
        add_type(type_expr->get_represented_type());
      }
      if (SgNonrealRefExp *reference = isSgNonrealRefExp(current)) {
        add_template_arguments(reference->get_templateArguments());
      }
    };

    add_expression_type(expr);
    RoseAst ast(expr);
    for (RoseAst::iterator it = ast.begin().withoutNullValues();
         it != ast.end(); ++it) {
      if (SgExpression *child = isSgExpression(*it)) {
        add_expression_type(child);
      }
      if (isSgBaseClass(*it) != nullptr) {
        it.skipChildrenOnForward();
      }
    }
  };

  add_type = [&](SgType *type) -> void {
    if (type == nullptr) {
      return;
    }
    AuxiliaryTypeGuard type_guard(type);
    if (SgPointerMemberType *member_pointer = isSgPointerMemberType(type)) {
      add_type(member_pointer->get_base_type());
      add_type(member_pointer->get_class_type());
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      add_type(pointer->get_base_type());
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      add_type(reference->get_base_type());
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      add_type(reference->get_base_type());
    } else if (SgTypeString *string_type = isSgTypeString(type)) {
      add_type_owned_expression_dependencies(
          string_type->get_lengthExpression());
      add_type_owned_expression_dependencies(string_type->get_type_kind());
    } else if (SgTypeComplex *complex_type = isSgTypeComplex(type)) {
      add_type(complex_type->get_base_type());
    } else if (SgArrayType *array = isSgArrayType(type)) {
      add_type(array->get_base_type());
      add_type_owned_expression_dependencies(array->get_index());
      add_type_owned_expression_dependencies(array->get_dim_info());
    } else if (SgModifierType *modifier = isSgModifierType(type)) {
      add_type(modifier->get_base_type());
    } else if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
      addSubtreeNodes(typedef_type->get_declaration(), nodes, seen);
    } else if (SgClassType *class_type = isSgClassType(type)) {
      addSubtreeNodes(class_type->get_declaration(), nodes, seen);
    } else if (SgEnumType *enum_type = isSgEnumType(type)) {
      addSubtreeNodes(enum_type->get_declaration(), nodes, seen);
    } else if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
      addSubtreeNodes(nonreal_type->get_declaration(), nodes, seen);
    } else if (SgTemplateType *template_type = isSgTemplateType(type)) {
      addSubtreeNodes(template_type->get_template_parameter(), nodes, seen);
      addNodeAncestors(template_type->get_template_parameter(), nodes, seen);
      add_type(template_type->get_class_type());
      add_type(template_type->get_parent_class_type());
    } else if (SgMemberFunctionType *member_type =
                   isSgMemberFunctionType(type)) {
      add_type(member_type->get_return_type());
      add_type(member_type->get_class_type());
      for (SgType *arg_type : member_type->get_arguments()) {
        add_type(arg_type);
      }
    } else if (SgFunctionType *function_type = isSgFunctionType(type)) {
      add_type(function_type->get_return_type());
      for (SgType *arg_type : function_type->get_arguments()) {
        add_type(arg_type);
      }
    } else if (SgDeclType *decl_type = isSgDeclType(type)) {
      add_type(decl_type->get_base_type());
      add_type_owned_expression_dependencies(decl_type->get_base_expression());
    }
  };
  if (SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node)) {
    OpenMPProducerSemanticRecords records =
        OmpSupport::snapshotOpenMPProducerSemanticRecords(pragma);
    auto add_semantic_node = [&](SgNode *semantic_node, SgSymbol *symbol) {
      if (symbol != nullptr) {
        addExactBoundSymbolDependency(symbol, nodes, seen);
      }
      if (semantic_node != nullptr && semantic_node != symbol) {
        addSubtreeNodes(semantic_node, nodes, seen);
        addNodeAncestors(semantic_node, nodes, seen);
      }
    };
    if (records.openacc_cxx_semantic_bindings.has_value()) {
      for (const OpenACCCxxExactSemanticBindings::ExpressionBindings &
               expression : records.openacc_cxx_semantic_bindings->bindings()) {
        for (const OpenACCCxxExactSemanticBindings::Binding &binding :
             expression.identifiers()) {
          add_semantic_node(binding.semanticNode(), binding.symbol());
        }
        for (const OmpExactSubexpressionType &subexpression :
             expression.subexpressions()) {
          add_type(subexpression.resultType());
        }
      }
    }
    if (records.fortran_exact_semantic_bindings.has_value()) {
      add_type(records.fortran_exact_semantic_bindings->defaultIntegerType());
      for (const OmpFortranExactSemanticBindings::Binding &binding :
           records.fortran_exact_semantic_bindings->bindings()) {
        add_semantic_node(binding.semanticNode(), binding.symbol());
        add_type(binding.directiveLocalType());
      }
      for (const OmpFortranExactSemanticBindings::ExpressionTypes &expression :
           records.fortran_exact_semantic_bindings->expressions()) {
        for (const OmpExactSubexpressionType &subexpression :
             expression.subexpressions()) {
          add_type(subexpression.resultType());
        }
      }
    }
  }
  auto add_template_parameter = [&](auto &self,
                                    SgTemplateParameter *parameter) -> void {
    if (parameter == nullptr) {
      return;
    }
    addSubtreeNodes(parameter, nodes, seen);
    add_type(parameter->get_type());
    add_type(parameter->get_defaultTypeParameter());
    add_expression_with_owner(parameter->get_expression());
    add_expression_with_owner(parameter->get_typeConstraint());
    add_expression_with_owner(parameter->get_sourceTypeConstraint());
    add_expression_with_owner(parameter->get_defaultExpressionParameter());
    addSubtreeNodes(parameter->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(parameter->get_sourceSpelledTemplateDeclaration(), nodes,
                    seen);
    addSubtreeNodes(parameter->get_defaultTemplateDeclarationParameter(), nodes,
                    seen);
    addSubtreeNodes(parameter->get_initializedName(), nodes, seen);
  };
  auto add_template_parameters =
      [&](const SgTemplateParameterPtrList &parameters) {
        for (SgTemplateParameter *parameter : parameters) {
          add_template_parameter(add_template_parameter, parameter);
        }
      };

  if (SgExpression *expr = isSgExpression(node)) {
    addSubtreeNodes(expr->get_originalExpressionTree(), nodes, seen);
    if (SgFortranCommonBlockRefExp *common =
            isSgFortranCommonBlockRefExp(expr)) {
      SageInterface::validateFortranCommonBlockRef(common);
      addSubtreeNodes(common->get_common_block(), nodes, seen);
    } else if (SgNewExp *new_expr = isSgNewExp(expr)) {
      add_type(new_expr->get_specified_type());
    } else if (SgPseudoDestructorRefExp *pseudo =
                   isSgPseudoDestructorRefExp(expr)) {
      add_type(pseudo->get_object_type());
      add_type(pseudo->get_type());
    } else if (SgSizeOfOp *size_of = isSgSizeOfOp(expr)) {
      add_type(size_of->get_operand_type());
      add_type(size_of->get_type());
    } else if (SgAlignOfOp *align_of = isSgAlignOfOp(expr)) {
      add_type(align_of->get_operand_type());
      add_type(align_of->get_type());
    } else if (expressionCarriesSemanticType(expr)) {
      add_type(expr->get_type());
    }
    if (SgCastExp *cast = isSgCastExp(expr)) {
      add_type(cast->get_source_type());
      for (SgType *base_type : cast->get_conversion_base_path()) {
        add_type(base_type);
      }
    }
    if (SgTypeExpression *type_expr = isSgTypeExpression(expr)) {
      add_type(type_expr->get_represented_type());
    }
    if (SgTypeRequirement *type_requirement = isSgTypeRequirement(expr)) {
      add_type(type_requirement->get_required_type());
    }
    if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
      addReferencedSymbolBasis(ref->get_fortran_source_visible_symbol(), nodes,
                               seen);
    } else if (SgTemplateFunctionRefExp *ref =
                   isSgTemplateFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
      addSubtreeNodes(ref->get_semantic_function_declaration(), nodes, seen);
      addNodeAncestors(ref->get_semantic_function_declaration(), nodes, seen);
    } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol_i(), nodes, seen);
    } else if (SgTemplateMemberFunctionRefExp *ref =
                   isSgTemplateMemberFunctionRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
      addSubtreeNodes(ref->get_semantic_member_function_declaration(), nodes,
                      seen);
      addNodeAncestors(ref->get_semantic_member_function_declaration(), nodes,
                       seen);
    } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
      addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
      addSubtreeNodes(ref->get_resolved_function_declaration(), nodes, seen);
      addNodeAncestors(ref->get_resolved_function_declaration(), nodes, seen);
      addSubtreeNodes(ref->get_resolved_variable_declaration(), nodes, seen);
      addNodeAncestors(ref->get_resolved_variable_declaration(), nodes, seen);
      add_template_arguments(ref->get_templateArguments());
    } else if (SgThisExp *this_expr = isSgThisExp(expr)) {
      addReferencedSymbolBasis(this_expr->get_class_symbol(), nodes, seen);
      addReferencedSymbolBasis(this_expr->get_nonreal_symbol(), nodes, seen);
    } else if (SgConstructorInitializer *init =
                   isSgConstructorInitializer(expr)) {
      addSubtreeNodes(init->get_declaration(), nodes, seen);
    }
  }
  if (SgSourceFile *file = isSgSourceFile(node)) {
    add_type(file->get_target_size_type());
  }
  if (SgInitializedName *name = isSgInitializedName(node)) {
    add_type(name->get_typeptr());
    add_type(name->get_fortran_source_type());
    add_type(name->get_cxx_source_type());
    if (collectionBoundaryFile != nullptr &&
        collectionBoundaryFile->get_inputLanguage() ==
            SgFile::e_Fortran_language) {
      if (isSgFunctionParameterList(name->get_parent()) != nullptr &&
          (name->get_fortran_source_type() != nullptr ||
           name->get_fortran_source_derived_type_symbol() != nullptr ||
           name->get_fortran_type_spec() !=
               SgInitializedName::e_fortran_type_spec_default ||
           !name->get_fortran_procedure_interface().is_null() ||
           name->get_fortran_separate_shape_declaration() != nullptr ||
           name->get_fortran_separate_pointer_declaration() != nullptr ||
           name->get_cray_pointer_pointee() != nullptr ||
           name->get_fortran_cray_pointer_pointee_shape() != nullptr ||
           name->get_shapeDeferred())) {
        throw std::runtime_error("AST JSON Fortran procedure parameter owns "
                                 "declaration-statement source syntax");
      }
      if (SgVariableDeclaration *declaration =
              isSgVariableDeclaration(name->get_parent())) {
        switch (declaration->get_fortran_declaration_origin()) {
        case SgVariableDeclaration::e_fortran_source_declaration:
          if (name->get_type() == nullptr ||
              name->get_fortran_source_type() == nullptr) {
            throw std::runtime_error(
                "AST JSON Fortran source declaration has no exact "
                "semantic/source type pair");
          }
          break;
        case SgVariableDeclaration::e_fortran_semantic_only_declaration:
          if (name->get_fortran_source_type() != nullptr) {
            throw std::runtime_error(
                "AST JSON semantic-only Fortran declaration owns a source "
                "type surface");
          }
          if (name->get_cray_pointer_pointee() != nullptr ||
              name->get_fortran_cray_pointer_pointee_shape() != nullptr ||
              name->get_fortran_separate_shape_declaration() != nullptr ||
              name->get_fortran_separate_pointer_declaration() != nullptr ||
              name->get_shapeDeferred()) {
            throw std::runtime_error(
                "AST JSON semantic-only Fortran declaration owns Cray "
                "pointer or separate-shape source state");
          }
          break;
        case SgVariableDeclaration::e_fortran_pending_source_declaration:
          throw std::runtime_error(
              "AST JSON pending Fortran source declaration escaped the "
              "frontend");
        default:
          throw std::runtime_error(
              "AST JSON Fortran declaration has an invalid typed origin");
        }
      }
    }
    addSubtreeNodes(name->get_declptr(), nodes, seen);
    addSubtreeNodes(name->get_definition(), nodes, seen);
    addSubtreeNodes(name->get_prev_decl_item(), nodes, seen);
    addSubtreeNodes(name->get_cray_pointer_pointee(), nodes, seen);
    addNodeAncestors(name->get_cray_pointer_pointee(), nodes, seen);
    addSubtreeNodes(name->get_fortran_cray_pointer_pointee_shape(), nodes,
                    seen);
    addSubtreeNodes(name->get_fortran_separate_shape_declaration(), nodes,
                    seen);
    addNodeAncestors(name->get_fortran_separate_shape_declaration(), nodes,
                     seen);
    addSubtreeNodes(name->get_fortran_separate_pointer_declaration(), nodes,
                    seen);
    addNodeAncestors(name->get_fortran_separate_pointer_declaration(), nodes,
                     seen);
  }
  if (SgVariableDefinition *def = isSgVariableDefinition(node)) {
    addSubtreeNodes(def->get_vardefn(), nodes, seen);
    addSubtreeNodes(def->get_bitfield(), nodes, seen);
  }
  if (SgVariableDeclaration *decl = isSgVariableDeclaration(node)) {
    for (SgTemplateParameterList *header :
         decl->get_sourceSpelledTemplateHeaders()) {
      if (header == nullptr || header->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON variable source template header is not owned by its "
            "exact SgVariableDeclaration");
      }
      addSubtreeNodes(header, nodes, seen);
    }
    add_type(decl->get_sourceSpelledTemplateOwnerType());
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    for (SgTemplateParameterList *header :
         decl->get_sourceSpelledTemplateHeaders()) {
      if (header == nullptr || header->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON class source template header is not owned by its exact "
            "SgClassDeclaration");
      }
      addSubtreeNodes(header, nodes, seen);
    }
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    for (SgTemplateParameterList *header :
         decl->get_sourceSpelledTemplateHeaders()) {
      if (header == nullptr || header->get_parent() != decl) {
        throw std::runtime_error(
            "AST JSON function source template header is not owned by its "
            "exact SgFunctionDeclaration");
      }
      addSubtreeNodes(header, nodes, seen);
    }
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    add_type(decl->get_base_type());
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    add_type(decl->get_type());
    add_type(decl->get_type_syntax());
    addSubtreeNodes(decl->get_parameterList(), nodes, seen);
    addSubtreeNodes(decl->get_parameterList_syntax(), nodes, seen);
    addSubtreeNodes(decl->get_functionParameterScope(), nodes, seen);
    addSubtreeNodes(decl->get_function_declarator_scope(), nodes, seen);
    addSubtreeNodes(decl->get_templateInstantiationPattern(), nodes, seen);
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    addSubtreeNodes(decl->get_scope(), nodes, seen);
    addSubtreeNodes(decl->get_firstNondefiningDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_definingDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_declarationScope(), nodes, seen);
    addSubtreeNodes(decl->get_nonreal_decl_scope(), nodes, seen);
  }
  if (SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(node)) {
    addSubtreeNodes(def->get_namespaceDeclaration(), nodes, seen);
    addSingleNode(def->get_previousNamespaceDefinition(), nodes, seen);
    addSingleNode(def->get_nextNamespaceDefinition(), nodes, seen);
    addSingleNode(def->get_global_definition(), nodes, seen);
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    add_type(decl->get_type());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    add_template_arguments(decl->get_tpl_args());
    add_expression_with_owner(decl->get_conceptConstraint());
  }
  if (SgFunctionDefinition *def = isSgFunctionDefinition(node)) {
    if (def->get_construction_physical_output_owner() != nullptr ||
        !def->get_fortran_construction_name().getString().empty()) {
      throw std::runtime_error(
          "AST JSON reached a function definition with an unconsumed "
          "Fortran construction transaction");
    }
  }
  if (SgClassDefinition *def = isSgClassDefinition(node)) {
    if (def->get_construction_physical_output_owner() != nullptr) {
      SgClassDeclaration *decl = def->get_declaration();
      Sg_File_Info *source =
          decl != nullptr ? decl->get_startOfConstruct() : nullptr;
      std::ostringstream message;
      message << "AST JSON reached class definition=" << def << " name="
              << (decl != nullptr ? decl->get_name().getString()
                                  : std::string("<missing>"))
              << " declaration-parent="
              << (decl != nullptr ? decl->get_parent() : nullptr)
              << " declaration-parent-class="
              << (decl != nullptr && decl->get_parent() != nullptr
                      ? decl->get_parent()->class_name()
                      : std::string("<missing>"))
              << " autonomous="
              << (decl != nullptr && decl->get_isAutonomousDeclaration() ? 1
                                                                         : 0)
              << " parent=" << def->get_parent() << " parent-class="
              << (def->get_parent() != nullptr ? def->get_parent()->class_name()
                                               : std::string("<missing>"))
              << " transaction-owner="
              << def->get_construction_physical_output_owner()
              << " transaction-owner-class="
              << def->get_construction_physical_output_owner()->class_name()
              << " source="
              << (source != nullptr ? source->get_filenameString()
                                    : std::string("<missing>"))
              << ':' << (source != nullptr ? source->get_line() : -1) << ':'
              << (source != nullptr ? source->get_col() : -1)
              << " with an unconsumed physical-output construction "
                 "transaction";
      throw std::runtime_error(message.str());
    }
    for (SgBaseClass *base : def->get_inheritances()) {
      addSingleNode(base, nodes, seen);
    }
  }
  if (SgBaseClass *base = isSgBaseClass(node)) {
    addSubtreeNodes(base->get_base_class(), nodes, seen);
    add_type(base->get_source_type());
    if (SgExpBaseClass *expr_base = isSgExpBaseClass(base)) {
      add_expression_with_owner(expr_base->get_base_class_exp());
    }
    if (SgNonrealBaseClass *nonreal_base = isSgNonrealBaseClass(base)) {
      addSubtreeNodes(nonreal_base->get_base_class_nonreal(), nodes, seen);
    }
  }
  if (SgTemplateInstantiationDecl *decl = isSgTemplateInstantiationDecl(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_semanticTemplateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateInstantiationTypedefDeclaration *decl =
          isSgTemplateInstantiationTypedefDeclaration(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateInstantiationFunctionDecl *decl =
          isSgTemplateInstantiationFunctionDecl(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
    for (SgDeclarationStatement *candidate :
         decl->get_dependentTemplateCandidates()) {
      addSubtreeNodes(candidate, nodes, seen);
    }
  }
  if (SgTemplateInstantiationMemberFunctionDecl *decl =
          isSgTemplateInstantiationMemberFunctionDecl(node)) {
    add_template_arguments(decl->get_templateArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_templateDeclaration(), nodes, seen);
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateVariableDeclaration *decl =
          isSgTemplateVariableDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_template_arguments(decl->get_deducedTemplateArguments());
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
  }
  if (SgTemplateTypedefDeclaration *decl =
          isSgTemplateTypedefDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgTemplateClassDeclaration *decl = isSgTemplateClassDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    addSubtreeNodes(decl->get_specializedTemplateDeclaration(), nodes, seen);
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgTemplateFunctionDeclaration *decl =
          isSgTemplateFunctionDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgTemplateMemberFunctionDeclaration *decl =
          isSgTemplateMemberFunctionDeclaration(node)) {
    add_template_parameters(decl->get_templateParameters());
    add_template_arguments(decl->get_templateSpecializationArguments());
    add_expression_with_owner(decl->get_requiresClause());
  }
  if (SgOmpExecStatement *stmt = isSgOmpExecStatement(node)) {
    addSubtreeNodes(stmt->get_omp_parent(), nodes, seen);
    for (SgStatement *child : stmt->get_omp_children()) {
      addSubtreeNodes(child, nodes, seen);
    }
  }
  if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    add_template_argument(argument);
  }
  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    add_template_parameter(add_template_parameter, parameter);
  }
  auto add_clause_list = [&](const SgOmpClausePtrList &clauses) {
    for (SgOmpClause *clause : clauses) {
      addSubtreeNodes(clause, nodes, seen);
    }
  };

  if (SgOmpDeclareSimdStatement *stmt = isSgOmpDeclareSimdStatement(node)) {
    if (stmt->get_function_ref() == nullptr ||
        stmt->get_function_ref()->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON declare simd statement has no required exact target");
    }
    add_expression_with_owner(stmt->get_function_ref());
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpDeclareVariantStatement *stmt =
          isSgOmpDeclareVariantStatement(node)) {
    if (stmt->get_base_function_ref() == nullptr ||
        stmt->get_base_function_ref()->get_parent() != stmt ||
        stmt->get_variant_function_ref() == nullptr ||
        stmt->get_variant_function_ref()->get_parent() != stmt) {
      throw std::runtime_error(
          "AST JSON declare variant statement has no required exact target");
    }
    add_expression_with_owner(stmt->get_base_function_ref());
    add_expression_with_owner(stmt->get_variant_function_ref());
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpRequiresStatement *stmt = isSgOmpRequiresStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }
  if (SgOmpTaskwaitStatement *stmt = isSgOmpTaskwaitStatement(node)) {
    add_clause_list(stmt->get_clauses());
  }

  if (SgOmpVariablesClause *clause = isSgOmpVariablesClause(node)) {
    addSubtreeNodes(clause->get_variables(), nodes, seen);
  }
  if (SgOmpExclusiveClause *clause = isSgOmpExclusiveClause(node)) {
    addSubtreeNodes(clause->get_variables(), nodes, seen);
  }
  if (SgOmpExpressionClause *clause = isSgOmpExpressionClause(node)) {
    add_expression_with_owner(clause->get_expression());
  }
  if (SgOmpAllocateClause *clause = isSgOmpAllocateClause(node)) {
    add_expression_with_owner(clause->get_user_defined_modifier());
    add_expression_with_owner(clause->get_alignment());
  }
  if (SgOmpAllocatorClause *clause = isSgOmpAllocatorClause(node)) {
    add_expression_with_owner(clause->get_user_defined_modifier());
  }
  if (SgOmpInitClause *clause = isSgOmpInitClause(node)) {
    std::string detail;
    if (!Rose::OpenMP::Detail::validateInitClause(clause, &detail)) {
      throw std::runtime_error("AST JSON malformed SgOmpInitClause: " + detail);
    }
  }
  if (SgOmpAdjustArgsClause *clause = isSgOmpAdjustArgsClause(node)) {
    std::string detail;
    if (!Rose::OpenMP::Detail::validateAdjustArgsClause(clause, &detail)) {
      throw std::runtime_error("AST JSON malformed SgOmpAdjustArgsClause: " +
                               detail);
    }
  }
  if (SgOmpAppendArgsClause *clause = isSgOmpAppendArgsClause(node)) {
    std::string detail;
    if (!Rose::OpenMP::Detail::validateAppendArgsClause(clause, &detail)) {
      throw std::runtime_error("AST JSON malformed SgOmpAppendArgsClause: " +
                               detail);
    }
    for (SgOmpAppendArgsOperation *operation :
         clause->get_interop_operations()) {
      addSubtreeNodes(operation, nodes, seen);
    }
  }
  if (SgOmpContextSelectorProperty *property =
          isSgOmpContextSelectorProperty(node)) {
    add_expression_with_owner(property->get_expression());
    add_expression_with_owner(property->get_requires_expression());
  }
  if (SgOmpContextSelector *selector = isSgOmpContextSelector(node)) {
    validateOmpContextSelector(selector);
    add_expression_with_owner(selector->get_score());
    for (SgOmpContextSelectorProperty *property : selector->get_properties()) {
      addSubtreeNodes(property, nodes, seen);
    }
    addSubtreeNodes(selector->get_construct_directive(), nodes, seen);
  }
  if (SgOmpContextSelectorSet *set = isSgOmpContextSelectorSet(node)) {
    validateOmpContextSelectorSet(set);
    for (SgOmpContextSelector *selector : set->get_selectors()) {
      addSubtreeNodes(selector, nodes, seen);
    }
  }
  if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
    validateOmpContextSelectorSets(clause->get_context_selector_sets(), clause);
    for (SgOmpContextSelectorSet *set : clause->get_context_selector_sets()) {
      addSubtreeNodes(set, nodes, seen);
    }
    addSubtreeNodes(clause->get_variant_directive(), nodes, seen);
  }
  if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
    validateOmpContextSelectorSets(clause->get_context_selector_sets(), clause);
    for (SgOmpContextSelectorSet *set : clause->get_context_selector_sets()) {
      addSubtreeNodes(set, nodes, seen);
    }
  }
  if (SgOmpUsesAllocatorsDefination *definition =
          isSgOmpUsesAllocatorsDefination(node)) {
    add_expression_with_owner(definition->get_user_defined_allocator());
    add_expression_with_owner(definition->get_allocator_traits_array());
  }
  if (SgOmpUsesAllocatorsClause *clause = isSgOmpUsesAllocatorsClause(node)) {
    for (SgOmpUsesAllocatorsDefination *definition :
         clause->get_uses_allocators_defination()) {
      if (definition == nullptr) {
        continue;
      }
      addSingleNode(definition, nodes, seen);
      add_expression_with_owner(definition->get_user_defined_allocator());
      add_expression_with_owner(definition->get_allocator_traits_array());
    }
  }
  if (SgOmpDeclareMapperStatement *stmt = isSgOmpDeclareMapperStatement(node)) {
    add_expression_with_owner(stmt->get_user_defined_identifier());
    add_expression_with_owner(stmt->get_mapper_type());
    add_expression_with_owner(stmt->get_mapper_variable());
  }
  if (SgAttributeSpecificationStatement *stmt =
          isSgAttributeSpecificationStatement(node)) {
    addSubtreeNodes(stmt->get_parameter_list(), nodes, seen);
    addSubtreeNodes(stmt->get_bind_list(), nodes, seen);
  }
  if (SgInterfaceStatement *stmt = isSgInterfaceStatement(node)) {
    for (SgInterfaceBody *body : stmt->get_interface_body_list()) {
      addSubtreeNodes(body, nodes, seen);
    }
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgInterfaceBody *body = isSgInterfaceBody(node)) {
    addSubtreeNodes(body->get_functionDeclaration(), nodes, seen);
  }
  if (SgIfStmt *stmt = isSgIfStmt(node)) {
    addSubtreeNodes(stmt->get_else_numeric_label(), nodes, seen);
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgStatement *stmt = isSgStatement(node)) {
    addSubtreeNodes(stmt->get_numeric_label(), nodes, seen);
  }
  if (SgBasicBlock *stmt = isSgBasicBlock(node)) {
    addSubtreeNodes(stmt->get_fortran_block_end_numeric_label(), nodes, seen);
  }
  if (SgWhileStmt *stmt = isSgWhileStmt(node)) {
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgGotoStatement *stmt = isSgGotoStatement(node)) {
    addSubtreeNodes(stmt->get_label(), nodes, seen);
    addSubtreeNodes(stmt->get_label_expression(), nodes, seen);
    addSubtreeNodes(stmt->get_selector_expression(), nodes, seen);
  }
  if (SgFortranNonblockedDo *stmt = isSgFortranNonblockedDo(node)) {
    addSubtreeNodes(stmt->get_end_statement(), nodes, seen);
  }
  if (SgDerivedTypeStatement *stmt = isSgDerivedTypeStatement(node)) {
    addSubtreeNodes(stmt->get_end_numeric_label(), nodes, seen);
  }
  if (SgUsingDeclarationStatement *decl = isSgUsingDeclarationStatement(node)) {
    addSubtreeNodes(decl->get_declaration(), nodes, seen);
    addSubtreeNodes(decl->get_initializedName(), nodes, seen);
  }
}

void addExpressionSymbolDependencies(SgNode *node, std::vector<SgNode *> &nodes,
                                     std::unordered_set<SgNode *> &seen) {
  SgExpression *expr = isSgExpression(node);
  if (expr == nullptr) {
    return;
  }
  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
  } else if (SgLabelRefExp *ref = isSgLabelRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
  } else if (SgFunctionRefExp *ref = isSgFunctionRefExp(expr)) {
    SgFunctionSymbol *symbol = ref->get_symbol();
    addReferencedSymbolBasis(symbol, nodes, seen);
    if (symbol != nullptr &&
        !isAstJsonExternalFunction(symbol->get_declaration())) {
      addSubtreeNodes(symbol->get_declaration(), nodes, seen);
      addNodeAncestors(symbol->get_declaration(), nodes, seen);
    }
    SgFunctionSymbol *sourceVisible = ref->get_fortran_source_visible_symbol();
    addReferencedSymbolBasis(sourceVisible, nodes, seen);
    if (sourceVisible != nullptr &&
        !isAstJsonExternalFunction(sourceVisible->get_declaration())) {
      addSubtreeNodes(sourceVisible->get_declaration(), nodes, seen);
      addNodeAncestors(sourceVisible->get_declaration(), nodes, seen);
    }
  } else if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    addSubtreeNodes(ref->get_semantic_function_declaration(), nodes, seen);
    addNodeAncestors(ref->get_semantic_function_declaration(), nodes, seen);
  } else if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol_i(), nodes, seen);
  } else if (SgTemplateMemberFunctionRefExp *ref =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    addSubtreeNodes(ref->get_semantic_member_function_declaration(), nodes,
                    seen);
    addNodeAncestors(ref->get_semantic_member_function_declaration(), nodes,
                     seen);
  } else if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    addReferencedSymbolBasis(ref->get_symbol(), nodes, seen);
    SgFunctionDeclaration *resolved_function =
        ref->get_resolved_function_declaration();
    if (resolved_function != nullptr &&
        !isAstJsonExternalFunction(resolved_function)) {
      addSubtreeNodes(resolved_function, nodes, seen);
      addNodeAncestors(resolved_function, nodes, seen);
    }
    SgTemplateVariableDeclaration *resolved_variable =
        ref->get_resolved_variable_declaration();
    if (resolved_variable != nullptr) {
      addSubtreeNodes(resolved_variable, nodes, seen);
      addNodeAncestors(resolved_variable, nodes, seen);
    }
  } else if (SgThisExp *this_expr = isSgThisExp(expr)) {
    addReferencedSymbolBasis(this_expr->get_class_symbol(), nodes, seen);
    addReferencedSymbolBasis(this_expr->get_nonreal_symbol(), nodes, seen);
  } else if (SgEnumVal *value = isSgEnumVal(expr)) {
    addSubtreeNodes(value->get_declaration(), nodes, seen);
    addNodeAncestors(value->get_declaration(), nodes, seen);
  }
}

void addScopeSymbolTableDependencies(SgNode *node, std::vector<SgNode *> &nodes,
                                     std::unordered_set<SgNode *> &seen) {
  SgScopeStatement *scope = isSgScopeStatement(node);
  SgSymbolTable *table = scope != nullptr ? scope->get_symbol_table() : nullptr;
  if (table == nullptr || table->get_table() == nullptr) {
    return;
  }
  for (const std::pair<const SgName, SgSymbol *> &entry : *table->get_table()) {
    SgSymbol *symbol = entry.second;
    addReferencedSymbolBasis(symbol, nodes, seen);
    if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      addReferencedSymbolBasis(alias->get_alias(), nodes, seen);
      for (SgNode *causal_node : alias->get_causal_nodes()) {
        addSubtreeNodes(causal_node, nodes, seen);
        addNodeAncestors(causal_node, nodes, seen);
      }
    }
    if (SgRenameSymbol *rename = isSgRenameSymbol(symbol)) {
      addReferencedSymbolBasis(rename->get_original_symbol(), nodes, seen);
    }
    if (SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
      addSubtreeNodes(namespace_symbol->get_aliasDeclaration(), nodes, seen);
      addNodeAncestors(namespace_symbol->get_aliasDeclaration(), nodes, seen);
    }
  }
}

std::string auxiliaryTypeSortKey(SgType *type) {
  if (type == nullptr) {
    return "<null>";
  }

  std::ostringstream key;
  key << type->sage_class_name();
  if (SgPointerMemberType *member_pointer = isSgPointerMemberType(type)) {
    key << '<' << auxiliaryTypeSortKey(member_pointer->get_base_type()) << ','
        << auxiliaryTypeSortKey(member_pointer->get_class_type()) << '>';
  } else if (SgPointerType *pointer = isSgPointerType(type)) {
    key << '<' << auxiliaryTypeSortKey(pointer->get_base_type()) << '>';
  } else if (SgReferenceType *reference = isSgReferenceType(type)) {
    key << '<' << auxiliaryTypeSortKey(reference->get_base_type()) << '>';
  } else if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type)) {
    key << '<' << auxiliaryTypeSortKey(reference->get_base_type()) << '>';
  } else if (SgModifierType *modifier = isSgModifierType(type)) {
    key << '<' << auxiliaryTypeSortKey(modifier->get_base_type()) << '>';
  } else if (SgArrayType *array = isSgArrayType(type)) {
    key << '<' << auxiliaryTypeSortKey(array->get_base_type()) << '>';
  } else if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
    if (SgTypedefDeclaration *decl =
            isSgTypedefDeclaration(typedef_type->get_declaration())) {
      key << ':' << decl->get_name().getString();
    }
  } else if (SgClassType *class_type = isSgClassType(type)) {
    if (SgClassDeclaration *decl =
            isSgClassDeclaration(class_type->get_declaration())) {
      key << ':' << decl->get_name().getString();
    }
  } else if (SgEnumType *enum_type = isSgEnumType(type)) {
    if (SgEnumDeclaration *decl =
            isSgEnumDeclaration(enum_type->get_declaration())) {
      key << ':' << decl->get_name().getString();
    }
  } else if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
    if (SgNonrealDecl *decl =
            isSgNonrealDecl(nonreal_type->get_declaration())) {
      key << ':' << decl->get_semantic_name().getString();
    }
  } else if (SgTemplateType *template_type = isSgTemplateType(type)) {
    key << ':' << template_type->get_name().getString() << ':'
        << template_type->get_template_parameter_depth() << ':'
        << template_type->get_template_parameter_position();
  } else if (SgFunctionType *function_type = isSgFunctionType(type)) {
    key << '<' << auxiliaryTypeSortKey(function_type->get_return_type());
    for (SgType *argument : function_type->get_arguments()) {
      key << ',' << auxiliaryTypeSortKey(argument);
    }
    key << '>';
  }
  return key.str();
}

std::string auxiliaryNodeSortKey(SgNode *node) {
  std::ostringstream key;
  key << (node != nullptr ? node->sage_class_name() : "") << '|';
  if (SgInitializedName *name = isSgInitializedName(node)) {
    key << name->get_name().getString();
  } else if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    key << decl->get_name().getString();
  } else if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    key << decl->get_name().getString();
  } else if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    key << argument->get_argumentType() << ':'
        << auxiliaryTypeSortKey(argument->get_type());
    key << ':' << auxiliaryTypeSortKey(argument->get_sourceSpelledType());
    if (argument->get_expression() != nullptr) {
      key << ':' << argument->get_expression()->sage_class_name() << ':'
          << safeNodeText(argument->get_expression());
    }
  } else if (SgIntVal *value = isSgIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgUnsignedIntVal *value = isSgUnsignedIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgLongIntVal *value = isSgLongIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgUnsignedLongVal *value = isSgUnsignedLongVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgLongLongIntVal *value = isSgLongLongIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  } else if (SgUnsignedLongLongIntVal *value =
                 isSgUnsignedLongLongIntVal(node)) {
    key << value->get_value() << ':' << value->get_valueString();
  }
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    if (Sg_File_Info *start = located->get_startOfConstruct()) {
      key << '|' << start->get_filenameString() << ':' << start->get_raw_line()
          << ':' << start->get_raw_col();
    }
  }
  return key.str();
}

std::vector<SgNode *> collectNodes(SgNode *root,
                                   SgSourceFile *collectionBoundary) {
  CollectionBoundaryGuard boundary(collectionBoundary != nullptr
                                       ? collectionBoundary
                                       : isSgSourceFile(root));
  std::vector<SgNode *> nodes;
  std::unordered_set<SgNode *> seen;
  std::unordered_set<SgNode *> expanded;
  SubtreeExpansionGuard expansion_guard(expanded);
  addSubtreeNodes(root, nodes, seen);
  if (SgSourceFile *file = isSgSourceFile(root)) {
    for (SgToken *token : file->get_token_list()) {
      addSingleNode(token, nodes, seen);
    }
  }
  const size_t original_count = nodes.size();
  for (size_t i = 0; i < nodes.size(); ++i) {
    addOmpAuxiliaryNodes(nodes[i], nodes, seen);
    addExpressionSymbolDependencies(nodes[i], nodes, seen);
    addScopeSymbolTableDependencies(nodes[i], nodes, seen);
  }
  for (size_t i = original_count; i < nodes.size(); ++i) {
    addOmpAuxiliaryNodes(nodes[i], nodes, seen);
    addExpressionSymbolDependencies(nodes[i], nodes, seen);
    addScopeSymbolTableDependencies(nodes[i], nodes, seen);
  }
  std::stable_sort(nodes.begin() + original_count, nodes.end(),
                   [](SgNode *lhs, SgNode *rhs) {
                     return auxiliaryNodeSortKey(lhs) <
                            auxiliaryNodeSortKey(rhs);
                   });
  return nodes;
}

std::string safeNodeText(SgNode *node) {
  if (node == nullptr) {
    return "";
  }
  if (SgInitializedName *name = isSgInitializedName(node)) {
    return name->get_name().getString();
  }
  if (SgFunctionDeclaration *decl = isSgFunctionDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgEnumDeclaration *decl = isSgEnumDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgTypedefDeclaration *decl = isSgTypedefDeclaration(node)) {
    return decl->get_name().getString();
  }
  if (SgNamespaceDeclarationStatement *decl =
          isSgNamespaceDeclarationStatement(node)) {
    return decl->get_name().getString();
  }
  if (SgNonrealDecl *decl = isSgNonrealDecl(node)) {
    return decl->get_name().getString();
  }
  if (SgPragma *pragma = isSgPragma(node)) {
    return pragma->get_name();
  }
  if (SgVarRefExp *ref = isSgVarRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  if (SgFunctionRefExp *ref = isSgFunctionRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  if (SgTemplateFunctionRefExp *ref = isSgTemplateFunctionRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(node)) {
    return ref->get_symbol_i() != nullptr
               ? ref->get_symbol_i()->get_name().getString()
               : "";
  }
  if (SgNonrealRefExp *ref = isSgNonrealRefExp(node)) {
    return ref->get_symbol() != nullptr
               ? ref->get_symbol()->get_name().getString()
               : "";
  }
  return "";
}

std::string sourceFileNameForNode(SgNode *node) {
  const std::string external_source =
      astJsonStringAttribute(node, kAstJsonExternalSourceFileAttribute);
  if (!external_source.empty()) {
    return external_source;
  }
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgSourceFile *source_file = isSgSourceFile(current)) {
      return source_file->getFileName();
    }
  }
  return "";
}

std::string sourceFileNameForExternalFunction(SgFunctionDeclaration *decl) {
  std::string source_file = sourceFileNameForNode(decl);
  if (source_file.empty() && collectionBoundaryFile != nullptr) {
    source_file = collectionBoundaryFile->getFileName();
  }
  if (source_file.empty()) {
    std::ostringstream message;
    message << "AST JSON external_function ";
    if (decl == nullptr) {
      message << "<null>";
    } else {
      message << decl->get_name().getString();
      if (SgNode *parent = decl->get_parent()) {
        message << " parent=" << parent->sage_class_name();
      } else {
        message << " parent=<null>";
      }
    }
    message << " requires a non-empty source_file";
    throw std::runtime_error(message.str());
  }
  return source_file;
}

SgModuleStatement *enclosingModuleStatement(SgNode *node) {
  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgModuleStatement *module = isSgModuleStatement(current)) {
      return module;
    }
  }
  return nullptr;
}

std::string moduleNameForNode(SgNode *node) {
  SgModuleStatement *module = enclosingModuleStatement(node);
  return module != nullptr ? module->get_name().getString() : "";
}

bool classDeclarationHasDefinition(SgClassDeclaration *decl) {
  return decl != nullptr && decl->get_definition() != nullptr;
}

bool classDeclarationIsFirstNondefining(SgClassDeclaration *decl) {
  return decl != nullptr && decl->get_firstNondefiningDeclaration() == decl;
}

} // namespace AstJson
} // namespace Rose
