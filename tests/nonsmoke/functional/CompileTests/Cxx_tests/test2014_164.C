//////////////////// preparation for function pointer cast ////////////////////
void foo(int)
{ }

void foo(int, int)
{ }

typedef void(*Func1)(int);
typedef void(*Func2)(int,int);

//////////////// preparation for member function pointer cast /////////////////
class Foo{

public:
  virtual void my_foo()
  { }
};

class Bar: public Foo{

public:
  void my_foo()
  { }
};

typedef void (Foo::*MEM_FUNC)();

//////////////////// preparation for variable pointer cast ////////////////////
class Base {};
class Derived: public Base {};

int main() {

  //////////// cast of member function pointer ////////////
  MEM_FUNC fp = static_cast<void (Foo::*)()>(&Bar::my_foo);
  // expected   MEM_FUNC fp = static_cast<void (Foo::*)()>(&Bar::my_foo);
  // result     MEM_FUNC fp = &Bar::my_foo;
  // Does NOT work

  MEM_FUNC fp1 = (MEM_FUNC)(&Bar::my_foo);
  // expected   MEM_FUNC fp1 = (MEM_FUNC)(&Bar::my_foo);
  // result     MEM_FUNC fp1 = &Bar::my_foo;
  // Does NOT work

  return 0;
}
