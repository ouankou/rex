
namespace std 
{
  template<typename _CharT, typename _Traits>
    streamsize
    __copy_streambufs_eof(basic_streambuf<_CharT, _Traits>*,
			  basic_streambuf<_CharT, _Traits>*, bool&);

  template<typename _CharT, typename _Traits>
    class basic_streambuf 
    {
  public:
    friend streamsize __copy_streambufs_eof<>(basic_streambuf *,
                                              basic_streambuf *, bool &);

  };

}
