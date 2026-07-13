template <typename Value> struct rex_template_surface_owner {
  struct nested;

  Value value;
  rex_template_surface_owner();
};

template <typename WrittenValue>
rex_template_surface_owner<WrittenValue>::rex_template_surface_owner()
    : value(WrittenValue()) {}

template <typename WrittenValue>
struct rex_template_surface_owner<WrittenValue>::nested {
  WrittenValue value;
  rex_template_surface_owner<WrittenValue> *owner;
};

rex_template_surface_owner<int> rex_template_surface_instance;
rex_template_surface_owner<int>::nested rex_template_nested_surface_instance{
    1, &rex_template_surface_instance};
