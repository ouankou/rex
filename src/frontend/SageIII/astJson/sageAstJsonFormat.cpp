#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

const char *const kFormat = "rex-sage-ast-json";
const int kSchemaVersion = 251;

namespace {

struct JsonKeyVocabulary {
  std::unordered_map<std::string, std::unique_ptr<JsonKey::Spelling>> spellings;
  std::vector<JsonKey::Spelling *> ordered;
};

JsonKeyVocabulary &jsonKeyVocabulary() {
  // Spellings are never released: members anywhere may refer to them.
  static auto *vocabulary = new JsonKeyVocabulary();
  return *vocabulary;
}

} // namespace

JsonKey::JsonKey(const std::string &text) {
  JsonKeyVocabulary &vocabulary = jsonKeyVocabulary();
  const auto found = vocabulary.spellings.find(text);
  if (found != vocabulary.spellings.end()) {
    spelling_ = found->second.get();
    return;
  }
  auto spelling = std::make_unique<Spelling>(Spelling{text, 0});
  Spelling *added = spelling.get();
  vocabulary.spellings.emplace(text, std::move(spelling));
  auto position =
      std::lower_bound(vocabulary.ordered.begin(), vocabulary.ordered.end(),
                       added, [](const Spelling *lhs, const Spelling *rhs) {
                         return lhs->text < rhs->text;
                       });
  position = vocabulary.ordered.insert(position, added);
  for (; position != vocabulary.ordered.end(); ++position)
    (*position)->rank = position - vocabulary.ordered.begin();
  spelling_ = added;
}

void appendCanonicalJson(std::ostream &out, const JsonValue &value) {
  switch (value.kind) {
  case JsonValue::Kind::Null:
    out << "null";
    return;
  case JsonValue::Kind::Bool:
    out << (value.bool_value ? "true" : "false");
    return;
  case JsonValue::Kind::Number:
    out << value.text;
    return;
  case JsonValue::Kind::String:
    out << jsonString(value.text);
    return;
  case JsonValue::Kind::Array:
    out << '[';
    for (size_t index = 0; index < value.array.size(); ++index) {
      if (index != 0) {
        out << ',';
      }
      appendCanonicalJson(out, value.array[index]);
    }
    out << ']';
    return;
  case JsonValue::Kind::Object:
    out << '{';
    {
      bool first = true;
      for (const auto &entry : value.object) {
        if (!first) {
          out << ',';
        }
        out << jsonString(entry.first) << ':';
        appendCanonicalJson(out, entry.second);
        first = false;
      }
    }
    out << '}';
    return;
  }
  ROSE_ABORT();
}

uint64_t requiredPositiveId(const JsonValue &value,
                            const std::string &context) {
  const int64_t raw_id = value.asInt();
  if (raw_id <= 0) {
    throw std::runtime_error("AST JSON " + context +
                             " must be a positive node ID");
  }
  return static_cast<uint64_t>(raw_id);
}

std::string rawInteger128Value(Rose::Integer128Value value) {
  const auto &words = value.words();
  std::ostringstream output;
  writeRawObject(
      output, 0,
      {rawBoolField("unsigned", value.isUnsigned()),
       jsonString("words") + ": [" +
           std::to_string(static_cast<uint32_t>(words[0])) + "," +
           std::to_string(static_cast<uint32_t>(words[0] >> 32)) + "," +
           std::to_string(static_cast<uint32_t>(words[1])) + "," +
           std::to_string(static_cast<uint32_t>(words[1] >> 32)) + "]"},
      false);
  return output.str();
}

Rose::Integer128Value integer128ValueFromJson(const JsonValue &json) {
  const bool is_unsigned = json.requiredBool("unsigned");
  const auto &array = json.at("words");
  if (array.kind != JsonValue::Kind::Array || array.array.size() != 4)
    throw std::runtime_error("AST JSON 128-bit integer requires four words");
  Rose::Integer128Value::Words words{};
  for (size_t index = 0; index < 4; ++index) {
    const int64_t word = array.array[index].asInt();
    if (word < 0 || static_cast<uint64_t>(word) > UINT32_MAX)
      throw std::runtime_error("AST JSON 128-bit integer word is out of range");
    words[index / 2] |= static_cast<uint64_t>(word) << (32 * (index % 2));
  }
  return Rose::Integer128Value::fromBits(words, is_unsigned);
}

namespace {
void validateUnaryTraitEvaluatedValue(
    const std::optional<Rose::Integer128Value> &value, bool alignment) {
  if (value &&
      (!value->isUnsigned() ||
       (alignment && value->words() == Rose::Integer128Value::Words{}))) {
    throw std::runtime_error(
        "AST JSON unary trait has an invalid evaluated value");
  }
}
} // namespace

std::string
rawUnaryTraitEvaluatedValue(const std::optional<Rose::Integer128Value> &value,
                            bool alignment) {
  validateUnaryTraitEvaluatedValue(value, alignment);
  return value ? rawInteger128Value(*value) : "null";
}

std::optional<Rose::Integer128Value>
unaryTraitEvaluatedValueFromJson(const JsonValue &json, bool alignment) {
  std::optional<Rose::Integer128Value> value;
  if (json.kind != JsonValue::Kind::Null)
    value = integer128ValueFromJson(json);
  validateUnaryTraitEvaluatedValue(value, alignment);
  return value;
}

uint64_t unsigned64ValueFromJson(const JsonValue &json) {
  const auto value = integer128ValueFromJson(json);
  if (!value.isUnsigned() || value.words()[1] != 0)
    throw std::runtime_error(
        "AST JSON value is not an unsigned 64-bit integer");
  return value.words()[0];
}

