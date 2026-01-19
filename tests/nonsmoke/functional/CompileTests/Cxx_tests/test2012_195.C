
namespace N
   {
     class any {};

     template < typename T, typename S = any >
     class array
        {
          public:
//             array(int x);
        };
   }


namespace M
   {
     class any {};
   }

// N::array<N::any> Y = N::array<N::any>(0);
N::array<N::any,N::any> Y;

// N::array<N::any,M::any> Z = N::array<N::any,M::any>(0);
N::array<N::any,M::any> Z;
