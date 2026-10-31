#ifndef ROSE_SAGE_AST_JSON_PRIVATE_H
#define ROSE_SAGE_AST_JSON_PRIVATE_H

#include "sageAstJson.h"

#include "RoseAst.h"
#include "sageBuilder.h"
#include "sageInterface.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <unistd.h>

namespace Rose {
namespace AstJson {

extern const char *const kFormat;
extern const int kSchemaVersion;

enum class ExternalFunctionParameterScopeOwner {
  FirstNondefining = 1,
  Defining = 2
};

extern SgSourceFile *collectionBoundaryFile;
extern SgNode *collectionBoundaryRoot;
extern SgProject *currentDeserializationProject;
extern std::unordered_map<SgSourceFile *, std::vector<SgClassDeclaration *>>
    currentDeserializationClassDeclarationCache;
extern const SgNode *currentTypeSerializationNode;
extern bool serializingTypeOwnedExpression;

struct JsonValue;
struct JsonMember;

// Object keys come from a small vocabulary, so each spelling is stored once
// and every member refers to it.  Each spelling also knows its rank among all
// spellings in lexicographic order; a new spelling shifts the ranks after it
// but never reorders existing ones, so members ordered by rank stay ordered
// exactly as std::map orders their spellings.
class JsonKey {
public:
  explicit JsonKey(const std::string &text);

  operator const std::string &() const { return spelling_->text; }
  const std::string &str() const { return spelling_->text; }
  size_t rank() const { return spelling_->rank; }

  friend bool operator==(const JsonKey &lhs, const JsonKey &rhs) {
    return lhs.spelling_ == rhs.spelling_;
  }
  friend bool operator!=(const JsonKey &lhs, const JsonKey &rhs) {
    return lhs.spelling_ != rhs.spelling_;
  }
  friend bool operator==(const JsonKey &lhs, const std::string &rhs) {
    return lhs.str() == rhs;
  }
  friend bool operator==(const std::string &lhs, const JsonKey &rhs) {
    return lhs == rhs.str();
  }
  friend bool operator!=(const JsonKey &lhs, const std::string &rhs) {
    return lhs.str() != rhs;
  }
  friend bool operator!=(const std::string &lhs, const JsonKey &rhs) {
    return lhs != rhs.str();
  }
  friend bool operator==(const JsonKey &lhs, const char *rhs) {
    return lhs.str() == rhs;
  }
  friend bool operator!=(const JsonKey &lhs, const char *rhs) {
    return lhs.str() != rhs;
  }
  friend std::ostream &operator<<(std::ostream &out, const JsonKey &key) {
    return out << key.str();
  }

  struct Spelling {
    std::string text;
    size_t rank;
  };

private:
  const Spelling *spelling_;
};

// The members of a JSON object, ordered by key like std::map.  Each member
// has its own allocation, owned by the object, so references to it survive
// the insertion and removal of other members.
class JsonObject {
  using Storage = std::vector<JsonMember *>;

public:
  template <typename Member, typename Base> class Iterator {
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::remove_const_t<Member>;
    using difference_type = std::ptrdiff_t;
    using pointer = Member *;
    using reference = Member &;

    Iterator() = default;
    explicit Iterator(Base base) : base_(base) {}
    template <
        typename OtherMember, typename OtherBase,
        typename = std::enable_if_t<std::is_convertible_v<OtherBase, Base> &&
                                    !std::is_same_v<OtherBase, Base>>>
    Iterator(const Iterator<OtherMember, OtherBase> &other)
        : base_(other.base()) {}

    reference operator*() const { return **base_; }
    pointer operator->() const { return *base_; }
    Iterator &operator++() {
      ++base_;
      return *this;
    }
    Iterator operator++(int) { return Iterator(base_++); }
    Iterator &operator--() {
      --base_;
      return *this;
    }
    Iterator operator--(int) { return Iterator(base_--); }
    friend bool operator==(const Iterator &lhs, const Iterator &rhs) {
      return lhs.base_ == rhs.base_;
    }
    friend bool operator!=(const Iterator &lhs, const Iterator &rhs) {
      return lhs.base_ != rhs.base_;
    }
    Base base() const { return base_; }

  private:
    Base base_{};
  };

  using iterator = Iterator<JsonMember, Storage::iterator>;
  using const_iterator = Iterator<const JsonMember, Storage::const_iterator>;

  JsonObject() = default;
  JsonObject(
      std::initializer_list<std::pair<const std::string, JsonValue>> members);
  JsonObject(const JsonObject &other);
  JsonObject(JsonObject &&other) noexcept;
  JsonObject &operator=(const JsonObject &other);
  JsonObject &operator=(JsonObject &&other) noexcept;
  ~JsonObject();

  size_t size() const { return members_.size(); }
  bool empty() const { return members_.empty(); }
  iterator begin() { return iterator(members_.begin()); }
  iterator end() { return iterator(members_.end()); }
  const_iterator begin() const { return const_iterator(members_.begin()); }
  const_iterator end() const { return const_iterator(members_.end()); }

  iterator find(const std::string &key);
  const_iterator find(const std::string &key) const;
  size_t count(const std::string &key) const { return find(key) != end(); }
  JsonValue &at(const std::string &key);
  const JsonValue &at(const std::string &key) const;
  JsonValue &operator[](const std::string &key);
  std::pair<iterator, bool> emplace(const std::string &key, JsonValue value);
  std::pair<iterator, bool> insert_or_assign(const std::string &key,
                                             JsonValue value);
  size_t erase(const std::string &key);
  void clear();

private:
  Storage::const_iterator lowerBound(const std::string &key) const;
  Storage::iterator lowerBound(const JsonKey &key);

  Storage members_;
};

struct JsonValue {
  enum class Kind { Null, Bool, Number, String, Array, Object };

  Kind kind = Kind::Null;
  bool bool_value = false;
  std::string text;
  std::vector<JsonValue> array;
  JsonObject object;

  static JsonValue null() { return JsonValue(); }

  static JsonValue boolean(bool value) {
    JsonValue result;
    result.kind = Kind::Bool;
    result.bool_value = value;
    return result;
  }

  static JsonValue number(std::string value) {
    JsonValue result;
    result.kind = Kind::Number;
    result.text = std::move(value);
    return result;
  }

  static JsonValue string(std::string value) {
    JsonValue result;
    result.kind = Kind::String;
    result.text = std::move(value);
    return result;
  }

  static JsonValue arrayValue(std::vector<JsonValue> value) {
    JsonValue result;
    result.kind = Kind::Array;
    result.array = std::move(value);
    return result;
  }

  static JsonValue objectValue(JsonObject value) {
    JsonValue result;
    result.kind = Kind::Object;
    result.object = std::move(value);
    return result;
  }

  bool exactlyEquals(const JsonValue &other) const;

  const JsonValue &at(const std::string &key) const;

  JsonValue &at(const std::string &key) {
    return const_cast<JsonValue &>(std::as_const(*this).at(key));
  }

  const JsonValue *find(const std::string &key) const;

  std::string asString() const {
    if (kind != Kind::String) {
      throw std::runtime_error("AST JSON value is not a string");
    }
    return text;
  }

  std::string requiredString(const std::string &key) const {
    return at(key).asString();
  }

  SgName requiredName(const std::string &key) const {
    const std::string name = requiredString(key);
    if (name.find('\0') != std::string::npos)
      throw std::runtime_error("AST JSON " + key + " contains embedded NUL");
    return SgName(name);
  }

  int64_t asInt() const {
    if (kind != Kind::Number) {
      throw std::runtime_error("AST JSON value is not a number");
    }
    std::size_t consumed = 0;
    int64_t value = 0;
    try {
      value = std::stoll(text, &consumed, 10);
    } catch (const std::invalid_argument &) {
      throw std::runtime_error("AST JSON number is not an integer: " + text);
    } catch (const std::out_of_range &) {
      throw std::runtime_error("AST JSON integer is out of range: " + text);
    }
    if (consumed != text.size()) {
      throw std::runtime_error("AST JSON number is not an integer: " + text);
    }
    return value;
  }

  int64_t requiredInt(const std::string &key) const { return at(key).asInt(); }

  int requiredIntInRange(const std::string &key, int minimum,
                         int maximum) const {
    const int64_t value = requiredInt(key);
    if (value < minimum || value > maximum) {
      throw std::runtime_error("AST JSON " + key + " is outside its int range");
    }
    return static_cast<int>(value);
  }

  int requiredNonnegativeInt(const std::string &key) const {
    return requiredIntInRange(key, 0, std::numeric_limits<int>::max());
  }

  uint64_t requiredNonnegativeNodeId(const std::string &key) const {
    const int64_t id = requiredInt(key);
    if (id < 0) {
      throw std::runtime_error("AST JSON " + key + " has a negative node ID");
    }
    return static_cast<uint64_t>(id);
  }

  bool asBool() const {
    if (kind != Kind::Bool) {
      throw std::runtime_error("AST JSON value is not a bool");
    }
    return bool_value;
  }

