short rex_initializer_source() { return 7; }

const char rex_initializer_text[] = "rex";

int rex_initializer_values[3] = {1, 2, 3};
int (&rex_initializer_array_reference)[3] = rex_initializer_values;

const long rex_initializer_qualified = rex_initializer_source();

struct rex_initializer_record {
  long wide;
  int count;
};

template <class T> struct rex_initializer_pair {
  const char *name;
  T value;
};

template <class T> int rex_initializer_nested_braces() {
  static const rex_initializer_pair<T> values[] = {{"left", {1, 2}},
                                                   {"right", {3, 4}}};
  return values[0].value.wide + values[1].value.count;
}

struct rex_initializer_nested_value {
  long wide;
  int count;
  rex_initializer_nested_value(long first, int second)
      : wide(first), count(second) {}
};

int rex_initializer_nested_result() {
  return rex_initializer_nested_braces<rex_initializer_nested_value>();
}

rex_initializer_record rex_initializer_make_record() {
  return {rex_initializer_source(), 2};
}

int rex_initializer_use() {
  rex_initializer_record value = rex_initializer_make_record();
  return static_cast<int>(rex_initializer_text[0]) +
         rex_initializer_array_reference[1] +
         static_cast<int>(rex_initializer_qualified + value.wide + value.count);
}
