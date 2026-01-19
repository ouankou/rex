template <class TElem> class ValueVectorOf
   {
     public :
          ValueVectorOf();

          TElem* fElemList;
   };

class X
   {
     public:
          class Y
             {
             };
   };

   void foo(const X::Y y) {
     // This is unparsed as: extern class ValueVectorOf< X::const Y  >
     // snapshot(); The error is: "X::const Y" instead of "const X::Y".
     ValueVectorOf<const X::Y> snapshot();
     ValueVectorOf<const X::Y *> *snapshot_ptr_2 = 0L;
   }