  bool requiredBool(const std::string &key) const { return at(key).asBool(); }
};

struct JsonMember {
  JsonKey first;
  JsonValue second;
};

inline bool JsonValue::exactlyEquals(const JsonValue &other) const {
  if (kind != other.kind || bool_value != other.bool_value ||
      text != other.text || array.size() != other.array.size() ||
      object.size() != other.object.size()) {
    return false;
  }
  for (size_t index = 0; index < array.size(); ++index) {
    if (!array[index].exactlyEquals(other.array[index])) {
      return false;
    }
  }
  auto this_field = object.begin();
  auto other_field = other.object.begin();
  for (; this_field != object.end(); ++this_field, ++other_field) {
    if (this_field->first != other_field->first ||
        !this_field->second.exactlyEquals(other_field->second)) {
      return false;
    }
  }
  return true;
}

inline const JsonValue &JsonValue::at(const std::string &key) const {
  if (kind != Kind::Object) {
    throw std::runtime_error("AST JSON value is not an object while reading " +
                             key);
  }
  auto found = object.find(key);
  if (found == object.end()) {
    throw std::runtime_error("AST JSON object is missing key: " + key);
  }
  return found->second;
}

inline const JsonValue *JsonValue::find(const std::string &key) const {
  if (kind != Kind::Object) {
    return nullptr;
  }
  auto found = object.find(key);
  return found == object.end() ? nullptr : &found->second;
}

// As with std::map, the first occurrence of a key wins.
inline JsonObject::JsonObject(
    std::initializer_list<std::pair<const std::string, JsonValue>> members) {
  for (const auto &member : members)
    emplace(member.first, member.second);
}

inline JsonObject::JsonObject(const JsonObject &other) {
  members_.reserve(other.members_.size());
  try {
    for (const JsonMember *member : other.members_)
      members_.push_back(new JsonMember(*member));
  } catch (...) {
    clear();
    throw;
  }
}

inline JsonObject &JsonObject::operator=(const JsonObject &other) {
  if (this != &other) {
    JsonObject copy(other);
    members_.swap(copy.members_);
  }
  return *this;
}

inline JsonObject::JsonObject(JsonObject &&other) noexcept
    : members_(std::move(other.members_)) {
  other.members_.clear();
}

inline JsonObject &JsonObject::operator=(JsonObject &&other) noexcept {
  if (this != &other) {
    clear();
    members_.swap(other.members_);
  }
  return *this;
}

inline JsonObject::~JsonObject() { clear(); }

inline void JsonObject::clear() {
  for (JsonMember *member : members_)
    delete member;
  members_.clear();
}

inline JsonObject::Storage::const_iterator
JsonObject::lowerBound(const std::string &key) const {
  return std::lower_bound(
      members_.begin(), members_.end(), key,
      [](const JsonMember *member, const std::string &probe) {
        return member->first.str() < probe;
      });
}

inline JsonObject::Storage::iterator
JsonObject::lowerBound(const JsonKey &key) {
  const size_t rank = key.rank();
  return std::lower_bound(members_.begin(), members_.end(), rank,
                          [](const JsonMember *member, size_t probe) {
                            return member->first.rank() < probe;
                          });
}

inline JsonObject::iterator JsonObject::find(const std::string &key) {
  return iterator(members_.begin() +
                  (std::as_const(*this).find(key).base() - members_.cbegin()));
}

inline JsonObject::const_iterator
JsonObject::find(const std::string &key) const {
  const auto found = lowerBound(key);
  return const_iterator(found != members_.end() && (*found)->first == key
                            ? found
                            : members_.end());
}

inline JsonValue &JsonObject::at(const std::string &key) {
  const auto found = find(key);
  if (found == end())
    throw std::out_of_range("AST JSON object has no member " + key);
  return found->second;
}

inline const JsonValue &JsonObject::at(const std::string &key) const {
  const auto found = find(key);
  if (found == end())
    throw std::out_of_range("AST JSON object has no member " + key);
  return found->second;
}

inline JsonValue &JsonObject::operator[](const std::string &key) {
  return emplace(key, JsonValue()).first->second;
}

inline std::pair<JsonObject::iterator, bool>
JsonObject::emplace(const std::string &key, JsonValue value) {
  const JsonKey interned(key);
  const auto position = lowerBound(interned);
  if (position != members_.end() && (*position)->first == interned)
    return {iterator(position), false};
  auto member =
      std::make_unique<JsonMember>(JsonMember{interned, std::move(value)});
  const auto inserted = members_.insert(position, member.get());
  member.release();
  return {iterator(inserted), true};
}

inline std::pair<JsonObject::iterator, bool>
JsonObject::insert_or_assign(const std::string &key, JsonValue value) {
  auto inserted = emplace(key, JsonValue());
  inserted.first->second = std::move(value);
  return inserted;
}

inline size_t JsonObject::erase(const std::string &key) {
  const auto found = find(key);
  if (found == end())
    return 0;
  delete *found.base();
  members_.erase(found.base());
  return 1;
}

std::string rawTargetAbiJson(const SgSourceFile::target_abi_info &abi);
SgSourceFile::target_abi_info targetAbiFromJson(const JsonValue &json);

// IEEE binary32/binary64 values use numeric 32-bit words, least significant
// first. Source spelling is separate: it may be absent or use another
// language's literal grammar. Integer words also preserve signed zero and NaN
// payloads.
template <typename Float>
using FloatingValueBits =
    std::conditional_t<std::is_same_v<Float, float>, uint32_t, uint64_t>;

template <typename Float> std::string rawFloatingValueBits(Float value) {
  static_assert(std::is_same_v<Float, float> || std::is_same_v<Float, double>);
  static_assert(std::numeric_limits<Float>::is_iec559 &&
                sizeof(Float) == sizeof(FloatingValueBits<Float>));
  FloatingValueBits<Float> bits;
  std::memcpy(&bits, &value, sizeof(bits));
  std::string result = "[";
  for (size_t index = 0; index < sizeof(bits) / sizeof(uint32_t); ++index) {
    if (index != 0)
      result += ',';
    result += std::to_string(static_cast<uint32_t>(bits >> (32 * index)));
  }
  return result + ']';
}

template <typename Float> Float floatingValueFromJson(const JsonValue &json) {
  static_assert(std::is_same_v<Float, float> || std::is_same_v<Float, double>);
  static_assert(std::numeric_limits<Float>::is_iec559 &&
                sizeof(Float) == sizeof(FloatingValueBits<Float>));
  if (json.kind != JsonValue::Kind::Array ||
      json.array.size() != sizeof(Float) / sizeof(uint32_t)) {
    throw std::runtime_error(
        "AST JSON floating value_bits has an invalid shape");
  }
  FloatingValueBits<Float> bits = 0;
  for (size_t index = 0; index < json.array.size(); ++index) {
    const int64_t word = json.array[index].asInt();
    if (word < 0 || static_cast<uint64_t>(word) > UINT32_MAX) {
      throw std::runtime_error(
          "AST JSON floating value_bits word is out of range");
    }
    bits |= static_cast<FloatingValueBits<Float>>(word) << (32 * index);
  }
  Float result;
  std::memcpy(&result, &bits, sizeof(result));
  return result;
}

std::string rawInteger128Value(Rose::Integer128Value value);
Rose::Integer128Value integer128ValueFromJson(const JsonValue &json);
std::string
rawUnaryTraitEvaluatedValue(const std::optional<Rose::Integer128Value> &value,
                            bool alignment);
std::optional<Rose::Integer128Value>
unaryTraitEvaluatedValueFromJson(const JsonValue &json, bool alignment);
uint64_t requiredPositiveId(const JsonValue &value, const std::string &context);
uint64_t unsigned64ValueFromJson(const JsonValue &json);
std::string rawFormatDescriptor(const SgFormatItem::Descriptor &descriptor);
struct FormatItemFields {
  SgFormatItem::Descriptor descriptor;
  std::optional<std::uint64_t> repeat_count;
};
FormatItemFields formatItemFieldsFromJson(const JsonValue &properties);
SgCxxSubobjectDestruction::target_kind_enum
subobjectDestructionKindFromJson(const JsonValue &json);
SgCastExp::value_category_enum
arrayInitBindingCategoryFromJson(const JsonValue &json);
struct ArrayInitReferenceFields {
  SgCxxArrayInitReference::reference_kind_enum kind;
  SgCastExp::value_category_enum category;
};
ArrayInitReferenceFields
arrayInitReferenceFieldsFromJson(const JsonValue &json);

struct OpaqueValueFields {
  SgCastExp::value_category_enum category;
  SgOpaqueValueBinding::object_kind_enum object_kind;
};
OpaqueValueFields opaqueValueFieldsFromJson(const JsonValue &json);

class OpaqueBindingScope {
  std::vector<SgOpaqueValueBinding *> &bindings_;
  unsigned &depth_;
  const size_t size_;

public:
  OpaqueBindingScope(std::vector<SgOpaqueValueBinding *> &bindings,
                     unsigned &depth)
      : bindings_(bindings), depth_(depth), size_(bindings.size()) {
    ++depth_;
  }
  ~OpaqueBindingScope() {
    bindings_.resize(size_);
    --depth_;
  }
  OpaqueBindingScope(const OpaqueBindingScope &) = delete;
  OpaqueBindingScope &operator=(const OpaqueBindingScope &) = delete;
};

class ArrayInitBindingScope {
  std::vector<SgCxxArrayInitBinding *> &bindings_;

public:
  ArrayInitBindingScope(std::vector<SgCxxArrayInitBinding *> &bindings,
                        SgCxxArrayInitBinding *binding)
      : bindings_(bindings) {
    bindings_.push_back(binding);
  }
  ~ArrayInitBindingScope() { bindings_.pop_back(); }
  ArrayInitBindingScope(const ArrayInitBindingScope &) = delete;
  ArrayInitBindingScope &operator=(const ArrayInitBindingScope &) = delete;
};

class InlineExpressionConstruction {
  struct DeleteLiteralTokens {
    void operator()(SgCxxLiteralTokenList *tokens) const;
  };
  std::list<std::unique_ptr<SgCxxLiteralTokenList, DeleteLiteralTokens>>
      tokens_;
  std::list<SgNode *> owned_;
  std::map<SgNode *, SgNode *> borrowed_parents_;
  void disconnect(SgNode *node) const;

public:
  InlineExpressionConstruction() = default;
  ~InlineExpressionConstruction();
  InlineExpressionConstruction(const InlineExpressionConstruction &) = delete;
  InlineExpressionConstruction &
  operator=(const InlineExpressionConstruction &) = delete;
  SgNode *own(SgNode *node);
  template <class Node> Node *borrow(Node *node) {
    ROSE_ASSERT(node != nullptr);
    borrowed_parents_.emplace(node, node->get_parent());
    return node;
  }
  SgCxxLiteralTokenList *ownTokens(SgCxxLiteralTokenList *tokens);
  bool owns(SgNode *node) const {
    return std::find(owned_.begin(), owned_.end(), node) != owned_.end();
  }
  bool empty() const { return owned_.empty(); }
  void transferTo(InlineExpressionConstruction &owner) noexcept;
  void forgetDeletedNodes(const std::unordered_set<SgNode *> &nodes);
  void publish();
  void detachTypeShapeBorrows();
};
class SemanticArrayConstruction {
  struct State;
  std::unique_ptr<State> state_;
  friend class SemanticArrayConstructionScope;

public:
  SemanticArrayConstruction();
  ~SemanticArrayConstruction();
  void prepareRollback();
  void publish();
  void take(SgArrayType *type,
            std::shared_ptr<InlineExpressionConstruction> shape);
  static SemanticArrayConstruction *current();
  void preparePublish();
  void transferTo(InlineExpressionConstruction &ownership);
  bool owns(SgArrayType *type) const;
  void forgetDeletedNodes(const std::unordered_set<SgNode *> &nodes);
};

class SemanticArrayConstructionScope {
  SemanticArrayConstruction *previous_;

public:
  explicit SemanticArrayConstructionScope(SemanticArrayConstruction *owner);
  ~SemanticArrayConstructionScope();
};
bool semanticArrayDependenciesReady(const JsonValue &json,
                                    const SemanticArrayConstruction *owner);

class SourceTypeConstruction {
  SourceTypeConstruction *previous_;
  SemanticArrayConstruction arrays_;
  SemanticArrayConstructionScope array_scope_{&arrays_};
  InlineExpressionConstruction ownership_;
  std::vector<uint64_t> identities_;
  static void forgetDeletedNodes(const std::unordered_set<SgNode *> &nodes);

public:
  static SourceTypeConstruction *&current();
  SourceTypeConstruction();
  SourceTypeConstruction(const SourceTypeConstruction &) = delete;
  SourceTypeConstruction &operator=(const SourceTypeConstruction &) = delete;
  ~SourceTypeConstruction();
  void take(InlineExpressionConstruction &nodes);
  SgType *own(SgType *type);
  void recordIdentity(uint64_t identity, const JsonValue &json, SgType *type);
  void preparePublish();
  void publish();
  SgType *publish(SgType *type);
  static void discardAst(SgNode *root);
  static void discardDetachedNode(SgNode *root);
};

std::string rawExtendedFloatingValue(Rose::FloatingValue value);
Rose::FloatingValue extendedFloatingValueFromJson(const JsonValue &json);

inline bool requiredIntegerBoolean(const JsonValue &object,
                                   const std::string &field) {
  const int64_t value = object.requiredInt(field);
  if (value != 0 && value != 1)
    throw std::runtime_error("AST JSON " + field + " must be 0 or 1");
  return value == 1;
}

template <typename Enum>
inline Enum requiredEnum(const JsonValue &object, const std::string &field,
                         const std::string &context,
                         std::initializer_list<Enum> legal_values,
                         const std::string &field_description = {}) {
  static_assert(std::is_enum_v<Enum>,
                "AST JSON enum decoder requires an enum type");
  const int64_t raw = object.requiredInt(field);
  for (Enum legal_value : legal_values) {
    if (raw == static_cast<int64_t>(legal_value)) {
      return legal_value;
    }
  }
  throw std::runtime_error(
      "AST JSON " + context + " has an invalid " +
      (field_description.empty() ? field : field_description) + " enum value");
}

template <typename Enum>
inline Enum
requiredClosedEnumRange(const JsonValue &object, const std::string &field,
                        const std::string &context, Enum first, Enum last) {
  static_assert(std::is_enum_v<Enum>,
                "AST JSON closed-range decoder requires an enum type");
  const int64_t raw = object.requiredInt(field);
  if (raw <= static_cast<int64_t>(first) || raw >= static_cast<int64_t>(last)) {
    throw std::runtime_error("AST JSON " + context + " has an invalid " +
                             field + " enum value");
  }
  return static_cast<Enum>(raw);
}

inline SgInitializedName::fortran_c_argument_passing_enum
fortranCArgumentPassingFromJson(const JsonValue &properties) {
  using Name = SgInitializedName;
  return requiredEnum<Name::fortran_c_argument_passing_enum>(
      properties, "fortran_c_argument_passing", "initialized name",
      {Name::e_fortran_c_argument_none, Name::e_fortran_c_argument_value,
       Name::e_fortran_c_argument_reference,
       Name::e_fortran_c_argument_descriptor,
       Name::e_fortran_c_argument_procedure,
       Name::e_fortran_c_argument_procedure_pointer});
}

inline SgClassDeclaration::class_types
requiredClassType(const JsonValue &object, const std::string &context) {
  return requiredEnum<SgClassDeclaration::class_types>(
      object, "class_type", context,
      {SgClassDeclaration::e_class, SgClassDeclaration::e_struct,
       SgClassDeclaration::e_union, SgClassDeclaration::e_template_parameter,
       SgClassDeclaration::e_fortran_module,
       SgClassDeclaration::e_lambda_closure});
}

inline SgStorageModifier::storage_modifier_enum
requiredStorageModifier(const JsonValue &object, const std::string &field,
                        const std::string &context) {
  return requiredEnum<SgStorageModifier::storage_modifier_enum>(
      object, field, context,
      {SgStorageModifier::e_default,
       SgStorageModifier::e_extern,
       SgStorageModifier::e_static,
       SgStorageModifier::e_auto,
       SgStorageModifier::e_unspecified,
       SgStorageModifier::e_register,
       SgStorageModifier::e_mutable,
       SgStorageModifier::e_typedef,
       SgStorageModifier::e_asm,
#ifdef FORTRAN_SUPPORTED
       SgStorageModifier::e_local,
       SgStorageModifier::e_common,
       SgStorageModifier::e_associated,
       SgStorageModifier::e_intrinsic,
       SgStorageModifier::e_pointer_based,
#endif
       SgStorageModifier::e_contiguous,
       SgStorageModifier::e_cuda_global,
       SgStorageModifier::e_cuda_constant,
       SgStorageModifier::e_cuda_shared,
       SgStorageModifier::e_cuda_dynamic_shared,
       SgStorageModifier::e_cuda_device_memory,
       SgStorageModifier::e_cuda_managed,
       SgStorageModifier::e_cuda_unified,
       SgStorageModifier::e_cuda_pinned,
       SgStorageModifier::e_cuda_texture});
}

inline SgAccessModifier::access_modifier_enum
requiredDeclarationAccessModifier(const JsonValue &object,
                                  const std::string &field,
                                  const std::string &context) {
  return requiredEnum<SgAccessModifier::access_modifier_enum>(
      object, field, context,
      {SgAccessModifier::e_private, SgAccessModifier::e_protected,
       SgAccessModifier::e_public, SgAccessModifier::e_default,
       SgAccessModifier::e_not_applicable, SgAccessModifier::e_undefined});
}

inline SgAccessModifier::access_modifier_enum
requiredBaseClassAccessModifier(const JsonValue &object,
                                const std::string &field,
                                const std::string &context) {
  return requiredEnum<SgAccessModifier::access_modifier_enum>(
      object, field, context,
      {SgAccessModifier::e_private, SgAccessModifier::e_protected,
       SgAccessModifier::e_public, SgAccessModifier::e_default,
       SgAccessModifier::e_undefined});
}

inline SgInitializedName::fortran_type_spec_enum
requiredFortranTypeSpec(const JsonValue &object, const std::string &context) {
  return requiredEnum<SgInitializedName::fortran_type_spec_enum>(
      object, "fortran_type_spec", context,
      {SgInitializedName::e_fortran_type_spec_default,
       SgInitializedName::e_fortran_type_spec_type,
       SgInitializedName::e_fortran_type_spec_class,
       SgInitializedName::e_fortran_type_spec_type_star,
       SgInitializedName::e_fortran_type_spec_class_star});
}

inline SgInitializedName::preinitialization_enum
requiredPreinitialization(const JsonValue &object, const std::string &context) {
  return requiredEnum<SgInitializedName::preinitialization_enum>(
      object, "preinitialization", context,
      {SgInitializedName::e_unknown_preinitialization,
       SgInitializedName::e_virtual_base_class,
       SgInitializedName::e_nonvirtual_base_class,
       SgInitializedName::e_data_member,
       SgInitializedName::e_delegation_constructor});
}

struct InitializedNameRoles {
  SgInitializedName::enum_constant_source_ownership_enum ownership;
  std::optional<long long> enum_value;
  SgInitializedName::preinitialization_enum preinitialization;
  std::optional<unsigned int> ctor_source_order;
  uint64_t ctor_field_target;
};
InitializedNameRoles initializedNameRolesFromJson(const JsonValue &properties);

inline SgInitializedName::structured_binding_kind_enum
structuredBindingKindFromJson(const JsonValue &properties) {
  const auto kind = properties.requiredInt("structured_binding_kind");
  if (kind < SgInitializedName::e_not_structured_binding ||
      kind > SgInitializedName::e_structured_binding_holding_variable)
    throw std::runtime_error("AST JSON structured-binding kind is invalid");
  return static_cast<SgInitializedName::structured_binding_kind_enum>(kind);
}

SgStringList stringListFromJson(const JsonValue &value,
                                const std::string &context);

std::optional<SgInitializedName::FortranProcedureComponentBinding>
fortranProcedureComponentBindingFromJson(const JsonValue &properties);
void requireFortranProcedureComponentBinding(
    const std::optional<SgInitializedName::FortranProcedureComponentBinding>
        &binding,
    SgType *type, SgScopeStatement *scope);

inline std::optional<long long> storageSizeFromJson(const JsonValue &properties,
                                                    const std::string &field) {
  const auto &value = properties.at(field);
  if (value.kind == JsonValue::Kind::Null)
    return std::nullopt;
  const auto size = value.asInt();
  if (size < 0)
    throw std::runtime_error("AST JSON storage size is negative");
  return size;
}

std::optional<unsigned int>
requiredOptionalUnsignedInteger(const JsonValue &properties,
                                const std::string &name,
                                const std::string &context);

struct InitializedNameMetadata {
  std::optional<long long> storage_size_bytes;
  std::optional<unsigned int> cxx_field_index;
  std::optional<SgInitializedName::FortranProcedureComponentBinding>
      procedure_component_binding;
  SgInitializedName::fortran_type_spec_enum fortran_type_spec;
  SgInitializedName::fortran_c_argument_passing_enum argument_passing;
  bool predefined_identifier;
  bool braced_initialized;
  bool assignment_copy_constructor_syntax;
  bool parameter_pack;
  bool pack_element;
  bool deduction_guide_default_available;
  SgInitializedName::generated_variable_role_enum generated_role;
  int name_qualification_length;
  bool type_elaboration_required;
  bool global_qualification_required;
  int name_qualification_length_for_type;
  bool type_elaboration_required_for_type;
  bool global_qualification_required_for_type;
  bool source_type_present;
  bool source_type_global;
  SgStringList source_type_tokens;
  bool source_name_present;
  bool source_name_global;
  SgStringList source_name_tokens;
  SgStorageModifier::storage_modifier_enum storage_modifier;
  std::string section;

