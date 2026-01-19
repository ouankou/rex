
// #define PD_inquire_type(file, name) (defstr *) SC_def_lookup(name, file->chart_hashed)
//   if (PD_inquire_type(fp,const_cast<char*>(expectedType)))

//   if ((SC_def_lookup((expectedType),fp -> chart_hashed)))

// #define PD_inquire_type(file, name) (defstr *) SC_def_lookup(name, file->chart_hashed)
#define PD_inquire_type(file, name) SC_def_lookup(name, file)

int SC_def_lookup(void*, int);

void foo(const char *expectedType)
   {
  // if (PD_inquire_type(fp,const_cast<char*>(expectedType)))
  // if (SC_def_lookup(const_cast<char*>(expectedType),fp->chart_hashed)))

     int fp = 0;
  // if (PD_inquire_type(fp,const_cast<char*>(expectedType)))
  // unparses to: if (SC_def_lookup((expectedType),fp))
     if (PD_inquire_type(fp,((char*)expectedType)))
        {
        }
   }


// (int *[3])&x" becomes "x", not "&x". (See test2005_106.C)

class X
   {
     public:
          char* array[10];
          void foo ( const char* c ) const;
   };

int main()
   {
     X x;

  // Generates warning: "Warning: compiler generated cast not explicit in Sage translation (skipped)"
     x.foo (x.array [0]);
   }
