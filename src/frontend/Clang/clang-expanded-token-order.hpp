#ifndef REX_CLANG_EXPANDED_TOKEN_ORDER_HPP
#define REX_CLANG_EXPANDED_TOKEN_ORDER_HPP

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <optional>

// A source location can identify one final expanded token or the exact
// half-closed ordinal span of several tokens produced by one macro expansion.
// Missing locations remain absent from the map; multiplicity is typed and
// never encoded with a numeric sentinel.
class ClangExpandedTokenOrder {
public:
  static ClangExpandedTokenOrder unique(unsigned int order) {
    if (order == 0) {
      std::fprintf(stderr,
                   "REX_FRONTEND_INVARIANT[expanded-token-order-state]: exact "
                   "expanded-token order must be positive\n");
      std::abort();
      __builtin_unreachable();
    }
    return ClangExpandedTokenOrder(order);
  }

  void publish(unsigned int order) {
    if (order == 0) {
      std::fprintf(stderr,
                   "REX_FRONTEND_INVARIANT[expanded-token-order-state]: exact "
                   "expanded-token order must be positive\n");
      std::abort();
      __builtin_unreachable();
    }
    first_order_ = std::min(first_order_, order);
    last_order_ = std::max(last_order_, order);
  }

  std::optional<unsigned int> uniqueOrder() const {
    return first_order_ == last_order_ ? std::optional(first_order_)
                                       : std::nullopt;
  }
  unsigned int firstOrder() const { return first_order_; }
  unsigned int lastOrder() const { return last_order_; }

private:
  explicit ClangExpandedTokenOrder(unsigned int order)
      : first_order_(order), last_order_(order) {}

  unsigned int first_order_;
  unsigned int last_order_;
};

#endif
