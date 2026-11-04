! Definitions only: the include still emits no executable Fortran.
! Tokens following its #include line must remain ignored.
#define REX_LEXICAL_FUNCTION(x) (x)
#define REX_LEXICAL_ALIAS REX_LEXICAL_FUNCTION
#define REX_LEXICAL_EMPTY(x)
#define REX_LEXICAL_FILENAME __FILE__
#define REX_LEXICAL_WARN_CALL(message,file,line) call rex_lexical_record(message,file,line)
#define REX_LEXICAL_WARNING(message) REX_LEXICAL_WARN_CALL(message,REX_LEXICAL_FILENAME,__LINE__)
#define REX_LEXICAL_WARNING_IF(condition,message) if (condition) REX_LEXICAL_WARNING(message)
#define REX_LEXICAL_IGNORED_WARNING_IF(condition,message) CONTINUE !message
#define REX_LEXICAL_STRINGIFY_INNER(x) #x
#define REX_LEXICAL_STRINGIFY(x) REX_LEXICAL_STRINGIFY_INNER(x)
#define REX_LEXICAL_CAPTURE(x) call rex_lexical_text(REX_LEXICAL_STRINGIFY(x))
#define REX_LEXICAL_VARIADIC(first,...) call rex_lexical_values(first,__VA_ARGS__)
#define REX_LEXICAL_D REX_LEXICAL_D
#define REX_LEXICAL_COMMENT !REX_LEXICAL_D
