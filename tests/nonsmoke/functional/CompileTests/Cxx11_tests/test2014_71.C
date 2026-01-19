
template <typename T>
struct A 
{
};

template <typename T>
struct B 
{
};

template <typename T>
struct C
{
};


A<B<int>> ab; // works
A<B<C<int>>> abc; // fails
