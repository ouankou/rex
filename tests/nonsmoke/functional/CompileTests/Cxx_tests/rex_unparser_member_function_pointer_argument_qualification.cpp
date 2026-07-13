namespace rex_member_pointer_arguments {
struct Payload {
  int value;
};

enum class Mode { read, write };
using Count = unsigned long;
} // namespace rex_member_pointer_arguments

namespace rex_member_pointer_owner {
struct Owner {
  typedef rex_member_pointer_arguments::Payload (Owner::*Callback)(
      rex_member_pointer_arguments::Mode,
      rex_member_pointer_arguments::Count) const;

  rex_member_pointer_arguments::Payload
  invoke(rex_member_pointer_arguments::Mode,
         rex_member_pointer_arguments::Count count) const {
    return {static_cast<int>(count)};
  }
};
} // namespace rex_member_pointer_owner

int main() {
  rex_member_pointer_owner::Owner owner;
  rex_member_pointer_owner::Owner::Callback callback =
      &rex_member_pointer_owner::Owner::invoke;
  return (owner.*callback)(rex_member_pointer_arguments::Mode::read, 7).value ==
                 7
             ? 0
             : 1;
}
