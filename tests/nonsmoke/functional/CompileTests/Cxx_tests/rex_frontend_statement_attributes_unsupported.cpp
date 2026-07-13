extern void rex_statement_attribute_target();

void rex_statement_attribute_unsupported() {
  [[clang::annotate(
      "rex-unsupported-statement-attribute")]] rex_statement_attribute_target();
}
