
namespace N
   {
     struct X {};
   }

// M::Y N::X::*pointer_to_data;

// M::Y (N::X::* ptfptr) (int) = 0L; // &N::X::f;
// M::Y (N::X::* ptfptr) (int);

// int (N::X::* ptfptr) (int);
int (N::X::* ptfptr) (int);

