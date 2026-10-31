template <class Receiver> struct RexSignatureForward {
  template <class Value>
  static auto apply(Receiver &receiver,
                    Value &value) noexcept(noexcept(receiver.accept(value)))
      -> decltype(receiver.accept(value));
};

template <class Receiver>
template <class Value>
auto RexSignatureForward<Receiver>::apply(
    Receiver &receiver, Value &value) noexcept(noexcept(receiver.accept(value)))
    -> decltype(receiver.accept(value)) {
  return receiver.accept(value);
}

struct RexSignatureReceiver {
  int accept(int &value) noexcept { return ++value; }
};

int rex_signature_use(RexSignatureReceiver &receiver, int &value) {
  return RexSignatureForward<RexSignatureReceiver>::apply(receiver, value);
}
