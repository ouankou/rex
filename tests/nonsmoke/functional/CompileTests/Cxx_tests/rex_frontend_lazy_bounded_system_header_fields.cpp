#include "rex_frontend_lazy_bounded_system_header_fields.hpp"

int rex_frontend_lazy_bounded_system_header_fields() {
  RexFrontendLazyBoundedSystemHeaderFields<int> fields{{17}};
  return fields.entry.value;
}
