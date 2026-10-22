struct rex_constexpr_if_value {
  constexpr int get() const { return 7; }
};

template <class T> constexpr int rex_constexpr_if_select(T value) {
  if constexpr (sizeof(T) == sizeof(int))
    return value;
  else
    return value.get();
}

template <class T> constexpr int rex_constexpr_if_init(T value) {
  if constexpr (constexpr int offset = 3; sizeof(T) == sizeof(int))
    return value + offset;
  else
    return value.get() + offset;
}

constexpr int rex_constexpr_if_false() {
  if constexpr (false)
    return 1;
  return 2;
}

constexpr int rex_constexpr_if_declaration() {
  if constexpr (constexpr int value = 3)
    return value;
  else
    return 0;
}

static_assert(rex_constexpr_if_select(4) == 4);
static_assert(rex_constexpr_if_select(rex_constexpr_if_value{}) == 7);
static_assert(rex_constexpr_if_init(4) == 7);
static_assert(rex_constexpr_if_init(rex_constexpr_if_value{}) == 10);
static_assert(rex_constexpr_if_false() == 2);
static_assert(rex_constexpr_if_declaration() == 3);
