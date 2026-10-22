struct rex_field_tag_owner {
  struct rex_field_tag *value;
  struct rex_field_return_tag *(*callback)(struct rex_field_parameter_tag *);
};

struct rex_field_return_tag *(*rex_variable_callback)(
    struct rex_field_parameter_tag *);
void rex_parameter_callback(
    struct rex_field_return_tag *(*callback)(struct rex_field_parameter_tag *));

typedef struct rex_field_tag {
  int member;
} rex_field_tag_definition;
