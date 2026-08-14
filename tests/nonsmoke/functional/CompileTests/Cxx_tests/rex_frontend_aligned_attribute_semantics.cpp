struct alignas(long double) TypeAligned {
  unsigned char value[sizeof(long double)];
};

struct alignas(long double &) ReferenceTypeAligned {
  unsigned char value[sizeof(long double)];
};

struct alignas(32) ExpressionAligned {
  unsigned char value;
};

struct __attribute__((aligned)) DefaultAligned {
  unsigned char value;
};

struct alignas(64) ForwardAligned;
struct alignas(64) ForwardAligned {
  unsigned char value;
};

typedef int TypedefAligned __attribute__((aligned(32)));

static_assert(alignof(TypeAligned) == alignof(long double),
              "type alignas must preserve the type alignment");
static_assert(alignof(ReferenceTypeAligned) == alignof(long double),
              "reference type alignas must use the referred type alignment");
static_assert(alignof(ExpressionAligned) == 32,
              "expression alignas must preserve its byte value");
static_assert(alignof(DefaultAligned) >= alignof(long double),
              "GNU default alignment must provide target maximum alignment");
static_assert(alignof(ForwardAligned) == 64,
              "forward and defining declarations must preserve alignment");
static_assert(alignof(TypedefAligned) == 32,
              "typedef alignment must preserve its byte value");

int main() {
  TypeAligned type_aligned{};
  ReferenceTypeAligned reference_aligned{};
  ExpressionAligned expression_aligned{};
  DefaultAligned default_aligned{};
  ForwardAligned forward_aligned{};
  TypedefAligned typedef_aligned{};
  return type_aligned.value[0] + reference_aligned.value[0] +
         expression_aligned.value + default_aligned.value +
         forward_aligned.value + typedef_aligned;
}
