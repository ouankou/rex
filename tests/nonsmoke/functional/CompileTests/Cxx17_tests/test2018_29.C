// [[fallthrough]] attribute

void g();
void h();
void i();

void f(int n) {
  switch (n) {
  case 1:
  case 2:
    g();
    [[fallthrough]];
  case 3:
    h();
    break;
  case 4:
    i();
    break;
  default:
    break;
  }
}
