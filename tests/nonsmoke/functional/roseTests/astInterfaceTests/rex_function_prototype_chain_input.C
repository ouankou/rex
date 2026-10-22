namespace rex_prototype_types {
struct RexArgument {};
} // namespace rex_prototype_types

int rex_free_definition(::rex_prototype_types::RexArgument argument) noexcept(
    noexcept(argument));
int rex_free_definition(::rex_prototype_types::RexArgument argument) noexcept(
    noexcept(argument));
// REX replacement must retain this definition-owned preprocessing record.
int rex_free_definition(::rex_prototype_types::RexArgument argument) noexcept(
    noexcept(argument)) {
  return 1;
}

int rex_array_definition(::rex_prototype_types::RexArgument
                             argument[2]) noexcept(noexcept(argument));
int rex_array_definition(::rex_prototype_types::RexArgument
                             argument[2]) noexcept(noexcept(argument)) {
  return 4;
}

class RexPrototypeOwner {
public:
  int rex_inline_member_definition(::rex_prototype_types::RexArgument) const & {
    return 2;
  }
  int rex_out_of_line_member_definition();
};

// An out-of-class definition can only leave its class-owned declaration.
int RexPrototypeOwner::rex_out_of_line_member_definition() { return 3; }

namespace rex_copy_reopened_namespace {
int rex_copy_reopened_definition();
}

namespace rex_copy_reopened_namespace {
int rex_copy_reopened_definition() {
  int result = 0;
  for (int rex_copy_loop_local = 0; rex_copy_loop_local < 2;
       ++rex_copy_loop_local) {
    result += rex_copy_loop_local;
  }
  return result;
}
} // namespace rex_copy_reopened_namespace

namespace rex_copy_reopened_alias = rex_copy_reopened_namespace;
