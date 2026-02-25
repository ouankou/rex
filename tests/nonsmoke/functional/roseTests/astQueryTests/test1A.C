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
