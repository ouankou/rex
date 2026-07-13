namespace rex_source_qualification_provenance_target {
struct Payload {
  int value;
};

typedef Payload QualifiedPayloadAlias;

enum Mode : int;

int global_value = 11;
void function();
} // namespace rex_source_qualification_provenance_target

namespace rex_source_qualification_provenance_alias =
    rex_source_qualification_provenance_target;

int rex_source_global_using_value = 3;

#define REX_SOURCE_QUALIFIER_ALIAS rex_source_qualification_provenance_alias
#define REX_SOURCE_DELIMITER_QUALIFIER                                         \
  rex_source_qualification_provenance_alias::

namespace rex_source_qualification_base_target {
template <typename T> struct Base {};
} // namespace rex_source_qualification_base_target

#define REX_SOURCE_BASE_NAME rex_source_qualification_base_target::Base
#define REX_SOURCE_BASE_TYPE(T) rex_source_qualification_base_target::Base<T>

template <typename T>
struct RexSourceMacroBaseName : public REX_SOURCE_BASE_NAME<T> {};
template <typename T>
struct RexSourceMacroBaseType : public REX_SOURCE_BASE_TYPE(T) {};

using RexMacroPayload = REX_SOURCE_QUALIFIER_ALIAS::Payload;
using RexMacroDelimiterPayload = REX_SOURCE_DELIMITER_QUALIFIER Payload;
using RexGlobalPayload = ::rex_source_qualification_provenance_target::Payload;
REX_SOURCE_QUALIFIER_ALIAS::Payload rex_source_qualified_value{};
rex_source_qualification_provenance_target::QualifiedPayloadAlias
    rex_namespace_qualified_typedef_value{};
::rex_source_qualification_provenance_target::QualifiedPayloadAlias
    rex_global_qualified_typedef_value{};

namespace rex_source_qualification_provenance_using {
using ::rex_source_global_using_value;
using REX_SOURCE_QUALIFIER_ALIAS::global_value;
} // namespace rex_source_qualification_provenance_using

enum REX_SOURCE_QUALIFIER_ALIAS::Mode : int { rex_mode_ready };

void REX_SOURCE_QUALIFIER_ALIAS::function() {}

struct RexSourceQualificationOwner {};
using RexQualifiedMemberPointer = REX_SOURCE_QUALIFIER_ALIAS::Payload (
    RexSourceQualificationOwner::*)() const;

template <typename T> struct RexSourceQualificationOuter {
  template <typename U> struct Inner {
    static T convert(U value);
  };
};

template <typename T>
template <typename U>
T RexSourceQualificationOuter<T>::Inner<U>::convert(U value) {
  return static_cast<T>(value);
}

int main() {
  RexSourceMacroBaseName<int> macro_base_name;
  RexSourceMacroBaseType<int> macro_base_type;
  RexMacroPayload macro_payload{
      rex_source_qualification_provenance_using::global_value};
  RexMacroDelimiterPayload macro_delimiter_payload{macro_payload.value};
  RexGlobalPayload global_payload{
      macro_delimiter_payload.value + rex_source_qualified_value.value +
      rex_namespace_qualified_typedef_value.value +
      rex_global_qualified_typedef_value.value +
      rex_source_qualification_provenance_using::rex_source_global_using_value};
  RexQualifiedMemberPointer member_pointer = nullptr;
  REX_SOURCE_QUALIFIER_ALIAS::function();
  (void)member_pointer;
  (void)macro_base_name;
  (void)macro_base_type;
  return RexSourceQualificationOuter<int>::Inner<int>::convert(
             global_payload.value) == 11
             ? 0
             : 1;
}
