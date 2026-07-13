extern struct rex_tag_redeclaration rex_tag_object;

struct rex_tag_redeclaration;

struct rex_tag_redeclaration *
rex_tag_identity(struct rex_tag_redeclaration *value) {
  return value;
}

struct rex_function_tag *
rex_function_tag_identity(struct rex_function_tag *value);

struct rex_function_tag;
