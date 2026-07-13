%option prefix="omp_exprparser_"
%option outfile="lex.yy.c"
%option stack
%option nounput
%option noyy_top_state
%option noyy_pop_state
%option noyy_push_state

%s FORTRAN


%{

/* DQ (12/10/2016): This is a technique to suppress warnings in generated code that we want to be an error elsewhere in ROSE. 
   See https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html for more detail.
 */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

/* lex requires me to use extern "C" here */
extern "C" int omp_exprparser_wrap() { return 1; }

extern int omp_exprparser_lex();

#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>
#include "sage3basic.h"
#include "omp_exprparser_parser.hh"

static const char* ompparserinput = NULL;
static size_t ompparserinput_remaining = 0;

/* pass user specified string to buf, indicate the size using 'result', 
   and shift the current position pointer of user input afterwards 
   to prepare next round of token recognition!!
*/
#define YY_INPUT(buf, result, max_size)                                            \
  do {                                                                              \
    if (ompparserinput == NULL || ompparserinput_remaining == 0) {                 \
      result = 0;                                                                   \
    } else {                                                                        \
      size_t to_copy =                                                               \
          ompparserinput_remaining < static_cast<size_t>(max_size)                  \
              ? ompparserinput_remaining                                            \
              : static_cast<size_t>(max_size);                                      \
      memcpy(buf, ompparserinput, to_copy);                                         \
      result = static_cast<int>(to_copy);                                           \
      ompparserinput += to_copy;                                                    \
      ompparserinput_remaining -= to_copy;                                          \
    }                                                                               \
  } while (0)

%}

blank           [ \t]
newline         [\n]
digit           [0-9]
integer_suffix  ([uU]([lL]{1,2})?|[lL]{1,2}[uU]?)

id              [a-zA-Z_][a-zA-Z0-9_]*
global_id       "::"{blank}*{id}
qualified_id    {global_id}|(("::"{blank}*)?{id}({blank}*"::"{blank}*{id})+)
fortran_block_end [ ]*[^a-zA-Z0-9_]

%%
0[xX][0-9a-fA-F]+{integer_suffix}? { omp_exprparser_lval.stype = strdup(yytext); return (HEXCONSTANT); }
{digit}+{integer_suffix}? { omp_exprparser_lval.stype = strdup(yytext); return (ICONSTANT); }

[.][Tt][Rr][Uu][Ee][.] { omp_exprparser_lval.stype = strdup("1"); return (ICONSTANT); }
[.][Ff][Aa][Ll][Ss][Ee][.] { omp_exprparser_lval.stype = strdup("0"); return (ICONSTANT); }
[.][Nn][Ee][Qq][Vv][.] { return (NE_OP2); }
[.][Ee][Qq][Vv][.] { return (EQ_OP2); }
[.][Ee][Qq][.] { return (EQ_OP2); }
[.][Nn][Ee][.] { return (NE_OP2); }
[.][Ll][Ee][.] { return (LE_OP2); }
[.][Ll][Tt][.] { return ('<'); }
[.][Gg][Ee][.] { return (GE_OP2); }
[.][Gg][Tt][.] { return ('>'); }
[.][Aa][Nn][Dd][.] { return (LOGAND); }
[.][Oo][Rr][.] { return (LOGOR); }
[.][Xx][Oo][Rr][.] { return (LOGXOR); }

"="             { return ('='); }
"("             { return ('('); }
")"             { return (')'); }
"["             { return ('['); }
"]"             { return (']'); }
","             { return (','); }
":"             { return (':'); }
"?"             { return ('?'); }
"+"             { return ('+'); }
"*"             { return ('*'); }
\/\*([^*]|\*+[^*/])*\*+\/ { /* Ignore C-style block comments */ }
"//"[^\n]*      { /* Ignore C++-style line comments */ }
"/"             { return ('/'); }
"%"/[ \t]*{id}  {
                  if (SageInterface::is_Fortran_language()) {
                    return ('.');
                  }
                  return ('%');
                }
"%"             { return ('%'); }
"-"             { return ('-'); }
"&"             { return ('&'); }
"!"             { return ('!'); }
"~"             { return ('~'); }
"^"             { return ('^'); }
"|"             { return ('|'); }
"&&"            { return (LOGAND); }
"||"            { return (LOGOR); }
"<<"            { return (SHLEFT); }
">>"            { return (SHRIGHT); }
"++"            { return (PLUSPLUS); }
"--"            { return (MINUSMINUS); }

