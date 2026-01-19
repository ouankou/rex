namespace Y
   {
  // typedef int Integer;
     typedef struct type_A typedefType_A;
//   typedef struct type_B typedefType_B;
//   struct type_B *Bptr;
   }

   namespace X {
   template <typename T> class A;

   //   template < typename T = B <> > class E;
   template <typename T = A<Y::typedefType_A>> class F;
   //   template < typename T = A < Y::type_A > > class G;
   //   template < typename T = A < Y::type_B > > class H;
   } // namespace X
