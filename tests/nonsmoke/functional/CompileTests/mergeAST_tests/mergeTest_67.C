// Example showing that we have to gather the statements in a block to build the delete list!
// Note that these fail for the case of variable declarations.
namespace X
   {
struct FirstDeclarationType {
  int x;
} a;

struct SecondDeclarationType {
  int x;
} b;
   }

// Example showing that we have to gather the statements in a block to build the delete list!
// Note that these fail for the case of variable declarations.
namespace X
   {
struct ThirdDeclarationType {
  int x;
};

struct FourthDeclarationType {
  int x;
};
} // namespace X
