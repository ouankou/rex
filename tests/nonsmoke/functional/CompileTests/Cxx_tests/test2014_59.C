

template <typename T = int>
class X {};

// class X<>;
// X b;

void foobar()
   {
     X< > a;

     class X{};
     X b;

     ::X< > c;
   }
