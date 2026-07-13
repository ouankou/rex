template <typename T> T *rex_address(T *);

template <typename Destination, typename Source> struct RexMemcpyable {
  static constexpr bool value = true;
};

template <typename Output, typename Input>
concept RexMemcpyableIterators = requires(Output output, Input input) {
  requires RexMemcpyable<decltype(rex_address(output)),
                         decltype(rex_address(input))>::value;
};

template <typename Input, typename Output>
void rex_copy(Input first, Output result) {
  using DestinationPointer = decltype(rex_address(result));
  using SourcePointer = decltype(rex_address(first));
  if constexpr (RexMemcpyable<DestinationPointer, SourcePointer>::value) {
  }
}

int main() {
  int source = 0;
  int destination = 0;
  rex_copy(&source, &destination);
  return RexMemcpyableIterators<int *, int *> ? 0 : 1;
}
