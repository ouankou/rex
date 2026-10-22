struct RexQueryStruct {
  int rex_struct_first, rex_struct_second;
};

union RexQueryUnion {
  int rex_union_first;
  double rex_union_second;
};

extern int x;

class LocalB {
public:
  LocalB(int x) {}
};

void incrementXInOtherFile() {
  LocalB value(x);
  ++x;
}
