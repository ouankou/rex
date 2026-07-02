#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

Sg_File_Info *buildFileInfo(const JsonValue &json, SgNode *parent) {
  if (json.kind != JsonValue::Kind::Object || !json.boolOr("present", false)) {
    return nullptr;
  }
  auto special_file_id = [](const std::string &name, int &file_id) -> bool {
    if (name == "COPY") {
      file_id = Sg_File_Info::COPY_FILE_ID;
      return true;
    }
    if (name == "NULL_FILE") {
      file_id = Sg_File_Info::NULL_FILE_ID;
      return true;
    }
    if (name == "transformation") {
      file_id = Sg_File_Info::TRANSFORMATION_FILE_ID;
      return true;
    }
    if (name == "compilerGenerated") {
      file_id = Sg_File_Info::COMPILER_GENERATED_FILE_ID;
      return true;
    }
    return false;
  };
  const std::string raw_filename =
      json.stringOr("raw_filename", json.stringOr("filename"));
  const int raw_line =
      static_cast<int>(json.intOr("raw_line", json.intOr("line", 0)));
  const int raw_column =
      static_cast<int>(json.intOr("raw_column", json.intOr("column", 0)));
  int raw_constructor_file_id = 0;
  std::unique_ptr<Sg_File_Info> info(
      special_file_id(raw_filename, raw_constructor_file_id)
          ? new Sg_File_Info(raw_constructor_file_id, raw_line, raw_column)
          : new Sg_File_Info(raw_filename, raw_line, raw_column));
  const JsonValue *physical_file_id_value = json.find("physical_file_id");
  const int physical_file_id =
      physical_file_id_value != nullptr
          ? static_cast<int>(physical_file_id_value->asInt())
          : 0;
  const int physical_internal_file_id = static_cast<int>(
      json.intOr("physical_internal_file_id", physical_file_id));
  const std::string physical_raw_filename =
      json.stringOr("physical_raw_filename");
  const std::string physical_filename = json.stringOr("physical_filename");
  if (physical_file_id >= 0) {
    const std::string registered_name = !physical_raw_filename.empty()
                                            ? physical_raw_filename
                                            : physical_filename;
    requireFileIdMapping(physical_file_id, registered_name,
                         "file info physical_file_id");
  }

  if (physical_internal_file_id >= 0) {
    const std::string registered_name = filenameForFileId(
        physical_internal_file_id, "file info physical_internal_file_id");
    requireFileIdMapping(physical_internal_file_id, registered_name,
                         "file info physical_internal_file_id");
    info->set_physical_file_id(physical_internal_file_id);
  } else if (physical_internal_file_id < 0) {
    info->set_physical_file_id(physical_internal_file_id);
  } else if (!physical_raw_filename.empty()) {
    info->set_physical_filename(physical_raw_filename);
  } else if (!physical_filename.empty()) {
    info->set_physical_filename(physical_filename);
  }
  info->set_physical_line(json.intOr("physical_line", 0));
  info->set_source_sequence_number(
      static_cast<unsigned int>(json.intOr("source_sequence", 0)));
  if (json.boolOr("compiler_generated", false)) {
    info->setCompilerGenerated();
  } else {
    info->unsetCompilerGenerated();
  }
  if (json.boolOr("transformation", false)) {
    info->setTransformation();
  } else {
    info->unsetTransformation();
  }
  if (json.boolOr("frontend_specific", false)) {
    info->setFrontendSpecific();
  } else {
    info->unsetFrontendSpecific();
  }
  if (json.boolOr("shared", false)) {
    info->setShared();
  } else {
    info->unsetShared();
  }
  if (json.boolOr("source_position_unavailable_in_frontend", false)) {
    info->setSourcePositionUnavailableInFrontend();
  } else {
    info->unsetSourcePositionUnavailableInFrontend();
  }
  if (json.boolOr("comment_or_directive", false)) {
    info->setCommentOrDirective();
  } else {
    info->unsetCommentOrDirective();
  }
  if (json.boolOr("token", false)) {
    info->setToken();
  } else {
    info->unsetToken();
  }
  if (json.boolOr("default_argument", false)) {
    info->setDefaultArgument();
  } else {
    info->unsetDefaultArgument();
  }
  if (json.boolOr("implicit_cast", false)) {
    info->setImplicitCast();
  } else {
    info->unsetImplicitCast();
  }
  const JsonValue *file_id_value = json.find("file_id");
  if (file_id_value != nullptr) {
    const int file_id = static_cast<int>(file_id_value->asInt());
    int raw_special_file_id = 0;
    const bool raw_is_special =
        special_file_id(raw_filename, raw_special_file_id);
    const bool special_reported_by_flags =
        (file_id == Sg_File_Info::TRANSFORMATION_FILE_ID &&
         info->isTransformation()) ||
        (file_id == Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
         info->isCompilerGenerated() && !info->isFrontendSpecific());
    if (file_id < 0 && ((raw_is_special && raw_special_file_id == file_id) ||
                        !special_reported_by_flags)) {
      info->set_file_id(file_id);
    }
  }
  if (json.boolOr("output_in_code_generation", false)) {
    info->setOutputInCodeGeneration();
  } else {
    info->unsetOutputInCodeGeneration();
  }
  info->set_parent(parent);
  return info.release();
}

bool sameClassDeclarationFamily(SgClassDeclaration *lhs,
                                SgClassDeclaration *rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  if (lhs == rhs) {
    return true;
  }
  std::array<SgClassDeclaration *, 3> lhs_family = {
      lhs, isSgClassDeclaration(lhs->get_definingDeclaration()),
      isSgClassDeclaration(lhs->get_firstNondefiningDeclaration())};
  std::array<SgClassDeclaration *, 3> rhs_family = {
      rhs, isSgClassDeclaration(rhs->get_definingDeclaration()),
      isSgClassDeclaration(rhs->get_firstNondefiningDeclaration())};
  for (SgClassDeclaration *lhs_member : lhs_family) {
    if (lhs_member == nullptr) {
      continue;
    }
    for (SgClassDeclaration *rhs_member : rhs_family) {
      if (lhs_member == rhs_member) {
        return true;
      }
    }
  }
  return false;
}

SgClassType *ensureClassTypeForDeclaration(SgClassDeclaration *decl) {
  if (decl == nullptr) {
    throw std::runtime_error("AST JSON class type requires a declaration");
  }
  SgClassType *type = decl->get_type();
  SgClassDeclaration *type_decl =
      type != nullptr ? isSgClassDeclaration(type->get_declaration()) : nullptr;
  if (type == nullptr || !sameClassDeclarationFamily(decl, type_decl)) {
    type = new SgClassType(decl);
    decl->set_type(type);
  }
  return type;
}

SgNode *nodeById(const NodeMap &nodes, uint64_t id) {
  auto found = nodes.find(id);
  if (found == nodes.end()) {
    throw std::runtime_error("AST JSON references node id that was not built " +
                             std::to_string(id));
  }
  return found->second;
}

void restoreQualifiedNameState(SgNode *node, const JsonValue &properties,
                               const NodeMap &nodes) {
  const JsonValue &state = properties.at("qualified_name_state");
  if (state.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON qualified_name_state field is not an object");
  }

  SgNode::get_globalQualifiedNameMapForNames().erase(node);
  SgNode::get_globalQualifiedNameMapForTypes().erase(node);
  SgNode::get_globalQualifiedNameMapForTemplateHeaders().erase(node);
  SgNode::get_globalTypeNameMap().erase(node);
  SgNode::get_globalQualifiedNameMapForMapsOfTypes().erase(node);

  if (const JsonValue *value = state.find("name_prefix")) {
    const std::string prefix = value->asString();
    if (shouldSerializeNamePrefix(node, prefix)) {
      SgNode::get_globalQualifiedNameMapForNames()[node] = prefix;
    }
  }
  if (const JsonValue *value = state.find("type_prefix")) {
    SgNode::get_globalQualifiedNameMapForTypes()[node] = value->asString();
  }
  if (const JsonValue *value = state.find("template_header")) {
    SgNode::get_globalQualifiedNameMapForTemplateHeaders()[node] =
        value->asString();
  }
  if (const JsonValue *value = state.find("type_name")) {
    SgNode::get_globalTypeNameMap()[node] = value->asString();
  }
  if (const JsonValue *entries = state.find("type_map_prefixes")) {
    if (entries->kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON type_map_prefixes field is not an array");
    }
    SgUnorderedMapNodeToString restored_entries;
    for (const JsonValue &entry : entries->array) {
      if (entry.kind != JsonValue::Kind::Object) {
        throw std::runtime_error(
            "AST JSON type_map_prefixes entry is not an object");
      }
      const uint64_t target_id =
          static_cast<uint64_t>(entry.at("node").asInt());
      restored_entries[nodeById(nodes, target_id)] =
          entry.at("prefix").asString();
    }
    SgNode::get_globalQualifiedNameMapForMapsOfTypes()[node] =
        std::move(restored_entries);
  }
}

void requireRestoredKind(SgNode *node, const NodeRecord &record) {
  const std::string actual =
      node != nullptr ? node->sage_class_name() : std::string("<null>");
  if (actual != record.kind) {
    throw std::runtime_error("AST JSON restored node kind mismatch for id " +
                             std::to_string(record.id) + ": expected " +
                             record.kind + ", got " + actual);
  }
}

bool requiresDelayedRebuild(const NodeRecord &record) {
  return record.kind == "SgTemplateInstantiationDefn";
}

SgScopeStatement *nearestScope(SgNode *node);

SgDeclType *buildDeclType(SgExpression *base_expression,
                          const JsonValue &type) {
  if (base_expression == nullptr) {
    throw std::runtime_error(
        "AST JSON SgDeclType requires a structured base_expression node");
  }
  SgDeclType *decl_type = new SgDeclType(base_expression);
  base_expression->set_parent(decl_type);
  decl_type->set_is_gnu_decltype(type.boolOr("is_gnu_decltype", false));
  return isSgDeclType(attachJsonTypeText(decl_type, type));
}

