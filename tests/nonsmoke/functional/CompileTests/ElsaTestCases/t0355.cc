// t0355.cc
// testing that a vendor extension is rejected in strict mode

class Integer {
public:
  // this is fine
  int operator<(int);
  
  // this is illegal, but allowed by some compiler extensions
  //ERROR(1): operator==(const Integer&) const { return 0; }
};
