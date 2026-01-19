// non-defining declaration before the definition
template<typename T> class X;

template<typename T> class X
   {
     public:
       // note qualified name X::X_int (which is a type alias) does not conflict 
       // with ::X_int (which is a variable) it would not be a problem if they 
       // were both type aliases or both variable names.
          typedef X<int> X_int;
   };

// non-defining declaration after the definition
template<typename T> class X;

// Specializations
template<> class X<char> {};
template<> class X<long> {};

X<char> X_char;
X<long> X_long;
