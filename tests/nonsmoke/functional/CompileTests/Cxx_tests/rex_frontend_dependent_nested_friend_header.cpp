template <class T> struct rex_friend_owner {
  struct target;
};

struct rex_befriender {
  template <class T> friend struct rex_friend_owner<T>::target;
};

template <class T> struct rex_friend_owner<T>::target {};

int main() { return 0; }
