

struct X {
  struct Y {
    enum { one, two };
  };
};

struct Z {
  // Note that for C, types are always in global scope.
  struct YY {
    enum { three, four };
  };
};

struct Y number;
