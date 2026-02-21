extern int x;

class LocalB {
public:
  LocalB(int x) {}
};

void incrementXInOtherFile() {
  LocalB value(x);
  ++x;
}
