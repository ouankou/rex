// read: a
// write: b
void bar(int a, int &b) { b = a; }

double gx;

// read: empty
// write: gx
void globalX() { gx = 0.0; }

class VectorXY
{
  public:
    VectorXY() {x=0.0; y=0.0;}
    VectorXY(double xx, double yy) {x=xx; y=yy;}  // side effect should be obvious

    double x;
    double y;
};

void foo()
{
  VectorXY * bcVelocity;
  for (int i = 0; i < 4; i ++)
    bcVelocity[i] = VectorXY(0xdeadbeef, 0xdeadbeef);   // VectorXY::VectorXY () side effect unknown, even the source code is available.
}