SgType *primitiveTypeFromKind(const std::string &kind) {
  if (kind == "SgTypeVoid")
    return SageBuilder::buildVoidType();
  if (kind == "SgTypeBool")
    return SageBuilder::buildBoolType();
  if (kind == "SgTypeChar")
    return SageBuilder::buildCharType();
  if (kind == "SgTypeSignedChar")
    return SageBuilder::buildSignedCharType();
  if (kind == "SgTypeUnsignedChar")
    return SageBuilder::buildUnsignedCharType();
  if (kind == "SgTypeShort")
    return SageBuilder::buildShortType();
  if (kind == "SgTypeSignedShort")
    return SageBuilder::buildSignedShortType();
  if (kind == "SgTypeUnsignedShort")
    return SageBuilder::buildUnsignedShortType();
  if (kind == "SgTypeInt")
    return SageBuilder::buildIntType();
  if (kind == "SgTypeSignedInt")
    return SageBuilder::buildSignedIntType();
  if (kind == "SgTypeUnsignedInt")
    return SageBuilder::buildUnsignedIntType();
  if (kind == "SgTypeLong")
    return SageBuilder::buildLongType();
  if (kind == "SgTypeSignedLong")
    return SageBuilder::buildSignedLongType();
  if (kind == "SgTypeUnsignedLong")
    return SageBuilder::buildUnsignedLongType();
  if (kind == "SgTypeLongLong")
    return SageBuilder::buildLongLongType();
  if (kind == "SgTypeSignedLongLong")
    return SageBuilder::buildSignedLongLongType();
  if (kind == "SgTypeUnsignedLongLong")
    return SageBuilder::buildUnsignedLongLongType();
  if (kind == "SgTypeFloat")
    return SageBuilder::buildFloatType();
  if (kind == "SgTypeDouble")
    return SageBuilder::buildDoubleType();
  if (kind == "SgTypeLongDouble")
    return SageBuilder::buildLongDoubleType();
  if (kind == "SgTypeFloat16")
    return SageBuilder::buildFloat16Type();
  if (kind == "SgTypeFloat80")
    return SageBuilder::buildFloat80Type();
  if (kind == "SgTypeFloat128")
    return SageBuilder::buildFloat128Type();
  if (kind == "SgTypeFp16")
    return SageBuilder::buildFp16Type();
  if (kind == "SgTypeBFloat16")
    return SageBuilder::buildBFloat16Type();
  if (kind == "SgTypeFloat32")
    return SageBuilder::buildFloat32Type();
  if (kind == "SgTypeFloat64")
    return SageBuilder::buildFloat64Type();
  if (kind == "SgTypeFloat32x")
    return SageBuilder::buildFloat32xType();
  if (kind == "SgTypeFloat64x")
    return SageBuilder::buildFloat64xType();
  if (kind == "SgTypeWchar")
    return SageBuilder::buildWcharType();
  if (kind == "SgTypeChar16")
    return SageBuilder::buildChar16Type();
  if (kind == "SgTypeChar32")
    return SageBuilder::buildChar32Type();
  if (kind == "SgAutoType")
    return SageBuilder::buildAutoType();
  return nullptr;
}

