struct RexPseudoRecord {
  int value;
};

RexPseudoRecord rex_pseudo_record{7};
int rex_pseudo_evaluations = 0;
int rex_pseudo_callbacks = 0;

RexPseudoRecord *rex_pseudo_input() {
  ++rex_pseudo_evaluations;
  return &rex_pseudo_record;
}

int rex_pseudo_callback(const char *, ...) {
  ++rex_pseudo_callbacks;
  return 0;
}

int main() {
  __builtin_dump_struct(rex_pseudo_input(), rex_pseudo_callback);
  return rex_pseudo_evaluations != 1 || rex_pseudo_callbacks != 4;
}
