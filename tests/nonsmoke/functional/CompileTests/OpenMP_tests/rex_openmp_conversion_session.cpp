#include "ompAstConstruction.h"
#include "rose.h"

#include <iostream>
#include <string>
#include <thread>

namespace {

void requireEmptyExpressionState(const char *context) {
  if (context == nullptr || !OmpSupport::openMPExpressionVariables().empty()) {
    std::cerr << "REX_OMP_TEST_INVARIANT[conversion-session]: "
              << (context != nullptr ? context : "<null>")
              << " has stale expression state\n";
    ROSE_ABORT();
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 || argv[1] == nullptr) {
    return 2;
  }
  const std::string mode = argv[1];

  if (mode == "sequential") {
    SgSourceFile source_file;
    {
      OmpSupport::OpenMPConversionSession first(&source_file);
      requireEmptyExpressionState("first session entry");
      SgExpression *marker = SageBuilder::buildNullExpression_nfi(
          SgNullExpression::e_null_expression_syntactic_absence);
      ROSE_ASSERT(marker != nullptr);
      OmpSupport::openMPExpressionVariables().push_back(marker);
      OmpSupport::openMPExpressionVariables().clear();
    }
    {
      OmpSupport::OpenMPConversionSession second(&source_file);
      requireEmptyExpressionState("second session entry");
    }
    return 0;
  }

  if (mode == "nested") {
    SgSourceFile source_file;
    OmpSupport::OpenMPConversionSession outer(&source_file);
    OmpSupport::OpenMPConversionSession inner(&source_file);
    return 0;
  }

  if (mode == "concurrent") {
    SgSourceFile source_file;
    OmpSupport::OpenMPConversionSession outer(&source_file);
    std::thread worker([] {
      SgSourceFile worker_source_file;
      OmpSupport::OpenMPConversionSession inner(&worker_source_file);
    });
    worker.join();
    return 0;
  }

  if (mode == "without-session") {
    static_cast<void>(OmpSupport::openMPExpressionVariables());
    return 0;
  }

  return 2;
}
