#include "sage3basic.h"

void SgBitComplementOp::post_construction_initialization() {
  // DQ (1/20/2019): This function must be defined here so that we can properly
  // mark this operator as a prefix operator (see test2019_10.C).
  set_mode(SgUnaryOp::prefix);
}