  void initialize(SgInitializedName *name) const;
};

bool validFortranSeparatePointerOwnership(const SgInitializedName *name);

inline void
requireFortranProcedureInterface(SgExpression *interface, SgType *type,
                                 SgInitializedName::fortran_type_spec_enum spec,
                                 SgSymbol *source_symbol) {
  if (!SgInitializedName::is_valid_fortran_procedure_interface(interface, type))
    throw std::runtime_error("AST JSON initializedName has a contradictory "
                             "procedure interface");
  if (interface == nullptr)
    return;
  SgType *represented = nullptr;
  if (auto *expression = isSgTypeExpression(interface))
    represented = expression->get_represented_type();
  if (isSgClassType(represented)) {
    if ((spec != SgInitializedName::e_fortran_type_spec_type &&
         spec != SgInitializedName::e_fortran_type_spec_class) ||
        source_symbol == nullptr || source_symbol->get_type() != represented)
      throw std::runtime_error("AST JSON derived procedure interface has no "
                               "exact source type binding");
  } else {
    const auto expected =
        isSgTypeFortranAssumed(represented)
            ? SgInitializedName::e_fortran_type_spec_type_star
        : isSgTypeFortranUnlimitedPolymorphic(represented)
            ? SgInitializedName::e_fortran_type_spec_class_star
            : SgInitializedName::e_fortran_type_spec_default;
    if (spec != expected || source_symbol != nullptr)
      throw std::runtime_error("AST JSON procedure interface has contradictory "
                               "source type-spec metadata");
  }
}

inline InitializedNameMetadata
initializedNameMetadataFromJson(const JsonValue &properties) {
  (void)properties.at("fortran_procedure_interface");
  const bool source_type_present =
      properties.requiredBool("source_type_qualification_present");
  const bool source_type_global =
      properties.requiredBool("source_type_global_qualification");
  const SgStringList source_type_tokens =
      stringListFromJson(properties.at("source_type_qualification_tokens"),
                         "source_type_qualification_tokens");
  if (!source_type_present &&
      (source_type_global || !source_type_tokens.empty()))
    throw std::runtime_error("AST JSON initialized-name source type qualifier "
                             "payload is present without its presence bit");
  const bool source_name_present =
      properties.requiredBool("source_name_qualification_present");
  const bool source_name_global =
      properties.requiredBool("source_name_global_qualification");
  const SgStringList source_name_tokens =
      stringListFromJson(properties.at("source_name_qualification_tokens"),
                         "source_name_qualification_tokens");
  if (!source_name_present &&
      (source_name_global || !source_name_tokens.empty()))
    throw std::runtime_error("AST JSON initialized-name source name qualifier "
                             "payload is present without its presence bit");
  return {
      storageSizeFromJson(properties, "storage_size_bytes"),
      requiredOptionalUnsignedInteger(properties, "cxx_field_index",
                                      "SgInitializedName"),
      fortranProcedureComponentBindingFromJson(properties),
      requiredFortranTypeSpec(properties, "SgInitializedName"),
      fortranCArgumentPassingFromJson(properties),
      properties.requiredBool("is_predefined_identifier"),
      properties.requiredBool("is_braced_initialized"),
      properties.requiredBool("using_assignment_copy_constructor_syntax"),
      properties.requiredBool("is_parameter_pack"),
      properties.requiredBool("is_pack_element"),
      properties.requiredBool("deduction_guide_default_available"),
      requiredEnum<SgInitializedName::generated_variable_role_enum>(
          properties, "generated_variable_role", "SgInitializedName",
          {SgInitializedName::e_generated_variable_none,
           SgInitializedName::e_generated_loop_tiling_index,
           SgInitializedName::e_generated_loop_tiling_increment}),
      properties.requiredNonnegativeInt("name_qualification_length"),
      properties.requiredBool("type_elaboration_required"),
      properties.requiredBool("global_qualification_required"),
      properties.requiredNonnegativeInt("name_qualification_length_for_type"),
      properties.requiredBool("type_elaboration_required_for_type"),
      properties.requiredBool("global_qualification_required_for_type"),
      source_type_present,
      source_type_global,
      source_type_tokens,
      source_name_present,
      source_name_global,
      source_name_tokens,
      requiredStorageModifier(properties, "storage_modifier",
                              "SgInitializedName"),
      properties.requiredString("gnu_attribute_section_name")};
}

inline void validateTypedefBaseShape(const JsonValue &properties) {
  const JsonValue &base = properties.at("base_type");
  if (!base.requiredBool("present")) {
    throw std::runtime_error(
        "AST JSON typedef construction requires a present base type");
  }
  const std::string base_kind = base.requiredString("kind");
  if (base_kind.empty() || base_kind == "SgTypeUnknown" ||
      base_kind == "SgTypeDefault") {
    throw std::runtime_error(
        "AST JSON typedef construction requires an exact base type");
  }
}

struct TypedefMetadata {
  SgTypedefDeclaration::typedef_type_enum kind;
  bool base_contains_defining_declaration;
  bool autonomous;
  bool elaboration_required;
  bool qualification_present;
  bool global_qualification;
  SgStringList qualification_tokens;
};

inline TypedefMetadata typedefMetadataFromJson(const JsonValue &properties) {
  const auto kind = requiredEnum<SgTypedefDeclaration::typedef_type_enum>(
      properties, "typedef_type", "SgTypedefDeclaration",
      {SgTypedefDeclaration::e_typedef, SgTypedefDeclaration::e_using});
  const bool base_contains_defining_declaration = properties.requiredBool(
      "typedef_base_type_contains_defining_declaration");
  const bool autonomous = properties.requiredBool("is_autonomous_declaration");
  const bool elaboration =
      properties.requiredBool("type_elaboration_required_for_base_type");
  const bool present =
      properties.requiredBool("source_base_type_qualification_present");
  const bool global =
      properties.requiredBool("source_base_type_global_qualification");
  const SgStringList tokens =
      stringListFromJson(properties.at("source_base_type_qualification_tokens"),
                         "source_base_type_qualification_tokens");
  if (!present && (global || !tokens.empty()))
    throw std::runtime_error("AST JSON typedef source base-type qualifier "
                             "payload is present without its presence bit");
  return {kind,       base_contains_defining_declaration,
          autonomous, elaboration,
          present,    global,
          tokens};
}

inline SgTemplateArgument::template_argument_enum
requiredTemplateArgumentType(const JsonValue &object,
                             const std::string &context) {
  return requiredEnum<SgTemplateArgument::template_argument_enum>(
      object, "argument_type", context,
      {SgTemplateArgument::type_argument, SgTemplateArgument::nontype_argument,
       SgTemplateArgument::template_template_argument,
       SgTemplateArgument::start_of_pack_expansion_argument});
}

inline SgTemplateParameter::template_parameter_enum
requiredTemplateParameterType(const JsonValue &object,
                              const std::string &context) {
  return requiredEnum<SgTemplateParameter::template_parameter_enum>(
      object, "parameter_type", context,
      {SgTemplateParameter::type_parameter,
       SgTemplateParameter::nontype_parameter,
       SgTemplateParameter::template_parameter});
}

struct TemplateParameterSourceFields {
  SgTemplateParameter::template_parameter_enum kind;
  SgTemplateParameter::template_parameter_keyword_enum keyword;
  bool abbreviated;
  bool pack;
  int64_t canonicalNonTypeParameter;
  std::optional<SgTemplateParameter::canonical_source_identity> sourceIdentity;
};

std::optional<SgTemplateParameter::canonical_source_identity>
canonicalSourceIdentityFromJson(const JsonValue &object, const char *owner);

inline TemplateParameterSourceFields
templateParameterSourceFieldsFromJson(const JsonValue &object) {
  TemplateParameterSourceFields fields{
      requiredTemplateParameterType(object, "SgTemplateParameter"),
      requiredEnum<SgTemplateParameter::template_parameter_keyword_enum>(
          object, "template_parameter_keyword", "SgTemplateParameter",
          {SgTemplateParameter::keyword_unspecified,
           SgTemplateParameter::keyword_class,
           SgTemplateParameter::keyword_typename}),
      object.requiredBool("is_abbreviated_function_template_parameter"),
      object.requiredBool("is_parameter_pack"),
      object.requiredInt("canonical_nontype_parameter"),
      canonicalSourceIdentityFromJson(object, "SgTemplateParameter")};
  if ((fields.kind == SgTemplateParameter::nontype_parameter) !=
      (fields.keyword == SgTemplateParameter::keyword_unspecified))
    throw std::runtime_error(
        "AST JSON template parameter kind and source keyword are incompatible");
  if (object.find("expression") != nullptr)
    throw std::runtime_error(
        "AST JSON template parameter cannot own an expression payload");
  if (fields.kind == SgTemplateParameter::nontype_parameter
          ? fields.canonicalNonTypeParameter <= 0
          : fields.canonicalNonTypeParameter != 0)
    throw std::runtime_error(
        "AST JSON template parameter has invalid canonical non-type identity");
  if (fields.kind != SgTemplateParameter::nontype_parameter &&
      fields.sourceIdentity.has_value())
    throw std::runtime_error(
        "AST JSON non-value template parameter has a value source identity");
  if (fields.kind != SgTemplateParameter::nontype_parameter) {
    const JsonValue &type = object.at("type");
    if (!type.requiredBool("present") ||
        type.requiredString("kind") != "SgTemplateType") {
      throw std::runtime_error(
          "AST JSON type parameter requires an exact template-parameter type");
    }
  }
  if (object.at("default_type_parameter").requiredBool("present") &&
      fields.kind != SgTemplateParameter::type_parameter)
    throw std::runtime_error(
        "AST JSON non-type/template parameter owns a type default");
  const JsonValue &default_expression =
      object.at("default_expression_parameter");
  if (default_expression.find("owned_kind") != nullptr ||
      (default_expression.requiredNonnegativeNodeId("node") != 0 &&
       fields.kind != SgTemplateParameter::nontype_parameter))
    throw std::runtime_error(
        "AST JSON template parameter has an invalid expression default");
  if (object.requiredNonnegativeNodeId("default_template_argument") != 0 &&
      fields.kind != SgTemplateParameter::template_parameter)
    throw std::runtime_error(
        "AST JSON non-template parameter owns a template default");
  if (object.requiredNonnegativeNodeId("source_spelled_template_declaration") !=
          0 &&
      fields.kind != SgTemplateParameter::template_parameter)
    throw std::runtime_error(
        "AST JSON non-template parameter owns a source-spelled template "
        "declaration");
  return fields;
}

inline SgDeclarationModifier::gnu_declaration_visibility_enum
requiredGnuDeclarationVisibility(const JsonValue &object,
                                 const std::string &field,
                                 const std::string &context) {
  const int64_t raw = object.requiredInt(field);
  switch (raw) {
  case SgDeclarationModifier::e_unspecified_visibility:
  case SgDeclarationModifier::e_hidden_visibility:
  case SgDeclarationModifier::e_protected_visibility:
  case SgDeclarationModifier::e_internal_visibility:
  case SgDeclarationModifier::e_default_visibility:
    return static_cast<SgDeclarationModifier::gnu_declaration_visibility_enum>(
        raw);
  default:
    throw std::runtime_error("AST JSON " + context + " " + field +
                             " is not a valid source visibility");
  }
}

struct EdgeRecord {
  std::string field;
  uint64_t target = 0;
  uint64_t index = 0;
};

struct NodeRecord {
  uint64_t id = 0;
  std::string kind;
  VariantT variant = V_SgNode;
  JsonValue flags;
  JsonValue location;
  JsonValue properties;
  JsonValue preprocessing;
  std::vector<EdgeRecord> edges;
};

struct AstFileRecord {
  uint64_t root_id = 0;
  uint64_t project_files_id = 0;
  std::string root_kind;
  std::string checkpoint;
  JsonValue metadata;
  std::vector<NodeRecord> nodes;
  std::unordered_map<uint64_t, size_t> index_by_id;

