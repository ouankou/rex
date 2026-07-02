#define REX_TEST2026_AS_FRIEND 1

namespace RexTest2026GuardedFriendTemplate {

class X {
public:
#if REX_TEST2026_AS_FRIEND
  template <typename T> friend bool guarded_equal(X, int);
#else
  template <typename T> bool guarded_equal(int);
#endif
};

#if REX_TEST2026_AS_FRIEND
template <typename T>
bool guarded_equal(X, int)
#else
template <typename T>
bool X::guarded_equal(int)
#endif
{
  return true;
}

} // namespace RexTest2026GuardedFriendTemplate

int rex_test2026_guarded_friend_template_definition() {
  RexTest2026GuardedFriendTemplate::X value;
  return RexTest2026GuardedFriendTemplate::guarded_equal<int>(value, 1) ? 0 : 1;
}
