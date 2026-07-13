template <typename Element> struct RexAstJsonEnumOwner {
  enum { local_capacity = 15 / sizeof(Element) };
  Element local_buffer[local_capacity + 1];
};

RexAstJsonEnumOwner<char> rex_ast_json_enum_owner;

int main() {
  return sizeof(rex_ast_json_enum_owner.local_buffer) == 16 ? 0 : 1;
}
