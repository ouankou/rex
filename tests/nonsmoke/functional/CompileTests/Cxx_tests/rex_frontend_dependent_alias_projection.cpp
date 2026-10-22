template <class T, T... Values> struct RexSequence {};
template <unsigned... Values>
using RexIndices = RexSequence<unsigned, Values...>;
template <unsigned... Values> int rexCount(RexIndices<Values...>) {
  return sizeof...(Values);
}
namespace RexQualified {
template <unsigned... Values> using Indices = RexSequence<unsigned, Values...>;
}
template <unsigned... Values>
int rexQualifiedCount(RexQualified::Indices<Values...>) {
  return sizeof...(Values);
}

template <class T> using RexIgnored = int;
template <class T> int rexIgnore(RexIgnored<T> value) { return value; }

struct RexRecord {
  int value;
};
template <class T> using RexValue = decltype(T::value);
template <class T> using RexValueAgain = RexValue<T>;
template <class T> int rexRead(RexValueAgain<T> value) { return value; }
template <class T> using RexPointer = decltype(T::value) *;
template <class T> int rexDeref(RexPointer<T> value) { return *value; }

int main() {
  int value = 4;
  return rexCount(RexIndices<1, 2>{}) != 2 || rexIgnore<double>(7) != 7 ||
         rexRead<RexRecord>(3) != 3 || rexDeref<RexRecord>(&value) != 4 ||
         rexQualifiedCount(RexQualified::Indices<1, 2>{}) != 2;
}
