#include "rex_test2025_cfe_token_stream_mapping_header.h"

#pragma message("rex_token_stream")

// leading comment
#if 1
int alpha = 1;
#elif 0
int alpha = 2;
#else
int alpha = 3;
#endif /* end if */

/* trailing comment */
int beta = alpha;
