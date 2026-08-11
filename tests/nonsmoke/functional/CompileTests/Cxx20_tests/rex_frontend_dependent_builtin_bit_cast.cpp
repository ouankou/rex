template <typename To, typename From>
To rex_dependent_builtin_bit_cast(const From &value) {
  return __builtin_bit_cast(To, value);
}

struct rex_bit_pattern {
  unsigned value;
};

int main() {
  rex_bit_pattern source{7};
  return rex_dependent_builtin_bit_cast<unsigned>(source) == 7 ? 0 : 1;
}
