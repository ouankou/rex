// This test code demonstrates where a struct is referenced before being defined
// (swig_type_info is referenced within a typedef).  Yes, this legal code!
// This code comes from a construction of code found in SWIG generated code.

// This is an error! Because swig_type_info is not defined
// swig_type_info* structurePointer2 = 0L;

// This is a typedef that references "swig_type_info" and defines "swig_dycast_func"
// What is interesting is that it also, in some sense, defines "swig_type_info"
// so that subsequent references to the "swig_type_info" type (not requiring 
// a defining declaration) are valid C++ code.
typedef struct swig_type_info *(*swig_dycast_func)(void **);

struct swig_type_info {};
