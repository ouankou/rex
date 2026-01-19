template<class T>
class DEF
   {
     public:
          T xyz;
          T foo ();
   };

template<class T>
T DEF<T>::foo ()
   { 
     return xyz;
   }
