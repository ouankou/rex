template <typename T> struct rex_constrained_owner {
  int same_signature()
    requires(sizeof(T) == 1)
  {
    return 1;
  }
  int same_signature()
    requires(sizeof(T) > 1)
  {
    return 2;
  }

  ~rex_constrained_owner()
    requires(sizeof(T) == 1)
  {}
  ~rex_constrained_owner()
    requires(sizeof(T) > 1)
  {}
};

int main() {
  rex_constrained_owner<char> narrow;
  rex_constrained_owner<int> wide;
  return narrow.same_signature() + wide.same_signature();
}
