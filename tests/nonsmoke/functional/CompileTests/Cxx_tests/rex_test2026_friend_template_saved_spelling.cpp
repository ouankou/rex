template <typename T> struct RexTest2026FriendTemplateHost {
  template <typename U>
  friend U rex_test2026_friend_template_saved_spelling_friend(U value) {
    return value;
  }

  int after_friend;
};

int rex_test2026_friend_template_saved_spelling() {
  RexTest2026FriendTemplateHost<int> host = {7};
  return host.after_friend;
}
