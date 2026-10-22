#include "rex_test2025_cfe_token_stream_mapping_header.h"

#pragma message("rex_token_stream")

// leading comment
/* directive prefix */ #define REX_DIRECTIVE_COMMENT 1
#\
define REX_DIRECTIVE_CONTINUED 2
%:define REX_DIRECTIVE_DIGRAPH 3
#undef REX_DIRECTIVE_COMMENT
#\
undef REX_DIRECTIVE_CONTINUED
%:undef REX_DIRECTIVE_DIGRAPH
#define REX_TOKEN_STREAM_ALPHA
#if defined(REX_TOKEN_STREAM_ALPHA)
int alpha = 1;
#elif defined(REX_TOKEN_STREAM_BETA)
int alpha = 2;
#else
int alpha = 3;
#endif /* end if */

/* trailing comment */ /* adjacent comment */
int beta = alpha;

#if \
0
inactive source need not be valid C++;
/* closing prefix */ %:endif
int gamma = beta;

#define REX_DIRECTIVE_TRIGRAPH ??/
int delta = gamma;
#undef REX_DIRECTIVE_TRIGRAPH
