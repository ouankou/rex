namespace std
{
  typedef long unsigned int 	size_t;
// typedef long int	ptrdiff_t;
// typedef decltype(nullptr)	nullptr_t;
}


namespace std
{
  
  template<class _E>
    class initializer_list
    {
  public:
    typedef const _E *const_iterator;

  private:
    // constexpr initializer_list(const_iterator __a, size_type __l);
    // constexpr initializer_list(const_iterator __a, size_type __l) :
    // _M_array(__a), _M_len(__l) { }
    constexpr initializer_list(const_iterator __a, size_t __l);

  public:
    // constexpr initializer_list() noexcept : _M_array(0), _M_len(0) { }
    // constexpr initializer_list() noexcept;
    };
}

class X
   {
     public:
          X (std::initializer_list<int> list);
   };

X x = {42};

