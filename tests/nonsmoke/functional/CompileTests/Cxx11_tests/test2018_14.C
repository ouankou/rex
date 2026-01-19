// struct struct1 {};

// struct struct2 {};

template <typename T> class template_class1 {};

// template_class1<long> ABC;

// This is a template typedef (C++11 feature)
template <typename P, typename T> using template_class2 = template_class1<int>;

// Function with template return type.
// Should be unparsed as:
// template_class2< struct1, double > instance_class1();
// template_class2< struct1, double > instance_class1();
// struct struct1 {};
// template_class2< struct1, double > XXX;
template_class2<int, double> XXX;

// Function with template return type.
// Should be unparsed as:
// template_class2< struct2, double > instance_class2();
// template_class2< struct2, double > instance_class2();

