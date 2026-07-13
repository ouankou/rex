#include "rose.h"

#include "unparseFormatHelp.h"

#include <sstream>

namespace {
class LifetimeCheckedFormat final : public UnparseFormatHelp {
public:
  ~LifetimeCheckedFormat() override { destroyed = true; }

  std::optional<OutputPosition> getPosition(SgLocatedNode *, SgUnparse_Info &,
                                            FormatOpt) override {
    return OutputPosition{1, 0};
  }

  static bool destroyed;
};

bool LifetimeCheckedFormat::destroyed = false;
} // namespace

int main() {
  LifetimeCheckedFormat format;
  Unparser_Opt options;

  {
    std::ostringstream output;
    Unparser first(&output, "first.f90", options, &format);
  }
  if (LifetimeCheckedFormat::destroyed) {
    return 1;
  }

  {
    std::ostringstream output;
    Unparser second(&output, "second.f90", options, &format);
  }
  return LifetimeCheckedFormat::destroyed ? 2 : 0;
}