SgType *earlyTypeFromJson(const JsonValue &type) {
  if (type.kind != JsonValue::Kind::Object || !type.boolOr("present", false)) {
    return SageBuilder::buildUnknownType();
  }

  const std::string kind = type.stringOr("kind");
  if (kind == "SgTypeDefault") {
    return attachJsonTypeText(SgTypeDefault::createType(), type);
  }
  if (kind == "SgTypeUnknown") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), type);
  }
  if (kind == "SgTypeEllipse") {
    return attachJsonTypeText(SgTypeEllipse::createType(), type);
  }
  if (kind == "SgTypeNullptr") {
    return attachJsonTypeText(SageBuilder::buildNullptrType(), type);
  }
  if (kind == "SgTypeLabel") {
    return attachJsonTypeText(
        SgTypeLabel::createType(SgName(type.stringOr("name"))), type);
  }
  if (kind == "SgTypeFloat128") {
    return attachJsonTypeText(SageBuilder::buildFloat128Type(), type);
  }
  if (kind == "SgTypeSigned128bitInteger") {
    return attachJsonTypeText(SageBuilder::buildSigned128bitIntegerType(),
                              type);
  }
  if (kind == "SgTypeUnsigned128bitInteger") {
    return attachJsonTypeText(SageBuilder::buildUnsigned128bitIntegerType(),
                              type);
  }
  if (SgType *primitive = primitiveTypeFromKind(kind)) {
    return attachJsonTypeText(primitive, type);
  }
  if (kind == "SgTypeString") {
    return attachJsonTypeText(new SgTypeString(nullptr), type);
  }
  if (kind == "SgTypeComplex") {
    return attachJsonTypeText(
        SgTypeComplex::createType(earlyTypeFromJson(type.at("base"))), type);
  }
  if (kind == "SgPointerMemberType") {
    return attachJsonTypeText(SgPointerMemberType::createType(
                                  earlyTypeFromJson(type.at("base")),
                                  earlyTypeFromJson(type.at("class_type"))),
                              type);
  }
  if (kind == "SgPointerType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    return attachJsonTypeText(buildCachedJsonPointerType(base), type);
  }
  if (kind == "SgReferenceType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    SgReferenceType *reference = new SgReferenceType(base);
    installReferenceCache(base, reference);
    return attachJsonTypeText(reference, type);
  }
  if (kind == "SgRvalueReferenceType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    SgRvalueReferenceType *reference = new SgRvalueReferenceType(base);
    installRvalueReferenceCache(base, reference);
    return attachJsonTypeText(reference, type);
  }
  if (kind == "SgArrayType") {
    SgArrayType *array =
        new SgArrayType(earlyTypeFromJson(type.at("base")), nullptr);
    array->set_is_variable_length_array(
        type.boolOr("is_variable_length_array", false));
    return attachJsonTypeText(array, type);
  }
  if (kind == "SgModifierType") {
    SgType *base = earlyTypeFromJson(type.at("base"));
    SgModifierType *modifier = new SgModifierType(base);
    SgTypeModifier &type_modifier = modifier->get_typeModifier();
    SgConstVolatileModifier &cv = type_modifier.get_constVolatileModifier();
    if (type.boolOr("modifier_const", false)) {
      cv.setConst();
    }
    if (type.boolOr("modifier_volatile", false)) {
      cv.setVolatile();
    }
    if (type.boolOr("modifier_restrict", false)) {
      type_modifier.setRestrict();
    }
    return attachJsonTypeText(modifier, type);
  }
  if (kind == "SgTypedefType") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), type);
  }
  if (kind == "SgClassType") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), type);
  }
  if (kind == "SgEnumType") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), type);
  }
  if (kind == "SgNonrealType") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), type);
  }
  if (kind == "SgDeclType") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), type);
  }
  if (kind == "SgTemplateType") {
    SgTemplateType *template_type = new SgTemplateType(
        SgName(type.stringOr("name", type.stringOr("text"))));
    template_type->set_template_parameter_position(
        static_cast<int>(type.intOr("template_parameter_position", -1)));
    template_type->set_template_parameter_depth(
        static_cast<int>(type.intOr("template_parameter_depth", -1)));
    if (const JsonValue *class_json = type.find("class_type")) {
      template_type->set_class_type(class_json->boolOr("present", false)
                                        ? earlyTypeFromJson(*class_json)
                                        : nullptr);
    } else {
      template_type->set_class_type(nullptr);
    }
    if (const JsonValue *parent_class_json = type.find("parent_class_type")) {
      template_type->set_parent_class_type(
          parent_class_json->boolOr("present", false)
              ? earlyTypeFromJson(*parent_class_json)
              : nullptr);
    } else {
      template_type->set_parent_class_type(nullptr);
    }
    return attachJsonTypeText(template_type, type);
  }
  if (kind == "SgMemberFunctionType") {
    SgType *return_type = SageBuilder::buildIntType();
    if (const JsonValue *return_json = type.find("return_type")) {
      return_type = earlyTypeFromJson(*return_json);
    }
    SgType *class_type = nullptr;
    if (const JsonValue *class_json = type.find("class_type")) {
      class_type = earlyTypeFromJson(*class_json);
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    if (const JsonValue *argument_json = type.find("arguments")) {
      if (argument_json->kind == JsonValue::Kind::Array) {
        for (const JsonValue &argument : argument_json->array) {
          arguments->append_argument(earlyTypeFromJson(argument));
        }
      }
    }
    SgMemberFunctionType *member_type = new SgMemberFunctionType(
        return_type, type.boolOr("has_ellipses", false), class_type,
        static_cast<unsigned int>(type.intOr("mfunc_specifier", 0)));
    member_type->set_argument_list(arguments);
    arguments->set_parent(member_type);
    return attachJsonTypeText(member_type, type);
  }
  if (kind == "SgFunctionType") {
    SgType *return_type = SageBuilder::buildIntType();
    if (const JsonValue *return_json = type.find("return_type")) {
      return_type = earlyTypeFromJson(*return_json);
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    if (const JsonValue *argument_json = type.find("arguments")) {
      if (argument_json->kind == JsonValue::Kind::Array) {
        for (const JsonValue &argument : argument_json->array) {
          arguments->append_argument(earlyTypeFromJson(argument));
        }
      }
    }
    SgFunctionType *function_type =
        new SgFunctionType(return_type, type.boolOr("has_ellipses", false));
    function_type->set_argument_list(arguments);
    arguments->set_parent(function_type);
    return attachJsonTypeText(function_type, type);
  }
  throw std::runtime_error("AST JSON deserializer does not support Sage type " +
                           kind + " during early construction");
}

SgType *earlyTypeFromProperties(const JsonValue &properties) {
  const JsonValue *type = properties.find("type");
  if (type == nullptr) {
    return SageBuilder::buildUnknownType();
  }
  return earlyTypeFromJson(*type);
}

bool isJsonBinaryOpKind(const std::string &kind) {
  static const std::unordered_set<std::string> kinds = {"SgAssignOp",
                                                        "SgAddOp",
                                                        "SgSubtractOp",
                                                        "SgMultiplyOp",
                                                        "SgDivideOp",
                                                        "SgModOp",
                                                        "SgLessThanOp",
                                                        "SgLessOrEqualOp",
                                                        "SgGreaterThanOp",
                                                        "SgGreaterOrEqualOp",
                                                        "SgEqualityOp",
                                                        "SgNotEqualOp",
                                                        "SgLshiftOp",
                                                        "SgRshiftOp",
                                                        "SgAndOp",
                                                        "SgOrOp",
                                                        "SgBitAndOp",
                                                        "SgBitOrOp",
                                                        "SgBitXorOp",
                                                        "SgCommaOpExp",
                                                        "SgDotExp",
                                                        "SgArrowExp",
                                                        "SgPntrArrRefExp",
                                                        "SgPlusAssignOp",
                                                        "SgMinusAssignOp",
                                                        "SgMultAssignOp",
                                                        "SgDivAssignOp",
                                                        "SgModAssignOp",
                                                        "SgAndAssignOp",
                                                        "SgIorAssignOp",
                                                        "SgXorAssignOp",
                                                        "SgLshiftAssignOp",
                                                        "SgRshiftAssignOp",
                                                        "SgConcatenationOp",
                                                        "SgExponentiationOp",
                                                        "SgPointerAssignOp"};
  return kinds.find(kind) != kinds.end();
}

SgBinaryOp *buildBinaryOpForKind(const std::string &kind, SgType *expr_type) {
  SgExpression *null_expr = nullptr;
  if (kind == "SgAssignOp")
    return new SgAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgAddOp")
    return new SgAddOp(null_expr, null_expr, expr_type);
  if (kind == "SgSubtractOp")
    return new SgSubtractOp(null_expr, null_expr, expr_type);
  if (kind == "SgMultiplyOp")
    return new SgMultiplyOp(null_expr, null_expr, expr_type);
  if (kind == "SgDivideOp")
    return new SgDivideOp(null_expr, null_expr, expr_type);
  if (kind == "SgModOp")
    return new SgModOp(null_expr, null_expr, expr_type);
  if (kind == "SgLessThanOp")
    return new SgLessThanOp(null_expr, null_expr, expr_type);
  if (kind == "SgLessOrEqualOp")
    return new SgLessOrEqualOp(null_expr, null_expr, expr_type);
  if (kind == "SgGreaterThanOp")
    return new SgGreaterThanOp(null_expr, null_expr, expr_type);
  if (kind == "SgGreaterOrEqualOp")
    return new SgGreaterOrEqualOp(null_expr, null_expr, expr_type);
  if (kind == "SgEqualityOp")
    return new SgEqualityOp(null_expr, null_expr, expr_type);
  if (kind == "SgNotEqualOp")
    return new SgNotEqualOp(null_expr, null_expr, expr_type);
  if (kind == "SgLshiftOp")
    return new SgLshiftOp(null_expr, null_expr, expr_type);
  if (kind == "SgRshiftOp")
    return new SgRshiftOp(null_expr, null_expr, expr_type);
  if (kind == "SgAndOp")
    return new SgAndOp(null_expr, null_expr, expr_type);
  if (kind == "SgOrOp")
    return new SgOrOp(null_expr, null_expr, expr_type);
  if (kind == "SgBitAndOp")
    return new SgBitAndOp(null_expr, null_expr, expr_type);
  if (kind == "SgBitOrOp")
    return new SgBitOrOp(null_expr, null_expr, expr_type);
  if (kind == "SgBitXorOp")
    return new SgBitXorOp(null_expr, null_expr, expr_type);
  if (kind == "SgCommaOpExp")
    return new SgCommaOpExp(null_expr, null_expr, expr_type);
  if (kind == "SgDotExp")
    return new SgDotExp(null_expr, null_expr, expr_type);
  if (kind == "SgArrowExp")
    return new SgArrowExp(null_expr, null_expr, expr_type);
  if (kind == "SgPntrArrRefExp")
    return new SgPntrArrRefExp(null_expr, null_expr, expr_type);
  if (kind == "SgConcatenationOp")
    return new SgConcatenationOp(null_expr, null_expr, expr_type);
  if (kind == "SgExponentiationOp")
    return new SgExponentiationOp(null_expr, null_expr, expr_type);
  if (kind == "SgPointerAssignOp")
    return new SgPointerAssignOp(static_cast<Sg_File_Info *>(nullptr),
                                 null_expr, null_expr, expr_type);
  if (kind == "SgPlusAssignOp")
    return new SgPlusAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgMinusAssignOp")
    return new SgMinusAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgMultAssignOp")
    return new SgMultAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgDivAssignOp")
    return new SgDivAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgModAssignOp")
    return new SgModAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgAndAssignOp")
    return new SgAndAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgIorAssignOp")
    return new SgIorAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgXorAssignOp")
    return new SgXorAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgLshiftAssignOp")
    return new SgLshiftAssignOp(null_expr, null_expr, expr_type);
  if (kind == "SgRshiftAssignOp")
    return new SgRshiftAssignOp(null_expr, null_expr, expr_type);
  return nullptr;
}

bool isJsonUnaryOpKind(const std::string &kind) {
  static const std::unordered_set<std::string> kinds = {
      "SgAddressOfOp", "SgPointerDerefExp", "SgNotOp",   "SgBitComplementOp",
      "SgPlusPlusOp",  "SgMinusMinusOp",    "SgMinusOp", "SgUnaryAddOp"};
  return kinds.find(kind) != kinds.end();
}

SgUnaryOp *buildUnaryOpForKind(const std::string &kind, SgType *expr_type,
                               const JsonValue &properties) {
  SgExpression *null_expr = nullptr;
  if (kind == "SgAddressOfOp") {
    return new SgAddressOfOp(null_expr, expr_type);
  }
  if (kind == "SgPointerDerefExp") {
    return new SgPointerDerefExp(null_expr, expr_type);
  }
  if (kind == "SgNotOp") {
    return new SgNotOp(null_expr, expr_type);
  }
  if (kind == "SgBitComplementOp") {
    return new SgBitComplementOp(null_expr, expr_type);
  }
  if (kind == "SgPlusPlusOp") {
    SgPlusPlusOp *op = new SgPlusPlusOp(null_expr, expr_type);
    op->set_mode(static_cast<SgUnaryOp::Sgop_mode>(
        properties.intOr("mode", SgUnaryOp::prefix)));
    return op;
  }
  if (kind == "SgMinusMinusOp") {
    SgMinusMinusOp *op = new SgMinusMinusOp(null_expr, expr_type);
    op->set_mode(static_cast<SgUnaryOp::Sgop_mode>(
        properties.intOr("mode", SgUnaryOp::prefix)));
    return op;
  }
  if (kind == "SgMinusOp") {
    return new SgMinusOp(null_expr, expr_type);
  }
  if (kind == "SgUnaryAddOp") {
    return new SgUnaryAddOp(null_expr, expr_type);
  }
  return nullptr;
}

void setOwnedExpressionSourcePosition(SgExpression *expr,
                                      const JsonValue &location) {
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error(
        "AST JSON type-owned expression location is not an object");
  }
  SgLocatedNode *located = isSgLocatedNode(expr);
  if (located == nullptr) {
    throw std::runtime_error("AST JSON type-owned expression is not located");
  }
  const JsonValue *start = location.find("start");
  const JsonValue *end = location.find("end");
  if (start == nullptr || end == nullptr) {
    throw std::runtime_error(
        "AST JSON type-owned expression is missing source location");
  }
  std::unique_ptr<Sg_File_Info> start_info(buildFileInfo(*start, expr));
  std::unique_ptr<Sg_File_Info> end_info(buildFileInfo(*end, expr));
  located->set_startOfConstruct(start_info.release());
  located->set_endOfConstruct(end_info.release());
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);

