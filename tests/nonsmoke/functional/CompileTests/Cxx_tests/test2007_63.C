
template<typename _CharT>
class basic_istream
  {
  };

template<typename _CharT>
class basic_streambuf
   {
     public:
          template<typename _CharT2> friend basic_istream<_CharT2>& operator>>(basic_istream<_CharT2>&, _CharT2*);
   };

// Error: This template declaration will not be output in the generated code.
// It appears to be in the AST, but is marked as compiler generated.
extern template class basic_streambuf<char>;

template<> basic_istream<char>& operator>>(basic_istream<char>& __in, char* __s);

