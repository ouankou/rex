template<typename _Tp, _Tp __v>
struct integral_constant {};

template<typename _Tp>
struct alignment_of
   : public integral_constant<int, __alignof__(_Tp)> { };
