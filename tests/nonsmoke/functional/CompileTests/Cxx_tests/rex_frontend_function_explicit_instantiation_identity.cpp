namespace rex_function_instantiation {
template <class T> T twice(T value) { return value + value; }

template <class T> struct ForwardAllocator;
template <class T> struct ForwardTraits;
template <class T, class TTraits = ForwardTraits<T>,
          class TAllocator = ForwardAllocator<T>>
struct ForwardString;
typedef ForwardString<char> ForwardStringTypedef;

template <class T> struct Allocator {};
template <class T> struct Traits {};
template <class T, class TTraits = Traits<T>, class TAllocator = Allocator<T>>
struct String {};
using StringAlias = String<char>;
typedef String<char> StringTypedef;

template <class T> struct ForwardAllocator {};
template <class T> struct ForwardTraits {};
template <class T, class TTraits, class TAllocator> struct ForwardString {};

template <class T> const T &facet(const T &value) { return value; }
template <class T> const T &deduced(const T &value) { return value; }
template <class T> const T &useFacet();
template <class T> const T &forwardDeduced(const T &value) { return value; }

extern template int twice<int>(int);
template long twice<long>(long);
extern template const String<char> &facet<String<char>>(const String<char> &);
extern template const StringTypedef &deduced(const StringTypedef &);
extern template const String<char> &useFacet<String<char>>();
extern template const ForwardStringTypedef &
forwardDeduced(const ForwardStringTypedef &);
} // namespace rex_function_instantiation

int rex_frontend_function_explicit_instantiation_identity() {
  rex_function_instantiation::StringAlias value;
  (void)rex_function_instantiation::facet(value);
  return rex_function_instantiation::twice(3);
}