std::string rawFormatDescriptor(const SgFormatItem::Descriptor &descriptor) {
  std::vector<std::string> fields{
      rawIntegerField("kind", descriptor.value.index())};
  auto character = [](const SgFormatItem::Character &value) {
    std::ostringstream out;
    writeRawObject(
        out, 0,
        {jsonString("value") + ": " + jsonString(value.value),
         rawIntegerField("delimiter", static_cast<int>(value.delimiter))},
        false);
    return out.str();
  };
  if (const auto *value =
          std::get_if<SgFormatItem::Intrinsic>(&descriptor.value)) {
    fields.push_back(
        rawIntegerField("intrinsic_kind", static_cast<int>(value->kind)));
    auto optional = [&](const char *name, std::optional<int> number) {
      fields.push_back(jsonString(name) + ": " +
                       (number ? std::to_string(*number) : "null"));
    };
    optional("width", value->width);
    optional("digits", value->digits);
    optional("exponent_width", value->exponent_width);
  } else if (const auto *value =
                 std::get_if<SgFormatItem::Derived>(&descriptor.value)) {
    fields.push_back(jsonString("tag") + ": " +
                     (value->tag ? character(*value->tag) : "null"));
    std::string parameters = "[";
    for (std::size_t index = 0; index < value->parameters.size(); ++index) {
      if (index != 0)
        parameters += ",";
      parameters += std::to_string(value->parameters[index]);
    }
    fields.push_back(jsonString("parameters") + ": " + parameters + "]");
  } else if (const auto *value =
                 std::get_if<SgFormatItem::Control>(&descriptor.value)) {
    fields.push_back(
        rawIntegerField("control_kind", static_cast<int>(value->kind)));
    fields.push_back(rawIntegerField("count", value->count));
  } else if (const auto *value =
                 std::get_if<SgFormatItem::Character>(&descriptor.value)) {
    fields.push_back(jsonString("character") + ": " + character(*value));
  } else {
    fields.push_back(rawIntegerField(
        "group_kind",
        static_cast<int>(std::get<SgFormatItem::Group>(descriptor.value))));
  }
  std::ostringstream out;
  writeRawObject(out, 0, fields, false);
  return out.str();
}

FormatItemFields formatItemFieldsFromJson(const JsonValue &properties) {
  using Item = SgFormatItem;
  const auto &json = properties.at("descriptor");
  auto character = [](const JsonValue &json) -> Item::Character {
    using Delimiter = Item::Character::Delimiter;
    if (json.kind != JsonValue::Kind::Object || json.object.size() != 2)
      throw std::runtime_error(
          "AST JSON FORMAT character fields are inconsistent");
    return {json.requiredString("value"),
            requiredEnum<Delimiter>(
                json, "delimiter", "FORMAT character",
                {Delimiter::single_quote, Delimiter::double_quote,
                 Delimiter::hollerith_upper, Delimiter::hollerith_lower})};
  };
  Item::Descriptor descriptor;
  switch (json.requiredInt("kind")) {
  case 0: {
    const auto kind = requiredEnum<Item::Intrinsic::Kind>(
        json, "intrinsic_kind", "FORMAT",
        {Item::Intrinsic::Kind::I, Item::Intrinsic::Kind::B,
         Item::Intrinsic::Kind::O, Item::Intrinsic::Kind::Z,
         Item::Intrinsic::Kind::F, Item::Intrinsic::Kind::E,
         Item::Intrinsic::Kind::EN, Item::Intrinsic::Kind::ES,
         Item::Intrinsic::Kind::EX, Item::Intrinsic::Kind::G,
         Item::Intrinsic::Kind::L, Item::Intrinsic::Kind::A,
         Item::Intrinsic::Kind::D});
    auto optional = [&](const char *name) -> std::optional<int> {
      const auto &value = json.at(name);
      if (value.kind == JsonValue::Kind::Null)
        return std::nullopt;
      const auto number = value.asInt();
      if (number < 0 || number > std::numeric_limits<int>::max())
        throw std::runtime_error("AST JSON FORMAT integer is out of range");
      return static_cast<int>(number);
    };
    descriptor.value =
        Item::Intrinsic{kind, optional("width"), optional("digits"),
                        optional("exponent_width")};
    break;
  }
  case 1: {
    Item::Derived value;
    const auto &tag = json.at("tag");
    if (tag.kind != JsonValue::Kind::Null)
      value.tag = character(tag);
    const auto &parameters = json.at("parameters");
    if (parameters.kind != JsonValue::Kind::Array)
      throw std::runtime_error("AST JSON FORMAT parameters must be an array");
    for (const auto &parameter : parameters.array)
      value.parameters.push_back(parameter.asInt());
    descriptor.value = std::move(value);
    break;
  }
  case 2: {
    using Kind = Item::Control::Kind;
    const auto kind = requiredEnum<Kind>(
        json, "control_kind", "FORMAT",
        {Kind::T,      Kind::TL,       Kind::TR, Kind::X,  Kind::Slash,
         Kind::Colon,  Kind::SS,       Kind::SP, Kind::S,  Kind::P,
         Kind::BN,     Kind::BZ,       Kind::RU, Kind::RD, Kind::RZ,
         Kind::RN,     Kind::RC,       Kind::RP, Kind::DC, Kind::DP,
         Kind::Dollar, Kind::Backslash});
    descriptor.value = Item::Control{kind, json.requiredInt("count")};
    break;
  }
  case 3:
    descriptor.value = character(json.at("character"));
    break;
  case 4:
    descriptor.value = requiredEnum<Item::Group>(
        json, "group_kind", "FORMAT",
        {Item::Group::nested, Item::Group::unlimited});
    break;
  default:
    throw std::runtime_error("AST JSON FORMAT descriptor kind is invalid");
  }
  constexpr std::size_t fieldCounts[] = {5, 3, 3, 2, 2};
  if (json.object.size() != fieldCounts[descriptor.value.index()])
    throw std::runtime_error(
        "AST JSON FORMAT descriptor fields are inconsistent");
  std::optional<std::uint64_t> repeat;
  const auto &count = properties.at("repeat_count");
  if (count.kind != JsonValue::Kind::Null)
    repeat = unsigned64ValueFromJson(count);
  if (properties.find("data") != nullptr ||
      properties.find("repeat_specification") != nullptr ||
      !Item::is_valid_descriptor(
          descriptor, repeat,
          std::holds_alternative<Item::Group>(descriptor.value)))
    throw std::runtime_error(
        "AST JSON FORMAT descriptor fields are inconsistent");
  return {std::move(descriptor), repeat};
}

SgCastExp::value_category_enum
arrayInitBindingCategoryFromJson(const JsonValue &json) {
  return requiredEnum<SgCastExp::value_category_enum>(
      json, "common_value_category", "array initialization binding",
      {SgCastExp::e_value_category_lvalue, SgCastExp::e_value_category_xvalue,
       SgCastExp::e_value_category_prvalue});
}

