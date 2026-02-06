#include "sage3basic.h"

void SgAssumedRankExp::post_construction_initialization() {}

SgType *SgAssumedRankExp::get_type() const {
  return SgTypeDefault::createType();
}
