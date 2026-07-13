struct RexAliasStruct {};
class RexAliasClass {};
enum RexAliasEnum { rex_alias_enum_value };

using RexExplicitStructAlias = struct RexAliasStruct;
using RexPlainStructAlias = RexAliasStruct;
using RexExplicitClassAlias = class RexAliasClass;
using RexPlainClassAlias = RexAliasClass;
using RexExplicitEnumAlias = enum RexAliasEnum;
using RexPlainEnumAlias = RexAliasEnum;

typedef struct RexAliasTypedefTag {
} RexAliasTypedefTag;

template <class> struct RexPlainArgumentBox {};
template <class> struct RexExplicitArgumentBox {};

using RexPlainTemplateArgument = RexPlainArgumentBox<RexAliasTypedefTag>;
using RexExplicitTemplateArgument =
    RexExplicitArgumentBox<struct RexAliasTypedefTag>;
