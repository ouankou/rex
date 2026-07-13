#define REX_TOKEN_REPLAY_VALUE(x) ((x) + 1)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

typedef int rex_token_replay_plain_alias;

int rex_token_replay_first = REX_TOKEN_REPLAY_VALUE(2); /* block comment */
int rex_token_replay_second = 4;                        // line comment
int rex_token_replay_adjacent = 5;
int rex_token_replay_no_gap = 6;

#pragma GCC diagnostic pop
