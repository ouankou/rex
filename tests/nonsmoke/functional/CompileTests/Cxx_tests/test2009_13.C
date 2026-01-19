

template< typename U, typename T >
U f2( T t )
{
    return static_cast<U>( t );
}


int main()
{
    // problem
    f2<int>( 3.5 );
    // quick solution
    //f2<int, double>( 3.5 );

    return 0;
}
