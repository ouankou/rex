template <typename First, typename Second, int Third>
class SomeType {};

class OtherType {};

// Example of C++11 support for template typedefs.
// This is not unparsed, but should be unparsed as:
// template <typename Second> using TypedefName = SomeType<OtherType, Second, 5>;
template <typename Second>
using TypedefName = SomeType<OtherType, Second, 5>;

void foo(TypedefName<int> x )
   {
   }
