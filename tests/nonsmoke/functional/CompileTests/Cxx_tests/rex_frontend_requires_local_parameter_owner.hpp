#ifndef REX_FRONTEND_REQUIRES_LOCAL_PARAMETER_OWNER_HPP
#define REX_FRONTEND_REQUIRES_LOCAL_PARAMETER_OWNER_HPP

template <typename Output, typename Value>
concept RexWritable = requires(Output &&output, Value &&value) {
  *output = value;
  *static_cast<Output &&>(output) = static_cast<Value &&>(value);
  const_cast<const decltype(*output) &&>(*output) =
      static_cast<Value &&>(value);
  const_cast<const decltype(*static_cast<Output &&>(output)) &&>(
      *static_cast<Output &&>(output)) = static_cast<Value &&>(value);
};

template <typename Output, typename Value>
  requires RexWritable<Output, Value>
inline void rex_write(Output output, Value value) {
  *output = value;
}

#endif
