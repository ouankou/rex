unsigned char rex_storage[8];

void *rex_address(void) {
  __typeof(&rex_storage) pointer = &rex_storage;
  return pointer;
}

static inline unsigned long rex_value(void) { return 1; }

unsigned long rex_statement_result(void) {
  __typeof(({
    unsigned long local = rex_value();
    (void)&rex_storage;
    local;
  })) value = 2;
  return value;
}

struct RexValue {
  int value;
};
struct RexValue *rex_identity(struct RexValue *value) { return value; }

unsigned long rex_inline_size(void) {
  return sizeof(struct RexInlineSize { int value; });
}

unsigned long rex_inline_align(void) {
  return _Alignof(struct RexInlineAlign { long value; });
}

int main(void) {
  struct RexValue value = {7};
  return rex_address() != &rex_storage || rex_statement_result() != 2 ||
         rex_identity(&value) != &value || rex_identity(&value)->value != 7 ||
         rex_inline_size() != sizeof(int) ||
         rex_inline_align() != _Alignof(long);
}
