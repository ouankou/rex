
template<typename _Res>
class _Mem_fn;

// C++ template class prototype that requires a class declaration to be constructed from the template parameter.
// The issue is that there is not scope into which to put the constructed class declaration.  So, to fix this
// we need to define a scope that can be in any declaration (just to support it in function declarations, and
// template class declarations (the SgDeclarationStatement is the only common parent of these two IR nodes).
// It is worth putting it into the SgDeclarationStatement because other IR nodes could be effected as well;
// eg. templated enums (C++14) and template typedefs (C++11) and template function declarations, and template 
// member function declarations.
template<typename _Res, typename _Class, typename... _ArgTypes>
class _Mem_fn<_Res (_Class::*)(_ArgTypes...)>;
