#def\
ine REX_SPLI\
CED(value)((value) + 1)

#define REX_HASH_SPLICE 4

#/**/ def\
ine REX_COMMENT_PREFIX 5

/* leading comment */ #define REX_LEADING_COMMENT 6

#define REX_SEPARATOR_SPLICE 7

static_assert(REX_SPLICED(2) == 3, "spliced macro name");
static_assert(REX_HASH_SPLICE == 4, "spliced directive prefix");
static_assert(REX_COMMENT_PREFIX == 5, "comment directive trivia");
static_assert(REX_LEADING_COMMENT == 6, "leading comment directive trivia");
static_assert(REX_SEPARATOR_SPLICE == 7, "splice before required separator");

int rex_frontend_macro_splice_contract() {
  return REX_SPLICED(REX_HASH_SPLICE) + REX_COMMENT_PREFIX +
         REX_LEADING_COMMENT + REX_SEPARATOR_SPLICE;
}
