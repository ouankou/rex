struct RexOperatorValue {
  int value;

  RexOperatorValue &operator=(const RexOperatorValue &);
  RexOperatorValue operator+(const RexOperatorValue &) const;
  RexOperatorValue &operator++();
  int &operator[](int);
};

RexOperatorValue operator-(const RexOperatorValue &, const RexOperatorValue &);

template <typename T>
RexOperatorValue operator<<(RexOperatorValue value, const T &) {
  return value;
}

template <typename T> struct RexFriendOperatorValue {
  T value;

  friend RexFriendOperatorValue operator%(const RexFriendOperatorValue &lhs,
                                          const RexFriendOperatorValue &rhs) {
    return RexFriendOperatorValue{lhs.value % rhs.value};
  }
};

struct RexNonTemplateFriendValue {
  int value;

  friend bool operator==(const RexNonTemplateFriendValue &lhs,
                         const RexNonTemplateFriendValue &rhs) {
    return lhs.value == rhs.value;
  }
};

struct RexArrowTarget {
  int value;
};

struct RexArrowProxy {
  RexArrowTarget *operator->() const;
};

struct RexPointerMemberTarget {
  int invoke(int) const;
};

typedef int (RexPointerMemberTarget::*RexPointerMemberCallback)(int) const;

int rex_frontend_operator_semantic_path(RexOperatorValue &lhs,
                                        const RexOperatorValue &rhs,
                                        RexArrowProxy proxy) {
  lhs = rhs;
  RexOperatorValue sum = lhs + rhs;
  RexOperatorValue difference = lhs - rhs;
  RexOperatorValue explicit_operator_template = operator<< <int>(lhs, 7);
  RexFriendOperatorValue<int> friend_lhs{5};
  RexFriendOperatorValue<int> friend_rhs{2};
  RexFriendOperatorValue<int> friend_remainder = friend_lhs % friend_rhs;
  RexNonTemplateFriendValue equal_lhs{7};
  RexNonTemplateFriendValue equal_rhs{7};
  bool friend_equal = equal_lhs == equal_rhs;
  ++lhs;
  lhs[1] = proxy->value;
  return sum.value + difference.value + explicit_operator_template.value +
         friend_remainder.value + friend_equal;
}

int rex_frontend_case_range_parent(int value) {
  switch (value) {
  case 1 ... 4:
    return 1;
  default:
    return 0;
  }
}

int rex_frontend_pointer_member_callable_type(
    RexPointerMemberTarget &object, RexPointerMemberTarget *pointer,
    RexPointerMemberCallback callback) {
  return (object.*callback)(1) + (pointer->*callback)(2);
}
