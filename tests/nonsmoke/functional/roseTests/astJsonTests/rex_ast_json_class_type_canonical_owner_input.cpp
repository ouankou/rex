struct RexAstJsonCanonicalOwner {
  int value;
};

namespace RexAstJsonCanonicalNamespace {
template <typename T> struct RexAstJsonCanonicalTemplate;
}

namespace RexAstJsonCanonicalNamespace {
template <typename T> struct RexAstJsonCanonicalTemplate {
  T value;
};
} // namespace RexAstJsonCanonicalNamespace

RexAstJsonCanonicalOwner rex_ast_json_canonical_owner{17};
RexAstJsonCanonicalNamespace::RexAstJsonCanonicalTemplate<int>
    rex_ast_json_canonical_template_owner{23};

int main() {
  return rex_ast_json_canonical_owner.value == 17 &&
                 rex_ast_json_canonical_template_owner.value == 23
             ? 0
             : 1;
}
