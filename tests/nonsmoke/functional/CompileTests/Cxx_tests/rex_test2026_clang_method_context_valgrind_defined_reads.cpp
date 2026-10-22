#include <memory>

namespace {

class RexTest2026Command {
private:
  virtual bool apply(unsigned index) const = 0;
};

class RexTest2026CleanCommand : public RexTest2026Command {
public:
  explicit RexTest2026CleanCommand(double fraction) : fraction_(fraction) {}

private:
  bool apply(unsigned index) const override;

  double fraction_;
};

template <class Base> class RexTest2026DependentCommand : public Base {
  bool apply(unsigned index) const override final { return index != 0; }
};

bool RexTest2026CleanCommand::apply(unsigned index) const {
  return index != 0 && fraction_ > 0.0;
}

std::auto_ptr<RexTest2026Command>
rex_test2026_clang_method_context_valgrind_defined_reads() {
  double fraction = 0.5;
  return std::auto_ptr<RexTest2026Command>(
      new RexTest2026CleanCommand(fraction));
}

} // namespace
