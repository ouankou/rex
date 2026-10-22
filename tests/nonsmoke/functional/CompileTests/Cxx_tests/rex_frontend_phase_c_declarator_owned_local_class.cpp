template <typename T, typename U = T>
int rex_phase_c_declarator_owned_local_class(T value) {
  struct RexPhaseCLocal {
    U payload;
  } rex_phase_c_local_instance{value};

  enum class RexPhaseCLocalEnum : int {
    zero = 0
  } rex_phase_c_local_enum = RexPhaseCLocalEnum::zero;
  enum class RexPhaseCStandaloneEnum : int { zero = 0 };
  using RexPhaseCAlias = struct RexPhaseCAliasClass {
    U payload;
  };
  RexPhaseCAlias rex_phase_c_alias_instance{value};

  return rex_phase_c_local_instance.payload +
         rex_phase_c_alias_instance.payload +
         static_cast<int>(rex_phase_c_local_enum) +
         static_cast<int>(RexPhaseCStandaloneEnum::zero);
}

int main() { return rex_phase_c_declarator_owned_local_class(0); }
