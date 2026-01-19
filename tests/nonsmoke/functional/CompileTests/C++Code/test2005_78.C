

template <typename T>
class X
   {
     int x;
   };

template <>
class X < int >  
   {
     int x;
   };

   // This can only be defined once!
   // template <> class X < int > { int x; };
