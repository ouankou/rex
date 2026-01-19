

class X
   {
     public:
          virtual const char *GetSymNameFromSymHandle() = 0;
   };

class Y : public X
   {
     public:
       // DQ: Source code should be changed so that return type matches the virtual function declaration
       // char *GetSymNameFromSymHandle ()
          const char *GetSymNameFromSymHandle ()
             {
               return 0;
             };
   };
