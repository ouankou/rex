template <typename Traits> struct rex_cuda_dependent_logical {
  static constexpr bool both = Traits::first && Traits::second;
  static constexpr bool either = Traits::first || Traits::second;
};

int main() { return 0; }
