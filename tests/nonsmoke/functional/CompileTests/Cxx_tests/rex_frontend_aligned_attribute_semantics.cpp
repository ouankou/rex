struct alignas(long double) TypeAligned {
  unsigned char value[sizeof(long double)];
};

struct alignas(long double &) ReferenceTypeAligned {
  unsigned char value[sizeof(long double)];
};

struct alignas(32) ExpressionAligned {
  unsigned char value;
};

template <class T> struct alignas(T) DependentTypeAligned {
  char value;
};

template <unsigned Alignment> struct alignas(Alignment) DependentValueAligned {
  char value;
};

template <unsigned Alignment> struct DependentMemberAligned {
  char value __attribute__((aligned(Alignment)));
};

template <class... Types> struct alignas(Types...) PackAligned {
  char value;
};

struct alignas(0) ZeroAligned {
  char value;
};

alignas(32) char grouped_alignment_first, grouped_alignment_second;

static_assert(alignof(DependentTypeAligned<ExpressionAligned>) == 32,
              "dependent type alignment was lost");
static_assert(alignof(DependentValueAligned<64>) == 64,
              "dependent expression alignment was lost");
static_assert(alignof(DependentMemberAligned<64>) == 64,
              "dependent member alignment was lost");
static_assert(alignof(PackAligned<char, ExpressionAligned>) == 32,
              "alignment pack expansion was lost");
static_assert(alignof(ZeroAligned) == alignof(char),
              "zero alignas must preserve natural alignment");
static_assert(__alignof__(grouped_alignment_first) == 32 &&
                  __alignof__(grouped_alignment_second) == 32,
              "grouped declarator alignment was lost");

struct __attribute__((aligned)) DefaultAligned {
  unsigned char value;
};

struct alignas(64) ForwardAligned;
struct alignas(64) ForwardAligned {
  unsigned char value;
};

typedef int TypedefAligned __attribute__((aligned(32)));

struct __attribute__((aligned(65536))) WideClassAligned {
  char value;
};
typedef int WideTypedefAligned __attribute__((aligned(32768)));
int wide_variable_aligned __attribute__((aligned(65536)));
struct WideFieldAligned {
  char value __attribute__((aligned(32768)));
};

static_assert(alignof(WideClassAligned) == 65536,
              "class alignment was narrowed");
static_assert(alignof(WideTypedefAligned) == 32768,
              "typedef alignment was narrowed");
static_assert(__alignof__(wide_variable_aligned) == 65536,
              "variable alignment was narrowed");
static_assert(alignof(WideFieldAligned) == 32768,
              "field alignment was narrowed");

constexpr int aligned(int value) { return value; }

struct AlignmentNames {
  constexpr int aligned(int value) const { return value; }
};

static_assert(aligned(3) == 3, "aligned must remain a legal function name");
static_assert(AlignmentNames{}.aligned(4) == 4,
              "aligned must remain a legal member name");

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
