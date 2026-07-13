namespace rex_explicit_instantiation {
template <class T> struct Holder {
  T value;
};

Holder<int> *observed_before_directive = nullptr;
extern template struct Holder<int>;
template struct Holder<long>;
} // namespace rex_explicit_instantiation

int rex_frontend_explicit_instantiation_identity() {
  rex_explicit_instantiation::Holder<long> value{11};
  return static_cast<int>(value.value);
}
