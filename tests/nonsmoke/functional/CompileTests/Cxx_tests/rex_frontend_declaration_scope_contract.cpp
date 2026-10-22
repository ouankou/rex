// Standard-header template identities must be translated from their exact
// Clang declarations; the frontend must not recreate them from a spelling.
#include <memory>

namespace RexScopeContract {
class Argument;
}

void rex_global_target(const RexScopeContract::Argument &);

namespace RexScopeContract {
void rex_global_target(const Argument &);
void rex_namespace_target(const Argument &);

class Argument {
public:
  friend void ::rex_global_target(const Argument &);
  friend void ::RexScopeContract::rex_namespace_target(const Argument &);
};
} // namespace RexScopeContract

void rex_global_target(const RexScopeContract::Argument &) {}

void rex_friend_adjusted_parameter(int *);
class RexFriendAdjustedParameterOwner {
  friend void rex_friend_adjusted_parameter(int values[3]);
};

template <class T> void rex_friend_template_adjusted_parameter(T *);
class RexFriendTemplateAdjustedParameterOwner {
  template <class T>
  friend void rex_friend_template_adjusted_parameter(T values[3]);
};

class RexAdjustedMemberOwner {
public:
  void rex_friend_adjusted_member(int *);
};
class RexAdjustedMemberFriend {
  friend void RexAdjustedMemberOwner::rex_friend_adjusted_member(int values[3]);
};

class RexHeaderMemberFriend {
  friend const char *std::exception::what() const noexcept;
};

template <class T> void rex_friend_specialization_adjusted_parameter(T *);
class RexFriendSpecializationAdjustedParameterOwner {
  friend void rex_friend_specialization_adjusted_parameter<int>(int values[3]);
};

class RexMemberOwner {
  template <int Dimension, class Input> void rex_member(const Input &);
};

template <int Dimension, class Input>
void RexMemberOwner::rex_member(const Input &) {}

template void RexMemberOwner::rex_member<1, double>(const double &);

template <typename GlobalParameter> void rex_friend_template(int);

template <typename GlobalParameter>
void rex_friend_specialization(GlobalParameter);

template <typename OwnerParameter> class RexFriendOwner {
public:
  template <typename FriendParameter>
  friend void rex_friend_template(int input);
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

#define REX_FRIEND_KEYWORD friend
class RexFriendClassOwner {
  template <class Value> REX_FRIEND_KEYWORD class RexFriendClass;
};
#undef REX_FRIEND_KEYWORD

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

#define REX_FRIEND_TAG_REFERENCE                                               \
  class rex_friend_tag_reference {};                                           \
  class rex_friend_tag_owner {                                                 \
    friend class rex_friend_tag_reference;                                     \
  };

REX_FRIEND_TAG_REFERENCE
