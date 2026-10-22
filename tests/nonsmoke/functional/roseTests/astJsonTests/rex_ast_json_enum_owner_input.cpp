using RexAstJsonEnumUnderlying = unsigned short;
enum class RexAstJsonFixedEnum : RexAstJsonEnumUnderlying { value = 7 };
namespace RexAstJsonEnumTypes {
using Underlying = unsigned char;
}
enum class RexAstJsonQualifiedEnum : RexAstJsonEnumTypes::Underlying { value = 3 };

template <typename Element> struct RexAstJsonEnumOwner {
  enum { local_capacity = 15 / sizeof(Element) };
  enum class State : Element { ready = 1 };
  Element local_buffer[local_capacity + 1];
};

RexAstJsonEnumOwner<char> rex_ast_json_enum_owner;
static_assert(sizeof(RexAstJsonFixedEnum) == sizeof(RexAstJsonEnumUnderlying));
static_assert(sizeof(RexAstJsonEnumOwner<char>::State) == sizeof(char));

int main() {
  return sizeof(rex_ast_json_enum_owner.local_buffer) == 16 ? 0 : 1;
}
