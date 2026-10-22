#define REX_NAMESPACE_BEGIN(NAME) namespace NAME {
#define REX_NAMESPACE_END() }

REX_NAMESPACE_BEGIN(rex_json_namespace)
struct RexJsonNamespaceType {};

template <typename T> struct RexJsonNamespaceBox {
  T value;
};

using RexJsonNamespaceAlias = RexJsonNamespaceBox<RexJsonNamespaceType>;

int value = 1;
REX_NAMESPACE_END()

using namespace rex_json_namespace;

int rex_json_after_namespace = 1;

int main() { return value + rex_json_after_namespace; }
