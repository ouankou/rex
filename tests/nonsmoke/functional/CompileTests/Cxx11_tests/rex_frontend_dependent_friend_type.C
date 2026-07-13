template <class T> class rex_friend_owner;

struct rex_friend_target {
  static int read(const rex_friend_owner<rex_friend_target> &owner);
};

template <class T> class rex_friend_owner {
  friend T;

public:
  explicit rex_friend_owner(int value) : value_(value) {}

private:
  int value_;
};

int rex_friend_target::read(const rex_friend_owner<rex_friend_target> &owner) {
  return owner.value_;
}

int rex_frontend_dependent_friend_type() {
  rex_friend_owner<rex_friend_target> owner(42);
  return rex_friend_target::read(owner);
}
