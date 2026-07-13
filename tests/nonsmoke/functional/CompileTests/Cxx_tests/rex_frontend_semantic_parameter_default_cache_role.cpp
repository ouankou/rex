template <typename T> struct rex_default_cache_owner {
  using state_type = unsigned;

  void clear(state_type state = ~state_type{0}) { value = state; }

  state_type value = 0;
};

template struct rex_default_cache_owner<int>;

unsigned rex_use_default_cache_owner() {
  rex_default_cache_owner<int> owner;
  owner.clear();
  return owner.value;
}
