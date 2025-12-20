template <typename T> struct Holder {
  T value;
};

extern template struct Holder<double>;

int use_holder() {
  Holder<double> *ptr = nullptr;
  return ptr == nullptr;
}
