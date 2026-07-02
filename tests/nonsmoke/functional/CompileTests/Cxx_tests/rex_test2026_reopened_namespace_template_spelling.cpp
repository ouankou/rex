#define REX_TEST2026_TEMPLATE_ENABLED 1

namespace RexTest2026ReopenedNamespaceTemplate {

#if REX_TEST2026_TEMPLATE_ENABLED
template <typename T> void foobar(T);
#else
void foobar(int);
#endif

} // namespace RexTest2026ReopenedNamespaceTemplate

namespace RexTest2026ReopenedNamespaceTemplate {

#if REX_TEST2026_TEMPLATE_ENABLED
template <typename T> void foobar(T) {}
#else
void foobar(int) {}
#endif

} // namespace RexTest2026ReopenedNamespaceTemplate

int rex_test2026_reopened_namespace_template_spelling() {
  RexTest2026ReopenedNamespaceTemplate::foobar(1);
  return 0;
}
