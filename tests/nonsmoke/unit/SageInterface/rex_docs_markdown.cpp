#include "rex_docs_markdown_dependency.hpp"

namespace llvm {
class APSInt;
}

/// Documentation rendering fixture: punctuation, templates and overloads.
namespace Docs {
/// An imported dependency is a using declaration, not an owned type.
using Dependency::vector;
/// A coordinate with linked return types.
struct Point {
  /// A conversion operator uses a keyword, not a type called operator.
  operator Point *();
  /// Stored index (must not overwrite the class page).
  int index;
  /// Retrieve the stored index.
  int get_index() const;
  /// Index this coordinate without creating a Markdown link.
  Point const &operator[](int i) const;
  /// Read this coordinate.
  Point const &get() const;
  /// Combine coordinates with operator| (not a table separator).
  friend Point operator|(Point left, Point right);
};
/// Inherit a project-owned hidden friend without copying it.
struct NamedPoint : Point {};
/// Do not copy a dependency's hidden friend into the public reference.
struct DependencyDerived : Dependency::Base {};
/// Mention a forward-declared external type without publishing its page.
llvm::APSInt const &external_value();
/// Hold a value of type T.
/// @note Uses **Markdown** punctuation and a linked @ref Point.
/// @tparam T Stored type.
template <class T> struct Holder {
  /// Stored value.
  T value;
  /// Retrieve the value.
  T get() const;
};
/// Nested external template arguments retain links to owned types.
Dependency::Container<Holder<Point *>> const &nested_points();
/// Import a container while retaining links to its owned type argument.
vector<Point *> imported_points();
/// Create a point.
Point make_point();
/// Create a point from an integer.
/// @param x Initial coordinate.
Point make_point(int x);
/// Point alias.
using Coordinate = Point;
/// Coordinate direction.
enum class Direction { north, south };
/// Default coordinate.
inline Point origin;
} // namespace Docs
/// Make namespace exports visible in the global index.
using namespace Docs;
/// Expose the coordinate type.
using Docs::Point;
