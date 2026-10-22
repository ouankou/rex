namespace rex_explicit_specialization_instantiation_chain {

template <typename T = short> class Holder {
public:
  Holder();

protected:
  T primary_value;
};

template <> class Holder<int> {
public:
  Holder();

protected:
  int specialized_value;
};

extern template class Holder<int>;

template <> class Holder<long>;
template <> struct Holder<long> {
  long value;
};

Holder<long> rex_holder_long{1};

template <> class Holder<short>;
template <> struct Holder<> {
  short value;
};

Holder<> rex_holder_defaulted{2};

#define REX_HOLDER_KEY struct
extern template REX_HOLDER_KEY Holder<double>;
#undef REX_HOLDER_KEY

} // namespace rex_explicit_specialization_instantiation_chain

int rex_frontend_explicit_specialization_instantiation_chain() {
  return sizeof(rex_explicit_specialization_instantiation_chain::Holder<int>);
}
