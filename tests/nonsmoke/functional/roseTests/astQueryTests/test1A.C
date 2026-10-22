struct RexQueryStruct {
  int rex_struct_first, rex_struct_second;
};

union RexQueryUnion {
  int rex_union_first;
  double rex_union_second;
};

class LocalA {
public:
  LocalA(int x) {}
};

int x = 1;

void incrementXInOtherFile();

void incrementX() {
  LocalA value(x);
  ++x;
}

int main() {
  incrementX();
  incrementXInOtherFile();
  return x == 3 ? 0 : 1;
}
