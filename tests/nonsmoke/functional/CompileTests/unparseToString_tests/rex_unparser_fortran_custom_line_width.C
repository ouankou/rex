#include "rose.h"

#include "unparseFormatHelp.h"

namespace {
class FortyEightColumnFormat final : public UnparseFormatHelp {
public:
  std::optional<OutputPosition> getPosition(SgLocatedNode *, SgUnparse_Info &,
                                            FormatOpt) override {
    return std::nullopt;
  }

  int maxLineLength() override { return 48; }
};
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ASSERT_not_null(project);
  FortyEightColumnFormat format;
  return backend(project, &format);
}
