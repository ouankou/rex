// Explicit instantiations of noexcept function templates.  Since C++17 the
// exception specification is part of each specialization's function type, and
// every declaration of a specialization carries it, written or not.
template <class T> void rex_free(T) noexcept {}
template void rex_free<int>(int);
template void rex_free(double) noexcept;

template <class T> struct rex_holder {
  int member() noexcept(sizeof(T) > 1) { return 1; }
};
template int rex_holder<long>::member();

int main() {
  static_assert(noexcept(rex_free(1)), "the specialization is noexcept");
  rex_free(1);
  rex_free(2.0);
  return rex_holder<long>().member() - 1;
}
