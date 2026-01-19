template <class T>
inline
const T&
NS_MIN( const T& a, const T& b )
 {
   return b < a ? b : a;
 }

typedef int size_type;

class nsString
 {
   public:

     size_type mLength;

     void Right( size_type aCount )
       {
         NS_MIN(mLength, aCount);
       }
};
