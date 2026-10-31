// Copied outside the fixture's input directory to model external dependencies.
namespace llvm {
struct APSInt {};
} // namespace llvm
namespace Dependency {
template <class T> struct Container {};
template <class T> struct vector {};
struct Base {
  friend bool operator==(const Base &, const Base &);
};
} // namespace Dependency