  const NodeRecord &node(uint64_t id) const;
};

using NodeMap = std::unordered_map<uint64_t, SgNode *>;

using NodeIds = std::unordered_map<SgNode *, uint64_t>;

struct TemplateParameterOperands {
  SgExpression *expression;
  SgTemplateArgument *argument;
  SgTemplateDeclaration *sourceDeclaration;
};
TemplateParameterOperands
templateParameterOperandsFromRecord(const NodeRecord &record,
                                    const NodeMap &nodes,
                                    const SgTemplateParameter *owner = nullptr);

inline NodeIds mappedNodePointers(const NodeMap &nodes) {
  NodeIds result;
  result.reserve(nodes.size());
  for (const auto &entry : nodes)
    result.emplace(entry.second, entry.first);
  return result;
}

// A record's node and constructor-created children remain private until
// every required property has been decoded. Borrowed types and enclosing
// scopes are not owned by this construction transaction.
class RecordNodeConstruction {
  using Owner =
      std::unique_ptr<SgNode,
                      decltype(&SourceTypeConstruction::discardDetachedNode)>;
  std::vector<Owner> owned_;
  std::unordered_set<SgNode *> borrowed_;
  const NodeIds *mapped_nodes_ = nullptr;
  bool contains(SgNode *node) const {
    return std::any_of(owned_.begin(), owned_.end(), [node](const auto &owner) {
      return owner.get() == node;
    });
  }

public:
  RecordNodeConstruction() = default;
  explicit RecordNodeConstruction(const NodeIds &mapped_nodes)
      : mapped_nodes_(&mapped_nodes) {}
  void borrow(SgNode *node) {
    if (node == nullptr || node->get_parent() != nullptr)
      throw std::runtime_error(
          "AST JSON node construction requires an unowned child");
    borrowed_.insert(node);
  }
  bool isBorrowed(SgNode *node) const {
    return borrowed_.count(node) != 0 ||
           (mapped_nodes_ != nullptr && mapped_nodes_->count(node) != 0);
  }
  SgNode *own(SgNode *node) {
    if (contains(node))
      return node;
    ROSE_ASSERT(node != nullptr);
    Owner pending(node, &SourceTypeConstruction::discardDetachedNode);
    owned_.push_back(std::move(pending));
    class Children : public SimpleReferenceToPointerHandler {
      RecordNodeConstruction &construction_;
      SgNode *parent_;
      std::vector<SgNode *> native_owned_;

    public:
      Children(RecordNodeConstruction &construction, SgNode *parent)
          : construction_(construction), parent_(parent) {
        parent->appendDestructorOwnedNodes(native_owned_);
      }
      void operator()(SgNode *&edge, const SgName &, bool) override {
        if (edge != nullptr && edge != parent_ &&
            edge->get_parent() == parent_ && !isSg_File_Info(edge)) {
          if (construction_.isBorrowed(edge))
            construction_.borrowed_.insert(edge);
          else if (std::find(native_owned_.begin(), native_owned_.end(),
                             edge) == native_owned_.end())
            construction_.own(edge);
        }
      }
    } children(*this, node);
    node->processDataMemberReferenceToPointers(&children);
    return node;
  }
  template <class Node, class... Args> Node *make(Args &&...args) {
    auto *node = new Node(std::forward<Args>(args)...);
    return static_cast<Node *>(own(node));
  }
  void publish() {
    for (auto &node : owned_)
      node.release();
    owned_.clear();
    borrowed_.clear();
  }
  ~RecordNodeConstruction() {
    for (const auto &owned : owned_) {
      if (auto *definition = isSgClassDefinition(owned.get())) {
        auto *declaration = isSgClassDeclaration(definition->get_parent());
        if (declaration != nullptr &&
            declaration->get_definition() == definition)
          declaration->set_definition(nullptr);
      } else if (auto *definition = isSgFunctionDefinition(owned.get())) {
        auto *declaration = isSgFunctionDeclaration(definition->get_parent());
        if (declaration != nullptr &&
            declaration->get_definition() == definition)
          declaration->set_definition(nullptr);
      }
    }
    for (SgNode *node : borrowed_) {
      if (contains(node->get_parent()))
        node->set_parent(nullptr);
    }
    class Disconnect : public SimpleReferenceToPointerHandler {
      const RecordNodeConstruction &construction_;

    public:
      explicit Disconnect(const RecordNodeConstruction &construction)
          : construction_(construction) {}
      void operator()(SgNode *&edge, const SgName &, bool) override {
        if (construction_.contains(edge) || construction_.isBorrowed(edge))
          edge = nullptr;
      }
    } disconnect(*this);
    for (const auto &node : owned_)
      node->processDataMemberReferenceToPointers(&disconnect);
  }
};

class SourceDeclaratorScopeConstruction {
  SgDeclarationStatement *owner_ = nullptr;
  SgDeclarationScope *scope_ = nullptr;
  bool attached_ = false;
  bool populated_ = false;

public:
  SourceDeclaratorScopeConstruction(const NodeRecord &record, SgNode *node,
                                    const NodeMap &nodes);
  ~SourceDeclaratorScopeConstruction();
  SourceDeclaratorScopeConstruction(const SourceDeclaratorScopeConstruction &) =
      delete;
  SourceDeclaratorScopeConstruction &
  operator=(const SourceDeclaratorScopeConstruction &) = delete;
  void populate(const AstFileRecord &ast, const NodeRecord &record,
                const NodeMap &nodes);
  void publish() { owner_ = nullptr; }
};

struct SymbolTableEntryJson {
  std::string entry_name;
  std::string symbol_kind;
  uint64_t basis_id = 0;
  bool lookup_preferred = false;
  std::string json;
};

class TypeSerializationContext {
public:
  explicit TypeSerializationContext(const SgNode *node);
  ~TypeSerializationContext();

private:
  const SgNode *previous_;
};

class CollectionBoundaryGuard {
public:
  explicit CollectionBoundaryGuard(SgSourceFile *file,
                                   SgFileList *project_files = nullptr);
  ~CollectionBoundaryGuard();

private:
  SgSourceFile *previous_;
  SgFileList *previous_project_files_;
};

class SubtreeBoundaryGuard {
public:
  explicit SubtreeBoundaryGuard(SgNode *root);
  ~SubtreeBoundaryGuard();

