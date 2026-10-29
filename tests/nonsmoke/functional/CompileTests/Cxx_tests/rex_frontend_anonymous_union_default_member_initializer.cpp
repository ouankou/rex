// An implicit constructor initializer for an injected anonymous-union member
// uses that member's default member initializer (libstdc++ 16 std::regex
// executor frames have this shape).  The initializer targets the
// IndirectFieldDecl, while the default initializer belongs to the physical
// field inside the anonymous union.
struct RexAnonymousUnionFrame {
  RexAnonymousUnionFrame(int opcode, long state)
      : opcode(opcode), state(state) {}
  int opcode;
  union {
    unsigned char byte0 = 7;
    void *pointer;
  };
  long state;
};

template <typename T> struct RexAnonymousUnionTemplateFrame {
  explicit RexAnonymousUnionTemplateFrame(T value) : value(value) {}
  T value;
  union {
    short tag = 3;
    T alternate;
  };
};

int main() {
  RexAnonymousUnionFrame frame(1, 2);
  RexAnonymousUnionTemplateFrame<long> templated(5);
  return frame.byte0 == 7 && templated.tag == 3 ? 0 : 1;
}
