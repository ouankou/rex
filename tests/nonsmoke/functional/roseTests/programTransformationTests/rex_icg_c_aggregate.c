struct RexCValue {
  int value;
};

static struct RexCValue global;

int main(void) {
  struct RexCValue value;
  struct RexCValue copy = {0};
  value.value = 7;
  copy = value;
  return global.value != 0 || copy.value != 7;
}