void restoreOwnedExpressionProperties(SgExpression *expr,
                                      const JsonValue &properties,
                                      const NodeMap &nodes) {
  if (const JsonValue *type = properties.find("type")) {
    SgType *restored_type = typeFromJson(*type, nodes);
    if (SgCastExp *cast = isSgCastExp(expr)) {
      cast->set_type(restored_type);
    } else if (SgTypeExpression *type_expr = isSgTypeExpression(expr)) {
      type_expr->set_type(restored_type);
    } else if (SgAggregateInitializer *init = isSgAggregateInitializer(expr)) {
      init->set_expression_type(restored_type);
    } else if (SgCompoundInitializer *init = isSgCompoundInitializer(expr)) {
      init->set_expression_type(restored_type);
    } else if (SgConstructorInitializer *init =
                   isSgConstructorInitializer(expr)) {
      init->set_expression_type(restored_type);
    }
  }
  expr->set_lvalue(properties.boolOr("lvalue", expr->get_lvalue()));
  expr->set_need_paren(properties.boolOr("need_paren", expr->get_need_paren()));
  expr->set_global_qualified_name(properties.boolOr(
      "global_qualified_name", expr->get_global_qualified_name()));
  restoreExpressionQualificationFields(expr, properties);

  if (SgSubscriptExpression *subscript = isSgSubscriptExpression(expr)) {
    auto restore_child = [&](const std::string &field, auto setter) {
      if (const JsonValue *value = properties.find(field)) {
        SgExpression *child = expressionFromRef(*value, nodes);
        (subscript->*setter)(child);
        if (child != nullptr) {
          child->set_parent(subscript);
        }
      }
    };
    restore_child("lower_bound", &SgSubscriptExpression::set_lowerBound);
    restore_child("upper_bound", &SgSubscriptExpression::set_upperBound);
    restore_child("stride", &SgSubscriptExpression::set_stride);
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    if (const JsonValue *value = properties.find("operand")) {
      SgExpression *operand = expressionFromRef(*value, nodes);
      if (operand == nullptr) {
        throw std::runtime_error(
            "AST JSON type-owned unary expression is missing operand");
      }
      unary->set_operand_i(operand);
      operand->set_parent(unary);
    }
  }
  if (SgBinaryOp *binary = isSgBinaryOp(expr)) {
    auto require_binary_child = [&](const std::string &field) {
      const JsonValue *value = properties.find(field);
      if (value == nullptr) {
        throw std::runtime_error("AST JSON type-owned binary expression is "
                                 "missing " +
                                 field);
      }
      SgExpression *child = expressionFromRef(*value, nodes);
      if (child == nullptr) {
        throw std::runtime_error("AST JSON type-owned binary expression has "
                                 "null " +
                                 field);
      }
      return child;
    };
    SgExpression *lhs = require_binary_child("lhs_operand");
    SgExpression *rhs = require_binary_child("rhs_operand");
    binary->set_lhs_operand_i(lhs);
    lhs->set_parent(binary);
    binary->set_rhs_operand_i(rhs);
    rhs->set_parent(binary);
  }
  if (SgComplexVal *value = isSgComplexVal(expr)) {
    if (const JsonValue *type = properties.find("precision_type")) {
      value->set_precisionType(typeFromJson(*type, nodes));
    }
    if (const JsonValue *real_json = properties.find("real_value")) {
      SgExpression *real = expressionFromRef(*real_json, nodes);
      if (real != nullptr && isSgValueExp(real) == nullptr) {
        throw std::runtime_error(
            "AST JSON SgComplexVal real_value is not a SgValueExp");
      }
      value->set_real_value(isSgValueExp(real));
      if (real != nullptr) {
        real->set_parent(value);
      }
    }
    if (const JsonValue *imag_json = properties.find("imaginary_value")) {
      SgExpression *imag = expressionFromRef(*imag_json, nodes);
      if (imag != nullptr && isSgValueExp(imag) == nullptr) {
        throw std::runtime_error(
            "AST JSON SgComplexVal imaginary_value is not a SgValueExp");
      }
      value->set_imaginary_value(isSgValueExp(imag));
      if (imag != nullptr) {
        imag->set_parent(value);
      }
    }
    value->set_valueString(properties.stringOr("value_string"));
  }

  if (SgVarRefExp *ref = isSgVarRefExp(expr)) {
    if (const JsonValue *symbol_json = properties.find("symbol")) {
      ref->set_symbol(isSgVariableSymbol(symbolFromJson(*symbol_json, nodes)));
    } else {
      const uint64_t decl_id =
          static_cast<uint64_t>(properties.intOr("symbol_declaration", 0));
      if (decl_id == 0) {
        throw std::runtime_error(
            "AST JSON type-owned SgVarRefExp has no symbol declaration");
      }
      SgInitializedName *decl = isSgInitializedName(nodeById(nodes, decl_id));
      if (decl != nullptr) {
        ref->set_symbol(new SgVariableSymbol(decl));
      }
    }
    if (ref->get_symbol() == nullptr) {
      throw std::runtime_error(
          "AST JSON failed to resolve type-owned SgVarRefExp symbol");
    }
    normalizeAnonymousDataMemberReference(ref);
  }
}

SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON expression reference is not an object");
  }
  const JsonValue *node_value = json.find("node");
  if (node_value == nullptr) {
    throw std::runtime_error("AST JSON expression reference is missing node");
  }
  const uint64_t id = static_cast<uint64_t>(json.intOr("node", 0));
  if (id != 0) {
    return nodeByIdAs<SgExpression>(nodes, id);
  }
  if (const JsonValue *owned_kind = json.find("owned_kind")) {
    NodeRecord record;
    record.kind = owned_kind->asString();
    record.variant = 0;
    record.flags = json.at("flags");
    record.location = json.at("location");
    record.properties = json.at("properties");
    record.preprocessing = record.properties.find("preprocessing") != nullptr
                               ? *record.properties.find("preprocessing")
                               : JsonValue::arrayValue({});
    SgNode *node =
        createNodeFromRecord(record, nullptr, JsonValue::objectValue({}));
    SgExpression *expr = isSgExpression(node);
    if (expr == nullptr) {
      throw std::runtime_error(
          "AST JSON type-owned record did not rebuild an expression");
    }
    if (record.flags.boolOr("contains_transformation", false)) {
      expr->set_containsTransformation(true);
    } else {
      expr->set_containsTransformation(false);
    }
    if (SgLocatedNode *located = isSgLocatedNode(expr)) {
      if (record.flags.boolOr(
              "contains_transformation_to_surrounding_whitespace", false)) {
        located->set_containsTransformationToSurroundingWhitespace(true);
      } else {
        located->set_containsTransformationToSurroundingWhitespace(false);
      }
    }
    setOwnedExpressionSourcePosition(expr, record.location);
    restoreOwnedExpressionProperties(expr, record.properties, nodes);
    return expr;
  }
  return nullptr;
}

SgExprListExp *exprListExpFromTypeJson(const JsonValue &json,
                                       const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object || !json.boolOr("present", false)) {
    return nullptr;
  }
  const std::string kind = json.at("kind").asString();
  if (kind != "SgExprListExp") {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info is not SgExprListExp: " + kind);
  }

  NodeRecord record;
  record.kind = kind;
  record.variant = 0;
  record.flags = json.at("flags");
  record.location = json.at("location");
  record.properties = json.at("properties");
  record.preprocessing = record.properties.find("preprocessing") != nullptr
                             ? *record.properties.find("preprocessing")
                             : JsonValue::arrayValue({});

  SgNode *node =
      createNodeFromRecord(record, nullptr, JsonValue::objectValue({}));
  SgExprListExp *list = isSgExprListExp(node);
  if (list == nullptr) {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info did not rebuild an expression list");
  }

  if (record.flags.boolOr("contains_transformation", false)) {
    list->set_containsTransformation(true);
  } else {
    list->set_containsTransformation(false);
  }
  if (record.flags.boolOr("contains_transformation_to_surrounding_whitespace",
                          false)) {
    list->set_containsTransformationToSurroundingWhitespace(true);
  } else {
    list->set_containsTransformationToSurroundingWhitespace(false);
  }
  setOwnedExpressionSourcePosition(list, record.location);
  restoreOwnedExpressionProperties(list, record.properties, nodes);

  const JsonValue &expressions = json.at("expressions");
  if (expressions.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON SgArrayType dim_info expressions field is not an array");
  }
  for (const JsonValue &expr_json : expressions.array) {
    SgExpression *expr = expressionFromRef(expr_json, nodes);
    if (expr == nullptr) {
      throw std::runtime_error(
          "AST JSON SgArrayType dim_info contains a null expression");
    }
    list->append_expression(expr);
  }
  return list;
}

bool sameAstJsonPath(const std::string &lhs, const std::string &rhs) {
  if (lhs.empty() || rhs.empty()) {
    return false;
  }
  if (lhs == rhs) {
    return true;
  }

  const std::filesystem::path lhs_path(lhs);
  const std::filesystem::path rhs_path(rhs);
  if (lhs_path.lexically_normal() == rhs_path.lexically_normal()) {
    return true;
  }

  std::error_code lhs_error;
  std::error_code rhs_error;
  const bool lhs_exists = std::filesystem::exists(lhs_path, lhs_error);
  const bool rhs_exists = std::filesystem::exists(rhs_path, rhs_error);
  if (!lhs_exists || !rhs_exists || lhs_error || rhs_error) {
    return false;
  }

  std::error_code canonical_lhs_error;
  std::error_code canonical_rhs_error;
  const std::filesystem::path canonical_lhs =
      std::filesystem::weakly_canonical(lhs_path, canonical_lhs_error);
  const std::filesystem::path canonical_rhs =
      std::filesystem::weakly_canonical(rhs_path, canonical_rhs_error);
  return !canonical_lhs_error && !canonical_rhs_error &&
         canonical_lhs == canonical_rhs;
}

bool sourceFileMatchesExternalRecord(SgSourceFile *file,
                                     const std::string &source_file) {
  if (file == nullptr || source_file.empty()) {
    return false;
  }
  return sameAstJsonPath(file->getFileName(), source_file) ||
         sameAstJsonPath(file->get_sourceFileNameWithPath(), source_file);
}

bool externalClassCandidateMatches(SgClassDeclaration *candidate,
                                   const JsonValue &json) {
  if (candidate == nullptr) {
    return false;
  }

  const std::string expected_kind = json.at("kind").asString();
  const std::string expected_name = json.at("name").asString();
  const int expected_class_type =
      static_cast<int>(json.at("class_type").asInt());
  const std::string expected_module = json.at("module_name").asString();
  const bool expected_has_definition = json.at("has_definition").asBool();
  const bool expected_is_first_nondefining =
      json.at("is_first_nondefining").asBool();

  return candidate->sage_class_name() == expected_kind &&
         candidate->get_name().getString() == expected_name &&
         static_cast<int>(candidate->get_class_type()) == expected_class_type &&
         moduleNameForNode(candidate) == expected_module &&
         classDeclarationHasDefinition(candidate) == expected_has_definition &&
         classDeclarationIsFirstNondefining(candidate) ==
             expected_is_first_nondefining;
}

const std::vector<SgClassDeclaration *> &
structuralClassDeclarationsForSource(SgSourceFile *source) {
  ROSE_ASSERT(source != nullptr);
  auto found = currentDeserializationClassDeclarationCache.find(source);
  if (found != currentDeserializationClassDeclarationCache.end()) {
    return found->second;
  }

  std::vector<SgClassDeclaration *> declarations;
  std::unordered_set<SgClassDeclaration *> seen;
  auto add_candidate = [&](SgClassDeclaration *decl) {
    if (decl != nullptr && seen.insert(decl).second) {
      declarations.push_back(decl);
    }
  };
  auto add_symbol_basis = [&](SgSymbol *symbol) {
    add_candidate(
        isSgClassDeclaration(const_cast<SgNode *>(symbolBasis(symbol))));
    if (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      add_candidate(isSgClassDeclaration(
          const_cast<SgNode *>(symbolBasis(alias->get_alias()))));
    }
  };

  RoseAst ast(source);
  for (RoseAst::iterator it = ast.begin().withoutNullValues(); it != ast.end();
       ++it) {
    add_candidate(isSgClassDeclaration(*it));
    SgScopeStatement *scope = isSgScopeStatement(*it);
    SgSymbolTable *table =
        scope != nullptr ? scope->get_symbol_table() : nullptr;
    if (table == nullptr || table->get_table() == nullptr) {
      continue;
    }
    for (const std::pair<const SgName, SgSymbol *> &entry :
         *table->get_table()) {
      add_symbol_basis(entry.second);
    }
  }

  auto inserted = currentDeserializationClassDeclarationCache.emplace(
      source, std::move(declarations));
  ROSE_ASSERT(inserted.second);
  return inserted.first->second;
}

