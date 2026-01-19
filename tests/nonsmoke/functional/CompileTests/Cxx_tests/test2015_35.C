namespace std {

  template<typename _CharT, typename _Traits>
  class basic_filebuf // : public basic_streambuf<_CharT, _Traits>
        {
          public:
               int foo();
        };

        template <> int basic_filebuf<char, char>::foo() { return 0; }
}


