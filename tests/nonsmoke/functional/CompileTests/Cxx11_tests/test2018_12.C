// To make the error happen there need to be two files:

// file2.cc:

#include "test2018_12.h"

Class1 parm1;
std::vector<unsigned> local1 = parm1.method1();

// The error will be:
// rose_file2.cc(5): error: no suitable user-defined conversion from
// "std::vector<std::seed_seq::result_type={std::uint_least32_t={unsigned int}},
// std::allocator<std::seed_seq::result_type={std::uint_least32_t={unsigned
// int}}>>" to "std::vector<unsigned long, std::allocator<unsigned long>>"
// exists
//    std::vector< std::char_traits< unsigned int > ::int_type  , class
//    std::allocator< std::char_traits< unsigned int > ::int_type  >  > local1 =
//    parm1 .  method1 ();
