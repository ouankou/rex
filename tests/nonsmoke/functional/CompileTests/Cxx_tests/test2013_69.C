
template<typename _Tp> _Tp __cmath_power(_Tp);

template<typename _Tp>
_Tp
__pow_helper(_Tp __x)
   {
     return __cmath_power(__x);
   }