void validateExternalClassDeclarationInProject(const JsonValue &json) {
  if (currentDeserializationProject == nullptr) {
    return;
  }

  const std::string source_file = json.at("source_file").asString();
  SgClassDeclaration *matched = nullptr;
  bool saw_source_file = false;
  for (SgFile *file : currentDeserializationProject->get_fileList()) {
    SgSourceFile *source = isSgSourceFile(file);
    if (!sourceFileMatchesExternalRecord(source, source_file)) {
      continue;
    }
    saw_source_file = true;

    for (SgClassDeclaration *candidate :
         structuralClassDeclarationsForSource(source)) {
      if (!externalClassCandidateMatches(candidate, json)) {
        continue;
      }
      if (matched != nullptr) {
        throw std::runtime_error(
            "AST JSON external class declaration is ambiguous in source file " +
            source_file + ": " + json.at("kind").asString() + " " +
            json.at("name").asString());
      }
      matched = candidate;
    }
  }

  if (saw_source_file && matched == nullptr) {
    throw std::runtime_error("AST JSON external class declaration was not "
                             "found in loaded source file " +
                             source_file + ": " + json.at("kind").asString() +
                             " " + json.at("name").asString());
  }
}

void installTransformationSourcePosition(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }

  Sg_File_Info *start =
      Sg_File_Info::generateDefaultFileInfoForTransformationNode();
  Sg_File_Info *end =
      Sg_File_Info::generateDefaultFileInfoForTransformationNode();
  start->set_parent(node);
  end->set_parent(node);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
  node->set_file_info(start);
}

bool nodeSupportsRestoredSourcePosition(SgNode *node) {
  return isSgLocatedNode(node) != nullptr ||
         isSgInitializedName(node) != nullptr || isSgPragma(node) != nullptr ||
         isSgFile(node) != nullptr;
}

void restoreNodeSourcePositionFromJson(SgNode *node, const JsonValue &location,
                                       const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a node with source position");
  }
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON " + context +
                             " location must be an object");
  }
  if (!nodeSupportsRestoredSourcePosition(node)) {
    throw std::runtime_error("AST JSON " + context +
                             " node kind has no restorable source position: " +
                             node->sage_class_name());
  }
  const JsonValue *start = location.find("start");
  if (start == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " location has no start field");
  }
  std::unique_ptr<Sg_File_Info> start_info(buildFileInfo(*start, node));
  if (start_info == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a present startOfConstruct");
  }
  const JsonValue *end = location.find("end");
  std::unique_ptr<Sg_File_Info> end_info(
      end != nullptr ? buildFileInfo(*end, node) : nullptr);
  Sg_File_Info *start_raw = start_info.release();
  Sg_File_Info *end_raw = end_info.release();
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_startOfConstruct(start_raw);
    located->set_endOfConstruct(end_raw);
    located->set_file_info(start_raw);
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_startOfConstruct(start_raw);
    name->set_endOfConstruct(end_raw);
    name->set_file_info(start_raw);
  } else if (SgPragma *pragma = isSgPragma(node)) {
    pragma->set_startOfConstruct(start_raw);
    pragma->set_endOfConstruct(end_raw);
  } else if (SgFile *file = isSgFile(node)) {
    file->set_startOfConstruct(start_raw);
  }
}

void restoreOptionalNodeSourcePositionFromJson(SgNode *node,
                                               const JsonValue &location,
                                               const std::string &context) {
  if (node == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a node with source position");
  }
  if (location.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON " + context +
                             " location must be an object");
  }
  const JsonValue *start = location.find("start");
  const JsonValue *end = location.find("end");
  if ((start != nullptr || end != nullptr) &&
      !nodeSupportsRestoredSourcePosition(node)) {
    throw std::runtime_error("AST JSON " + context +
                             " node kind has no restorable source position: " +
                             node->sage_class_name());
  }
  std::unique_ptr<Sg_File_Info> start_info(
      start != nullptr ? buildFileInfo(*start, node) : nullptr);
  std::unique_ptr<Sg_File_Info> end_info(
      end != nullptr ? buildFileInfo(*end, node) : nullptr);
  Sg_File_Info *start_raw = start_info.release();
  Sg_File_Info *end_raw = end_info.release();
  if (SgLocatedNode *located = isSgLocatedNode(node)) {
    located->set_startOfConstruct(start_raw);
    located->set_endOfConstruct(end_raw);
    located->set_file_info(start_raw);
  } else if (SgInitializedName *name = isSgInitializedName(node)) {
    name->set_startOfConstruct(start_raw);
    name->set_endOfConstruct(end_raw);
    name->set_file_info(start_raw);
  } else if (SgPragma *pragma = isSgPragma(node)) {
    pragma->set_startOfConstruct(start_raw);
    pragma->set_endOfConstruct(end_raw);
  } else if (SgFile *file = isSgFile(node)) {
    file->set_startOfConstruct(start_raw);
  }
}

SgBitVector bitVectorFromJson(const JsonValue &json,
                              const std::string &field_name);
SgModuleStatement *externalModuleFromJson(const JsonValue &json);
SgSymbol *createSymbolForKindAndBasis(const std::string &kind, SgNode *basis);
SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);
void restoreLabelSymbolFields(SgLabelSymbol *symbol, const JsonValue &json);
void attachExternalSymbolBasisToScope(SgSymbol *symbol,
                                      SgScopeStatement *scope);

void restoreExternalDeclarationStatementFields(SgDeclarationStatement *decl,
                                               const JsonValue &json,
                                               const std::string &context) {
  if (decl == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " requires a declaration statement");
  }
  decl->set_decl_attributes(
      static_cast<unsigned int>(json.at("decl_attributes").asInt()));
  decl->set_linkage(json.at("linkage").asString());
  decl->get_declarationModifier().set_modifierVector(
      bitVectorFromJson(json.at("declaration_modifier_vector"),
                        context + " declaration_modifier_vector"));
  decl->get_declarationModifier().get_typeModifier().set_modifierVector(
      bitVectorFromJson(json.at("declaration_type_modifier_vector"),
                        context + " declaration_type_modifier_vector"));
  decl->get_declarationModifier().get_storageModifier().set_modifier(
      static_cast<SgStorageModifier::storage_modifier_enum>(
          json.at("declaration_storage_modifier").asInt()));
  decl->get_declarationModifier().get_accessModifier().set_modifier(
      static_cast<SgAccessModifier::access_modifier_enum>(
          json.at("declaration_access_modifier").asInt()));
  decl->get_declarationModifier().get_accessModifier().set_is_explicit(
      json.boolOr("declaration_access_is_explicit", false));
  decl->set_nameOnly(json.at("name_only").asBool());
  decl->set_forward(json.at("forward").asBool());
  decl->set_externBrace(json.at("extern_brace").asBool());
  decl->set_skipElaborateType(json.at("skip_elaborate_type").asBool());
  decl->set_binding_label(json.at("binding_label").asString());
  decl->set_unparse_template_ast(json.at("unparse_template_ast").asBool());
  decl->get_declarationModifier().set_gnu_attribute_section_name(
      json.at("declaration_gnu_attribute_section_name").asString());
  decl->get_declarationModifier().set_gnu_attribute_visability(
      static_cast<SgDeclarationModifier::gnu_declaration_visability_enum>(
          json.at("declaration_gnu_attribute_visability").asInt()));

  const bool first_is_self = json.at("first_nondefining_is_self").asBool();
  const bool first_is_null = json.at("first_nondefining_is_null").asBool();
  const bool defining_is_self =
      json.at("defining_declaration_is_self").asBool();
  const bool defining_is_null =
      json.at("defining_declaration_is_null").asBool();
  if (first_is_self == first_is_null) {
    throw std::runtime_error(
        "AST JSON " + context +
        " first-nondefining declaration state is not exclusive");
  }
  if (defining_is_self == defining_is_null) {
    throw std::runtime_error("AST JSON " + context +
                             " defining declaration state is not exclusive");
  }
  decl->set_firstNondefiningDeclaration(first_is_self ? decl : nullptr);
  decl->set_definingDeclaration(defining_is_self ? decl : nullptr);
}

SgInitializedName *externalInitializedNameFromJson(const JsonValue &json,
                                                   const NodeMap &nodes,
                                                   SgVariableDeclaration *decl,
                                                   SgScopeStatement *scope,
                                                   const std::string &context) {
  const std::string name = json.at("name").asString();
  SgType *type = typeFromJson(json.at("type"), nodes);
  SgInitializedName *initialized_name = new SgInitializedName(
      nullptr, SgName(name), type, nullptr, nullptr, nullptr, nullptr);
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON " + context +
                             " initializedName has no location");
  }
  restoreNodeSourcePositionFromJson(initialized_name, *location,
                                    context + " initializedName " + name);
  initialized_name->set_parent(decl);
  initialized_name->set_declptr(decl);
  initialized_name->set_scope(scope);
  initialized_name->get_storageModifier().set_modifier(
      static_cast<SgStorageModifier::storage_modifier_enum>(
          json.intOr("storage_modifier", SgStorageModifier::e_default)));
  const std::string section = json.stringOr("gnu_attribute_section_name");
  if (!section.empty()) {
    initialized_name->set_gnu_attribute_section_name(section);
  }
  initialized_name->set_name_qualification_length(
      static_cast<int>(json.intOr("name_qualification_length", 0)));
  initialized_name->set_type_elaboration_required(
      json.boolOr("type_elaboration_required", false));
  initialized_name->set_global_qualification_required(
      json.boolOr("global_qualification_required", false));
  initialized_name->set_name_qualification_length_for_type(
      static_cast<int>(json.intOr("name_qualification_length_for_type", 0)));
  initialized_name->set_type_elaboration_required_for_type(
      json.boolOr("type_elaboration_required_for_type", false));
  initialized_name->set_global_qualification_required_for_type(
      json.boolOr("global_qualification_required_for_type", false));
  return initialized_name;
}