SgCxxSubobjectDestruction::target_kind_enum
subobjectDestructionKindFromJson(const JsonValue &json) {
  return requiredEnum<SgCxxSubobjectDestruction::target_kind_enum>(
      json, "target_kind", "subobject destruction",
      {SgCxxSubobjectDestruction::e_member,
       SgCxxSubobjectDestruction::e_nonvirtual_base,
       SgCxxSubobjectDestruction::e_virtual_base});
}

ArrayInitReferenceFields
arrayInitReferenceFieldsFromJson(const JsonValue &json) {
  const auto kind = requiredEnum<SgCxxArrayInitReference::reference_kind_enum>(
      json, "reference_kind", "array initialization reference",
      {SgCxxArrayInitReference::e_common_value,
       SgCxxArrayInitReference::e_current_index});
  const auto category = requiredEnum<SgCastExp::value_category_enum>(
      json, "value_category", "array initialization reference",
      {SgCastExp::e_value_category_lvalue, SgCastExp::e_value_category_xvalue,
       SgCastExp::e_value_category_prvalue});
  if (json.requiredBool("lvalue") !=
          (category == SgCastExp::e_value_category_lvalue) ||
      (kind == SgCxxArrayInitReference::e_current_index &&
       category != SgCastExp::e_value_category_prvalue))
    throw std::runtime_error("AST JSON array initialization reference has "
                             "inconsistent value category");
  return {kind, category};
}

OpaqueValueFields opaqueValueFieldsFromJson(const JsonValue &json) {
  const auto category = requiredEnum<SgCastExp::value_category_enum>(
      json, "value_category", "opaque value",
      {SgCastExp::e_value_category_lvalue, SgCastExp::e_value_category_xvalue,
       SgCastExp::e_value_category_prvalue});
  const auto kind = requiredEnum<SgOpaqueValueBinding::object_kind_enum>(
      json, "object_kind", "opaque value",
      {SgOpaqueValueBinding::e_ordinary_object,
       SgOpaqueValueBinding::e_bit_field,
       SgOpaqueValueBinding::e_vector_component,
       SgOpaqueValueBinding::e_objc_property,
       SgOpaqueValueBinding::e_objc_subscript,
       SgOpaqueValueBinding::e_matrix_component});
  if (json.requiredBool("lvalue") !=
      (category == SgCastExp::e_value_category_lvalue))
    throw std::runtime_error("AST JSON opaque value has inconsistent category");
  return {category, kind};
}

std::string rawExtendedFloatingValue(Rose::FloatingValue value) {
  const auto &words = value.words();
  std::ostringstream output;
  writeRawObject(
      output, 0,
      {rawIntegerField("format", static_cast<int>(value.format())),
       jsonString("words") + ": [" +
           std::to_string(static_cast<uint32_t>(words[0])) + "," +
           std::to_string(static_cast<uint32_t>(words[0] >> 32)) + "," +
           std::to_string(static_cast<uint32_t>(words[1])) + "," +
           std::to_string(static_cast<uint32_t>(words[1] >> 32)) + "]"},
      false);
  return output.str();
}

Rose::FloatingValue extendedFloatingValueFromJson(const JsonValue &json) {
  using Value = Rose::FloatingValue;
  const auto format = requiredEnum<Value::Format>(
      json, "format", "extended floating value",
      {Value::Format::binary64, Value::Format::x87_binary80,
       Value::Format::binary128});
  const auto &array = json.at("words");
  if (array.kind != JsonValue::Kind::Array || array.array.size() != 4)
    throw std::runtime_error(
        "AST JSON extended floating value requires four words");
  Value::Words words{};
  for (size_t index = 0; index < 4; ++index) {
    const int64_t word = array.array[index].asInt();
    if (word < 0 || static_cast<uint64_t>(word) > UINT32_MAX)
      throw std::runtime_error(
          "AST JSON extended floating word is out of range");
    words[index / 2] |= static_cast<uint64_t>(word) << (32 * (index % 2));
  }
  try {
    return Value::fromBits(format, words);
  } catch (const std::invalid_argument &error) {
    throw std::runtime_error(std::string("AST JSON extended floating value: ") +
                             error.what());
  }
}

namespace {
const std::initializer_list<VariantT> targetAbiScalarVariants{
    V_SgTypeBool,
    V_SgTypeChar,
    V_SgTypeWchar,
    V_SgTypeChar8,
    V_SgTypeChar16,
    V_SgTypeChar32,
    V_SgTypeSignedChar,
    V_SgTypeUnsignedChar,
    V_SgTypeShort,
    V_SgTypeUnsignedShort,
    V_SgTypeInt,
    V_SgTypeUnsignedInt,
    V_SgTypeLong,
    V_SgTypeUnsignedLong,
    V_SgTypeLongLong,
    V_SgTypeUnsignedLongLong,
    V_SgTypeSigned128bitInteger,
    V_SgTypeUnsigned128bitInteger,
    V_SgTypeFloat,
    V_SgTypeDouble,
    V_SgTypeLongDouble,
    V_SgTypeFloat128,
    V_SgTypeFp16,
    V_SgTypeFloat16,
    V_SgTypeBFloat16,
    V_SgTypeString};

void validateTargetAbi(const SgSourceFile::target_abi_info &abi) {
  if (!abi.available) {
    if (abi.big_endian || abi.pointer_size_bits != 0 ||
        abi.pointer_alignment_bits != 0 || !abi.scalars.empty()) {
      throw std::runtime_error(
          "AST JSON unavailable target ABI has layout data");
    }
    return;
  }
  if (abi.pointer_size_bits == 0 || abi.pointer_alignment_bits == 0 ||
      abi.scalars.empty()) {
    throw std::runtime_error("AST JSON target ABI has incomplete layouts");
  }
  const bool fortran = abi.scalars.begin()->first.second > 0;
  for (const auto &[key, layout] : abi.scalars) {
    const bool fortranVariant =
        key.first == V_SgTypeInt || key.first == V_SgTypeUnsignedInt ||
        key.first == V_SgTypeFloat || key.first == V_SgTypeBool ||
        key.first == V_SgTypeString;
    const bool floating =
        key.first == V_SgTypeFloat || key.first == V_SgTypeDouble ||
        key.first == V_SgTypeLongDouble || key.first == V_SgTypeFloat128 ||
        key.first == V_SgTypeFloat16 || key.first == V_SgTypeBFloat16 ||
        key.first == V_SgTypeFp16;
    const bool unsignedInteger =
        key.first == V_SgTypeUnsignedChar ||
        key.first == V_SgTypeUnsignedShort ||
        key.first == V_SgTypeUnsignedInt || key.first == V_SgTypeUnsignedLong ||
        key.first == V_SgTypeUnsignedLongLong ||
        key.first == V_SgTypeUnsigned128bitInteger ||
        key.first == V_SgTypeChar8 || key.first == V_SgTypeChar16 ||
        key.first == V_SgTypeChar32 || (!fortran && key.first == V_SgTypeBool);
    const bool signedInteger =
        key.first == V_SgTypeSignedChar || key.first == V_SgTypeShort ||
        key.first == V_SgTypeInt || key.first == V_SgTypeLong ||
        key.first == V_SgTypeLongLong ||
        key.first == V_SgTypeSigned128bitInteger;
    if (std::find(targetAbiScalarVariants.begin(),
                  targetAbiScalarVariants.end(),
                  key.first) == targetAbiScalarVariants.end() ||
        key.second < 0 || (key.second > 0) != fortran ||
        (fortran && !fortranVariant) ||
        (!fortran && key.first == V_SgTypeString) || layout.size_bits == 0 ||
        layout.alignment_bits == 0 ||
        (unsignedInteger && !layout.is_unsigned) ||
        ((signedInteger || floating || key.first == V_SgTypeString ||
          (fortran && key.first == V_SgTypeBool)) &&
         layout.is_unsigned) ||
        (floating &&
         (layout.precision == 0 || layout.precision > layout.size_bits ||
          layout.max_exponent <= 0)) ||
        (!floating && (layout.precision != 0 || layout.max_exponent != 0))) {
      throw std::runtime_error("AST JSON target ABI has invalid scalar layout");
    }
  }
}
} // namespace

