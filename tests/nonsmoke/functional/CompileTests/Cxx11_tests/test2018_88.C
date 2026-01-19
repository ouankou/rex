template<typename _Rep>
struct duration {
  constexpr duration() = default;
};

template<typename _Rep1, typename _Rep2>
constexpr duration<int>
operator/(const duration<_Rep1>& __d,
          const          _Rep2&  __s);

void func1()
{
   duration<int> var1;
   duration<int> var2;
   int                        var3;
   var1 = var2 / ( var3 - 1 );
}
