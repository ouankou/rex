/*
 * The original reproducer used GNU C transparent-union semantics in a C++
 * test. Clang ignores that attribute in C++, so the C++ version uses an
 * explicit typed pointer conversion instead.
 */

struct sockaddr_ax25;

extern int accept(struct sockaddr_ax25 *addr);

void pt_accept_cont() {
  void *buffer = 0;
  accept(static_cast<struct sockaddr_ax25 *>(buffer));
}