std::string rawTargetAbiJson(const SgSourceFile::target_abi_info &abi) {
  validateTargetAbi(abi);
  std::ostringstream scalars;
  scalars << '[';
  bool first = true;
  for (const auto &[key, layout] : abi.scalars) {
    if (!first) {
      scalars << ',';
    }
    first = false;
    writeRawObject(scalars, 0,
                   {rawIntegerField("variant", key.first),
                    rawIntegerField("kind", key.second),
                    rawIntegerField("size_bits", layout.size_bits),
                    rawIntegerField("alignment_bits", layout.alignment_bits),
                    rawIntegerField("precision", layout.precision),
                    rawIntegerField("max_exponent", layout.max_exponent),
                    rawBoolField("is_unsigned", layout.is_unsigned)},
                   false);
  }
  scalars << ']';
  std::ostringstream out;
  writeRawObject(
      out, 0,
      {rawBoolField("available", abi.available),
       rawBoolField("big_endian", abi.big_endian),
       rawIntegerField("pointer_size_bits", abi.pointer_size_bits),
       rawIntegerField("pointer_alignment_bits", abi.pointer_alignment_bits),
       jsonString("scalars") + ": " + scalars.str()},
      false);
  return out.str();
}

SgSourceFile::target_abi_info targetAbiFromJson(const JsonValue &json) {
  const auto unsignedField = [](const JsonValue &object, const char *name) {
    const int64_t value = object.requiredInt(name);
    if (value < 0 || value > std::numeric_limits<unsigned>::max()) {
      throw std::runtime_error("AST JSON target ABI integer is out of range");
    }
    return static_cast<unsigned>(value);
  };
  SgSourceFile::target_abi_info abi;
  abi.available = json.requiredBool("available");
  abi.big_endian = json.requiredBool("big_endian");
  abi.pointer_size_bits = unsignedField(json, "pointer_size_bits");
  abi.pointer_alignment_bits = unsignedField(json, "pointer_alignment_bits");
  const JsonValue &scalars = json.at("scalars");
  if (scalars.kind != JsonValue::Kind::Array) {
    throw std::runtime_error(
        "AST JSON target ABI scalar layouts are not an array");
  }
  for (const JsonValue &scalar : scalars.array) {
    const VariantT variant = requiredEnum<VariantT>(
        scalar, "variant", "target ABI", targetAbiScalarVariants);
    const unsigned kind = unsignedField(scalar, "kind");
    const unsigned exponent = unsignedField(scalar, "max_exponent");
    if (kind > std::numeric_limits<int>::max() ||
        exponent > std::numeric_limits<int>::max()) {
      throw std::runtime_error(
          "AST JSON target ABI signed integer is out of range");
    }
    SgSourceFile::scalar_abi_layout layout;
    layout.size_bits = unsignedField(scalar, "size_bits");
    layout.alignment_bits = unsignedField(scalar, "alignment_bits");
    layout.precision = unsignedField(scalar, "precision");
    layout.max_exponent = exponent;
    layout.is_unsigned = scalar.requiredBool("is_unsigned");
    if (!abi.scalars
             .emplace(std::make_pair(variant, static_cast<int>(kind)), layout)
             .second) {
      throw std::runtime_error(
          "AST JSON target ABI has duplicate scalar layouts");
    }
  }
  validateTargetAbi(abi);
  return abi;
}

std::optional<unsigned int>
requiredOptionalUnsignedInteger(const JsonValue &properties,
                                const std::string &name,
                                const std::string &context) {
  const JsonValue &field = properties.at(name);
  if (field.kind == JsonValue::Kind::Null) {
    return std::nullopt;
  }
  if (field.kind != JsonValue::Kind::Number) {
    throw std::runtime_error("AST JSON " + context + " has a nonnumeric " +
                             name);
  }
  const int64_t value = field.asInt();
  if (value < 0 ||
      static_cast<uint64_t>(value) > std::numeric_limits<unsigned int>::max()) {
    throw std::runtime_error("AST JSON " + context + " has an invalid " + name);
  }
  return static_cast<unsigned int>(value);
}

std::optional<unsigned int>
requiredTranslationUnitSourceOrder(const JsonValue &properties,
                                   const std::string &context) {
  const std::optional<unsigned int> source_order =
      requiredOptionalUnsignedInteger(properties,
                                      "translation_unit_source_order", context);
  if (source_order.has_value() && *source_order == 0) {
    throw std::runtime_error("AST JSON " + context +
                             " has an invalid translation_unit_source_order");
  }
  return source_order;
}

