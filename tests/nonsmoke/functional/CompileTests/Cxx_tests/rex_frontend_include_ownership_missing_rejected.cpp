#include "clang-frontend-private.hpp"

int main() {
  SagePreprocessorRecord recorder(nullptr, nullptr, false, false);
  (void)recorder.includeOwnershipForPath(
      "/rex/missing/preprocessor/ownership.hpp");
  return 0;
}
