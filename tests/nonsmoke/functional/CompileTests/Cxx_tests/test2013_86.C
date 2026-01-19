// template<typename T> 
class Class_A
   {
     public:
          Class_A();
   };

// struct xyz
   struct {
     // This presence of a type that calls a constructor causes a constructor to
     // be generated for the struct, this constructor will be a function with
     // out a name.
     Class_A var_0;
   } tcl;
