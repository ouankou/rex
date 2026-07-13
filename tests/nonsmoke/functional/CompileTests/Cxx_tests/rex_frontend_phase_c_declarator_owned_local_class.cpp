template <typename T> int rex_phase_c_declarator_owned_local_class(T value) {
  struct RexPhaseCLocal {
    T payload;
  } rex_phase_c_local_instance{value};

  return rex_phase_c_local_instance.payload;
}

int main() { return rex_phase_c_declarator_owned_local_class(0); }
