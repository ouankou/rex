void f() {
  int i = 0;
  int *iptr = &i;
  typedef int *intPointer;
  iptr.intPointer::~intPointer();
}
