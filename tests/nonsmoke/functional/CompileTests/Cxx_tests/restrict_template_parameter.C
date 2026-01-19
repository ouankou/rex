// Liao, This is a test to check if ROSE can differentiate
// T * __restrict__ * ptr   vs. T *  * ptr
template <typename T> void Release(T *__restrict__ *ptr) {}

void Release2(int * __restrict__ *ptr2)
{
}

template < typename T >
void Release ( T *  * ptr )
{
}