std::optional<unsigned int> requiredOmpDeclareVariantRegionOrdinal(
    const JsonValue &properties, const std::string &semantic_function_name,
    const std::string &context) {
  const std::string source_name =
      properties.requiredString("omp_declare_variant_source_name");
  const std::optional<unsigned int> region_ordinal =
      requiredOptionalUnsignedInteger(
          properties, "omp_declare_variant_region_ordinal", context);
  if (source_name.empty() != !region_ordinal.has_value()) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has incomplete OpenMP declare-variant source identity");
  }
  if (!source_name.empty() && source_name == semantic_function_name) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has a declare-variant source name equal to its semantic identity");
  }
  return region_ordinal;
}

DeclarationSourceFields declarationSourceFieldsFromJson(
    const JsonValue &properties, const std::string &context,
    const std::string &qualifier_context, bool variable) {
  std::optional<SgVariableDeclaration::inline_kind> variable_inline;
  if (variable) {
    using Kind = SgVariableDeclaration::inline_kind;
    variable_inline =
        requiredEnum<Kind>(properties, "variable_inline_kind", context,
                           {Kind::none, Kind::implicit, Kind::written});
  } else if (properties.find("variable_inline_kind") != nullptr) {
    throw std::runtime_error(
        "AST JSON " + context +
        " has inline variable state on a non-variable declaration");
  }
  DeclarationSourceFields fields{
      requiredTranslationUnitSourceOrder(properties, context),
      properties.requiredBool("source_name_qualification_present"),
      properties.requiredBool("source_name_global_qualification"),
      stringListFromJson(properties.at("source_name_qualification_tokens"),
                         "source_name_qualification_tokens"),
      variable_inline};
  if (!fields.qualified &&
      (fields.globally_qualified || !fields.qualification_tokens.empty()))
    throw std::runtime_error("AST JSON " + qualifier_context +
                             " payload is present without its presence bit");
  return fields;
}

DeclarationNameQualificationFields
declarationNameQualificationFieldsFromJson(const JsonValue &properties) {
  return {properties.requiredNonnegativeInt("name_qualification_length"),
          properties.requiredBool("type_elaboration_required"),
          properties.requiredBool("global_qualification_required")};
}

void validateFortranInterfaceFields(const FortranInterfaceFields &fields) {
  const bool unnamed =
      fields.kind == SgInterfaceStatement::e_unnamed_interface_type;
  const bool named =
      fields.kind == SgInterfaceStatement::e_named_interface_type ||
      fields.kind == SgInterfaceStatement::e_operator_interface_type ||
      fields.kind == SgInterfaceStatement::e_assignment_interface_type;
  if ((!unnamed && !named) || unnamed != fields.name.is_null() ||
      (fields.is_abstract && !unnamed) ||
      (fields.kind == SgInterfaceStatement::e_assignment_interface_type &&
       fields.name != "="))
    throw std::runtime_error(
        "AST JSON Fortran interface has contradictory source role");
}

FortranInterfaceFields
fortranInterfaceFieldsFromJson(const JsonValue &properties) {
  FortranInterfaceFields fields{
      properties.requiredName("name"),
      requiredEnum<SgInterfaceStatement::generic_spec_enum>(
          properties, "generic_spec", "SgInterfaceStatement",
          {SgInterfaceStatement::e_unnamed_interface_type,
           SgInterfaceStatement::e_named_interface_type,
           SgInterfaceStatement::e_operator_interface_type,
           SgInterfaceStatement::e_assignment_interface_type}),
      bitVectorFromJson(
          properties.at("declaration_type_modifier_vector"),
          "SgInterfaceStatement declaration_type_modifier_vector",
          SgTypeModifier::e_last_modifier)[SgTypeModifier::e_abstract]};
  validateFortranInterfaceFields(fields);
  return fields;
}

DeclarationFields
declarationFieldsFromJson(const JsonValue &properties, VariantT kind,
                          const std::string &context,
                          const std::string &qualifier_context) {
  static const VariantVector declaration_kinds(V_SgDeclarationStatement);
  ROSE_ASSERT(std::find(declaration_kinds.begin(), declaration_kinds.end(),
                        kind) != declaration_kinds.end());
  static const VariantVector variable_kinds(V_SgVariableDeclaration);
  static const std::array<VariantVector, 7> qualification_kinds{
      VariantVector(V_SgFunctionDeclaration),
      VariantVector(V_SgVariableDeclaration),
      VariantVector(V_SgEnumDeclaration),
      VariantVector(V_SgTypedefDeclaration),
      VariantVector(V_SgUsingDirectiveStatement),
      VariantVector(V_SgUsingDeclarationStatement),
      VariantVector(V_SgClassDeclaration)};
  const bool variable = std::find(variable_kinds.begin(), variable_kinds.end(),
                                  kind) != variable_kinds.end();
  const bool qualified = std::any_of(
      qualification_kinds.begin(), qualification_kinds.end(),
      [&](const VariantVector &kinds) {
        return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
      });
  return {declarationSourceFieldsFromJson(properties, context,
                                          qualifier_context, variable),
          qualified
              ? std::optional<DeclarationNameQualificationFields>(
                    declarationNameQualificationFieldsFromJson(properties))
              : std::nullopt,
          properties.requiredString("linkage"),
          bitVectorFromJson(properties.at("declaration_modifier_vector"),
                            context + " declaration_modifier_vector",
                            SgDeclarationModifier::e_last_modifier),
          requiredStorageModifier(properties, "declaration_storage_modifier",
                                  context),
          properties.requiredBool("declaration_thread_local_storage"),
          kind == V_SgAccessLabelStatement
              ? requiredEnum<SgAccessModifier::access_modifier_enum>(
                    properties, "declaration_access_modifier", context,
                    {SgAccessModifier::e_unknown})
              : requiredDeclarationAccessModifier(
                    properties, "declaration_access_modifier", context),
          properties.requiredBool("declaration_access_is_explicit"),
          properties.requiredBool("name_only"),
          properties.requiredBool("forward"),
          properties.requiredBool("extern_brace"),
          properties.requiredBool("skip_elaborate_type"),
          properties.requiredString("binding_label"),
          properties.requiredBool("binding_cdefined"),
          properties.requiredBool("binding_label_present"),
          properties.requiredBool("unparse_template_ast"),
          requiredGnuDeclarationVisibility(
              properties, "declaration_gnu_attribute_visibility", context),
          requiredGnuDeclarationVisibility(
              properties, "declaration_gnu_type_visibility", context),
          typeModifierFieldsFromJson(properties, context)};
}