">>="           { return (RIGHT_ASSIGN2); }
"<<="           { return (LEFT_ASSIGN2); }
"+="            { return (ADD_ASSIGN2); }
"-="            { return (SUB_ASSIGN2); }
"*="            { return (MUL_ASSIGN2); }
"/="            { return (DIV_ASSIGN2); }
"%="            { return (MOD_ASSIGN2); }
"&="            { return (AND_ASSIGN2); }
"^="            { return (XOR_ASSIGN2); }
"|="            { return (OR_ASSIGN2); }

"<"             { return ('<'); }
">"             { return ('>'); }
"<="            { return (LE_OP2);}
">="            { return (GE_OP2);}
"=="            { return (EQ_OP2);}
"!="            { return (NE_OP2);}
"\\"            { /*printf("found a backslash\n"); This does not work properly but can be ignored*/}

"->"            { return (PTR_TO); }
"."             { return ('.'); }

{newline}       { /* printf("found a new line\n"); */ /* return (NEWLINE); We ignore NEWLINE since we only care about the pragma string , We relax the syntax check by allowing it as part of line continuation */ }

expr            { return (EXPRESSION); }
varlist         { return (VARLIST); }
identifier      { return (IDENTIFIER); /*not in use for now*/ }
array_section   { return (ARRAY_SECTION); }
static_cast     { return (STATIC_CAST); }
sizeof          { return (SIZEOF); }
int             { return (TYPE_INT); }
<INITIAL>\"([^\\\"\n]|\\.)*\" {
                  omp_exprparser_lval.stype = strdup(yytext);
                  return (STRING_LITERAL);
                }
<FORTRAN>\"([^\"\n]|\"\")*\" {
                  omp_exprparser_lval.stype = strdup(yytext);
                  return (STRING_LITERAL);
                }
<FORTRAN>\'([^\'\n]|\'\')*\' {
                  omp_exprparser_lval.stype = strdup(yytext);
                  return (STRING_LITERAL);
                }
{qualified_id}  { std::string spelling;
                  for (const char *character = yytext; *character != '\0';
                       ++character) {
                    if (*character != ' ' && *character != '\t' &&
                        *character != '\r' && *character != '\n') {
                      spelling.push_back(*character);
                    }
                  }
                  omp_exprparser_lval.stype = strdup(spelling.c_str());
                  return (ID_EXPRESSION); }
{id}            { omp_exprparser_lval.stype = strdup(yytext);
                  return (ID_EXPRESSION); }

"/"{id}"/"/{fortran_block_end}  { omp_exprparser_lval.stype = strdup(yytext); return (FORTRAN_COMMON_BLOCK); }
{blank}*        ;
.               { return (LEXICALERROR);}

%%

/* entry point invoked by callers to start scanning for a string */
extern void omp_exprparser_lexer_init(const char* str) {
  if (str == NULL || str[0] == '\0' || ompparserinput != NULL ||
      ompparserinput_remaining != 0) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-lexer-lifecycle]: "
                 "OpenMP expression lexer requires clean state and nonempty "
                 "input\n";
    ROSE_ABORT();
  }
  ompparserinput = str;
  ompparserinput_remaining = strlen(str);
  if (SageInterface::is_Fortran_language()) {
    BEGIN(FORTRAN);
  } else {
    BEGIN(INITIAL);
  }
  /* We have omp_ suffix for all flex functions */
  omp_exprparser_restart(omp_exprparser_in);
}

extern void omp_exprparser_lexer_finish() {
  if (ompparserinput == NULL || ompparserinput_remaining != 0) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-lexer-lifecycle]: "
                 "OpenMP expression lexer finished without consuming its "
                 "complete active input\n";
    ROSE_ABORT();
  }
  ompparserinput = NULL;
  ompparserinput_remaining = 0;
  BEGIN(INITIAL);
}

extern bool omp_exprparser_lexer_is_clean() {
  return ompparserinput == NULL && ompparserinput_remaining == 0 &&
         YY_START == INITIAL;
}
/**
 * @file
 * Lexer for OpenMP-pragmas.
 */
