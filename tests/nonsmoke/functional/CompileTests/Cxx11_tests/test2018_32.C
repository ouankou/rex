// struct struct1 {};

// struct struct2 {};

template <typename T> class template_class1 {};

// This is a template typedef (C++11 feature)
template <typename P, typename T> using template_class2 = template_class1<T>;

// Test the use of a redundant templated typedef (allowed in C++11, same as for non-templated typedefs).
template <typename P, typename T> using template_class2 = template_class1<T>;
