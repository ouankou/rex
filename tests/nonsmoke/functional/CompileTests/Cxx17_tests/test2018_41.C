// C++17 exception specification compatibility

void maybe_throw();
void never_throw() noexcept;

void (*p)() = &maybe_throw;
void (*pn)() noexcept = &never_throw;

struct S {
  using p = void (*)() noexcept;

  static void target() noexcept {}

  operator p() const { return &target; }
};

void (*q)() noexcept = S{};