SgVariableDeclaration *
externalVariableDeclarationFromJson(const JsonValue &json, const NodeMap &nodes,
                                    SgFunctionParameterScope *scope,
                                    const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind != "SgVariableDeclaration") {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " functionParameterScope declaration kind is unsupported: " + kind);
  }
  SgVariableDeclaration *decl = new SgVariableDeclaration();
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " variable declaration has no location");
  }
  restoreNodeSourcePositionFromJson(decl, *location,
                                    "external_function variableDeclaration " +
                                        function_name);
  decl->set_parent(scope);
  decl->set_scope(scope);
  restoreExternalDeclarationStatementFields(
      decl, json,
      "external_function " + function_name + " variableDeclaration");
  decl->set_requiresGlobalNameQualificationOnType(
      json.boolOr("requires_global_name_qualification_on_type", false));
  decl->set_name_qualification_length(
      static_cast<int>(json.intOr("name_qualification_length", 0)));
  decl->set_type_elaboration_required(
      json.boolOr("type_elaboration_required", false));
  decl->set_global_qualification_required(
      json.boolOr("global_qualification_required", false));

  const JsonValue &variables = json.at("variables");
  if (variables.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " variable declaration variables is not an array");
  }
  for (const JsonValue &variable : variables.array) {
    SgInitializedName *initialized_name = externalInitializedNameFromJson(
        variable, nodes, decl, scope,
        "external_function " + function_name + " variableDeclaration");
    decl->get_variables().push_back(initialized_name);
  }
  return decl;
}

SgRenamePair *externalRenamePairFromJson(const JsonValue &json,
                                         SgUseStatement *parent,
                                         const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind != "SgRenamePair") {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename_list entry has "
                             "unsupported kind: " +
                             kind);
  }
  SgRenamePair *rename =
      new SgRenamePair(SgName(json.at("local_name").asString()),
                       SgName(json.at("use_name").asString()));
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename pair has no location");
  }
  restoreOptionalNodeSourcePositionFromJson(
      rename, *location,
      "external_function " + function_name + " SgUseStatement rename pair");
  rename->set_parent(parent);
  return rename;
}

SgUseStatement *externalUseStatementFromJson(const JsonValue &json,
                                             const NodeMap &nodes,
                                             SgFunctionParameterScope *scope,
                                             const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind != "SgUseStatement") {
    throw std::runtime_error(
        "AST JSON external_function " + function_name +
        " functionParameterScope declaration kind is unsupported: " + kind);
  }
  SgUseStatement *stmt = new SgUseStatement(SgName(json.at("name").asString()),
                                            json.boolOr("only_option", false),
                                            json.stringOr("module_nature"));
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " use statement has no location");
  }
  restoreNodeSourcePositionFromJson(
      stmt, *location, "external_function useStatement " + function_name);
  stmt->set_parent(scope);
  stmt->set_scope(scope);
  restoreExternalDeclarationStatementFields(
      stmt, json, "external_function " + function_name + " useStatement");

  const uint64_t module_id = static_cast<uint64_t>(json.intOr("module", 0));
  if (module_id != 0) {
    stmt->set_module(nodeByIdAs<SgModuleStatement>(nodes, module_id));
  } else if (const JsonValue *external_module = json.find("external_module")) {
    stmt->set_module(externalModuleFromJson(*external_module));
  }

  const JsonValue &rename_list = json.at("rename_list");
  if (rename_list.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " SgUseStatement rename_list is not an array");
  }
  for (const JsonValue &rename_json : rename_list.array) {
    stmt->get_rename_list().push_back(
        externalRenamePairFromJson(rename_json, stmt, function_name));
  }
  return stmt;
}

SgDeclarationStatement *externalParameterScopeDeclarationFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const std::string &function_name) {
  const std::string kind = json.at("kind").asString();
  if (kind == "SgVariableDeclaration") {
    return externalVariableDeclarationFromJson(json, nodes, scope,
                                               function_name);
  }
  if (kind == "SgUseStatement") {
    return externalUseStatementFromJson(json, nodes, scope, function_name);
  }
  throw std::runtime_error(
      "AST JSON external_function " + function_name +
      " functionParameterScope declaration kind is unsupported: " + kind);
}

SgFunctionParameterScope *
externalFunctionParameterScopeFromJson(const JsonValue &json,
                                       const NodeMap &nodes,
                                       const std::string &function_name) {
  if (!json.boolOr("present", false)) {
    return nullptr;
  }
  SgFunctionParameterScope *scope = new SgFunctionParameterScope();
  const JsonValue *location = json.find("location");
  if (location == nullptr) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " functionParameterScope has no location");
  }
  restoreNodeSourcePositionFromJson(
      scope, *location,
      "external_function functionParameterScope " + function_name);

  std::vector<std::vector<SgInitializedName *>> variables_by_declaration;
  std::vector<SgDeclarationStatement *> declarations_by_index;
  const JsonValue &declarations = json.at("declarations");
  if (declarations.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON external_function " + function_name +
                             " functionParameterScope declarations is not an "
                             "array");
  }
  for (const JsonValue &declaration : declarations.array) {
    SgDeclarationStatement *decl = externalParameterScopeDeclarationFromJson(
        declaration, nodes, scope, function_name);
    scope->append_declaration(decl);
    declarations_by_index.push_back(decl);
    std::vector<SgInitializedName *> names;
    if (SgVariableDeclaration *variable = isSgVariableDeclaration(decl)) {
      for (SgInitializedName *name : variable->get_variables()) {
        names.push_back(name);
      }
    }
    variables_by_declaration.push_back(names);
  }

  if (json.boolOr("symbol_table_present", false)) {
    SgSymbolTable *table = new SgSymbolTable(17);
    table->set_parent(scope);
    table->setCaseInsensitive(
        json.boolOr("symbol_table_case_insensitive", false));
    scope->set_symbol_table(table);
    const JsonValue *symbol_table = json.find("symbol_table");
    if (symbol_table == nullptr ||
        symbol_table->kind != JsonValue::Kind::Array) {
      throw std::runtime_error(
          "AST JSON external_function " + function_name +
          " functionParameterScope symbol_table is missing or not an array");
    }
    if (static_cast<int64_t>(symbol_table->array.size()) !=
        json.intOr("symbol_table_size", 0)) {
      throw std::runtime_error(
          "AST JSON external_function " + function_name +
          " functionParameterScope symbol_table size does not match metadata");
    }
    std::vector<const JsonValue *> insertion_entries;
    insertion_entries.reserve(symbol_table->array.size());
    for (const JsonValue &entry : symbol_table->array) {
      insertion_entries.push_back(&entry);
    }
    for (const JsonValue *entry_ptr : insertion_entries) {
      const JsonValue &entry = *entry_ptr;
      const std::string symbol_kind = entry.at("symbol_kind").asString();
      const SgName entry_name(entry.at("entry_name").asString());
      if (const JsonValue *declaration_index_value =
              entry.find("declaration_index")) {
        const size_t declaration_index =
            static_cast<size_t>(declaration_index_value->asInt());
        const size_t variable_index =
            static_cast<size_t>(entry.at("variable_index").asInt());
        if (declaration_index >= variables_by_declaration.size() ||
            variable_index >=
                variables_by_declaration[declaration_index].size()) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope symbol_table indexes are out of range");
        }
        if (symbol_kind != "SgVariableSymbol") {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope indexed symbol kind is unsupported: " +
              symbol_kind);
        }
        SgInitializedName *declaration =
            variables_by_declaration[declaration_index][variable_index];
        table->insert(entry_name, new SgVariableSymbol(declaration));
        continue;
      }
      const JsonValue *symbol_json = entry.find("symbol");
      if (symbol_json == nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol_table entry has neither nested "
            "variable indexes nor a structured symbol reference: " +
            entry_name.getString());
      }
      const std::string referenced_kind =
          symbol_json->at("symbol_kind").asString();
      if (referenced_kind != symbol_kind) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope symbol_table kind mismatch for " +
            entry_name.getString() + ": entry=" + symbol_kind +
            " symbol=" + referenced_kind);
      }
      if (symbol_kind == "SgAliasSymbol") {
        const JsonValue *target = entry.find("alias_target");
        if (target == nullptr) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope SgAliasSymbol has no alias_target: " +
              entry_name.getString());
        }
        SgSymbol *target_symbol = symbolFromJson(*target, nodes);
        if (target_symbol == nullptr ||
            target_symbol->get_symbol_basis() == nullptr) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope cannot reconstruct alias target for " +
              entry_name.getString());
        }
        SgAliasSymbol *alias = new SgAliasSymbol(
            target_symbol, entry.boolOr("alias_is_renamed", false),
            SgName(entry.stringOr("alias_new_name")));
        if (const JsonValue *causal_nodes = entry.find("alias_causal_nodes")) {
          if (causal_nodes->kind != JsonValue::Kind::Array) {
            throw std::runtime_error(
                "AST JSON external_function " + function_name +
                " functionParameterScope alias_causal_nodes is not an array");
          }
          for (const JsonValue &causal_node : causal_nodes->array) {
            if (causal_node.kind != JsonValue::Kind::Object) {
              throw std::runtime_error(
                  "AST JSON external_function " + function_name +
                  " functionParameterScope alias causal node is not an object");
            }
            const uint64_t node_id =
                static_cast<uint64_t>(causal_node.intOr("node", 0));
            const int64_t declaration_index =
                causal_node.intOr("declaration_index", -1);
            if (node_id != 0) {
              alias->get_causal_nodes().push_back(nodeById(nodes, node_id));
            } else if (declaration_index >= 0 &&
                       static_cast<size_t>(declaration_index) <
                           declarations_by_index.size()) {
              alias->get_causal_nodes().push_back(
                  declarations_by_index[static_cast<size_t>(
                      declaration_index)]);
            } else if (declaration_index >= 0) {
              throw std::runtime_error(
                  "AST JSON external_function " + function_name +
                  " functionParameterScope alias causal declaration index is "
                  "out of range");
            }
          }
        }
        attachExternalSymbolBasisToScope(alias, scope);
        table->insert(entry_name, alias);
        continue;
      }
      if (symbol_kind == "SgRenameSymbol") {
        const JsonValue *original = entry.find("original_symbol");
        if (original == nullptr) {
          throw std::runtime_error("AST JSON external_function " +
                                   function_name +
                                   " functionParameterScope SgRenameSymbol has "
                                   "no original_symbol: " +
                                   entry_name.getString());
        }
        const uint64_t basis_id = static_cast<uint64_t>(
            symbol_json->at("symbol_declaration").asInt());
        if (basis_id == 0) {
          throw std::runtime_error(
              "AST JSON external_function " + function_name +
              " functionParameterScope SgRenameSymbol has no collected "
              "function declaration basis: " +
              entry_name.getString());
        }
        SgRenameSymbol *rename = new SgRenameSymbol(
            nodeByIdAs<SgFunctionDeclaration>(nodes, basis_id),
            symbolFromJson(*original, nodes),
            SgName(entry.stringOr("rename_new_name")));
        table->insert(entry_name, rename);
        continue;
      }
      const uint64_t basis_id =
          static_cast<uint64_t>(symbol_json->at("symbol_declaration").asInt());
      SgSymbol *symbol = nullptr;
      if (basis_id == 0) {
        symbol = createExternalSymbolFromJson(*symbol_json, nodes);
      } else {
        SgNode *basis = nodeById(nodes, basis_id);
        symbol = createSymbolForKindAndBasis(symbol_kind, basis);
        if (SgLabelSymbol *label_symbol = isSgLabelSymbol(symbol)) {
          restoreLabelSymbolFields(label_symbol, *symbol_json);
        }
      }
      if (symbol == nullptr || symbol->get_symbol_basis() == nullptr) {
        throw std::runtime_error(
            "AST JSON external_function " + function_name +
            " functionParameterScope cannot reconstruct symbol_table entry " +
            entry_name.getString() + " kind=" + symbol_kind);
      }
      attachExternalSymbolBasisToScope(symbol, scope);
      table->insert(entry_name, symbol);
    }
  }
  return scope;
}

