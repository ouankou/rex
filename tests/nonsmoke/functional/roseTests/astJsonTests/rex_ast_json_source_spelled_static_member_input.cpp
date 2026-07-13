namespace rex_ast_json_scope {

using Size = int;

typedef struct {
  int payload;
} SourceSpelledAlias;

template <typename T> struct SourceSpelledHolder {
  T value;
};

SourceSpelledHolder<SourceSpelledAlias> source_spelled_holder;

template <Size N> struct RexAstJsonStaticMember {
  static int value;
};

template <typename T> struct RexAstJsonTypedStaticMember {
  static int typed_value;
};

template <typename T> struct RexAstJsonFunctionOwner {
  static int function();
};

template <typename T> struct RexAstJsonClassOwner {
  struct Nested;
};

template <typename Value, int Count> struct RexAstJsonFactory {};

template <template <typename SemanticType, int SemanticCount> class Factory>
struct RexAstJsonTemplateTemplateOwner {
  struct Nested;
};

} // namespace rex_ast_json_scope

rex_ast_json_scope::SourceSpelledHolder<rex_ast_json_scope::SourceSpelledAlias>
    rex_ast_json_qualified_holder;

using RexAstJsonQualifiedAlias = rex_ast_json_scope::SourceSpelledAlias;
rex_ast_json_scope::SourceSpelledAlias rex_ast_json_qualified_value{};

template <rex_ast_json_scope::Size N>
int rex_ast_json_scope::RexAstJsonStaticMember<N>::value = 1;

template <> int rex_ast_json_scope::RexAstJsonStaticMember<2>::value = 2;

template <typename WrittenType>
int rex_ast_json_scope::RexAstJsonTypedStaticMember<WrittenType>::typed_value =
    5;

template <typename T>
int rex_ast_json_scope::RexAstJsonFunctionOwner<T>::function() {
  return 3;
}

template <> int rex_ast_json_scope::RexAstJsonFunctionOwner<int>::function() {
  return 4;
}

template <typename T>
struct rex_ast_json_scope::RexAstJsonClassOwner<T>::Nested {
  T value;
};

template <> struct rex_ast_json_scope::RexAstJsonClassOwner<int>::Nested {
  int value;
};

template <template <typename WrittenType, int WrittenCount> class Factory>
struct rex_ast_json_scope::RexAstJsonTemplateTemplateOwner<Factory>::Nested {
  int value;
};

using RexAstJsonTemplateTemplateInstantiation =
    rex_ast_json_scope::RexAstJsonTemplateTemplateOwner<
        rex_ast_json_scope::RexAstJsonFactory>;

int main() {
  RexAstJsonTemplateTemplateInstantiation::Nested nested{};
  return rex_ast_json_scope::RexAstJsonStaticMember<1>::value +
                     rex_ast_json_scope::RexAstJsonStaticMember<2>::value +
                     rex_ast_json_scope::RexAstJsonTypedStaticMember<
                         int>::typed_value +
                     nested.value ==
                 8
             ? 0
             : 1;
}