void DeclarationFields::initialize(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  auto &modifier = declaration->get_declarationModifier();
  type_modifier.initialize(modifier.get_typeModifier());
  if (source.translation_unit_order.has_value())
    declaration->initialize_translation_unit_source_order(
        *source.translation_unit_order);
  modifier.set_modifierVector(modifier_bits);
  modifier.get_storageModifier().set_modifier(storage);
  modifier.get_storageModifier().set_thread_local_storage(thread_local_storage);
  modifier.get_accessModifier().set_modifier(access);
  modifier.get_accessModifier().set_is_explicit(access_is_explicit);
  modifier.set_gnu_attribute_visibility(attribute_visibility);
  modifier.set_gnu_type_visibility(type_visibility);
  if (auto *variable = isSgVariableDeclaration(declaration)) {
    ROSE_ASSERT(source.variable_inline.has_value());
    variable->set_inline_kind(*source.variable_inline);
  }
  auto initialize_qualification = [&](auto *qualified) {
    ROSE_ASSERT(qualification.has_value());
    qualified->set_name_qualification_length(qualification->length);
    qualified->set_type_elaboration_required(
        qualification->elaboration_required);
    qualified->set_global_qualification_required(
        qualification->globally_qualified);
  };
  if (auto *qualified = isSgFunctionDeclaration(declaration))
    initialize_qualification(qualified);
  else if (auto *qualified = isSgVariableDeclaration(declaration))
    initialize_qualification(qualified);
  else if (auto *qualified = isSgEnumDeclaration(declaration))
    initialize_qualification(qualified);
  else if (auto *qualified = isSgTypedefDeclaration(declaration))
    initialize_qualification(qualified);
  else if (auto *qualified = isSgUsingDirectiveStatement(declaration))
    initialize_qualification(qualified);
  else if (auto *qualified = isSgUsingDeclarationStatement(declaration))
    initialize_qualification(qualified);
  else if (auto *qualified = isSgClassDeclaration(declaration))
    initialize_qualification(qualified);
  declaration->set_source_name_global_qualification(source.globally_qualified);
  declaration->get_source_name_qualification_tokens() =
      std::move(source.qualification_tokens);
  declaration->set_source_name_qualification_present(source.qualified);
  declaration->set_linkage(linkage);
  declaration->set_nameOnly(name_only);
  declaration->set_forward(forward);
  declaration->set_externBrace(extern_brace);
  declaration->set_skipElaborateType(skip_elaborate_type);
  declaration->set_binding_label(binding_label);
  declaration->set_binding_cdefined(binding_cdefined);
  declaration->set_binding_label_present(binding_label_present);
  declaration->set_unparse_template_ast(unparse_template_ast);
}

void validateFortranProgramNameMetadata(const std::string &opening_name,
                                        int64_t statement_kind,
                                        bool named_in_end_statement,
                                        const std::string &end_statement_name) {
  if (statement_kind !=
          SgProgramHeaderStatement::e_implicit_program_statement &&
      statement_kind !=
          SgProgramHeaderStatement::e_explicit_program_statement) {
    throw std::runtime_error(
        "AST JSON has invalid Fortran PROGRAM-statement metadata");
  }

  const bool has_end_statement_name = !end_statement_name.empty();
  if (named_in_end_statement != has_end_statement_name) {
    throw std::runtime_error(
        "AST JSON Fortran END PROGRAM name metadata is inconsistent");
  }
  if (statement_kind ==
          SgProgramHeaderStatement::e_implicit_program_statement &&
      (!opening_name.empty() || has_end_statement_name)) {
    throw std::runtime_error(
        "AST JSON implicit Fortran PROGRAM carries a source name");
  }
  if (statement_kind ==
          SgProgramHeaderStatement::e_explicit_program_statement &&
      !SageInterface::isValidFortranSourceIdentifier(opening_name)) {
    throw std::runtime_error(
        "AST JSON explicit Fortran PROGRAM has an invalid source name");
  }
  if (has_end_statement_name &&
      (!SageInterface::isValidFortranSourceIdentifier(end_statement_name) ||
       !SgName::caseInsensitiveEquality(SgName(opening_name),
                                        SgName(end_statement_name)))) {
    throw std::runtime_error(
        "AST JSON END PROGRAM name is invalid or does not match PROGRAM name");
  }
}

class JsonParser {
public:
  explicit JsonParser(const std::string &input) : input_(input) {}

  JsonValue parse() {
    JsonValue value = parseValue();
    skipWhitespace();
    if (pos_ != input_.size()) {
      fail("trailing data after JSON value");
    }
    return value;
  }

private:
  const std::string &input_;
  size_t pos_ = 0;

  [[noreturn]] void fail(const std::string &message) const {
    std::ostringstream out;
    out << "invalid AST JSON at byte " << pos_ << ": " << message;
    throw std::runtime_error(out.str());
  }

