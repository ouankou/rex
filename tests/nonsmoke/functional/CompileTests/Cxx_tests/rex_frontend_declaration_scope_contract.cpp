// Standard-header template identities must be translated from their exact
// Clang declarations; the frontend must not recreate them from a spelling.
#include <memory>

namespace RexScopeContract {
class Argument;
}

void rex_global_target(const RexScopeContract::Argument &);

namespace RexScopeContract {
void rex_global_target(const Argument &);

class Argument {
public:
  friend void ::rex_global_target(const Argument &);
};
} // namespace RexScopeContract

void rex_global_target(const RexScopeContract::Argument &) {}

class RexMemberOwner {
  template <int Dimension, class Input> void rex_member(const Input &);
};

template <int Dimension, class Input>
void RexMemberOwner::rex_member(const Input &) {}

template void RexMemberOwner::rex_member<1, double>(const double &);

template <typename GlobalParameter> void rex_friend_template();

template <typename GlobalParameter>
void rex_friend_specialization(GlobalParameter);

template <typename OwnerParameter> class RexFriendOwner {
public:
  template <typename FriendParameter> friend void rex_friend_template();
};

template <typename OwnerParameter> class RexFriendSpecializationOwner {
public:
  friend void rex_friend_specialization<OwnerParameter>(OwnerParameter);
};

class RexNestedFriendOwner {
public:
  class RexNestedFriend;

private:
  friend class RexNestedFriend;
};

namespace RexTemplateReopen {
int rex_seed;
}

namespace RexTemplateReopen {
template <class Value> struct LaterOnly {
  Value value;
};

template <class Value> struct Across;
} // namespace RexTemplateReopen

namespace RexTemplateReopen {
template <class Value> struct Across {
  Value value;
};
} // namespace RexTemplateReopen

template <class Value> class RexFriendClass;

class RexFriendClassOwner {
  template <class Value> friend class RexFriendClass;
};

class RexMemberClassOwner {
public:
  template <class Value> class Nested;
};

template <class Value> class RexMemberClassOwner::Nested {
  Value value;
};

template <template <class> class Template> struct RexTemplateIdentityUse {
  Template<int> value;
};

std::allocator<int> rex_exact_allocator_identity;
RexTemplateIdentityUse<std::allocator> rex_exact_template_argument_identity;

namespace RexFriendIdentityLeft {
template <class Value> class Target;
}

namespace RexFriendIdentityRight {
template <class Value> class Target;
}

template <class Value> class RexQualifiedFriendIdentityOwner {
  friend class RexFriendIdentityLeft::Target<Value>;
};

namespace RexFriendIdentityLeft {
template <class Value> class Target {};
} // namespace RexFriendIdentityLeft

namespace RexFriendIdentityRight {
template <class Value> class Target {};
} // namespace RexFriendIdentityRight

RexQualifiedFriendIdentityOwner<int> rex_exact_friend_template_identity;
