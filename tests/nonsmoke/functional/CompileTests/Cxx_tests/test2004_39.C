


template<class _Tp, class _Ptr>
struct _List_iterator 
   {
     typedef _List_iterator<_Tp,_Tp*> iterator;
   };

_List_iterator<int,int> L;

