template <class T> T rex_base_type();

struct RexFirstBase {};
struct RexSecondBase {};

template <class T>
struct RexDerived : virtual decltype(rex_base_type<T>()), RexSecondBase {};

template <class... T>
struct RexPackDerived : decltype(rex_base_type<T>())... {};

RexFirstBase *rex_virtual_base(RexDerived<RexFirstBase> *value) {
  return value;
}

RexSecondBase *
rex_pack_base(RexPackDerived<RexFirstBase, RexSecondBase> *value) {
  return value;
}
