

// template<typename T> class foobar(T);
// template<typename T, typename _Class> class foobar<T (_Class::*)(void)>;
// template<typename _Re, typename _Class, typename... _ArgTypes> class foobar<T (_Class::*)(_ArgTypes...)>; class _Mem_fn<_Res (_Class::*)(_ArgTypes...)>;

// template<typename _Res> class foobar;
// template<typename _Res, typename _Class, typename... _ArgTypes> class foobar<_Res (_Class::*)(_ArgTypes...)>;

template<typename _Res> void foobar (_Res);
// template<typename _Res, typename _Class, typename... _ArgTypes> void foobar<_Res (_Class::*)(_ArgTypes...)> (_Res);

// Working example:
// template<typename _Res, typename _Class, typename... _ArgTypes> void foobar(_Res (_Class::*)(_ArgTypes...));

// Working (simpler) example:
// template<typename _Res, typename _Class> void foobar(_Res (_Class::*)());

template<typename _Res, typename _Class> void foobar(void (_Class::*)(_Res));