  void skipWhitespace() {
    while (pos_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
  }

  bool consume(char expected) {
    skipWhitespace();
    if (pos_ < input_.size() && input_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  void require(char expected) {
    if (!consume(expected)) {
      std::string message = "expected '";
      message += expected;
      message += "'";
      fail(message);
    }
  }

  JsonValue parseValue() {
    skipWhitespace();
    if (pos_ >= input_.size()) {
      fail("unexpected end of input");
    }

    const char c = input_[pos_];
    if (c == '"') {
      return JsonValue::string(parseString());
    }
    if (c == '{') {
      return parseObject();
    }
    if (c == '[') {
      return parseArray();
    }
    if (startsWith("true")) {
      pos_ += 4;
      return JsonValue::boolean(true);
    }
    if (startsWith("false")) {
      pos_ += 5;
      return JsonValue::boolean(false);
    }
    if (startsWith("null")) {
      pos_ += 4;
      return JsonValue::null();
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parseNumber();
    }
    fail("unexpected JSON value");
  }

  bool startsWith(const char *keyword) const {
    const size_t length = std::char_traits<char>::length(keyword);
    return input_.compare(pos_, length, keyword) == 0;
  }

  JsonValue parseObject() {
    require('{');
    JsonObject object;
    skipWhitespace();
    if (consume('}')) {
      return JsonValue::objectValue(std::move(object));
    }
    while (true) {
      skipWhitespace();
      if (pos_ >= input_.size() || input_[pos_] != '"') {
        fail("expected object key");
      }
      std::string key = parseString();
      require(':');
      if (!object.emplace(std::move(key), parseValue()).second)
        fail("duplicate object key");
      if (consume('}')) {
        break;
      }
      require(',');
    }
    return JsonValue::objectValue(std::move(object));
  }

  JsonValue parseArray() {
    require('[');
    std::vector<JsonValue> array;
    skipWhitespace();
    if (consume(']')) {
      return JsonValue::arrayValue(std::move(array));
    }
    while (true) {
      array.push_back(parseValue());
      if (consume(']')) {
        break;
      }
      require(',');
    }
    return JsonValue::arrayValue(std::move(array));
  }

  JsonValue parseNumber() {
    const size_t begin = pos_;
    auto consume_digits = [&]() {
      const size_t first = pos_;
      while (pos_ < input_.size() && input_[pos_] >= '0' &&
             input_[pos_] <= '9') {
        ++pos_;
      }
      if (first == pos_)
        fail("malformed number");
    };
    if (input_[pos_] == '-') {
      ++pos_;
    }
    if (pos_ < input_.size() && input_[pos_] == '0') {
      ++pos_;
      if (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
        fail("malformed number");
    } else {
      consume_digits();
    }
    if (pos_ < input_.size() && input_[pos_] == '.') {
      ++pos_;
      consume_digits();
    }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < input_.size() &&
          (input_[pos_] == '+' || input_[pos_] == '-')) {
        ++pos_;
      }
      consume_digits();
    }
    return JsonValue::number(input_.substr(begin, pos_ - begin));
  }

  unsigned parseUnicodeEscape() {
    if (pos_ + 4 > input_.size())
      fail("short unicode escape");
    unsigned value = 0;
    for (unsigned i = 0; i < 4; ++i) {
      const char digit = input_[pos_++];
      value <<= 4;
      if (digit >= '0' && digit <= '9')
        value |= digit - '0';
      else if (digit >= 'a' && digit <= 'f')
        value |= digit - 'a' + 10;
      else if (digit >= 'A' && digit <= 'F')
        value |= digit - 'A' + 10;
      else
        fail("invalid unicode escape");
    }
    return value;
  }

  std::string parseString() {
    require('"');
    std::string result;
    while (pos_ < input_.size()) {
      char c = input_[pos_++];
      if (c == '"') {
        return result;
      }
      if (c != '\\') {
        if (static_cast<unsigned char>(c) < 0x20)
          fail("unescaped control character");
        result += c;
        continue;
      }
      if (pos_ >= input_.size()) {
        fail("unterminated escape sequence");
      }
      const char escaped = input_[pos_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        result += escaped;
        break;
      case 'b':
        result += '\b';
        break;
      case 'f':
        result += '\f';
        break;
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      case 'u': {
        unsigned value = parseUnicodeEscape();
        if (value >= 0xd800 && value <= 0xdbff) {
          if (input_.compare(pos_, 2, "\\u") != 0)
            fail("unpaired unicode surrogate");
          pos_ += 2;
          const unsigned low = parseUnicodeEscape();
          if (low < 0xdc00 || low > 0xdfff)
            fail("unpaired unicode surrogate");
          value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
        } else if (value >= 0xdc00 && value <= 0xdfff) {
          fail("unpaired unicode surrogate");
        }
        if (value <= 0x7f) {
          result += static_cast<char>(value);
        } else {
          unsigned continuation_count;
          unsigned prefix;
          if (value <= 0x7ff) {
            continuation_count = 1;
            prefix = 0xc0;
          } else if (value <= 0xffff) {
            continuation_count = 2;
            prefix = 0xe0;
          } else {
            continuation_count = 3;
            prefix = 0xf0;
          }
          result +=
              static_cast<char>(prefix | (value >> (6 * continuation_count)));
          for (unsigned i = continuation_count; i > 0; --i)
            result +=
                static_cast<char>(0x80 | ((value >> (6 * (i - 1))) & 0x3f));
        }
        break;
      }
      default:
        fail("unsupported escape sequence");
      }
    }
    fail("unterminated string");
  }
};

JsonValue parseJson(const std::string &json) {
  return JsonParser(json).parse();
}

const NodeRecord &AstFileRecord::node(uint64_t id) const {
  auto found = index_by_id.find(id);
  if (found == index_by_id.end()) {
    throw std::runtime_error("AST JSON references unknown node id " +
                             std::to_string(id));
  }
  if (found->second >= nodes.size() || nodes[found->second].id != id)
    throw std::runtime_error("AST JSON has inconsistent node index for id " +
                             std::to_string(id));
  return nodes[found->second];
}

bool startsWith(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

std::string sanitizePathComponent(std::string name) {
  for (char &c : name) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (!std::isalnum(uc) && c != '_' && c != '-' && c != '.') {
      c = '_';
    }
  }
  if (name.empty()) {
    return "ast";
  }
  return name;
}

uint64_t fnv1a64(const std::string &value) {
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char c : value) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string hex64(uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

uint64_t processId() { return static_cast<uint64_t>(::getpid()); }

std::vector<std::string> commandLine(const SgSourceFile *file) {
  if (file == nullptr) {
    return {};
  }
  return file->get_originalCommandLineArgumentList();
}

bool argumentSelectsCheckpoint(const std::string &value,
                               Checkpoint checkpoint) {
  return value == "all" || value == checkpointName(checkpoint);
}

bool hasCheckpointArgument(const SgSourceFile *file, Checkpoint checkpoint) {
  const std::string prefix = "-rex:ast-json-checkpoint=";
  for (const std::string &arg : commandLine(file)) {
    if (startsWith(arg, prefix) &&
        argumentSelectsCheckpoint(arg.substr(prefix.size()), checkpoint)) {
      return true;
    }
  }

  const char *env = std::getenv("REX_AST_JSON_CHECKPOINT");
  return env != nullptr && argumentSelectsCheckpoint(env, checkpoint);
}

std::string commandLineValue(const SgSourceFile *file,
                             const std::string &prefix) {
  for (const std::string &arg : commandLine(file)) {
    if (startsWith(arg, prefix)) {
      return arg.substr(prefix.size());
    }
  }
  return "";
}

std::string defaultOutputDirectory(const SgSourceFile *file) {
  std::string from_arg = commandLineValue(file, "-rex:ast-json-dir=");
  if (!from_arg.empty()) {
    return from_arg;
  }

  const char *env = std::getenv("REX_AST_JSON_DIR");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }

  return ".rex-ast-json";
}

std::string trim(const std::string &input) {
  size_t begin = 0;
  while (begin < input.size() &&
         std::isspace(static_cast<unsigned char>(input[begin]))) {
    ++begin;
  }
  size_t end = input.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    --end;
  }
  return input.substr(begin, end - begin);
}

std::string jsonString(const std::string &input) {
  std::string out;
  out.reserve(input.size() + 2);
  out += '"';
  for (unsigned char c : input) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c < 0x20) {
        const char *digits = "0123456789abcdef";
        out += "\\u00";
        out += digits[(c >> 4) & 0xf];
        out += digits[c & 0xf];
      } else {
        out += static_cast<char>(c);
      }
      break;
    }
  }
  out += '"';
  return out;
}

