

enum class vector : int { value = 0 };

class myVector {
public:
  myVector operator/(double x) const;

  myVector();

  myVector operator=(vector &x) const;
};

void foo() {
  myVector a;

  // Problem code
  myVector b = a / 1.0;
}
