template <class T>
class A
   {
     public:
          class B
             {
          public:
            B(int x);
             };
   };

// This should be: A<int>::B x = 0;
A<int>::B x = 0;