void indent(std::ostream &out, int level) {
  static const std::string spaces(64, ' ');
  for (; level > 0; level -= static_cast<int>(spaces.size()))
    out.write(spaces.data(), std::min<int>(level, spaces.size()));
}

void writeStringField(std::ostream &out, int level, const char *name,
                      const std::string &value, bool comma) {
  indent(out, level);
  out << jsonString(name) << ": " << jsonString(value);
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeIntegerField(std::ostream &out, int level, const char *name,
                       int64_t value, bool comma) {
  indent(out, level);
  out << jsonString(name) << ": " << value;
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeBoolField(std::ostream &out, int level, const char *name, bool value,
                    bool comma) {
  indent(out, level);
  out << jsonString(name) << ": " << (value ? "true" : "false");
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void requireFileIdMapping(int id, const std::string &filename,
                          const std::string &context) {
  if (id < 0) {
    throw std::runtime_error("AST JSON " + context +
                             " uses negative file id as a registered filename");
  }
  if (filename.empty()) {
    throw std::runtime_error("AST JSON " + context +
                             " has an empty registered filename");
  }

  std::map<int, std::string> &id_to_name = Sg_File_Info::get_fileidtoname_map();
  std::map<std::string, int> &name_to_id = Sg_File_Info::get_nametofileid_map();

  auto id_entry = id_to_name.find(id);
  if (id_entry != id_to_name.end() && id_entry->second != filename) {
    std::ostringstream message;
    message << "AST JSON " << context << " conflicts with existing file id "
            << id << ": existing " << id_entry->second << ", JSON " << filename;
    throw std::runtime_error(message.str());
  }

  auto name_entry = name_to_id.find(filename);
  if (name_entry != name_to_id.end() && name_entry->second != id) {
    std::ostringstream message;
    message << "AST JSON " << context
            << " conflicts with existing filename id for " << filename
            << ": existing " << name_entry->second << ", JSON " << id;
    throw std::runtime_error(message.str());
  }

  id_to_name[id] = filename;
  name_to_id[filename] = id;
}

std::string filenameForFileId(int id, const std::string &context) {
  const std::map<int, std::string> &id_to_name =
      Sg_File_Info::get_fileidtoname_map();
  auto found = id_to_name.find(id);
  if (found == id_to_name.end()) {
    std::ostringstream message;
    message << "AST JSON " << context << " references unregistered file id "
            << id;
    throw std::runtime_error(message.str());
  }
  return found->second;
}

std::string rawStringField(const std::string &name, const std::string &value) {
  return jsonString(name) + ": " + jsonString(value);
}

std::string rawIntegerField(const std::string &name, int64_t value) {
  return jsonString(name) + ": " + std::to_string(value);
}

std::string
rawOptionalUnsignedIntegerField(const std::string &name,
                                const std::optional<unsigned int> &value) {
  return jsonString(name) + ": " +
         (value.has_value() ? std::to_string(*value) : "null");
}

std::string rawBoolField(const std::string &name, bool value) {
  return jsonString(name) + ": " + (value ? "true" : "false");
}

std::string rawBitVectorJson(const SgBitVector &bits) {
  std::ostringstream out;
  out << '[';
  for (size_t i = 0; i < bits.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << (bits[i] ? "true" : "false");
  }
  out << ']';
  return out.str();
}

std::string rawStringListJson(const SgStringList &values) {
  std::ostringstream out;
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << jsonString(values[i]);
  }
  out << ']';
  return out.str();
}

SgStringList stringListFromJson(const JsonValue &value,
                                const std::string &field_name) {
  if (value.kind != JsonValue::Kind::Array) {
    throw std::runtime_error("AST JSON " + field_name +
                             " field is not an array");
  }
  SgStringList result;
  result.reserve(value.array.size());
  for (const JsonValue &entry : value.array) {
    result.push_back(entry.asString());
  }
  return result;
}

void writeFileIdMapJson(std::ostream &out, int level, bool comma) {
  const std::map<int, std::string> &file_id_map =
      Sg_File_Info::get_fileidtoname_map();

  int expected_id = 0;
  for (const auto &entry : file_id_map) {
    if (entry.first != expected_id) {
      std::ostringstream message;
      message << "AST JSON cannot serialize sparse Sg_File_Info file id map: "
              << "expected id " << expected_id << ", found " << entry.first;
      throw std::runtime_error(message.str());
    }
    if (entry.second.empty()) {
      std::ostringstream message;
      message << "AST JSON cannot serialize empty filename for file id "
              << entry.first;
      throw std::runtime_error(message.str());
    }
    ++expected_id;
  }

  indent(out, level);
  out << jsonString("file_id_map") << ": [\n";
  for (auto entry = file_id_map.begin(); entry != file_id_map.end(); ++entry) {
    indent(out, level + 2);
    out << "{\n";
    writeIntegerField(out, level + 4, "id", entry->first);
    writeStringField(out, level + 4, "filename", entry->second, false);
    indent(out, level + 2);
    out << '}';
    if (std::next(entry) != file_id_map.end()) {
      out << ',';
    }
    out << '\n';
  }
  indent(out, level);
  out << ']';
  if (comma) {
    out << ',';
  }
  out << '\n';
}

void writeRawObject(std::ostream &out, int level,
                    const std::vector<std::string> &fields, bool comma);

} // namespace AstJson
} // namespace Rose