SgClassDeclaration *newClassDeclarationForExternalRecord(
    const std::string &kind, const std::string &name,
    SgClassDeclaration::class_types class_type) {
  if (kind == "SgDerivedTypeStatement") {
    return new SgDerivedTypeStatement(SgName(name), class_type, nullptr,
                                      nullptr);
  }
  if (kind == "SgModuleStatement") {
    return new SgModuleStatement(SgName(name), class_type, nullptr, nullptr);
  }
  if (kind == "SgClassDeclaration") {
    return new SgClassDeclaration(SgName(name), class_type, nullptr, nullptr);
  }
  throw std::runtime_error(
      "AST JSON external class declaration has unsupported declaration kind: " +
      kind);
}

SgClassDeclaration *externalClassDeclarationFromJson(const JsonValue &json) {
  if (!json.boolOr("present", false)) {
    return nullptr;
  }

  const std::string kind = json.at("kind").asString();
  const std::string name = json.at("name").asString();
  const std::string source_file = json.at("source_file").asString();
  if (kind.empty() || name.empty() || source_file.empty()) {
    throw std::runtime_error("AST JSON external class declaration requires "
                             "kind, name, and source_file");
  }
  validateExternalClassDeclarationInProject(json);

  const auto class_type = static_cast<SgClassDeclaration::class_types>(
      json.at("class_type").asInt());
  const std::string module_name = json.at("module_name").asString();
  const bool has_definition = json.at("has_definition").asBool();
  const bool is_first_nondefining = json.at("is_first_nondefining").asBool();

  SgModuleStatement *module = nullptr;
  SgClassDefinition *module_definition = nullptr;
  if (!module_name.empty() && kind != "SgModuleStatement") {
    module = new SgModuleStatement(SgName(module_name),
                                   SgClassDeclaration::e_fortran_module,
                                   nullptr, nullptr);
    installTransformationSourcePosition(module);
    module_definition = new SgClassDefinition();
    installTransformationSourcePosition(module_definition);
    module->set_definition(module_definition);
    module_definition->set_declaration(module);
    module_definition->set_parent(module);
    module->set_firstNondefiningDeclaration(module);
    module->set_definingDeclaration(module);
    module->set_type(new SgClassType(module));
    markAstJsonExternalModule(module, source_file);
  }

  SgClassDeclaration *decl =
      newClassDeclarationForExternalRecord(kind, name, class_type);
  installTransformationSourcePosition(decl);
  decl->set_parent(module_definition);
  decl->set_scope(module_definition);
  markAstJsonExternalClassDeclaration(decl, source_file);

  SgClassDeclaration *first_nondefining = decl;
  SgClassDeclaration *defining = has_definition ? decl : nullptr;
  if (has_definition) {
    SgClassDefinition *definition = new SgClassDefinition();
    installTransformationSourcePosition(definition);
    definition->set_declaration(decl);
    definition->set_parent(decl);
    decl->set_definition(definition);
    if (!is_first_nondefining) {
      first_nondefining =
          newClassDeclarationForExternalRecord(kind, name, class_type);
      installTransformationSourcePosition(first_nondefining);
      first_nondefining->set_parent(module_definition);
      first_nondefining->set_scope(module_definition);
      markAstJsonExternalClassDeclaration(first_nondefining, source_file);
      first_nondefining->set_firstNondefiningDeclaration(first_nondefining);
      first_nondefining->set_definingDeclaration(decl);
    }
  }

  decl->set_firstNondefiningDeclaration(first_nondefining);
  decl->set_definingDeclaration(defining);
  decl->set_type(new SgClassType(decl));
  if (first_nondefining != decl) {
    first_nondefining->set_type(new SgClassType(first_nondefining));
  }

  if (module_definition != nullptr) {
    module_definition->get_members().push_back(first_nondefining);
    if (decl != first_nondefining) {
      module_definition->get_members().push_back(decl);
    }
  }

  return decl;
}

