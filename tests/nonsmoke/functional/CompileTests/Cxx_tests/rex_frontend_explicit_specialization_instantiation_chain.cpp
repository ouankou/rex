namespace rex_explicit_specialization_instantiation_chain {

template <typename T> class Holder {
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

} // namespace rex_explicit_specialization_instantiation_chain

int rex_frontend_explicit_specialization_instantiation_chain() {
  return sizeof(rex_explicit_specialization_instantiation_chain::Holder<int>);
}