  SubtreeBoundaryGuard(const SubtreeBoundaryGuard &) = delete;
  SubtreeBoundaryGuard &operator=(const SubtreeBoundaryGuard &) = delete;

private:
  SgNode *previous_;
};

class DeserializationProjectGuard {
public:
  DeserializationProjectGuard(SgProject *project, SgSourceFile *replaced_file,
                              bool replacing_project_files);
  ~DeserializationProjectGuard();

private:
  SgProject *previous_;
  SgSourceFile *previous_replaced_file_;
  bool previous_replacing_project_files_;
};

bool sameAstJsonPath(const std::string &lhs, const std::string &rhs);
bool sourceFileMatchesExternalRecord(SgSourceFile *file,
                                     const std::string &source_file);
bool sourceIsBeingReplaced(SgSourceFile *file);
std::vector<SgSourceFile *> currentDeserializationSourceFiles();
bool startsWith(const std::string &s, const std::string &prefix);
std::string sanitizePathComponent(std::string name);
uint64_t fnv1a64(const std::string &value);
std::string hex64(uint64_t value);
uint64_t processId();
std::vector<std::string> commandLine(const SgSourceFile *file);
bool argumentSelectsCheckpoint(const std::string &value, Checkpoint checkpoint);
bool hasCheckpointArgument(const SgSourceFile *file, Checkpoint checkpoint);

void validateFortranProgramNameMetadata(const std::string &opening_name,
                                        int64_t statement_kind,
                                        bool named_in_end_statement,
                                        const std::string &end_statement_name);
std::string commandLineValue(const SgSourceFile *file,
                             const std::string &option);
std::string defaultOutputDirectory(const SgSourceFile *file);
std::string trim(const std::string &input);
JsonValue parseJson(const std::string &json);
uint64_t exactJsonValueHash(const JsonValue &value);
std::string jsonString(const std::string &input);
void appendCanonicalJson(std::ostream &out, const JsonValue &value);
void indent(std::ostream &out, int level);
void writeStringField(std::ostream &out, int level, const char *name,
                      const std::string &value, bool comma = true);
void writeIntegerField(std::ostream &out, int level, const char *name,
                       int64_t value, bool comma = true);
void writeBoolField(std::ostream &out, int level, const char *name, bool value,
                    bool comma = true);
void requireFileIdMapping(int id, const std::string &filename,
                          const std::string &context);
std::string filenameForFileId(int id, const std::string &context);
std::string rawStringField(const std::string &name, const std::string &value);
std::string rawIntegerField(const std::string &name, int64_t value);
std::string
rawOptionalUnsignedIntegerField(const std::string &name,
                                const std::optional<unsigned int> &value);
std::string rawBoolField(const std::string &name, bool value);
std::string rawBitVectorJson(const SgBitVector &bits);
void appendRawTypeModifierFields(
    std::vector<std::string> &fields, const SgTypeModifier &modifier,
    const std::string &prefix = "declaration_type");
std::string rawStringListJson(const SgStringList &values);
void writeFileIdMapJson(std::ostream &out, int level, bool comma = true);
void writeRawObject(std::ostream &out, int level,
                    const std::vector<std::string> &fields, bool comma = true);

uint64_t idFor(const std::unordered_map<const SgNode *, uint64_t> &ids,
               const SgNode *node);
uint64_t canonicalTypedefDeclarationId(
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const SgTypedefDeclaration *decl);
bool edgeTargetIsInParentChain(const SgNode *node, const SgNode *target);
bool isRightHandSideOfMemberAccess(const SgNode *node);
bool isAnonymousDataMemberReference(SgVarRefExp *ref);
void validateAnonymousDataMemberReferenceQualification(SgVarRefExp *ref);
bool isAstJsonExternalFunction(SgFunctionDeclaration *decl);
bool isAstJsonExternalModule(SgModuleStatement *module);
bool isAstJsonExternalClassDeclaration(SgClassDeclaration *decl);
void markAstJsonExternalFunction(SgFunctionDeclaration *decl,
                                 const std::string &source_file);
void markAstJsonExternalModule(SgModuleStatement *module,
                               const std::string &source_file);
void markAstJsonExternalClassDeclaration(SgClassDeclaration *decl,
                                         const std::string &source_file);
void attachJsonNodeId(SgNode *node, uint64_t id);
uint64_t preservedJsonNodeId(SgNode *node);
uint64_t semanticArrayJsonIdentity(SgArrayType *type);
uint64_t preservedSemanticArrayJsonIdentity(const SgArrayType *type);
void attachSemanticArrayJsonIdentity(SgArrayType *type, uint64_t identity);
inline bool typeOwnsSourceJsonIdentity(const SgType *type) {
  return type != nullptr &&
         (type->get_fortran_source_syntax() || isSgAutoType(type) ||
          (((isSgPointerType(type) && !isSgPointerMemberType(type)) ||
            isSgReferenceType(type) || isSgRvalueReferenceType(type) ||
            isSgModifierType(type)) &&
           type->has_source_prototype()));
}
uint64_t sourceTypeJsonIdentity(SgType *type);
uint64_t preservedSourceTypeJsonIdentity(const SgType *type);
void attachSourceTypeJsonIdentity(SgType *type, uint64_t identity);
uint64_t pointerMemberJsonIdentity(SgPointerMemberType *type);
uint64_t preservedPointerMemberJsonIdentity(const SgPointerMemberType *type);
void attachPointerMemberJsonIdentity(SgPointerMemberType *type,
                                     uint64_t identity);
SgPointerType *buildCachedJsonPointerType(SgType *base);
SgType *requireUnaryTraitResultType(SgType *type, const std::string &context);
struct UnaryTraitSourceFields {
  bool elaboration;
  SgNonrealDecl::source_elaboration_kind_enum kind;
  std::optional<Rose::Integer128Value> evaluated_value;
};
UnaryTraitSourceFields
unaryTraitSourceFieldsFromJson(const JsonValue &properties,
                               const std::string &context);
SgExpression::unary_type_trait_kind_enum
unaryTraitKindFromProperties(const NodeRecord &record);
SgSourceLocationBuiltinExp::source_location_builtin_kind_enum
sourceLocationBuiltinKindFromJson(const JsonValue &properties);
SgSourceLocationBuiltinExp *
sourceLocationBuiltinFromProperties(const JsonValue &properties,
                                    const NodeMap &nodes);
struct NewConstructionFields {
  SgType *specified_type;
  SgPointerType *result_type;
  short global_specifier;
  bool type_id_is_parenthesized;
  unsigned int allocation_implicit_argument_count;
  SgNewExp::array_bound_source_form_enum array_bound_source_form;
};
SgNewExp::array_bound_source_form_enum
newArrayBoundSourceFormFromJson(const JsonValue &properties);
NewConstructionFields newConstructionFieldsFromJson(const JsonValue &properties,
                                                    const NodeMap &nodes);
SgNewExp *constructNewExpression(const NewConstructionFields &fields,
                                 SgExpression *array_bound,
                                 SgExprListExp *placement,
                                 SgConstructorInitializer *constructor,
                                 SgExpression *builtin,
                                 SgFunctionDeclaration *operator_declaration,
                                 const SgExpressionPtrList &placement_elements);
struct AssignmentInitializerSourceFields {
  SgAssignInitializer::assignment_initializer_source_form_enum source_form;
  bool braced;
};
AssignmentInitializerSourceFields
assignmentInitializerSourceFieldsFromJson(const JsonValue &properties);
struct AggregateConstructionFields {
  SgAggregateInitializer::aggregate_initializer_source_form_enum source_form;
  bool braced;
  SgType *type;
  bool has_explicit_type;
  SgType *explicit_type;
  SgSymbol *source_symbol;
  SgUnsignedCharList source_roles;
};
struct AsmOperandConstructionFields {
  SgAsmOp::constraint_info constraint;
  SgAsmOp::asm_operand_modifier_enum modifiers;
  std::string name;
};
struct AsmStatementConstructionFields {
  std::string assembly;
  bool extended, is_volatile;
  SgAsmStmt::AsmRegisterNameList clobbers;
};
AsmOperandConstructionFields
asmOperandFieldsFromJson(const JsonValue &properties);
AsmStatementConstructionFields
asmStatementFieldsFromJson(const JsonValue &properties);
SgAsmOp *constructAsmOperand(const AsmOperandConstructionFields &fields,
                             SgExpression *expression);
SgAsmStmt *constructAsmStatement(const AsmStatementConstructionFields &fields,
                                 const SgExpressionPtrList &operands);
SgAlignmentSpecifier::spelling_enum
alignmentSpellingFromProperties(const JsonValue &properties, bool has_operand);
SgAlignmentSpecifier *
alignmentSpecifierFromProperties(const JsonValue &properties,
                                 SgExpression *operand);
void constructExternalFunctionOwnedAttributes(
    SgFunctionDeclaration *declaration, const JsonValue &json,
    const NodeMap &nodes);
std::string rawAlignmentSpecifiersJson(
    const SgAlignmentSpecifierPtrList &specifiers, SgNode *owner,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
AggregateConstructionFields
aggregateConstructionFieldsFromJson(const JsonValue &properties,
                                    const NodeMap &nodes,
                                    SgUnsignedCharList source_roles);
SgUnsignedCharList
aggregateSourceElementRolesFromJson(const JsonValue &properties);
void validateFortranAggregateConstruction(
    const AggregateConstructionFields &fields);
SgAggregateInitializer *
constructAggregateInitializer(const AggregateConstructionFields &fields,
                              SgExprListExp *initializers,
                              const SgExpressionPtrList &elements);
SgConditionalExp::conditional_operator_kind_enum
conditionalOperatorKindFromJson(const JsonValue &properties);
SgChooseExpression::selection_kind_enum
chooseSelectionFromJson(const JsonValue &properties);
SgChooseExpression *constructChooseExpression(
    SgType *type, SgChooseExpression::selection_kind_enum selection,
    SgExpression *condition, SgExpression *true_expression,
    SgExpression *false_expression);
SgIfStmt::if_statement_kind_enum
ifStatementKindFromJson(const JsonValue &properties);
SgConditionalExp *
conditionalFromProperties(const JsonValue &properties, const NodeMap &nodes,
                          SgConditionalExp::conditional_operator_kind_enum kind,
                          SgExpression *condition,
                          SgExpression *true_expression,
                          SgExpression *false_expression);
SgTypeTraitBuiltinOperator *
typeTraitBuiltinFromProperties(const JsonValue &properties,
                               const NodeMap &nodes,
                               const SgExpressionPtrList &arguments);
enum class OperandRequirementKind { simple, compound, nested };
SgExpression *constructOperandRequirement(OperandRequirementKind kind,
                                          SgExpression *operand,
                                          SgExpression *type_constraint,
                                          bool noexcept_required);
SgTypeRequirement *typeRequirementFromProperties(const JsonValue &properties,
                                                 const NodeMap &nodes);
SgTemplateParameterVal *
templateParameterValueFromProperties(const JsonValue &properties,
                                     const NodeMap &nodes);
SgEnumVal *enumValueFromProperties(const JsonValue &properties,
                                   const NodeMap &nodes);
SgNullptrValExp *nullptrValueFromProperties(const JsonValue &properties,
                                            const NodeMap &nodes);
template <typename Identity>
void validateNonrealResolvedTargetIdentity(Identity function, Identity variable,
                                           Identity spelling_target) {
  if (function != Identity{} && function != spelling_target)
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp resolved function identity disagrees "
        "with its synthetic callable edge");
  if (variable != Identity{} && variable != spelling_target)
    throw std::runtime_error(
        "AST JSON SgNonrealRefExp resolved variable identity disagrees "
        "with its synthetic specialization edge");
}
SgNonrealRefExp *nonrealReferenceFromProperties(const JsonValue &properties,
                                                const NodeMap &nodes,
                                                SgTypeExpression *type_id);
SgThisExp *thisExpressionFromProperties(const JsonValue &properties,
                                        const NodeMap &nodes);
struct FoldConstructionFields {
  SgFoldExpression::operator_kind_enum operator_kind;
  bool is_left_associative;
};
SgAwaitExpression::coroutine_keyword_kind_enum
coroutineKeywordKindFromJson(const JsonValue &properties);
FoldConstructionFields
foldConstructionFieldsFromJson(const JsonValue &properties);

struct CastConstructionFields {
  SgCastExp::cast_type_enum cast_type;
  SgCastExp::semantic_conversion_kind_enum conversion_kind;
  SgCastExp::value_category_enum value_category;
};
CastConstructionFields
castConstructionFieldsFromJson(const JsonValue &properties);
bool constructorElisionFromProperties(const JsonValue &properties);
SgConstructorInitializer::argument_source_enum
constructorArgumentSourceFromProperties(const JsonValue &properties);
SgConstructorInitializer *constructorInitializerFromProperties(
    const JsonValue &properties, SgMemberFunctionDeclaration *declaration,
    SgExprListExp *arguments, const SgExpressionPtrList &elements,
    SgType *type);
SgCastExp *castExpressionFromProperties(const JsonValue &properties,
                                        const NodeMap &nodes,
                                        SgExpression *operand, SgType *type,
                                        const CastConstructionFields &fields);
void writeFileInfoJson(std::ostream &out, int level, const Sg_File_Info *info,
                       bool comma = true);
std::string rawFileInfoJson(const Sg_File_Info *info);
std::string rawLocationJson(SgNode *node);
std::string rawRequiredLocationJson(SgNode *node, const std::string &context);
std::string rawNodeFlagsJson(SgNode *node);
std::string rawTypeOwnedExpressionRef(
    SgExpression *expr,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawTypeOwnedExpressionListJson(
    const SgExpressionPtrList &expressions,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawTypeOwnedExprListExpJson(
    SgExprListExp *expr_list,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExpressionRef(SgExpression *expression,
                 const std::unordered_map<const SgNode *, uint64_t> &ids);
const SgNode *symbolBasis(const SgSymbol *symbol);
std::string symbolName(const SgSymbol *symbol);
std::string
rawSymbolRef(SgSymbol *symbol,
             const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExactBoundSymbolRef(SgSymbol *symbol,
                       const std::unordered_map<const SgNode *, uint64_t> &ids,
                       const SgScopeStatement *inline_scope = nullptr);
std::vector<std::string> rawVisibilitySymbolFields(
    SgSymbol *symbol, const std::unordered_map<const SgNode *, uint64_t> &ids,
    const SgScopeStatement *inline_scope = nullptr);
std::string
rawSymbolTableJson(SgScopeStatement *scope,
                   const std::unordered_map<const SgNode *, uint64_t> &ids);
bool symbolIsLookupPreferred(SgSymbolTable *table, const SgName &name,
                             SgSymbol *symbol);

uint64_t varRefSymbolDeclarationId(
    SgVarRefExp *ref, const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawTypeJson(SgType *type,
            const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawCanonicalSourceIdentityJson(
    const std::optional<SgTemplateParameter::canonical_source_identity>
        &identity,
    const char *owner);
class PointerMemberTypeSerializationIdentityGuard {
public:
  PointerMemberTypeSerializationIdentityGuard();
  ~PointerMemberTypeSerializationIdentityGuard();

  PointerMemberTypeSerializationIdentityGuard(
      const PointerMemberTypeSerializationIdentityGuard &) = delete;
  PointerMemberTypeSerializationIdentityGuard &
  operator=(const PointerMemberTypeSerializationIdentityGuard &) = delete;
};
class FullTypeSerializationIdentityGuard {
  bool owner_;

public:
  FullTypeSerializationIdentityGuard();
  ~FullTypeSerializationIdentityGuard();

  FullTypeSerializationIdentityGuard(
      const FullTypeSerializationIdentityGuard &) = delete;
  FullTypeSerializationIdentityGuard &
  operator=(const FullTypeSerializationIdentityGuard &) = delete;
};
std::string rawTemplateArgumentListJson(
    const SgTemplateArgumentPtrList &arguments,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawAstAttributesJson(SgNode *node);
void addExpressionType(std::vector<std::string> &fields, SgExpression *expr,
                       const std::unordered_map<const SgNode *, uint64_t> &ids);
void addExpressionQualificationFields(std::vector<std::string> &fields,
                                      SgExpression *expr);
struct ExpressionQualificationFields {
  int length;
  bool type_elaboration_required;
  bool global_qualification_required;
  int explicit_length = -1;
  bool explicit_present = false;
  bool explicit_global = false;
  std::vector<std::string> explicit_tokens;
  void initialize(SgExpression *expression) const;
};
std::optional<ExpressionQualificationFields>
expressionQualificationFieldsFromJson(const std::string &kind,
                                      const JsonValue &properties);
struct ExpressionSourceFields {
  std::optional<SgValueExp::literal_spelling_form_enum> spelling;
  std::optional<SgArrowExp::emission_role_enum> arrow_role;
  std::optional<bool> braced;
};
ExpressionSourceFields
expressionSourceFieldsFromJson(const std::string &kind,
                               const JsonValue &properties);
struct ExpressionConstructionFields {
  bool folded_value_available;
  std::int64_t folded_value;
  bool lvalue;
  bool need_paren;
  bool global_qualified;
  SgExpression::semantic_wrapper_mask_enum wrappers;
  uint64_t bound_temporary_destructor;
  SgExpression::temporary_storage_duration_enum materialized_storage_duration;
  uint64_t materialized_lifetime_owner;
  std::optional<ExpressionQualificationFields> qualification;
  ExpressionSourceFields source;
  void initialize(SgExpression *expression, const NodeMap &nodes) const;
};
ExpressionConstructionFields
expressionConstructionFieldsFromJson(const JsonValue &properties);
void validateExpressionLifetime(const ExpressionConstructionFields &fields,
                                const JsonValue &properties,
                                const NodeMap &nodes);
void initializeScalarLiteralType(SgValueExp *value, const JsonValue &properties,
                                 const NodeMap &nodes);
void initializeTemplateParameterTypes(SgTemplateParameter *parameter,
                                      const NodeRecord &record,
                                      const NodeMap &nodes);
void addLocatedPreprocessing(std::vector<std::string> &fields,
                             const SgLocatedNode *node);
bool insideCollectionBoundary(SgNode *node);
std::vector<SgNode *> collectNodes(SgNode *root);
std::string safeNodeText(SgNode *node);
std::string sourceFileNameForNode(SgNode *node);
std::string sourceFileNameForExternalFunction(SgFunctionDeclaration *decl);
std::string moduleNameForNode(SgNode *node);
bool classDeclarationHasDefinition(SgClassDeclaration *decl);
bool classDeclarationIsFirstNondefining(SgClassDeclaration *decl);
bool isStructuralAstChildOfParent(SgNode *node);
bool hasExternalMarkerAncestor(SgNode *node);
void validateTemplateParameterContract(const SgTemplateParameter *parameter,
                                       const std::string &context);
void validateOmpContextSelectorProperty(
    const SgOmpContextSelectorProperty *property,
    const SgOmpContextSelector *selector);
using OmpContextPropertyIdentity = std::variant<
    SgOmpClause::omp_when_context_kind_enum,
    SgOmpClause::omp_when_context_vendor_enum,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum,
    SgOmpClause::omp_requires_property_kind_enum,
    std::pair<SgOmpClause::omp_requires_property_kind_enum, std::string>,
    std::string, std::monostate>;
OmpContextPropertyIdentity validateOmpContextSelectorPropertyPayload(
    const SgOmpContextSelectorProperty *property,
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind);
void validateOmpContextPropertySelectorKind(
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind,
    bool has_expression, SgOmpClause::omp_when_context_kind_enum kind,
    SgOmpClause::omp_when_context_vendor_enum vendor,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum atomic_order,
    SgOmpClause::omp_requires_property_kind_enum requires_kind);
struct OmpContextPropertyFields {
  SgOmpClause::omp_when_context_kind_enum kind;
  SgOmpClause::omp_when_context_vendor_enum vendor;
  SgOmpClause::omp_atomic_default_mem_order_kind_enum atomic_order;
  SgOmpClause::omp_requires_property_kind_enum requires_kind;
  SgOmpClause::omp_atomic_default_mem_order_kind_enum requires_atomic_order;
  SgName requires_extension;
  void initialize(SgOmpContextSelectorProperty *property,
                  SgExpression *expression,
                  SgExpression *requires_expression) const;
};
OmpContextPropertyFields
contextPropertyFieldsFromRecord(const NodeRecord &record);
void validateOmpContextPropertyPayload(
    bool has_expression, SgOmpClause::omp_when_context_kind_enum kind,
    SgOmpClause::omp_when_context_vendor_enum vendor,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum atomic_order,
    SgOmpClause::omp_requires_property_kind_enum requires_kind,
    SgOmpClause::omp_atomic_default_mem_order_kind_enum requires_atomic_order,
    bool has_requires_expression, bool has_requires_extension);
void validateOmpContextSelector(const SgOmpContextSelector *selector);
struct OmpContextSelectorFields {
  SgOmpClause::omp_context_trait_selector_kind_enum kind;
  SgName implementation_name;
};
OmpContextSelectorFields
contextSelectorFieldsFromJson(const JsonValue &properties);
SgOmpClause::omp_context_selector_set_kind_enum
contextSelectorSetKindFromJson(const JsonValue &properties);
void validateOmpContextSelectorPayload(
    SgOmpClause::omp_context_trait_selector_kind_enum kind,
    const SgName &implementation_name, SgExpression *score,
    SgStatement *construct_directive,
    const SgOmpContextSelectorPropertyPtrList &properties);
void validateOmpContextSelectorShape(
    SgOmpClause::omp_context_trait_selector_kind_enum kind,
    const SgName &implementation_name, bool has_score, bool has_construct,
    size_t property_count);
void validateOmpContextSelectorSet(const SgOmpContextSelectorSet *set);
void validateOmpContextTraitInSet(
    SgOmpClause::omp_context_selector_set_kind_enum set_kind,
    SgOmpClause::omp_context_trait_selector_kind_enum selector_kind);
using OmpContextSelectorIdentity =
    std::variant<SgOmpClause::omp_context_trait_selector_kind_enum, VariantT,
                 std::string>;
OmpContextSelectorIdentity ompContextSelectorIdentity(
    SgOmpClause::omp_context_trait_selector_kind_enum kind,
    const SgName &implementation_name, std::optional<VariantT> construct_kind);
void validateOmpContextKindAny(bool has_any, size_t property_count,
                               size_t selector_count);
void validateOmpContextSelectorSetPayload(
    SgOmpClause::omp_context_selector_set_kind_enum kind,
    const SgOmpContextSelectorPtrList &selectors);
void validateOmpContextSelectorSets(const SgOmpContextSelectorSetPtrList &sets,
                                    const SgNode *owner);
void validateOmpContextSelectorSetsPayload(
    const SgOmpContextSelectorSetPtrList &sets);
bool expressionCarriesSemanticType(const SgExpression *expression);
void validateExactSemanticExpressionType(const SgExpression *expression,
                                         const SgType *type,
                                         const std::string &phase);
const JsonValue *validatedExpressionTypeProperty(const SgExpression *expression,
                                                 const JsonValue &properties);
const JsonValue *validatedExpressionTypeProperty(const NodeRecord &record);

std::string rawExternalClassDeclarationJson(SgClassDeclaration *decl);
std::string
rawExternalModuleJson(SgModuleStatement *module,
                      const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawExternalFunctionJson(SgFunctionDeclaration *decl,
                        const std::unordered_map<const SgNode *, uint64_t> &ids,
                        bool force_external = false);
std::string rawExternalFunctionParameterScopeJson(
    SgFunctionDeclaration *function, SgFunctionParameterScope *scope,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);
bool declarationNeedsExternalReferenceRecord(
    SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
SgFunctionDeclaration *externalFunctionParameterScopeOwner(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
bool functionParameterScopeNeedsExternalReferenceRecord(
    SgFunctionDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawExternalDeclarationReferenceJson(
    SgDeclarationStatement *decl, SgDeclarationStatement *owner,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string rawExternalInitializedNameJson(
    SgInitializedName *name,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);
void appendRawExternalDeclarationStatementFields(
    std::vector<std::string> &fields, SgDeclarationStatement *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &context);
std::string rawExternalVariableDeclarationJson(
    SgVariableDeclaration *decl,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);
std::string rawExternalUseStatementJson(
    SgUseStatement *stmt,
    const std::unordered_map<const SgNode *, uint64_t> &ids,
    const std::string &function_name);
bool isExternalUseModuleEdge(
    SgNode *source, SgNode *target, const std::string &field,
    const std::unordered_map<const SgNode *, uint64_t> &ids);
std::string
rawNodeProperties(SgNode *node,
                  const std::unordered_map<const SgNode *, uint64_t> &ids);

void writeNodeJson(std::ostream &out, SgNode *node,
                   const std::unordered_map<const SgNode *, uint64_t> &ids,
                   bool comma);
void writeJson(std::ostream &out, SgNode *root, Checkpoint checkpoint,
               SgSourceFile *file, SgFileList *project_files = nullptr,
               NodeMap *serialized_nodes = nullptr);
std::string buildJson(SgNode *root, Checkpoint checkpoint, SgSourceFile *file,
                      SgFileList *project_files = nullptr,
                      NodeMap *serialized_nodes = nullptr);

// Collects a writer's output in one string.  Unlike std::ostringstream::str(),
// it does not copy the text, so a checkpoint-sized document exists once.
std::string writeToString(const std::function<void(std::ostream &)> &write);

std::filesystem::path temporaryWritePath(const std::filesystem::path &path);
void writeFile(const std::filesystem::path &path,
               const std::function<void(std::ostream &)> &write);
void writeFile(const std::filesystem::path &path, const std::string &content);
std::string readFile(const std::filesystem::path &path);
AstFileRecord parseAstFileJson(const std::string &json,
                               const std::string &expected_checkpoint);
void writeSemanticSignature(std::ostream &out, const AstFileRecord &ast);
std::string semanticSignature(const AstFileRecord &ast);
bool semanticSignatureEquals(const AstFileRecord &ast,
                             const std::string &signature);
std::filesystem::path checkpointPath(SgSourceFile *file, Checkpoint checkpoint,
                                     const Options &options);
void replaceFileInProject(SgSourceFile *old_file, SgSourceFile *new_file);
void replaceProjectFiles(SgProject *project, uint64_t root_id,
                         uint64_t project_files_id, const NodeMap &original,
                         const NodeMap &restored);
SgProject *owningProject(SgSourceFile *file);
std::vector<std::string> commandLineFromJson(const JsonValue &properties);
void restoreFileIdMapFromMetadata(const JsonValue &metadata);

SgNode *nodeById(const NodeMap &nodes, uint64_t id);

template <typename T> T *nodeByIdAs(const NodeMap &nodes, uint64_t id) {
  SgNode *raw_node = nodeById(nodes, id);
  T *node = dynamic_cast<T *>(raw_node);
  if (node == nullptr) {
    throw std::runtime_error(
        "AST JSON node id has unexpected Sage type " + std::to_string(id) +
        ": expected " + typeid(T).name() + ", got " +
        (raw_node != nullptr ? raw_node->sage_class_name() : "<null>"));
  }
  return node;
}

std::optional<unsigned int>
requiredTranslationUnitSourceOrder(const JsonValue &properties,
                                   const std::string &context);
std::optional<unsigned int> requiredOmpDeclareVariantRegionOrdinal(
    const JsonValue &properties, const std::string &semantic_function_name,
    const std::string &context);
struct DeclarationSourceFields {
  std::optional<unsigned int> translation_unit_order;
  bool qualified;
  bool globally_qualified;
  SgStringList qualification_tokens;
  std::optional<SgVariableDeclaration::inline_kind> variable_inline;
};
struct DeclarationNameQualificationFields {
  int length;
  bool elaboration_required;
  bool globally_qualified;
};
DeclarationNameQualificationFields
declarationNameQualificationFieldsFromJson(const JsonValue &properties);
DeclarationSourceFields declarationSourceFieldsFromJson(
    const JsonValue &properties, const std::string &context,
    const std::string &qualifier_context = "declaration source qualifier",
    bool variable = false);
struct TypeModifierFields {
  SgBitVector bits;
  SgConstVolatileModifier::cv_modifier_enum const_volatile;
  SgElaboratedTypeModifier::elaborated_type_modifier_enum elaborated;
  int alignment;
  long sentinel;
  unsigned address_space;
  unsigned vector_size;
  void initialize(SgTypeModifier &modifier) const;
};
TypeModifierFields
typeModifierFieldsFromJson(const JsonValue &json, const std::string &context,
                           const std::string &prefix = "declaration_type");
struct DeclarationFields {
  DeclarationSourceFields source;
  std::optional<DeclarationNameQualificationFields> qualification;
  std::string linkage;
  SgBitVector modifier_bits;
  SgStorageModifier::storage_modifier_enum storage;
  bool thread_local_storage;
  SgAccessModifier::access_modifier_enum access;
  bool access_is_explicit;
  bool name_only;
  bool forward;
  bool extern_brace;
  bool skip_elaborate_type;
  std::string binding_label;
  bool binding_cdefined;
  bool binding_label_present;
  bool unparse_template_ast;
  SgDeclarationModifier::gnu_declaration_visibility_enum attribute_visibility;
  SgDeclarationModifier::gnu_declaration_visibility_enum type_visibility;
  TypeModifierFields type_modifier;
  void initialize(SgDeclarationStatement *declaration);
};
DeclarationFields declarationFieldsFromJson(
    const JsonValue &properties, VariantT kind, const std::string &context,
    const std::string &qualifier_context = "declaration source qualifier");
void rejectRemovedQualifiedNameState(const JsonValue &properties);
void requireRestoredKind(SgNode *node, const NodeRecord &record);
std::optional<VariantT>
ordinaryOmpVariablesClauseVariant(const std::string &kind);
std::optional<VariantT> ompContextHierarchyVariant(const std::string &kind);
bool requiresDelayedRebuild(const NodeRecord &record);
bool isJsonScalarLiteralKind(const std::string &kind);
bool isJsonValueExpressionKind(const std::string &kind);
inline bool kindInVariants(const std::string &kind,
                           const VariantVector &variants) {
  return std::any_of(variants.begin(), variants.end(), [&](VariantT variant) {
    return kind == roseGlobalVariantNameList[variant];
  });
}
SgValueExp::literal_spelling_form_enum
literalSpellingFormFromJson(const JsonValue &properties);
SgTemplateFunctionRefExp *
templateFunctionReferenceFromProperties(const JsonValue &properties,
                                        const NodeMap &nodes);
SgTemplateMemberFunctionRefExp *
templateMemberFunctionReferenceFromProperties(const JsonValue &properties,
                                              const NodeMap &nodes,
                                              SgTypeExpression *type_id);
SgMemberFunctionRefExp *
memberFunctionReferenceFromProperties(const JsonValue &properties,
                                      const NodeMap &nodes,
                                      SgTypeExpression *type_id);
SgFunctionRefExp *functionReferenceFromProperties(const JsonValue &properties,
                                                  const NodeMap &nodes);
SgVarRefExp *variableReferenceFromProperties(const JsonValue &properties,
                                             const NodeMap &nodes);
SgCompoundLiteralExp *compoundLiteralFromProperties(const JsonValue &properties,
                                                    const NodeMap &nodes);
SgLabelRefExp *labelReferenceFromProperties(const JsonValue &properties,
                                            const NodeMap &nodes);
class PointerMemberTypeDeserializationIdentityGuard {
public:
  PointerMemberTypeDeserializationIdentityGuard();
  ~PointerMemberTypeDeserializationIdentityGuard();

  PointerMemberTypeDeserializationIdentityGuard(
      const PointerMemberTypeDeserializationIdentityGuard &) = delete;
  PointerMemberTypeDeserializationIdentityGuard &
  operator=(const PointerMemberTypeDeserializationIdentityGuard &) = delete;
};
class FunctionTypeDeserializationIdentityGuard {
public:
  FunctionTypeDeserializationIdentityGuard();
  ~FunctionTypeDeserializationIdentityGuard();

  FunctionTypeDeserializationIdentityGuard(
      const FunctionTypeDeserializationIdentityGuard &) = delete;
  FunctionTypeDeserializationIdentityGuard &
  operator=(const FunctionTypeDeserializationIdentityGuard &) = delete;
};
class FullTypeDeserializationIdentityGuard {
  bool owner_;

public:
  FullTypeDeserializationIdentityGuard();
  ~FullTypeDeserializationIdentityGuard();

  FullTypeDeserializationIdentityGuard(
      const FullTypeDeserializationIdentityGuard &) = delete;
  FullTypeDeserializationIdentityGuard &
  operator=(const FullTypeDeserializationIdentityGuard &) = delete;
};
class TemplateTypeDeserializationIdentityGuard {
public:
  TemplateTypeDeserializationIdentityGuard();
  ~TemplateTypeDeserializationIdentityGuard();

  TemplateTypeDeserializationIdentityGuard(
      const TemplateTypeDeserializationIdentityGuard &) = delete;
  TemplateTypeDeserializationIdentityGuard &
  operator=(const TemplateTypeDeserializationIdentityGuard &) = delete;
};
class ExternalClassDeserializationIdentityGuard {
public:
  ExternalClassDeserializationIdentityGuard();
  ~ExternalClassDeserializationIdentityGuard();

  ExternalClassDeserializationIdentityGuard(
      const ExternalClassDeserializationIdentityGuard &) = delete;
  ExternalClassDeserializationIdentityGuard &
  operator=(const ExternalClassDeserializationIdentityGuard &) = delete;
};
class ExternalFunctionDeserializationIdentityGuard {
public:
  explicit ExternalFunctionDeserializationIdentityGuard(
      const AstFileRecord &ast);
  ~ExternalFunctionDeserializationIdentityGuard();

  ExternalFunctionDeserializationIdentityGuard(
      const ExternalFunctionDeserializationIdentityGuard &) = delete;
  ExternalFunctionDeserializationIdentityGuard &
  operator=(const ExternalFunctionDeserializationIdentityGuard &) = delete;
};
void collectExternalFunctionConstructionDependencies(
    const JsonValue &json, std::unordered_set<uint64_t> &ids);
void collectTypeDependencyIds(const JsonValue &json,
                              std::unordered_set<uint64_t> &ids,
                              std::unordered_set<uint64_t> &symbol_ids,
                              std::unordered_set<uint64_t> &type_ids);
bool declarationConstructionDependencyReady(const AstFileRecord &ast,
                                            const NodeMap &nodes, uint64_t id,
                                            bool needs_symbol,
                                            bool needs_type_identity,
                                            VariantT symbol_kind = V_SgSymbol);
bool constructExternalFunctionParameterScopeSymbols(const AstFileRecord &ast,
                                                    const NodeMap &nodes,
                                                    bool require_complete);
SgType *typeFromJson(const JsonValue &json, const NodeMap &nodes,
                     const std::string &exact_type_error =
                         "AST JSON type requires an exact type");
SgFortranImplicitRange *
constructFortranImplicitRange(char first, std::optional<char> last,
                              SgFortranImplicitRange::type_spec_enum type_spec,
                              const JsonValue &properties,
                              const NodeMap &nodes);
SgFunctionType *nullableFunctionSyntaxTypeFromJson(const JsonValue &json,
                                                   const NodeMap &nodes);
SgFunctionType *
nullableDeclaredExceptionSignatureFromJson(const JsonValue &json,
                                           const NodeMap &nodes);
SgType *nullableTypeFromJson(const JsonValue &json, const NodeMap &nodes);
SgType *nullableCxxSourceTypeFromJson(const JsonValue &json,
                                      const NodeMap &nodes);
SgType *targetSizeTypeFromJson(const JsonValue &json, const NodeMap &nodes,
                               bool target_available);
VariantT semanticFunctionTypeKindFromJson(const JsonValue &json);
SgFunctionType *semanticFunctionTypeFromJson(const JsonValue &json,
                                             const NodeMap &nodes);
SgTypeIdOp *typeIdFromProperties(const JsonValue &properties,
                                 const NodeMap &nodes, SgExpression *operand,
                                 SgType *operand_type,
                                 bool operand_is_evaluated);
SgNoexceptOp *noexceptFromProperties(const JsonValue &properties,
                                     const NodeMap &nodes,
                                     SgExpression *operand);
SgFunctionParameterRefExp *
functionParameterReferenceFromProperties(const JsonValue &properties,
                                         const NodeMap &nodes);
SgVarArgOp *varArgFromProperties(const JsonValue &properties,
                                 const NodeMap &nodes, SgExpression *operand);
bool isJsonBinaryOpKind(const std::string &kind);
SgType *exactTypedefBaseType(const JsonValue &properties, const NodeMap &nodes);
inline bool isTypedefOwnedSourceType(SgType *base) {
  auto *member = isSgPointerMemberType(base);
  return isSgDeclType(base) != nullptr || isSgTypeOfType(base) != nullptr ||
         base->has_source_prototype() ||
         (member != nullptr &&
          !SgPointerMemberType::isCanonicalSemanticType(member));
}
SgTypedefDeclaration *
constructTypedefDeclaration(const SgName &name, SgType *base_type,
                            SgDeclarationStatement *base_declaration,
                            bool owns_base_declaration, VariantT kind,
                            SgTemplateTypedefDeclaration *template_declaration,
                            const SgTemplateArgumentPtrList &arguments,
                            const SgTemplateArgumentPtrList &deduced_arguments,
                            const TypedefMetadata &metadata);
SgType *exactInitializedNameType(const JsonValue &properties,
                                 const NodeMap &nodes);
SgInitializedName *constructInitializedName(const SgName &name, SgType *type);
std::optional<SgArrowExp::emission_role_enum>
binaryEmissionRoleFromJson(const JsonValue &properties,
                           const std::string &kind);
std::optional<long long> binaryStorageSizeFromJson(const JsonValue &properties,
                                                   const std::string &kind);
std::vector<std::optional<long long>>
binarySubscriptStridesFromJson(const JsonValue &properties,
                               const std::string &kind);
SgBinaryOp *
constructBinaryOp(const std::string &kind, SgExpression *lhs, SgExpression *rhs,
                  SgType *result_type,
                  std::optional<SgArrowExp::emission_role_enum> arrow_role,
                  std::optional<long long> result_storage_size_bytes,
                  const std::vector<std::optional<long long>> &strides);
bool isJsonUnaryOpKind(const std::string &kind);
struct UnaryConstructionFields {
  SgUnaryOp::Sgop_mode mode;
  SgAddressOfOp::address_operand_form_enum address_form;
  SgThrowOp::e_throw_kind throw_kind;
  std::vector<unsigned int> vector_components = {};
  SgVectorElementExp::selector_kind_enum vector_selector =
      SgVectorElementExp::e_components;
  bool vector_is_arrow = false;
  SgName defined_operator_name;
  SgFunctionSymbol *defined_operator_symbol = nullptr;
};
UnaryConstructionFields
unaryConstructionFieldsFromJson(const JsonValue &properties,
                                const std::string &kind, const NodeMap &nodes);
SgUnaryOp *constructUnaryOp(const std::string &kind, SgExpression *operand,
                            const UnaryConstructionFields &fields,
                            SgType *result_type);
SgFile::languageOption_enum fileLanguageFromJson(const JsonValue &properties,
                                                 const std::string &key);
SgFile::outputFormatOption_enum
fileOutputFormatFromJson(const JsonValue &properties, const std::string &key);
Sg_File_Info *buildFileInfo(const JsonValue &json, SgNode *parent);
struct FileInfoFields {
  int raw_file_id;
  int raw_line;
  int raw_column;
  int physical_file_id;
  int physical_line;
  unsigned int physical_file_occurrence;
  unsigned int source_sequence;
  unsigned int classification;
  int file_id;
  int line;
  int column;
};
std::optional<FileInfoFields> fileInfoFieldsFromJson(const JsonValue &json);
Sg_File_Info *constructFileInfo(const std::optional<FileInfoFields> &fields,
                                SgNode *parent);
struct SourcePositionFields {
  std::optional<FileInfoFields> start;
  std::optional<FileInfoFields> end;
  std::optional<FileInfoFields> operator_position;
};
struct SourcePositions {
  std::unique_ptr<Sg_File_Info> start;
  std::unique_ptr<Sg_File_Info> end;
  std::unique_ptr<Sg_File_Info> operator_position;
};
SourcePositions constructSourcePositions(const SourcePositionFields &fields);
SourcePositionFields
requiredSourcePositionFieldsFromJson(const JsonValue &location,
                                     const std::string &context);
struct ExternalInitializedNameFields {
  SgName name;
  InitializedNameMetadata metadata;
  SourcePositionFields positions;
  std::function<SgType *()> type;
  std::function<SgType *()> auto_decltype;
  std::function<SgType *()> fortran_source_type;
  std::function<SgType *()> cxx_source_type;
  SgSymbol *source_symbol;
};
ExternalInitializedNameFields externalInitializedNameFieldsFromJson(
    const JsonValue &json, const NodeMap &nodes, const std::string &context);
SourcePositions sourcePositionsFromJson(const JsonValue &location,
                                        bool expression);
SourcePositionFields validateSourcePositionRecord(const NodeRecord &record);
void installSourcePositions(SgNode *node, SourcePositions positions,
                            const std::string &context);
void installTransformationSourcePosition(SgLocatedNode *node);
SourcePositions requiredSourcePositionsFromJson(const JsonValue &location,
                                                const std::string &context);
struct FunctionModifierFields {
  SgBitVector function;
  SgBitVector special;
};
FunctionModifierFields
functionModifierFieldsFromJson(const NodeRecord &record);
struct FortranInterfaceFields {
  SgName name;
  SgInterfaceStatement::generic_spec_enum kind;
  bool is_abstract;
};
void validateFortranInterfaceFields(const FortranInterfaceFields &fields);
FortranInterfaceFields
fortranInterfaceFieldsFromJson(const JsonValue &properties);
SgBitVector bitVectorFromJson(const JsonValue &json, const std::string &context,
                              std::size_t expected_size);
struct ReturnStatementFields {
  SgReturnStmt::return_keyword_kind_enum keyword;
  uint64_t nrvo_candidate;
};
ReturnStatementFields
returnStatementFieldsFromJson(const JsonValue &properties);
SgOmpClause::omp_when_context_kind_enum
ompDeviceTypeKindFromJson(const JsonValue &properties,
                          const std::string &node_kind);
SgOmpClause::omp_default_option_enum
ompDefaultOptionFromJson(const JsonValue &properties);
SgOmpClause::omp_doacross_kind_enum
ompDoacrossKindFromJson(const JsonValue &properties);

SgNode *createNodeFromRecord(
    const NodeRecord &record, SgProject *project,
    SgPragma *pragma_child = nullptr,
    SgTemplateClassDeclaration *template_declaration = nullptr,
    SgClassDeclaration *class_declaration = nullptr,
    SgNamespaceDeclarationStatement *namespace_declaration = nullptr,
    SgDeclarationStatement *declaration_target = nullptr,
    SgInitializedName *using_name = nullptr,
    const SgDeclarationStatementPtrList *dependent_template_candidates =
        nullptr,
    const NodeMap &dependencies = {});
SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const NodeMap &dependencies);
SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             const NodeMap &dependencies,
                             SourcePositions positions);
SgSourceFile *createSourceFileFromRecord(const NodeRecord &record,
                                         SgProject *project, SgFileList *owner,
                                         const NodeMap &dependencies = {});
SgNode *createUseStatementFromRecord(const NodeRecord &record,
                                     SgProject *project,
                                     SgModuleStatement *module);
SgNode *createNodeFromRecord(const NodeRecord &record, SgProject *project,
                             SgFunctionDeclaration *declaration,
                             SgBasicBlock *body);
SgSymbol *createExternalSymbolFromJson(const JsonValue &json,
                                       const NodeMap &nodes,
                                       SgSymbolTable *owner_table);
std::function<SgSymbol *()>
prepareExternalSymbolFromJson(const JsonValue &json, const NodeMap &nodes,
                              SgSymbolTable *owner_table);
struct ExternalDeclarationFields {
  DeclarationFields declaration;
  bool first_is_self;
  bool defining_is_self;
  void initialize(SgDeclarationStatement *decl);
};
ExternalDeclarationFields
externalDeclarationFieldsFromJson(const JsonValue &json, VariantT kind,
                                  const std::string &context);
SgClassType *constructClassTypeForDeclaration(SgClassDeclaration *decl);
SgClassType *classTypeForDeclaration(SgClassDeclaration *decl);
SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes);
SgExpression *expressionFromRef(const JsonValue &json, const NodeMap &nodes,
                                InlineExpressionConstruction &ownership);
SgComplexVal *complexValueFromProperties(const JsonValue &properties,
                                         const NodeMap &nodes);
SgExprListExp *exprListExpFromTypeJson(const JsonValue &json,
                                       const NodeMap &nodes);
SgSymbol *symbolFromJson(const JsonValue &json, const NodeMap &nodes);
SgSymbol *exactBoundSymbolFromJson(const JsonValue &json, const NodeMap &nodes,
                                   SgSymbolTable *local_table = nullptr);
size_t exactSymbolBindingEntryIndex(const JsonValue &binding,
                                    const JsonValue &entries);
std::vector<size_t>
symbolDependencyOrder(const std::vector<std::optional<size_t>> &dependencies);
bool exactSymbolBindingDependenciesReady(const JsonValue &json,
                                         const NodeMap &nodes);
SgSymbol *resolveExistingSymbolFromJson(const JsonValue &json,
                                        const NodeMap &nodes);
SgModuleStatement *externalModuleFromJson(const JsonValue &json);
std::function<SgModuleStatement *()>
decodeExternalModule(const JsonValue &json);
SgFunctionDeclaration *restoredExternalFunctionFromJson(const JsonValue &json);
std::function<SgFunctionDeclaration *()>
decodeExternalFunction(const JsonValue &json, const NodeMap &nodes);
SgFunctionDeclaration *externalFunctionFromJson(const JsonValue &json,
                                                const NodeMap &nodes);
uint64_t defaultInitializerDeclarationNodeFromJson(const JsonValue &json);
SgCxxDefaultInitExp::source_kind_enum
defaultInitializerKindFromJson(const JsonValue &properties);
SgInitializedName *defaultInitializerDeclarationFromJson(const JsonValue &json,
                                                         const NodeMap &nodes);
SgClassDeclaration *restoredExternalClassFromJson(const JsonValue &json);
SgClassDeclaration *externalClassDeclarationFromJson(const JsonValue &json);
std::function<SgClassDeclaration *()>
decodeExternalClassDeclaration(const JsonValue &json);
SgDeclarationStatement *externalDeclarationReferenceFromJson(
    const JsonValue &json, const NodeMap &nodes, const std::string &context);
SgInitializedName *externalInitializedNameFromJson(
    const JsonValue &json, const NodeMap &nodes, SgDeclarationStatement *decl,
    SgNode *parent, SgScopeStatement *scope, const std::string &context);
SgInitializedName *constructExternalInitializedName(
    const JsonValue &json, const NodeMap &nodes,
    const ExternalInitializedNameFields &fields, SgDeclarationStatement *decl,
    SgNode *parent, SgScopeStatement *scope, const std::string &context);
void populateExternalFunctionParameterScopeFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const SgInitializedNamePtrList &parameters,
    const std::string &function_name);
SgDeclarationStatement *externalParameterScopeDeclarationFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const std::string &function_name);
void constructExternalFunctionParameterScopeDeclarationsFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const SgInitializedNamePtrList &parameters,
    const std::string &function_name, std::optional<SourcePositions> positions);
void constructExternalFunctionParameterScopeSymbolsFromJson(
    const JsonValue &json, const NodeMap &nodes,
    SgFunctionParameterScope *scope, const SgInitializedNamePtrList &parameters,
    const std::string &function_name);
VariantT symbolVariantFromJsonKind(const std::string &kind);
enum class LabelSymbolBasisKind {
  Declaration,
  FortranStatement,
  AlternateReturn
};
struct LabelSymbolFields {
  LabelSymbolBasisKind basis;
  int value;
  SgLabelSymbol::label_type_enum type;
};
LabelSymbolFields labelSymbolFieldsFromJson(const JsonValue &json);
std::function<SgSymbol *()> prepareSymbolForKindAndBasis(
    VariantT kind, SgNode *basis,
    const std::optional<LabelSymbolFields> &label_fields);
std::function<SgSymbol *()>
prepareSymbolForKindAndBasis(VariantT kind, SgNode *basis,
                             const JsonValue &properties);
SgSymbol *createSymbolForKindAndBasis(VariantT kind, SgNode *basis,
                                      const JsonValue &properties);
void validateNamespaceSymbolKey(const SgName &entry_name, SgNode *basis);
std::vector<EdgeRecord> edgesFor(const NodeRecord &record,
                                 const std::string &field);
uint64_t singleEdgeTarget(const NodeRecord &record, const std::string &field);
void validateParentChains(const AstFileRecord &ast);
uint64_t exactSerializedPointerTarget(const NodeRecord &record,
                                      const std::string &property,
                                      const std::string &edge);
uint64_t requiredSingleEdgeTarget(const NodeRecord &record,
                                  const std::string &field);
struct NodeConstructionFlags {
  bool modified;
  bool contains_transformation;
  bool contains_transformation_to_surrounding_whitespace;
  bool source_range_ends_in_macro_expansion;
  bool source_range_is_macro_expansion_fragment;
  void initialize(SgNode *node) const;
};
NodeConstructionFlags validateConstructionFlags(const NodeRecord &record);
struct StatementFields {
  std::optional<bool> case_insensitive;
  int source_sequence;
  SgStatement::directive_end_kind_enum directive_end;
  SgStatement::omp_fortran_spelling_enum fortran_spelling;
  std::optional<bool> variable_constexpr;
  std::optional<bool> variable_thread_local;
  std::optional<SgVariableDeclaration::fortran_declaration_origin_enum>
      variable_origin;
  std::optional<bool> variable_global_type_qualification;
  std::optional<SgDeclarationStatement::template_specialization_enum>
      specialization;
  void initialize(SgStatement *statement) const;
};
StatementFields statementFieldsFromJson(const NodeRecord &record,
                                        VariantT kind);
struct OmpClauseFields {
  SgOmpClause::omp_directive_name_modifier_enum directive_modifier;
  std::optional<std::size_t> source_order;
  void initialize(SgOmpClause *clause) const;
};
OmpClauseFields ompClauseFieldsFromJson(const NodeRecord &record);
SgOmpClause::omp_init_modifier_kind_enum
initModifierKindFromJson(const JsonValue &properties);
void restoreAvailableConstructionScopes(const AstFileRecord &ast,
                                        const NodeMap &nodes);
void restoreAvailableParentsAndScopes(const AstFileRecord &ast,
                                      const NodeMap &nodes);
void restoreAvailableAuxiliaryDeclarationOwnership(const AstFileRecord &ast,
                                                   const NodeMap &nodes);
void attachPreprocessingInfo(SgNode *node, const NodeRecord &record);
using PreparedPreprocessingInfo =
    std::vector<std::unique_ptr<PreprocessingInfo>>;
struct PreprocessingInfoFields {
  FileInfoFields position;
  PreprocessingInfo::DirectiveType directive;
  std::string text;
  int lines;
  PreprocessingInfo::RelativePositionType relative;
  bool transformation;
  PreprocessingInfo::OutputPlacementType placement;
};
std::vector<PreprocessingInfoFields>
preprocessingInfoFieldsFromJson(const NodeRecord &record);
PreparedPreprocessingInfo
constructPreprocessingInfo(const std::vector<PreprocessingInfoFields> &fields);
PreparedPreprocessingInfo preparePreprocessingInfo(const NodeRecord &record);
void attachPreprocessingInfo(SgNode *node, PreparedPreprocessingInfo entries);
struct LiteralTokenFields {
  SgCxxLiteralToken::token_kind_enum kind;
  std::string spelling;
};
LiteralTokenFields literalTokenFieldsFromJson(const JsonValue &properties);
using PreparedAstAttributes =
    std::vector<std::pair<std::string, std::unique_ptr<AstAttribute>>>;
PreparedAstAttributes prepareAstAttributes(const NodeRecord &record);
void attachAstAttributes(SgNode *node, PreparedAstAttributes attributes);
void attachAstAttributes(SgNode *node, const NodeRecord &record);
struct FunctionReturnTypeSourceFields {
  bool qualified;
  bool globally_qualified;
  SgStringList qualification_tokens;
  SgFunctionDeclaration::source_return_type_elaboration_kind_enum elaboration;
  bool elaboration_required;
  int name_qualification_length;
  bool requires_name_qualification;
  bool global_qualification_required;
};
SgFunctionDeclaration::constexpr_specifier_enum
functionConstexprSpecifierFromJson(const JsonValue &properties,
                                   const std::string &kind);
FunctionReturnTypeSourceFields
functionReturnTypeSourceFieldsFromJson(const JsonValue &properties);

struct FunctionCallSourceFields {
  bool uses_operator_syntax;
  SgFunctionCallExp::source_syntax_enum syntax;
  SgFunctionCallExp::source_operator_surface_enum surface;
  SgFunctionCallExp::source_operator_callee_form_enum callee_form;
  SgUnsignedCharList operand_roles;
  SgName literal_suffix;
  SgUnsignedCharList suffix_roles;
  bool literal_modified;
};
FunctionCallSourceFields
functionCallSourceFieldsFromJson(const JsonValue &properties);
void validateFunctionCallInputs(const FunctionCallSourceFields &fields,
                                SgExpression *function,
                                SgExprListExp *arguments, SgType *type,
                                SgCxxLiteralTokenList *literal_tokens,
                                size_t argument_count, size_t token_count);
SgFunctionCallExp *constructFunctionCall(const FunctionCallSourceFields &fields,
                                         SgExpression *function,
                                         SgExprListExp *arguments, SgType *type,
                                         SgCxxLiteralTokenList *literal_tokens);
void applyFunctionCallSourceFields(SgFunctionCallExp *call,
                                   const FunctionCallSourceFields &fields);
void linkNodeEdges(const NodeRecord &record, const NodeMap &nodes);
void validateRestoredOmpOwnedClausePayloads(const AstFileRecord &ast,
                                            const NodeMap &nodes);
void linkDeclarationGroupEdges(const AstFileRecord &ast, const NodeMap &nodes);
SgScopeStatement *nearestScope(SgNode *node);

LabelSymbolBasisKind labelSymbolBasisKind(const SgLabelSymbol *symbol);
SgScopeStatement *labelSymbolScopeForBasis(SgNode *basis);
void validateLabelSymbolFields(const SgLabelSymbol *symbol,
                               const JsonValue &json);
void validateLabelSymbolFields(const SgLabelSymbol *symbol,
                               const LabelSymbolFields &fields);
SgClassSymbol *classSymbolForDeclaration(SgClassDeclaration *decl);
SgNonrealSymbol *nonrealSymbolForDeclaration(SgNonrealDecl *decl);
void validateExternalSymbolBasisOwnership(SgSymbol *symbol);
struct VisibilitySymbolFields {
  VariantT kind;
  const JsonValue *target_binding;
  const JsonValue *target;
  VariantT target_kind;
  uint64_t basis_id;
  SgName new_name;
  bool is_renamed;
  SgSymbolTable *local_table;
  std::function<SgSymbol *()> construct_external_target;

  explicit VisibilitySymbolFields(const JsonValue &entry,
                                  SgSymbolTable *local_table = nullptr);
};
std::unique_ptr<SgSymbol>
constructVisibilitySymbol(const VisibilitySymbolFields &fields,
                          const NodeMap &nodes, SgNodePtrList causes);
struct SerializedSymbolTableEntry {
  SgSymbolTable *table = nullptr;
  VariantT symbol_kind;
  bool lookup_preferred;
  std::optional<uint64_t> basis_id;
  SgName entry_name;
  std::vector<uint64_t> causal_ids;
  std::vector<uint64_t> namelist_declarations;
  std::optional<VisibilitySymbolFields> visibility;
  std::optional<LabelSymbolFields> label_fields;
  std::function<SgSymbol *()> construct;
  std::function<SgSymbol *()> prepare(const NodeMap &nodes) const;

  explicit SerializedSymbolTableEntry(const JsonValue &entry);
  SerializedSymbolTableEntry(SgSymbolTable *owner, const JsonValue &entry);
};
using SerializedSymbolTableRecords =
    std::unordered_map<uint64_t, std::vector<SerializedSymbolTableEntry>>;
std::vector<SerializedSymbolTableEntry>
symbolTableEntriesFromRecord(const NodeRecord &record);
VariantT symbolLookupPreferenceCategory(VariantT kind);
VariantT symbolLookupPreferenceCategory(SgSymbol *symbol);
struct ExpectedSymbolTablePreference {
  SgSymbolTable *table = nullptr;
  SgName entry_name;
  SgSymbol *symbol = nullptr;
  bool lookup_preferred = false;
  VariantT category;

  ExpectedSymbolTablePreference(SgSymbolTable *owner, const SgName &name,
                                VariantT kind, bool preferred)
      : table(owner), entry_name(name), lookup_preferred(preferred),
        category(symbolLookupPreferenceCategory(kind)) {}
  ExpectedSymbolTablePreference(SgSymbolTable *owner, const SgName &name,
                                SgSymbol *binding, bool preferred)
      : table(owner), entry_name(name), symbol(binding),
        lookup_preferred(preferred),
        category(symbolLookupPreferenceCategory(binding)) {
    ASSERT_not_null(binding);
  }
};
using SymbolTablePreferenceGroups =
    std::vector<std::vector<const ExpectedSymbolTablePreference *>>;
SymbolTablePreferenceGroups groupSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries,
    std::optional<bool> case_insensitive = std::nullopt);
void validateSymbolTablePreferenceGroups(
    const std::vector<ExpectedSymbolTablePreference> &entries,
    std::optional<bool> case_insensitive = std::nullopt);
void enforceSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries);
void validateSymbolTablePreferences(
    const std::vector<ExpectedSymbolTablePreference> &entries);
struct SerializedSymbolTableConstruction {
  std::vector<SerializedSymbolTableEntry> pending;
  std::vector<ExpectedSymbolTablePreference> preferences;
  bool preferences_finalized = false;
};
bool constructPendingSerializedSymbols(
    SerializedSymbolTableConstruction &construction, const NodeMap &nodes,
    bool require_complete);
void restoreSerializedSymbolTables(
    const AstFileRecord &ast, const NodeMap &nodes,
    const std::unordered_set<uint64_t> &scope_ids,
    SerializedSymbolTableConstruction *construction = nullptr);
void restoreSerializedSymbolTables(
    const AstFileRecord &ast, const NodeMap &nodes,
    const std::unordered_set<uint64_t> &scope_ids,
    const SerializedSymbolTableRecords &records,
    SerializedSymbolTableConstruction *construction);
SgTemplateArgumentPtrList templateArgumentListFromJson(const JsonValue &json,
                                                       const NodeMap &nodes,
                                                       SgNode *parent);

void validateReconstructedTypesAndSymbols(const AstFileRecord &ast,
                                          const NodeMap &nodes);
void rebuildConstructorOnlyNodes(const AstFileRecord &ast, NodeMap &nodes);
std::vector<const NodeRecord *>
collectDelayedDeclarations(const AstFileRecord &ast, const NodeMap &nodes);
using SymbolConstructionAdvance = std::function<bool(const NodeMap &)>;
void drainDelayedDeclarations(const AstFileRecord &ast,
                              std::vector<const NodeRecord *> &pending,
                              NodeMap &nodes, bool require_complete,
                              const SymbolConstructionAdvance &symbols = {});
void restoreRecordedParentEdges(const AstFileRecord &ast, const NodeMap &nodes);
SgSourceFile *reconstructSourceFile(const AstFileRecord &ast,
                                    SgSourceFile *old_file,
                                    NodeMap *restored_nodes = nullptr);

} // namespace AstJson
} // namespace Rose

#endif
