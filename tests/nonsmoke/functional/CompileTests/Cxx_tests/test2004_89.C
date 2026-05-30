// Regression for a compiler-generated default constructor used in a throw
// expression. REX must materialize a usable constructor symbol even when the
// class does not explicitly declare the constructor.

class use_count_is_zero // : public std::exception
{
public:
  virtual char const *what() { return ""; }
};

class counted_base {
public:
  void add_ref() { throw(use_count_is_zero()); }
};
