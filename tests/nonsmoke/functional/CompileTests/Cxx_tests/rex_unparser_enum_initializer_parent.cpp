enum class mode : unsigned {
  idle,
  active = static_cast<unsigned>(idle) + 1,
  stopped = static_cast<unsigned>(active) + 1,
};

unsigned enum_initializer_value() {
  return static_cast<unsigned>(mode::stopped);
}
