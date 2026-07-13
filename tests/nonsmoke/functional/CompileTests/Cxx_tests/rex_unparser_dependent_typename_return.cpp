template <typename Value> struct rex_return_identity {
  using type = Value;
};

template <typename Value>
typename rex_return_identity<Value>::type
rex_dependent_typename_return(Value value) {
  return value;
}

int rex_dependent_typename_return_result = rex_dependent_typename_return(42);
