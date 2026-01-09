#include "rex_test2025_cfe_token_stream_mapping_header.h"

#define REX_OBJ 7
#define REX_FUNC(x) ((x) + REX_OBJ)

int compute() {
  return REX_FUNC(rex_header_value()) + REX_OBJ;
}
