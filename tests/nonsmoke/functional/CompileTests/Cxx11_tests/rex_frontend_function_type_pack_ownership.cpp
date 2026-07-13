template <typename> struct rex_function_shape;

template <typename Result, typename... Arguments>
struct rex_function_shape<Result(Arguments...)> {};

template <typename Result, typename... Arguments>
struct rex_function_shape<Result(Arguments..., ...)> {};

template <typename... Values> struct rex_type_chain;

template <> struct rex_type_chain<> {};

template <typename Head, typename... Tail>
struct rex_type_chain<Head, Tail...> {
  using next_type = rex_type_chain<Tail...>;
};

using rex_shape = rex_function_shape<int(double, char)>;
using rex_variadic_shape = rex_function_shape<int(double, char, ...)>;
using rex_chain = rex_type_chain<int, double, char>;

int main() {
  rex_shape shape;
  rex_variadic_shape variadic_shape;
  rex_chain chain;
  (void)shape;
  (void)variadic_shape;
  (void)chain;
}
