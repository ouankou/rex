short rex_initializer_source() { return 7; }

const char rex_initializer_text[] = "rex";

int rex_initializer_values[3] = {1, 2, 3};
int (&rex_initializer_array_reference)[3] = rex_initializer_values;

const long rex_initializer_qualified = rex_initializer_source();

struct rex_initializer_record {
  long wide;
  int count;
};

rex_initializer_record rex_initializer_make_record() {
  return {rex_initializer_source(), 2};
}

int rex_initializer_use() {
  rex_initializer_record value = rex_initializer_make_record();
  return static_cast<int>(rex_initializer_text[0]) +
         rex_initializer_array_reference[1] +
         static_cast<int>(rex_initializer_qualified + value.wide + value.count);
}
