template <typename T>
class map
   {
     class private_struct;

     public:
      // This case fails because map::iterator is translated to the private typedef base type.
         typedef private_struct* iterator;
   };

// map<int>::iterator it;

class foobar_class
   {
     public:
          map<int>::iterator it;
   };


