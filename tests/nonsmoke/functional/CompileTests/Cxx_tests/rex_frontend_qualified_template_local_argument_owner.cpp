namespace rex_qualified_template_local_argument {
template <bool Value> struct Holder {};
} // namespace rex_qualified_template_local_argument

template <class T> void rex_build_qualified_template_local_argument() {
  const bool local_value = sizeof(T) != 0;
  typedef rex_qualified_template_local_argument::Holder<local_value>
      LocalHolder;
  LocalHolder holder;
  (void)holder;
}
