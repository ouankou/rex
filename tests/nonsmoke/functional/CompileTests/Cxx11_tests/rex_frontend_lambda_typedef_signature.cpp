namespace rex_lambda_typedef_signature {

  typedef int index_type;

  template <typename Function> void apply(Function function) { function(1); }

  void run() {
    apply([](index_type value) {
      if (value == 0) {
        return;
      }
    });
  }

} // namespace rex_lambda_typedef_signature

namespace rex_new_expression_qualification {

  struct item {};

} // namespace rex_new_expression_qualification

rex_new_expression_qualification::item *make_qualified_item() {
  return new rex_new_expression_qualification::item();
}
