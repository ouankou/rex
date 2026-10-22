#include "rex_frontend_application_header_semantic_pragma_owner.hpp"

void rex_pragma_physical_identity(int &value) {
#line 50 "one/rex_same_pragma.h"
#pragma rex_physical_marker
  ++value;
}
#line 50 "two/rex_same_pragma.h"
#pragma rex_physical_marker

#line 1 "rex_pragma_scope.cpp"
namespace rex_pragma_outer::rex_pragma_inner {
#line 100 "left/rex_scope_marker.h"
#pragma rex_namespace_marker
struct RexPragmaOwner {
#line 100 "right/rex_scope_marker.h"
#pragma rex_record_marker
};
void rex_empty_pragma_body() {
#line 100 "left/rex_scope_marker.h"
#pragma rex_block_marker
}
}
