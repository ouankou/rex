#define REX_HEADER_VARIABLE rex_first_header_ownership_rename_me
namespace rex_first_header_owner {
#include "rex_unparse_multiple_header_ownership.hpp"
}
#undef REX_HEADER_VARIABLE

#define REX_HEADER_VARIABLE rex_second_header_ownership_rename_me
namespace rex_second_header_owner {
#include "rex_unparse_multiple_header_ownership.hpp"
}
#undef REX_HEADER_VARIABLE

int main() { return 0; }
