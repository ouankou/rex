enum {
  rex_unparser_global_first = 1,
  rex_unparser_global_second = 2
} rex_unparser_global_left = rex_unparser_global_first,
  rex_unparser_global_right = rex_unparser_global_second;

int rex_unparser_anonymous_enum_declarator_group() {
  enum {
    rex_unparser_local_first = 3,
    rex_unparser_local_second = 4
  } rex_unparser_local_left = rex_unparser_local_first,
    rex_unparser_local_right = rex_unparser_local_second;

  return rex_unparser_global_left + rex_unparser_global_right +
         rex_unparser_local_left + rex_unparser_local_right;
}
