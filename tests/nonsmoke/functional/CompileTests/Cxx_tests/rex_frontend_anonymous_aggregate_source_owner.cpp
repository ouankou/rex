struct RexAnonymousAggregateOwner {
  union {
    int value;
    unsigned bits;
  };
};

int rex_use_anonymous_aggregate(RexAnonymousAggregateOwner &owner) {
  owner.value = 1;
  return static_cast<int>(owner.bits);
}
