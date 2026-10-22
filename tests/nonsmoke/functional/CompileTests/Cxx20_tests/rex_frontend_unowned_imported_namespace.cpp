import "rex_frontend_unowned_imported_namespace.hpp";

using namespace rex_unowned_imported_namespace;

marker make_marker() { return {42}; }

int imported_marker_sum() {
  int direct = make_marker() + 1;
  return add_marker({direct});
}
