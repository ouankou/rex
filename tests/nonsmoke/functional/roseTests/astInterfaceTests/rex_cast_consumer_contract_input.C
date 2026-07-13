int rex_cast_identity(int value) { return value; }

bool rex_cast_narrow_false() { return static_cast<unsigned char>(256); }

bool rex_cast_narrow_true() { return static_cast<unsigned char>(257); }

long rex_cast_c_surface() { return (long)1; }

long rex_cast_static_surface() { return static_cast<long>(1); }

float rex_cast_floating_narrow(double value) {
  return static_cast<float>(value);
}

double rex_cast_floating_widen(float value) {
  return static_cast<double>(value);
}

void *rex_cast_pointer_precision(long value) {
  return reinterpret_cast<void *>(value);
}

template <class T> T rex_cast_dependent_target(int value) {
  return static_cast<T>(value);
}
