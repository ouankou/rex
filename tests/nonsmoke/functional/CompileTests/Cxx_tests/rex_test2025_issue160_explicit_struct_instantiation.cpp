template <typename T> struct Crate {
  T value;
  T get() const { return value; }
};

template struct Crate<long>;

int use_crate() {
  Crate<long> crate;
  crate.value = 11;
  return crate.get();
}
