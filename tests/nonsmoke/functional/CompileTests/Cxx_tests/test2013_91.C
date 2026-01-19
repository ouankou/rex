// template <enum ENUM_TYPE>
template <typename ENUM_TYPE>
class A
   {
     public:
         void foo (ENUM_TYPE x);
   };

class B
   {
     public:
       enum x_enum { START, END };
       void x_enum();
   };

class X
   {
public:
  struct x_enum {
    int x;
  };
          struct Y
             {
            // This is output as: "class A<B::x_enum> a;" and should be "class A<enum B::x_enum> a;".
               A<enum B::x_enum> a;
             };
   };



