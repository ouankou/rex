namespace X {
struct nothrow_t {};

typedef unsigned long size_t;

// extern const nothrow_t nothrow;
} // namespace X

// void operator delete[](void*) throw();
// void* operator new(X::size_t, const X::nothrow_t&) throw();
// void* operator new[](X::size_t, const X::nothrow_t&) throw();
// void operator delete(void*, const X::nothrow_t&) throw();
// void operator delete[](void*, const X::nothrow_t&) throw();


void* foo(X::size_t input_size, const X::nothrow_t& input_throw) throw();
