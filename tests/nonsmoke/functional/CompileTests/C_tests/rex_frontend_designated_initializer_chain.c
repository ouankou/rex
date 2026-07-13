struct RexLeaf {
  int values[6];
};

struct RexRoot {
  struct RexLeaf leaves[2];
};

struct RexRoot rex_designated_chain = {
    .leaves[1].values[2] = 7,
};

int rex_designated_range[8] = {
    [2 ... 5] = 9,
};
