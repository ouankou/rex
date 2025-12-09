template <typename T> class UAF_Owner {
  // Friend function defined inside class template.
  // When instantiated, Clang might reuse the body statements from the template
  // pattern. If we delete the implementation during re-translation but query
  // the cache later, we might hit a Use-After-Free if the cache points to the
  // deleted nodes. This test ensures the cache is properly invalidated BEFORE
  // deletion/re-translation.
  friend void uaf_target(UAF_Owner<T> &) {
    if (true) {
      int x = 42;
      return;
    }
    int y = 100;
  }
};

void trigger() {
  UAF_Owner<int> obj;
  uaf_target(obj);
}
