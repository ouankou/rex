
// Test simple namespace
namespace X_long_name
   {
     int x;
     int Xfoo();
   }

// Test namespace alias
namespace X = X_long_name;

// Build a new namespace to test the using directive
namespace Y
   {
  // int Yfoo();
  // using X::Xfoo;

  // Simple using directive
     using X_long_name::Xfoo;

  // Simple using directive
     using X_long_name::x;
   }

// int Xfoo() { return 0; };

// Test unnamed namespace declaration
namespace
   {
  // Simple using directive
     using X::x;

  // extern int Xfoo();
     extern int Xfoo();
   }
