// User-defined literals

// C++03 provides a number of literals. The characters \u201c12.5\u201d are a literal that is resolved by 
// the compiler as a type double with the value of 12.5. However, the addition of the suffix \u201cf\u201d, 
// as in \u201c12.5f\u201d, creates a value of type float that contains the value 12.5. The suffix modifiers 
// for literals are fixed by the C++ specification, and C++ code cannot create new literal modifiers.

// C++11 also includes the ability for the user to define new kinds of literal modifiers that will construct 
// objects based on the string of characters that the literal modifies.

// Literals transformation is redefined into two distinct phases: raw and cooked. A raw literal is a sequence 
// of characters of some specific type, while the cooked literal is of a separate type. The C++ literal 1234, 
// as a raw literal, is this sequence of characters '1', '2', '3', '4'. As a cooked literal, it is the 
// integer 1234. The C++ literal 0xA in raw form is '0', 'x', 'A', while in cooked form it is the integer 10.

// Literals can be extended in both raw and cooked forms, with the exception of string literals, which can 
// be processed only in cooked form. This exception is due to the fact that strings have prefixes that affect 
// the specific meaning and type of the characters in question.

// All user-defined literals are suffixes; defining prefix literals is not possible.

// User-defined literals processing the raw form of the literal are defined as follows:

// OutputType operator "" _suffix(const char *literal_string);

#include <iostream>
 
// used as conversion
constexpr long double operator"" _deg ( long double deg )
{
    return deg*3.141592/180;
}
 
// used with custom type
struct mytype
{
    mytype ( unsigned long long m):m(m){}
    unsigned long long m;
};
mytype operator"" _mytype ( unsigned long long n )
{
    return mytype(n);
}
 
// used for side-effects
void operator"" _print ( const char* str )
{
    std::cout << str;
}
 
int main(){
    double x = 90.0_deg;
    std::cout << std::fixed << x << '\n';
    mytype y = 123_mytype;
    std::cout << y.m << '\n';
    0x123ABC_print;
}

// and

void operator "" _km(long double); // OK, will be called for 1.0_km
std::string operator "" _i18n(const char*, std::size_t); // OK
template <char...> double operator "" _n(); // OK
float operator ""_e(const char*); // OK
 
// float operator ""Z(const char*); // error: suffix must begin with underscore
double operator"" _Z(long double); // error: all names that begin with underscore
                                   // followed by uppercase letter are reserved
double operator""_Z(long double); // OK: even though _Z is reserved ""_Z is allowed


void foobar()
   {
     long double operator""_E(long double);
     long double operator""_a(long double);
     int operator""_p(unsigned long long);
 
  // auto x = 1.0_E+2.0;   // error
     auto y = 1.0_a+2.0;   // OK
     auto z = 1.0_E +2.0;  // OK
     auto q = (1.0_E)+2.0; // OK

     // This is supported in GNU g++ version 6.1, but not in legacy frontend (it
     // should be an error). auto w = 1_p+2;       // error

     auto u = 1_p +2;      // OK
   }
