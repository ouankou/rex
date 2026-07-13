int rex_extract_leaf(int value);
bool rex_extract_predicate(int value);

void rex_extract_condition_insertion_scope(int value) {
  if (rex_extract_predicate(rex_extract_leaf(value)))
    ++value;
}
