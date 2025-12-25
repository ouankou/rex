namespace third_party {

// Template declaration
template< typename T, typename param > struct next {};

// Partial specialization.
template< typename T > struct next<T,int> {};

} // namespace third_party
