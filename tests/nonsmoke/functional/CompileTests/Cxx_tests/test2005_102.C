#include <exception>

#include <typeinfo>

namespace boost {
template <class E> void throw_exception(E const &e) { throw e; }
} // namespace boost

void foo() { boost::throw_exception(std::bad_cast()); }
