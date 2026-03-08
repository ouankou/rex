
// This code passes for legacy frontend, but fails for GNU 5.1 (so the identity
// translator fails in the backend as well).

enum vector : int { value = 0 };

class myVector {
public:
  myVector operator/(double x) const;

  myVector();

  myVector operator=(enum vector &x) const;
};

void foo() {
  myVector a;

  // Problem code
  myVector b = a / 1.0;
}
