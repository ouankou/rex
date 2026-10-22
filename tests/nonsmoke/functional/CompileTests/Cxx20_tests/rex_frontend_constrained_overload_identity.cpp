struct rex_copy_move_owner {
  rex_copy_move_owner() = default;
  rex_copy_move_owner(const rex_copy_move_owner &) = default;
  rex_copy_move_owner(rex_copy_move_owner &&) = default;
  rex_copy_move_owner &operator=(const rex_copy_move_owner &) = default;
  rex_copy_move_owner &operator=(rex_copy_move_owner &&) = default;
  template <typename T> rex_copy_move_owner &operator=(const T &) {
    return *this;
  }
};

void rex_copy_like_assignment() {
  rex_copy_move_owner first, second;
  first.operator= <rex_copy_move_owner>(second);
}

template <typename T> struct rex_constrained_owner {
  int same_signature()
    requires(sizeof(T) == 1)
  {
    return 1;
  }
  int same_signature()
    requires(sizeof(T) > 1)
  {
    return 2;
  }

  ~rex_constrained_owner()
    requires(sizeof(T) == 1)
  {}
  ~rex_constrained_owner()
    requires(sizeof(T) > 1)
  {}
};

struct rex_trivial_destructor_owner {
  int value;
  explicit rex_trivial_destructor_owner(int input) : value(input) {}
};

struct rex_implicit_nontrivial_owner {
  rex_constrained_owner<char> member;
};

struct rex_defaulted_destructor_owner {
  ~rex_defaulted_destructor_owner() = default;
};

struct rex_late_defaulted_destructor_owner {
  ~rex_late_defaulted_destructor_owner();
};
rex_late_defaulted_destructor_owner::~rex_late_defaulted_destructor_owner() =
    default;

template <typename T>
concept rex_destructor_constraint = sizeof(T) > 0;

template <typename T> struct rex_selected_trivial_owner {
  ~rex_selected_trivial_owner()
    requires rex_destructor_constraint<T>
  = default;
  ~rex_selected_trivial_owner() {}
};

int main() {
  rex_constrained_owner<char> narrow;
  rex_constrained_owner<int> wide;
  rex_trivial_destructor_owner trivial{0};
  rex_implicit_nontrivial_owner implicit;
  rex_defaulted_destructor_owner defaulted;
  rex_late_defaulted_destructor_owner late_defaulted;
  rex_selected_trivial_owner<int> selected;
  return narrow.same_signature() + wide.same_signature();
}
