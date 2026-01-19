#include "rex_test2025_cfe_token_stream_mapping_header.h"

#pragma message("rex_token_stream")

// leading comment
#define REX_TOKEN_STREAM_ALPHA
#if defined(REX_TOKEN_STREAM_ALPHA)
int alpha = 1;
#elif defined(REX_TOKEN_STREAM_BETA)
int alpha = 2;
#else
int alpha = 3;
#endif /* end if */

/* trailing comment */
int beta = alpha;
