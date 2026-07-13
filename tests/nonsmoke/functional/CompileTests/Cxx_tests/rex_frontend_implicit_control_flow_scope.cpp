struct RexImplicitCondition {
  operator bool() const { return false; }
};

int rex_implicit_values[] = {0, 1};

void rex_implicit_control_flow_scope(RexImplicitCondition condition) {
  if (condition)
    class RexIfLocal {
    public:
      int value() const { return 1; }
    } rex_if_local;
  else
    class RexIfLocal {
    public:
      int value() const { return 2; }
    } rex_if_local;

  while (condition)
    class RexWhileLocal {
    } rex_while_local;

  do
    class RexDoLocal {
    } rex_do_local;
  while (condition);

  for (; condition;)
    class RexForLocal {
    } rex_for_local;

  for (int value : rex_implicit_values)
    class RexRangeForLocal {
    } rex_range_for_local;

  switch (0)
    class RexSwitchLocal {
    } rex_switch_local;

  if (condition) {
    class RexExplicitLocal {
    } rex_explicit_local;
  }

  if (condition)
    (void)condition;
}
