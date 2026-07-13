template <class Value> struct RexFriendIdentity {
  typedef Value type;
};

template <class Outer> struct RexFriendOwner {
  template <class First, class Second>
  friend typename RexFriendIdentity<Second *>::type
      rex_nested_friend_signature(RexFriendOwner, First, Second);
};

RexFriendOwner<int> rex_friend_owner;
