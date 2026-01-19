// Debug ambiguous template specialization if template <> X :: X() {}

template <typename T>
class X
   {
     private:
          T t;
     public:
          X(T t) : t(t) {};

          template <typename S> X(S t) {};      
   };

int main()
   {
     X<int> a(1);
     X<int> b(1.0);
   }

