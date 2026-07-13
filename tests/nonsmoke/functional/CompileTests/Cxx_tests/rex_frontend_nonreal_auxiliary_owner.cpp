template <class T>
concept rex_nonreal_auxiliary_concept = true;

namespace rex_nonreal_auxiliary_namespace {
template <class T>
concept rex_nonreal_nested_concept = true;

static_assert(rex_nonreal_nested_concept<long>);
} // namespace rex_nonreal_auxiliary_namespace

using rex_nonreal_auxiliary_builtin = __type_pack_element<1, int, long>;

static_assert(rex_nonreal_auxiliary_concept<int>);
static_assert(__is_same(rex_nonreal_auxiliary_builtin, long));
