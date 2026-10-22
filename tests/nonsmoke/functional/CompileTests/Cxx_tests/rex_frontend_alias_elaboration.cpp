struct RexAliasStruct {};
class RexAliasClass {};
enum RexAliasEnum { rex_alias_enum_value };

struct RexAliasRedeclaredStruct {
} rex_alias_redeclared_struct;
struct RexAliasRedeclaredStruct;

class RexAliasForwardClass;
class RexAliasForwardClass {
} rex_alias_forward_class;

enum class RexAliasRedeclaredEnum : int {
  zero = 0
} rex_alias_redeclared_enum = RexAliasRedeclaredEnum::zero;
enum class RexAliasRedeclaredEnum : int;

enum class RexAliasForwardEnum : int;
enum class RexAliasForwardEnum : int {
  zero = 0
} rex_alias_forward_enum = RexAliasForwardEnum::zero;

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

namespace RexAliasImport {
using ::RexPlainStructAlias;
RexPlainStructAlias local;
} // namespace RexAliasImport
::RexPlainStructAlias rex_alias_qualified;
RexAliasImport::RexPlainStructAlias rex_alias_imported;
