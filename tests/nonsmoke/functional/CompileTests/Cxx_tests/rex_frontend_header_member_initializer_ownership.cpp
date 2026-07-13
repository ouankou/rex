#include "rex_frontend_header_member_initializer_ownership.hpp"

RexHeaderMemberInitializerOwnership::RexHeaderMemberInitializerOwnership()
    : p_char(""), p_size(0) {}

const char *RexHeaderMemberInitializerOwnership::value() const {
  return p_char;
}

unsigned RexHeaderMemberInitializerOwnership::size() const { return p_size; }
