static_assert(sizeof(int) >= 2);
static_assert(true, u8"utf8 message");
static_assert(true, "left "
                    "right");

template <typename T> struct RexStaticAssertTemplate {
  static_assert(sizeof(T) != 0, "semantic specialization assertion");
  T value;
};

RexStaticAssertTemplate<int> rex_static_assert_template_instance;
