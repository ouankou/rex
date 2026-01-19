

template <typename U>
struct pointer_has_rebind
{
   template <typename X>
   char test(typename X::template rebind<U>*);
};
