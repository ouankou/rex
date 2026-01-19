

class complex 
   {
     public:
          float real;
          float imaginary;
   };

void foo()
   {
/*
Unparses to be:
  class ::complex X[10];
  X[0].::complex::real = (0.0);
  class __rose_generated_structure_tag_name_0 {
  public: float real;
  float imaginary;}complexArray[10];
  complexArray[0].__rose_generated_structure_tag_name_0::real = (0);
*/
     complex X[10];
     X[0].real = 0.0;

     class 
        {
          public:
               float real;
               float imaginary;
        } complexArray[10];

     complexArray[0]. real = 0;
   }