SgType *typeFromJson(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object) {
    throw std::runtime_error("AST JSON type record is not an object");
  }
  if (!json.boolOr("present", false)) {
    throw std::runtime_error("AST JSON required type record is absent");
  }

  const std::string kind = json.stringOr("kind");
  if (kind == "SgTypeDefault") {
    return attachJsonTypeText(SgTypeDefault::createType(), json);
  }
  if (kind == "SgTypeUnknown") {
    return attachJsonTypeText(SageBuilder::buildUnknownType(), json);
  }
  if (kind == "SgTypeEllipse") {
    return attachJsonTypeText(SgTypeEllipse::createType(), json);
  }
  if (kind == "SgTypeNullptr") {
    return attachJsonTypeText(SageBuilder::buildNullptrType(), json);
  }
  if (kind == "SgTypeLabel") {
    return attachJsonTypeText(
        SgTypeLabel::createType(SgName(json.stringOr("name"))), json);
  }
  if (kind == "SgTypeFloat128") {
    return attachJsonTypeText(SageBuilder::buildFloat128Type(), json);
  }
  if (kind == "SgTypeSigned128bitInteger") {
    return attachJsonTypeText(SageBuilder::buildSigned128bitIntegerType(),
                              json);
  }
  if (kind == "SgTypeUnsigned128bitInteger") {
    return attachJsonTypeText(SageBuilder::buildUnsigned128bitIntegerType(),
                              json);
  }
  if (SgType *primitive = primitiveTypeFromKind(kind)) {
    return attachJsonTypeText(primitive, json);
  }
  if (kind == "SgTypeString") {
    SgExpression *length = nullptr;
    if (const JsonValue *length_json = json.find("length_expression")) {
      length = expressionFromRef(*length_json, nodes);
    }
    SgExpression *type_kind = nullptr;
    if (const JsonValue *kind_json = json.find("type_kind")) {
      type_kind = expressionFromRef(*kind_json, nodes);
    }
    SgTypeString *string_type = new SgTypeString(length);
    string_type->set_type_kind(type_kind);
    const SgName mangled = string_type->get_mangled();

    SgTypeTable *type_table = SgNode::get_globalTypeTable();
    ROSE_ASSERT(type_table != nullptr);
    if (type_table->lookup_type(mangled) != nullptr) {
      type_table->remove_type(mangled);
    }

    if (length != nullptr) {
      length->set_parent(string_type);
    }
    if (type_kind != nullptr) {
      type_kind->set_parent(string_type);
    }
    type_table->insert_type(mangled, string_type);
    return attachJsonTypeText(string_type, json);
  }
  if (kind == "SgTypeComplex") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    SgExpression *type_kind = nullptr;
    if (const JsonValue *kind_json = json.find("type_kind")) {
      type_kind = expressionFromRef(*kind_json, nodes);
    }
    SgTypeComplex *complex_type = SgTypeComplex::createType(base, type_kind);
    if (type_kind != nullptr) {
      type_kind->set_parent(complex_type);
    }
    return attachJsonTypeText(complex_type, json);
  }
  if (kind == "SgPointerType") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    return attachJsonTypeText(buildCachedJsonPointerType(base), json);
  }
  if (kind == "SgPointerMemberType") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    SgType *class_type = typeFromJson(json.at("class_type"), nodes);
    return attachJsonTypeText(SgPointerMemberType::createType(base, class_type),
                              json);
  }
  if (kind == "SgReferenceType") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    SgReferenceType *reference = new SgReferenceType(base);
    installReferenceCache(base, reference);
    return attachJsonTypeText(reference, json);
  }
  if (kind == "SgRvalueReferenceType") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    SgRvalueReferenceType *reference = new SgRvalueReferenceType(base);
    installRvalueReferenceCache(base, reference);
    return attachJsonTypeText(reference, json);
  }
  if (kind == "SgArrayType") {
    SgExpression *index = nullptr;
    if (const JsonValue *index_json = json.find("index")) {
      index = expressionFromRef(*index_json, nodes);
    }
    SgArrayType *array =
        new SgArrayType(typeFromJson(json.at("base"), nodes), index);
    if (index != nullptr && array->get_index() != index) {
      array->set_index(index);
    }
    if (index != nullptr) {
      index->set_parent(array);
    }
    if (const JsonValue *dim_info = json.find("dim_info")) {
      SgExprListExp *dimensions = exprListExpFromTypeJson(*dim_info, nodes);
      array->set_dim_info(dimensions);
      if (dimensions != nullptr) {
        dimensions->set_parent(array);
      }
    }
    array->set_rank(static_cast<int>(json.intOr("rank", array->get_rank())));
    array->set_number_of_elements(static_cast<int>(
        json.intOr("number_of_elements", array->get_number_of_elements())));
    array->set_isCoArray(json.boolOr("is_coarray", array->get_isCoArray()));
    array->set_is_variable_length_array(json.boolOr(
        "is_variable_length_array", array->get_is_variable_length_array()));
    const SgName mangled = array->get_mangled();
    SgTypeTable *type_table = SgNode::get_globalTypeTable();
    ROSE_ASSERT(type_table != nullptr);
    if (type_table->lookup_type(mangled) != nullptr) {
      type_table->remove_type(mangled);
    }
    type_table->insert_type(mangled, array);
    return attachJsonTypeText(array, json);
  }
  if (kind == "SgModifierType") {
    SgType *base = typeFromJson(json.at("base"), nodes);
    SgModifierType *modifier = new SgModifierType(base);
    SgTypeModifier &type_modifier = modifier->get_typeModifier();
    SgConstVolatileModifier &cv = type_modifier.get_constVolatileModifier();
    if (json.boolOr("modifier_const", false)) {
      cv.setConst();
    }
    if (json.boolOr("modifier_volatile", false)) {
      cv.setVolatile();
    }
    if (json.boolOr("modifier_restrict", false)) {
      type_modifier.setRestrict();
    }
    return attachJsonTypeText(modifier, json);
  }
  if (kind == "SgTypedefType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.intOr("declaration", 0));
    if (decl_id != 0) {
      if (SgTypedefDeclaration *decl =
              isSgTypedefDeclaration(nodeById(nodes, decl_id))) {
        if (decl->get_type() == nullptr) {
          decl->set_type(new SgTypedefType(decl, nullptr));
        }
        decl->get_type()->set_autonomous_declaration(
            json.boolOr("autonomous_declaration",
                        decl->get_type()->get_autonomous_declaration()));
        return attachJsonTypeText(decl->get_type(), json);
      }
    }
    throw std::runtime_error(
        "AST JSON SgTypedefType requires a valid declaration id");
  }
  if (kind == "SgClassType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.intOr("declaration", 0));
    if (decl_id != 0) {
      if (SgClassDeclaration *decl =
              isSgClassDeclaration(nodeById(nodes, decl_id))) {
        SgClassType *class_type = ensureClassTypeForDeclaration(decl);
        class_type->set_autonomous_declaration(
            json.boolOr("autonomous_declaration",
                        class_type->get_autonomous_declaration()));
        return attachJsonTypeText(class_type, json);
      }
    }
    if (const JsonValue *external_declaration =
            json.find("external_declaration")) {
      SgClassDeclaration *decl =
          externalClassDeclarationFromJson(*external_declaration);
      SgClassType *class_type = ensureClassTypeForDeclaration(decl);
      class_type->set_autonomous_declaration(json.boolOr(
          "autonomous_declaration", class_type->get_autonomous_declaration()));
      return attachJsonTypeText(class_type, json);
    }
    throw std::runtime_error(
        "AST JSON SgClassType requires a declaration id or an "
        "external_declaration record");
  }
  if (kind == "SgEnumType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.intOr("declaration", 0));
    if (decl_id != 0) {
      if (SgEnumDeclaration *decl =
              isSgEnumDeclaration(nodeById(nodes, decl_id))) {
        if (decl->get_type() == nullptr) {
          if (decl->get_scope() == nullptr) {
            decl->set_scope(nearestScope(decl));
          }
          decl->set_type(SgEnumType::createType(decl));
        }
        decl->get_type()->set_autonomous_declaration(
            json.boolOr("autonomous_declaration",
                        decl->get_type()->get_autonomous_declaration()));
        return attachJsonTypeText(decl->get_type(), json);
      }
    }
    throw std::runtime_error(
        "AST JSON SgEnumType requires a valid declaration id");
  }
  if (kind == "SgNonrealType") {
    const uint64_t decl_id =
        static_cast<uint64_t>(json.intOr("declaration", 0));
    if (decl_id != 0) {
      if (SgNonrealDecl *decl = isSgNonrealDecl(nodeById(nodes, decl_id))) {
        if (decl->get_type() == nullptr) {
          decl->set_type(new SgNonrealType(decl));
        }
        decl->get_type()->set_autonomous_declaration(
            json.boolOr("autonomous_declaration",
                        decl->get_type()->get_autonomous_declaration()));
        return attachJsonTypeText(decl->get_type(), json);
      }
    }
    throw std::runtime_error(
        "AST JSON SgNonrealType requires a valid declaration id");
  }
  if (kind == "SgDeclType") {
    SgExpression *base_expression = nullptr;
    if (const JsonValue *base_json = json.find("base_expression")) {
      base_expression = expressionFromRef(*base_json, nodes);
    }
    return buildDeclType(base_expression, json);
  }
  if (kind == "SgTemplateType") {
    SgTemplateType *template_type = new SgTemplateType(
        SgName(json.stringOr("name", json.stringOr("text"))));
    template_type->set_template_parameter_position(
        static_cast<int>(json.intOr("template_parameter_position", -1)));
    template_type->set_template_parameter_depth(
        static_cast<int>(json.intOr("template_parameter_depth", -1)));
    if (const JsonValue *class_json = json.find("class_type")) {
      template_type->set_class_type(class_json->boolOr("present", false)
                                        ? typeFromJson(*class_json, nodes)
                                        : nullptr);
    } else {
      template_type->set_class_type(nullptr);
    }
    if (const JsonValue *parent_class_json = json.find("parent_class_type")) {
      template_type->set_parent_class_type(
          parent_class_json->boolOr("present", false)
              ? typeFromJson(*parent_class_json, nodes)
              : nullptr);
    } else {
      template_type->set_parent_class_type(nullptr);
    }
    return attachJsonTypeText(template_type, json);
  }
  if (kind == "SgMemberFunctionType") {
    SgType *return_type = SageBuilder::buildIntType();
    if (const JsonValue *return_json = json.find("return_type")) {
      return_type = typeFromJson(*return_json, nodes);
    }
    SgType *class_type = nullptr;
    if (const JsonValue *class_json = json.find("class_type")) {
      class_type = typeFromJson(*class_json, nodes);
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    if (const JsonValue *argument_json = json.find("arguments")) {
      if (argument_json->kind != JsonValue::Kind::Array) {
        throw std::runtime_error(
            "AST JSON member function type arguments field is not an array");
      }
      for (const JsonValue &argument : argument_json->array) {
        arguments->append_argument(typeFromJson(argument, nodes));
      }
    }
    SgMemberFunctionType *member_type = new SgMemberFunctionType(
        return_type, json.boolOr("has_ellipses", false), class_type,
        static_cast<unsigned int>(json.intOr("mfunc_specifier", 0)));
    member_type->set_argument_list(arguments);
    arguments->set_parent(member_type);
    return attachJsonTypeText(member_type, json);
  }
  if (kind == "SgFunctionType") {
    SgType *return_type = SageBuilder::buildIntType();
    if (const JsonValue *return_json = json.find("return_type")) {
      return_type = typeFromJson(*return_json, nodes);
    }
    SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
    if (const JsonValue *argument_json = json.find("arguments")) {
      if (argument_json->kind != JsonValue::Kind::Array) {
        throw std::runtime_error(
            "AST JSON function type arguments field is not an array");
      }
      for (const JsonValue &argument : argument_json->array) {
        arguments->append_argument(typeFromJson(argument, nodes));
      }
    }
    SgFunctionType *function_type =
        new SgFunctionType(return_type, json.boolOr("has_ellipses", false));
    function_type->set_argument_list(arguments);
    arguments->set_parent(function_type);
    return attachJsonTypeText(function_type, json);
  }
  throw std::runtime_error("AST JSON deserializer does not support Sage type " +
                           kind);
}

SgType *nullableTypeFromJson(const JsonValue &json, const NodeMap &nodes) {
  if (json.kind != JsonValue::Kind::Object || !json.boolOr("present", false)) {
    return nullptr;
  }
  return typeFromJson(json, nodes);
}

SgFile::languageOption_enum
fileLanguageFromJson(const JsonValue &properties, const std::string &key,
                     SgFile::languageOption_enum fallback) {
  const int64_t value = properties.intOr(key, static_cast<int64_t>(fallback));
  if (value < static_cast<int64_t>(SgFile::e_error_language) ||
      value >= static_cast<int64_t>(SgFile::e_last_language)) {
    throw std::runtime_error(
        "AST JSON SgSourceFile language enum is invalid: " + key);
  }
  return static_cast<SgFile::languageOption_enum>(value);
}

SgFile::outputFormatOption_enum
fileOutputFormatFromJson(const JsonValue &properties, const std::string &key,
                         SgFile::outputFormatOption_enum fallback) {
  const int64_t value = properties.intOr(key, static_cast<int64_t>(fallback));
  if (value < static_cast<int64_t>(SgFile::e_unknown_output_format) ||
      value > static_cast<int64_t>(SgFile::e_free_form_output_format)) {
    throw std::runtime_error(
        "AST JSON SgSourceFile output-format enum is invalid: " + key);
  }
  return static_cast<SgFile::outputFormatOption_enum>(value);
}

} // namespace AstJson
} // namespace Rose
