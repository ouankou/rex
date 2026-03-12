// Alias templates are valid at namespace or class scope.
template <class T> using ptr = T *;

void foobar() {
  // the name 'ptr<T>' is now an alias for pointer to T
  ptr<int> x = nullptr;
  (void)x;
}
