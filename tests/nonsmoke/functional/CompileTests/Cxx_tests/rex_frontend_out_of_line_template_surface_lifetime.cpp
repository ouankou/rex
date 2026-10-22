template <typename Value = int, int Count = 2>
struct rex_template_surface_owner {
  struct nested;
  template <typename Element> struct matching_template;
  template <typename Element> struct renamed_template;

  Value value;
  rex_template_surface_owner();
};

template <typename WrittenValue, int WrittenCount>
rex_template_surface_owner<WrittenValue,
                           WrittenCount>::rex_template_surface_owner()
    : value(WrittenValue()) {}

template <typename WrittenValue, int WrittenCount>
struct rex_template_surface_owner<WrittenValue, WrittenCount>::nested {
  WrittenValue value;
  rex_template_surface_owner<WrittenValue, WrittenCount> *owner;
};

template <typename Value, int Count>
template <typename Element>
struct rex_template_surface_owner<Value, Count>::matching_template {
  Value value;
  Element element;
};

template <typename WrittenValue, int WrittenCount>
template <typename Element>
struct rex_template_surface_owner<WrittenValue,
                                  WrittenCount>::renamed_template {
  WrittenValue value;
  Element element;
};

rex_template_surface_owner<> rex_template_surface_instance;
rex_template_surface_owner<>::nested rex_template_nested_surface_instance{
    1, &rex_template_surface_instance};
rex_template_surface_owner<>::matching_template<int> rex_matching_template;
rex_template_surface_owner<>::renamed_template<int> rex_renamed_template;
