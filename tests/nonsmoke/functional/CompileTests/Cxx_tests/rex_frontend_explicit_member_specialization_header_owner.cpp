template <typename T> struct RexExplicitMemberOwner {
  void value();
};

template <> void RexExplicitMemberOwner<int>::value() {}

int main() {
  RexExplicitMemberOwner<int> object;
  object.value();
}
