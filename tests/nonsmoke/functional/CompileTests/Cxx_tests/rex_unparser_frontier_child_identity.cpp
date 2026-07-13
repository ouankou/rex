typedef int rex_plain_alias;

typedef struct RexInlineFrontierTag {
  int value;
} RexInlineFrontierAlias;

int rex_frontier_child_identity(int value) {
  RexInlineFrontierAlias aggregate = {value};

  if (aggregate.value > 0)
    --aggregate.value;

  if (aggregate.value < 0)
    aggregate.value = -aggregate.value;
  else
    ++aggregate.value;

  for (int index = 0; index < value; ++index)
    aggregate.value += index;

  return aggregate.value;
}
