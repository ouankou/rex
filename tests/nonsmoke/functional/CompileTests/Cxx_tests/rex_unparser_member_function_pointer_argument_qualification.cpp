namespace rex_member_pointer_arguments {
struct Payload {
  int value;
};

enum class Mode { read, write };
using Count = unsigned long;
} // namespace rex_member_pointer_arguments

namespace rex_member_pointer_owner {
struct Owner {
  using LvalueCallback = int (Owner::*)() const &;
  using RvalueCallback = int (Owner::*)() &&;

  int lvalue() const & { return 3; }
  int rvalue() && { return 4; }

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

using PayloadAlias = rex_member_pointer_arguments::Payload;
using DataMember = int PayloadAlias::*;
template <class T> using DependentMember = int T::*;

int main() {
  PayloadAlias payload{9};
  DataMember member = &PayloadAlias::value;
  DependentMember<PayloadAlias> dependent_member = member;
  rex_member_pointer_owner::Owner owner;
  rex_member_pointer_owner::Owner::LvalueCallback lvalue_callback =
      &rex_member_pointer_owner::Owner::lvalue;
  rex_member_pointer_owner::Owner::RvalueCallback rvalue_callback =
      &rex_member_pointer_owner::Owner::rvalue;
  rex_member_pointer_owner::Owner::Callback callback =
      &rex_member_pointer_owner::Owner::invoke;
  return (owner.*callback)(rex_member_pointer_arguments::Mode::read, 7).value ==
                     7 &&
                 payload.*dependent_member == 9 &&
                 (owner.*lvalue_callback)() == 3 &&
                 (rex_member_pointer_owner::Owner{}.*rvalue_callback)() == 4
             ? 0
             : 1;
}
