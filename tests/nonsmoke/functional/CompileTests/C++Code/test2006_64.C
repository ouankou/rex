


class A
   {

     public:
          void operator=(int a);
          void operator++();

   };

class B : public A
   {

     public:
          void operator++();

   };

void A::operator=(int a)
   {
   }

void B::operator++()
   {
     A::operator=(42);
     A::operator++();
   }

void A::operator++()
   {
   }

int main()
   {
     B b;
     ++b;
   }

