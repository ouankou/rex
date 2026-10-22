static int constructed;
static int destroyed;

struct RexRequiredArgument {
  int value;
  explicit RexRequiredArgument(int value) : value(value) {}
};

struct RexElidedResult {
  RexRequiredArgument member;
  static RexRequiredArgument declared;
  inline static RexRequiredArgument initialized{13};
  RexElidedResult() : member(7) { ++constructed; }
  RexElidedResult(const RexElidedResult &) = delete;
  RexElidedResult(RexElidedResult &&) = delete;
  ~RexElidedResult() { ++destroyed; }
};

RexRequiredArgument RexElidedResult::declared(11);

RexElidedResult rex_elided_result() { return RexElidedResult{}; }

int main() {
  constexpr int expected = 2;
  {
    RexElidedResult value = rex_elided_result();
    auto *allocated = new RexElidedResult(rex_elided_result());
    if (value.member.value != 7 || allocated->member.value != 7 ||
        RexElidedResult::declared.value != 11 ||
        RexElidedResult::initialized.value != 13)
      return 1;
    delete allocated;
  }
  return constructed == expected && destroyed == expected ? 0 : 1;
}
