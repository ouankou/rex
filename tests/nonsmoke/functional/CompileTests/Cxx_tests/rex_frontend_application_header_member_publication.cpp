#include "rex_frontend_application_header_member_publication.hpp"

int rex_application_header_member_publication() {
  RexApplicationHeaderMemberPublication<int> value;
  return value.first(7);
}
