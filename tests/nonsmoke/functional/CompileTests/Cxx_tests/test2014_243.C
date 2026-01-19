
#include <inttypes.h>

#include <cassert>

#include <cmath>

#include <cstdlib>

#include <iostream>

template <class T> class Range {
public:
  // typedef T Value;
  // typedef std::pair<Range, Range> Pair;               /**< A pair of ranges.
  // */

  friend std::ostream &operator<<(std::ostream &o, const Range &x) {
    // x.print(o);
    return o;
  }
};
