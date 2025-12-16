typedef struct X {
  struct Y {
    int dummy;
  } y;

  struct {
    int a;
  } anon;
} X;

int main(void) {
  X x;
  x.y.dummy = 0;
  x.anon.a = 1;
  return x.y.dummy + x.anon.a;
}
