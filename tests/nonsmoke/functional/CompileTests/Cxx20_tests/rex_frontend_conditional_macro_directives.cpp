constexpr int rex_elifdef_value() {
#if REX_PRIMARY_BRANCH
  return 1;
#elifdef REX_SECONDARY_BRANCH
  return 2;
#else
  return 3;
#endif
}

constexpr int rex_elifndef_value() {
#if REX_PRIMARY_BRANCH
  return 1;
#elifndef REX_SECONDARY_BRANCH
  return 2;
#else
  return 3;
#endif
}

static_assert(rex_elifdef_value() == REX_EXPECT_ELIFDEF);
static_assert(rex_elifndef_value() == REX_EXPECT_ELIFNDEF);
