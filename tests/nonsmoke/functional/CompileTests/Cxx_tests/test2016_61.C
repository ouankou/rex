//////////////// preparation for member function pointer cast /////////////////
class Foo
   {
     public:
          virtual void my_foo() { }
   };

class Bar: public Foo
   {
     public:
          void my_foo() { }
   };

typedef void (Foo::*MEM_FUNC)();

void foobar() {
  //////////// cast of member function pointer ////////////
  MEM_FUNC fp = static_cast<void (Foo::*)()>(&Bar::my_foo);
  // This issue is that we are missing the cast!
  // expected   MEM_FUNC fp = static_cast<void (Foo::*)()>(&Bar::my_foo);
  // result     MEM_FUNC fp = &Bar::my_foo;
  // Does NOT work
}
