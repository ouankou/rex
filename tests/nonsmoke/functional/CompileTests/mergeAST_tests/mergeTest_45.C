// This examples demonstrates issues specific to member function function constructors
  template<typename _Tp>
    class new_allocator
    {
  public:
    new_allocator() throw() {}

    //    new_allocator(const new_allocator&) throw() { }
    ~new_allocator() throw() {}
  };

new_allocator<char> X;
