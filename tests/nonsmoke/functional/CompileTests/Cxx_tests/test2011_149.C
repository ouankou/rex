// This is a modified version of test2001_10.C used for debugging details of the
// legacy frontend 4.3 support in ROSE.

/*
   Constructor call not unparsed correctly

Original code:
   return doubleArray ( New_Data_Pointer , Vectorized_Domain_Pointer );
Unparsed code:
   return (New_Data_Pointer,Vectorized_Domain_Pointer);
 */

class Domain
   {
     public:
          Domain (int i);
};

class A
   {
public:
  A(double *dataPtr, Domain X);

  // legacy frontend 4.3 version of ROSE does not handle the const
  // well...
  //        A operator()();
  A operator()() const;
   };

   // legacy frontend 4.3 version of ROSE does not handle the const well...
   // A A::operator()()
   A A::operator()() const {
     int i = 999;
     double* xPtr = 0x0000;
     return A(xPtr,2);
   }
