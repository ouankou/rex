namespace rex_palette {
enum Tone { warm = 2, warm_alias = warm, cool = 5 };
enum class Hue { red = 7, blue = 11 };
using ToneAlias = Tone;
} // namespace rex_palette

enum Tone { warm = 13, cool = 17 };

namespace rex_enum_signature_a {
enum { rex_shared_enumerator = 1 };
struct RexSharedRecord;
struct RexSharedRecord {
  int value;
};
namespace inner {
using RexSharedAlias = int;
template <class T> using RexAlias = T;
} // namespace inner
} // namespace rex_enum_signature_a
namespace rex_enum_signature_b {
enum { rex_shared_enumerator = 2 };
struct RexSharedRecord {
  double value;
};
namespace inner {
using RexSharedAlias = double;
template <class T> using RexAlias = T;
} // namespace inner
} // namespace rex_enum_signature_b

using namespace rex_enum_signature_a::inner;
using namespace rex_enum_signature_b::inner;
rex_enum_signature_a::inner::RexAlias<int> rex_alias_a = 0;
rex_enum_signature_b::inner::RexAlias<double> rex_alias_b = 0;

// Stock LLVM22.1.8 also reports evaluator definedness errors in this specimen.
// See EXT-CLANG-ENUM-TEMPLATE-EVALUATION; classification remains open.
template <rex_palette::Tone Shade = rex_palette::warm> struct RexSwatch {
  static constexpr int value = Shade;
};
template <rex_palette::Hue Color = rex_palette::Hue::red> struct RexSample {
  static constexpr int value = static_cast<int>(Color);
};
template <rex_palette::ToneAlias AliasShade> struct RexAliasSwatch {
  static constexpr int value = AliasShade;
};
template <rex_palette::Tone... Shades> struct RexSwatchPack {
  static constexpr unsigned count = sizeof...(Shades);
};
template <template <rex_palette::Tone NestedShade> class Swatch>
struct RexSwatchFamily {
  static constexpr int value = Swatch<rex_palette::cool>::value;
};
struct RexRankOwner {
  enum Rank : unsigned long long {
    low = 19,
    high = 23,
    boundary = 0x7fffffffffffffffULL,
    after_boundary,
    maximum = 0xffffffffffffffffULL
  };
  template <Rank SelectedRank> struct Selection {
    static constexpr unsigned long long value = SelectedRank;
  };
};
template <rex_palette::Hue SelectedHue> constexpr int rex_hue_value() {
  return static_cast<int>(SelectedHue);
}

using RexArgumentAlias = int;
template <class T, class U = T> inline constexpr int rex_semantic_variable = 1;
template <class T> inline constexpr int rex_semantic_variable<T *, T> = 2;
template <> inline constexpr int rex_semantic_variable<RexArgumentAlias> = 3;

#define REX_ENUM_WARM rex_palette::warm
#define REX_ENUM_MAXIMUM maximum

int main() {
  RexRankOwner owner;
  return REX_ENUM_WARM == 2 &&
                 owner.REX_ENUM_MAXIMUM == 0xffffffffffffffffULL &&
                 RexSwatch<>::value == 2 && RexSample<>::value == 7 &&
                 RexSwatch<rex_palette::warm_alias>::value == 2 &&
                 RexAliasSwatch<rex_palette::cool>::value == 5 &&
                 RexSwatchPack<rex_palette::warm, rex_palette::cool>::count ==
                     2 &&
                 RexSwatchFamily<RexSwatch>::value == 5 &&
                 RexRankOwner::Selection<RexRankOwner::high>::value == 23 &&
                 RexRankOwner::Selection<RexRankOwner::maximum>::value ==
                     0xffffffffffffffffULL &&
                 owner.maximum == 0xffffffffffffffffULL &&
                 owner.after_boundary == 0x8000000000000000ULL &&
                 rex_semantic_variable<int> == 3 &&
                 rex_semantic_variable<int *, int> == 2 &&
                 rex_semantic_variable<double> == 1 &&
                 rex_hue_value<rex_palette::Hue::blue>() == 11
             ? 0
             : 1;
}
