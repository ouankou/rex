#include "unparseFormatHelp.h"

#include "rose.h"
#include "unparse_format.h"

#include <sstream>
#include <string>

namespace {
enum class PositionMode {
  UseDefault,
  ExplicitForward,
  ExplicitFromEmpty,
  InvalidLine,
  InvalidColumn,
  Retrograde
};

class PositionFormat final : public UnparseFormatHelp {
public:
  explicit PositionFormat(PositionMode mode) : mode_(mode) {}

  std::optional<OutputPosition> getPosition(SgLocatedNode *, SgUnparse_Info &,
                                            FormatOpt) override {
    switch (mode_) {
    case PositionMode::UseDefault:
      return std::nullopt;
    case PositionMode::ExplicitForward:
    case PositionMode::ExplicitFromEmpty:
      return OutputPosition{2, 3};
    case PositionMode::InvalidLine:
      return OutputPosition{0, 0};
    case PositionMode::InvalidColumn:
      return OutputPosition{1, -1};
    case PositionMode::Retrograde:
      return OutputPosition{1, 0};
    }
    ROSE_ABORT();
  }

private:
  PositionMode mode_;
};
} // namespace

int main(int argc, char **argv) {
  PositionMode mode = PositionMode::UseDefault;
  if (argc == 2 && std::string(argv[1]) == "--explicit-forward") {
    mode = PositionMode::ExplicitForward;
  } else if (argc == 2 && std::string(argv[1]) == "--explicit-from-empty") {
    mode = PositionMode::ExplicitFromEmpty;
  } else if (argc == 2 && std::string(argv[1]) == "--invalid-line") {
    mode = PositionMode::InvalidLine;
  } else if (argc == 2 && std::string(argv[1]) == "--invalid-column") {
    mode = PositionMode::InvalidColumn;
  } else if (argc == 2 && std::string(argv[1]) == "--retrograde") {
    mode = PositionMode::Retrograde;
  } else if (argc != 1) {
    return 2;
  }

  PositionFormat helper(mode);
  std::ostringstream output;
  UnparseFormat formatter(&output, &helper);
  SgExprStatement *statement =
      new SgExprStatement(static_cast<SgExpression *>(nullptr));
  SgUnparse_Info info;

  if (mode != PositionMode::ExplicitFromEmpty) {
    formatter << "prefix";
  }
  formatter.format(statement, info, FORMAT_BEFORE_STMT);
  formatter.flush();

  std::string expected = "prefix\n";
  if (mode == PositionMode::ExplicitForward) {
    expected = "prefix\n   ";
  } else if (mode == PositionMode::ExplicitFromEmpty) {
    expected = "\n   ";
  }
  return output.str() == expected ? 0 : 1;
}
