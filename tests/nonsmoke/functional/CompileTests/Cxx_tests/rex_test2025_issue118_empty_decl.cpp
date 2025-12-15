namespace N {
; // empty decl: keep as an anchor node
int x = 1;
} // namespace N

struct S {
  ; // empty decl: keep as an anchor node
  int y = 2;
};

int main() {
  S s;
  return N::x + s.y;
}
