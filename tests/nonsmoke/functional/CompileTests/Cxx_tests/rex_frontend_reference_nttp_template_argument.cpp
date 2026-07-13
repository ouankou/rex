template <int &Reference> int rex_reference_nttp_value() { return Reference; }

int rex_reference_nttp_object;

int rex_reference_nttp_use() {
  return rex_reference_nttp_value<rex_reference_nttp_object>();
}
