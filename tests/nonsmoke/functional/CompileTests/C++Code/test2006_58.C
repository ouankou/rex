
class Foo {
 public:
  Foo() { ; }  // Foo::Foo is user-defined
};

class Bar : public Foo {
 public:
  // Bar::Bar is compiler generated, but
  // Bar::Bar invoekes the user-defined Foo::Foo
};

class Baz {
 public:
  // Baz::Baz is compiler generated.
};

int main()
{
  Bar *b = new Bar;  // Invokes the compiler-generated Bar::Bar,
                     // translator says Bar::Bar is not
                     // compiler generated.  WRONG!
  Foo *f = new Foo;  // Invokes the user-defined Foo::Foo,
                     // translator says Foo::Foo is not
                     // compiler generated.  Correct.
  Baz *bz = new Baz; // Invokes the compiler-generated Baz::Baz.
                     // Translator says SgMemberFunctionDeclaration
                     // is NULL.  If this is synonymous with
                     // compiler-generated, OK.
  return 0;
}
