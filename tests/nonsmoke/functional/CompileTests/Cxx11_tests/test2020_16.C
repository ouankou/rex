
template<class T> class X;

template<class T> X<T> *g(X<T>);

template<class T> 
class X
   {
     public:
          friend X<T>* g<>(X<T>);

     private:
          T i, j;
   };

template<class T> X<T>* g(X<T> r)
   {
     return 0L;
   }

void foobar()
   {
     X<short> si;

     X<short> *ps = g(si);
   }
