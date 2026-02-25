/* OpenMP C and C++ Grammar */
/* Author: Markus Schordan, 2003 */
/* Modified by Christian Biesinger 2006 for OpenMP 2.0 */
/* Modified by Chunhua Liao for OpenMP 3.0, 2008 */
/* Updated by Chunhua Liao for OpenMP 4.5,  2017 */

/*
To debug bison conflicts, use the following command line in the build tree

/bin/sh ../../../../sourcetree/config/ylwrap ../../../../sourcetree/src/frontend/Sab.h `echo expression_parser.cc | sed -e s/cc$/hh/ -e s/cpp$/hpp/ -e s/cxx$/hxx/ -e s/c++$/h++/ -e s/c$/h/` y.output expression_parser.output -- bison -y -d -r state
in the build tree
*/
%define api.prefix {omp_exprparser_}
%defines
%define parse.error verbose

%{
/* DQ (2/10/2014): IF is conflicting with template IF. */
#undef IF

#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include "sage3basic.h" // Sage Interface and Builders
#include "sageBuilder.h"

using namespace OmpSupport;
using namespace SageInterface;

/* Parser - BISON */

/*the scanner function*/
extern int omp_exprparser_lex();

/*A customized initialization function for the scanner, str is the string to be scanned.*/
extern void omp_exprparser_lexer_init(const char* str);

//! Initialize the parser with the originating SgPragmaDeclaration and its pragma text
extern void omp_exprparser_parser_init(SgNode* aNode, const char* str);
extern SgExpression* parseExpression(SgNode*, const char*);
extern SgExpression* parseArraySectionExpression(SgNode*, const char*);

static int omp_exprparser_error(const char*);

// The context node with the pragma annotation being parsed
//
// We attach the attribute to the pragma declaration directly for now, 
// A few OpenMP directive does not affect the next structure block
// This variable is set by the prefix_parser_init() before prefix_parse() is called.
//Liao
static SgNode* omp_directive_node;

static const char* orig_str; 

// The current expression node being generated 
static SgExpression* current_exp = NULL;
// a flag to indicate if the program is looking forward in the symbol table
static bool omp_exprparser_look_forward = false;

// We now follow the OpenMP 4.0 standard's C-style array section syntax: [lower-bound:length] or just [length]
// the latest variable symbol being parsed, used to help parsing the array dimensions associated with array symbol
// such as a[0:n][0:m]
static SgVariableSymbol* array_symbol; 
static SgExpression* lower_exp = NULL;
static SgExpression* length_exp = NULL;
// check if the parsed a[][] is an array element access a[i][j] or array section a[lower:length][lower:length]
// 
static bool arraySection=true; 

static SgSubscriptExpression* buildOpenMPArraySectionSubscript(
    SgExpression* lower, SgExpression* length, SgExpression* stride = NULL) {
    ROSE_ASSERT(lower != NULL);
    if (length == NULL) {
        length = SageBuilder::buildNullExpression_nfi();
    }
    if (stride == NULL) {
        stride = SageBuilder::buildIntVal(1);
    }
    return SageBuilder::buildSubscriptExpression_nfi(lower, length, stride);
}

static bool isKnownVariableSymbolType(SgVariableSymbol *symbol);
static SgVariableSymbol *lookupVariableSymbolPreferTyped(
    const std::string &name, SgScopeStatement *scope);

static bool containsFortranArraySectionArgument(SgExprListExp *args) {
    if (args == NULL) {
        return false;
    }
    const SgExpressionPtrList &exprs = args->get_expressions();
    for (SgExpression *expr : exprs) {
        if (isSgSubscriptExpression(expr) != NULL) {
            return true;
        }
    }
    return false;
}

static bool isFortranArrayLikeType(SgType *type) {
    if (type == NULL) {
        return false;
    }

    type = type->stripType(SgType::STRIP_MODIFIER_TYPE |
                           SgType::STRIP_TYPEDEF_TYPE |
                           SgType::STRIP_REFERENCE_TYPE |
                           SgType::STRIP_RVALUE_REFERENCE_TYPE);
    if (isSgArrayType(type) != NULL) {
        return true;
    }

    if (SgPointerType *ptr_type = isSgPointerType(type)) {
        SgType *base_type = ptr_type->get_base_type();
        if (base_type != NULL) {
            base_type = base_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                             SgType::STRIP_TYPEDEF_TYPE |
                                             SgType::STRIP_REFERENCE_TYPE |
                                             SgType::STRIP_RVALUE_REFERENCE_TYPE);
            if (isSgArrayType(base_type) != NULL) {
                return true;
            }
        }
    }

    return false;
}

static SgVariableSymbol *lookupFortranTypedVariableSymbolFromExpression(
    SgExpression *expr) {
    if (!SageInterface::is_Fortran_language() || expr == NULL) {
        return NULL;
    }

    SgVarRefExp *vref = isSgVarRefExp(expr);
    if (vref == NULL) {
        return NULL;
    }

    if (SgVariableSymbol *symbol = isSgVariableSymbol(vref->get_symbol())) {
        if (isKnownVariableSymbolType(symbol)) {
            return symbol;
        }
        SgScopeStatement *scope = SageInterface::getScope(omp_directive_node);
        if (scope != NULL) {
            SgVariableSymbol *resolved =
                lookupVariableSymbolPreferTyped(symbol->get_name().getString(),
                                                scope);
            if (resolved != NULL) {
                return resolved;
            }
        }
    }

    return NULL;
}

static bool shouldTreatFortranParenAsArrayRef(SgExpression *base,
                                              SgExprListExp *args) {
    if (!SageInterface::is_Fortran_language() || base == NULL || args == NULL) {
        return false;
    }

    if (containsFortranArraySectionArgument(args)) {
        return true;
    }

    if (SgVariableSymbol *symbol =
            lookupFortranTypedVariableSymbolFromExpression(base)) {
        if (isFortranArrayLikeType(symbol->get_type())) {
            return true;
        }
    }

    return isFortranArrayLikeType(base->get_type());
}

static SgExpression* buildPostfixParenExpression(SgExpression *base,
                                                 SgExprListExp *args) {
    if (base == NULL || args == NULL) {
        return NULL;
    }

    if (shouldTreatFortranParenAsArrayRef(base, args)) {
        arraySection = containsFortranArraySectionArgument(args);
        // Keep Fortran multi-dimensional subscripts as one list node so
        // unparsing preserves "a(i,j,k)" rather than "a(i)(j)(k)".
        return SageBuilder::buildPntrArrRefExp(base, args);
    }

    arraySection = false;
    return SageBuilder::buildFunctionCallExp(base, args);
}

// mark whether it is for ompparser
static bool is_ompparser_variable = false;
static bool is_ompparser_expression = false;
// add ompparser var
static bool addOmpVariable(const char*);
static bool addOmpVariableExpr(SgExpression*);
std::vector<std::pair<std::string, SgNode*> > omp_variable_list;
std::map<SgSymbol*,  std::vector < std::pair <SgExpression*, SgExpression*> > >  array_dimensions;  

static std::string lowercaseIdentifier(const std::string &text) {
    std::string lowered(text);
    for (char &ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

static std::string uppercaseIdentifier(const std::string &text) {
    std::string upper(text);
    for (char &ch : upper) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return upper;
}

static bool isKnownVariableSymbolType(SgVariableSymbol *symbol) {
    if (symbol == NULL) {
        return false;
    }
    SgType *type = symbol->get_type();
    if (type == NULL) {
        return false;
    }
    type = type->stripType(SgType::STRIP_MODIFIER_TYPE |
                           SgType::STRIP_TYPEDEF_TYPE |
                           SgType::STRIP_REFERENCE_TYPE |
                           SgType::STRIP_RVALUE_REFERENCE_TYPE);
    return isSgTypeUnknown(type) == NULL;
}

static SgVariableSymbol *lookupVariableSymbolInScopeWithVariants(
    SgScopeStatement *scope, const std::string &name) {
    if (scope == NULL || name.empty()) {
        return NULL;
    }

    if (SgVariableSymbol *symbol = scope->lookup_var_symbol(name)) {
        return symbol;
    }

    if (!SageInterface::is_Fortran_language()) {
        return NULL;
    }

    const std::string lower = lowercaseIdentifier(name);
    if (lower != name) {
        if (SgVariableSymbol *symbol = scope->lookup_var_symbol(lower)) {
            return symbol;
        }
    }

    const std::string upper = uppercaseIdentifier(name);
    if (upper != name) {
        if (SgVariableSymbol *symbol = scope->lookup_var_symbol(upper)) {
            return symbol;
        }
    }

    return NULL;
}

static SgVariableSymbol *lookupVariableSymbolPreferTyped(
    const std::string &name, SgScopeStatement *scope) {
    if (scope == NULL || name.empty()) {
        return NULL;
    }

    SgVariableSymbol *symbol = lookupVariableSymbolInParentScopes(name, scope);
    if (isKnownVariableSymbolType(symbol)) {
        return symbol;
    }

    SgVariableSymbol *fallback = symbol;
    SgScopeStatement *current_scope = scope;
    while (current_scope != NULL) {
        SgVariableSymbol *candidate =
            lookupVariableSymbolInScopeWithVariants(current_scope, name);
        if (candidate == NULL) {
            SgScopeStatement *next_scope = current_scope->get_scope();
            if (next_scope == current_scope) {
                break;
            }
            current_scope = next_scope;
            continue;
        }
        if (fallback == NULL) {
            fallback = candidate;
        }
        if (isKnownVariableSymbolType(candidate)) {
            return candidate;
        }

        SgScopeStatement *next_scope = current_scope->get_scope();
        if (next_scope == current_scope) {
            break;
        }
        current_scope = next_scope;
    }

    return fallback;
}

static bool namesMatchInCurrentLanguage(const std::string &lhs,
                                        const std::string &rhs) {
    if (SageInterface::is_Fortran_language()) {
        return lowercaseIdentifier(lhs) == lowercaseIdentifier(rhs);
    }
    return lhs == rhs;
}

static SgFunctionDeclaration* findNextDeclareSimdFunction(SgStatement* pragma_stmt) {
    if (pragma_stmt == NULL) {
        return NULL;
    }

    SgScopeStatement* scope = pragma_stmt->get_scope();
    if (scope == NULL) {
        return NULL;
    }

    std::string pragma_file;
    int pragma_line = 0;
    if (Sg_File_Info* fi = pragma_stmt->get_file_info()) {
        pragma_file = fi->get_filenameString();
        pragma_line = fi->get_line();
    }

    auto matches_pragma_file = [&](SgStatement* stmt) -> bool {
        if (stmt == NULL) {
            return false;
        }
        if (pragma_file.empty()) {
            return true;
        }
        Sg_File_Info* stmt_fi = stmt->get_file_info();
        if (stmt_fi == NULL) {
            return false;
        }
        if (stmt_fi->get_filenameString() != pragma_file) {
            return false;
        }
        if (pragma_line > 0 && stmt_fi->get_line() <= pragma_line) {
            return false;
        }
        return true;
    };

    auto scan_stmt_list = [&](const SgStatementPtrList& stmts)
        -> SgFunctionDeclaration* {
        bool seen = false;
        for (SgStatement* stmt : stmts) {
            if (!seen) {
                if (stmt == pragma_stmt) {
                    seen = true;
                }
                continue;
            }

            if (isSgPragmaDeclaration(stmt)) {
                continue;
            }

            if (!matches_pragma_file(stmt)) {
                continue;
            }

            if (SgFunctionDeclaration* func = isSgFunctionDeclaration(stmt)) {
                return func;
            }
        }
        return NULL;
    };

    auto scan_decl_list =
        [&](const SgDeclarationStatementPtrList& decls)
            -> SgFunctionDeclaration* {
        bool seen = false;
        for (SgDeclarationStatement* decl : decls) {
            SgStatement* stmt = isSgStatement(decl);
            if (!seen) {
                if (stmt == pragma_stmt) {
                    seen = true;
                }
                continue;
            }

            if (isSgPragmaDeclaration(stmt)) {
                continue;
            }

            if (!matches_pragma_file(stmt)) {
                continue;
            }

            if (SgFunctionDeclaration* func = isSgFunctionDeclaration(stmt)) {
                return func;
            }
        }
        return NULL;
    };

    if (SgGlobal* global = isSgGlobal(scope)) {
        return scan_decl_list(global->get_declarations());
    }
    if (SgNamespaceDefinitionStatement* ns_def =
            isSgNamespaceDefinitionStatement(scope)) {
        return scan_decl_list(ns_def->get_declarations());
    }
    if (SgClassDefinition* class_def = isSgClassDefinition(scope)) {
        return scan_decl_list(class_def->get_members());
    }
    if (SgTemplateClassDefinition* template_def =
            isSgTemplateClassDefinition(scope)) {
        return scan_decl_list(template_def->get_members());
    }
    if (SgTemplateInstantiationDefn* inst_def =
            isSgTemplateInstantiationDefn(scope)) {
        return scan_decl_list(inst_def->get_members());
    }
    if (scope->containsOnlyDeclarations()) {
        return scan_decl_list(scope->getDeclarationList());
    }

    return scan_stmt_list(scope->getStatementList());
}
%}

%locations

/* The %union declaration specifies the entire collection of possible data types for semantic values. these names are used in the %token and %type declarations to pick one of the types for a terminal or nonterminal symbol
corresponding C type is union name defaults to YYSTYPE.
*/

%union {  int itype;
          double ftype;
          const char* stype;
          void* ptype; /* For expressions */
        }

/*Some operators have a suffix 2 to avoid name conflicts with ROSE's existing types, We may want to reuse them if it is proper. 
  experimental BEGIN END are defined by default, we use TARGET_BEGIN TARGET_END instead. 
  Liao*/
%token  '(' ')' ',' ':' '+' '*' '-' '&' '^' '|' LOGAND LOGOR LOGXOR SHLEFT SHRIGHT PLUSPLUS MINUSMINUS PTR_TO '.'
        LE_OP2 GE_OP2 EQ_OP2 NE_OP2 RIGHT_ASSIGN2 LEFT_ASSIGN2 ADD_ASSIGN2
        SUB_ASSIGN2 MUL_ASSIGN2 DIV_ASSIGN2 MOD_ASSIGN2 AND_ASSIGN2 
        XOR_ASSIGN2 OR_ASSIGN2 DEPEND IN OUT INOUT MERGEABLE
        LEXICALERROR IDENTIFIER MIN MAX
        VARLIST ARRAY_SECTION
/*We ignore NEWLINE since we only care about the pragma string , We relax the syntax check by allowing it as part of line continuation */
%token <stype> ICONSTANT EXPRESSION ID_EXPRESSION HEXCONSTANT STRING_LITERAL

/* associativity and precedence */
%left '<' '>' '=' "!=" "<=" ">="
%left '+' '-'
%left '*' '/' '%'

/* nonterminals names, types for semantic values, only for nonterminals representing expressions!! not for clauses with expressions.
 */
%type <ptype> expression assignment_expr conditional_expr 
              logical_or_expr logical_and_expr
              inclusive_or_expr exclusive_or_expr and_expr
              equality_expr relational_expr 
              shift_expr additive_expr multiplicative_expr 
              primary_expr unary_expr postfix_expr
              argument_expression_list argument_expression_list_opt
              parenthesized_argument_list parenthesized_argument_list_opt
              parenthesized_argument_item fortran_subscript_item

/* start point for the parsing */
%start openmp_expression

%%

/* NOTE: We can't use the EXPRESSION lexer token directly. Instead, we have
 * to first call omp_parse_expr, because we parse up to the terminating
 * paren.
 */

openmp_expression : omp_varlist
                  | omp_expression
                  | omp_array_section
                  ;

omp_varlist : VARLIST {
                    is_ompparser_variable = true;
                    } variable_list { is_ompparser_variable = false; }
               ;

omp_expression : EXPRESSION {
                is_ompparser_expression = true;
            } '(' expression ')' {
                is_ompparser_expression = false;
            }
            ;

omp_array_section : ARRAY_SECTION {
                      is_ompparser_expression = true;
                  } '(' array_section_list ')' {
                      is_ompparser_expression = false;
                  }
                  ;

array_section_list : assignment_expr {
                      if (!addOmpVariableExpr((SgExpression*)($1))) YYABORT;
                    }
                   | array_section_list ',' assignment_expr {
                      if (!addOmpVariableExpr((SgExpression*)($3))) YYABORT;
                    }
                   ;

/* Sara Royuela, 04/27/2012
 * Extending grammar to accept conditional expressions, arithmetic and bitwise expressions and member accesses
 */
expression : assignment_expr

assignment_expr : conditional_expr
                | logical_or_expr 
                | unary_expr '=' assignment_expr  {
                    current_exp = SageBuilder::buildAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr RIGHT_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildRshiftAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr LEFT_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildLshiftAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr ADD_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildPlusAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr SUB_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildMinusAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr MUL_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildMultAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr DIV_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildDivAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr MOD_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildModAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr AND_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildAndAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr XOR_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildXorAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                | unary_expr OR_ASSIGN2 assignment_expr {
                    current_exp = SageBuilder::buildIorAssignOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp;
                  }
                ;

conditional_expr : logical_or_expr '?' assignment_expr ':' assignment_expr {
                     current_exp = SageBuilder::buildConditionalExp(
                       (SgExpression*)($1),
                       (SgExpression*)($3),
                       (SgExpression*)($5)
                     );
                     $$ = current_exp;
                   }
                 ;

logical_or_expr : logical_and_expr
                | logical_or_expr LOGOR logical_and_expr {
                    current_exp = SageBuilder::buildOrOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    );
                    $$ = current_exp;
                  }
                ;

logical_and_expr : inclusive_or_expr
                 | logical_and_expr LOGAND inclusive_or_expr {
                     current_exp = SageBuilder::buildAndOp(
                       (SgExpression*)($1),
                       (SgExpression*)($3)
                     );
                   $$ = current_exp;
                 }
                 ;

inclusive_or_expr : exclusive_or_expr
                  | inclusive_or_expr '|' exclusive_or_expr {
                      current_exp = SageBuilder::buildBitOrOp(
                        (SgExpression*)($1),
                        (SgExpression*)($3)
                      );
                      $$ = current_exp;
                    }
                  ;

exclusive_or_expr : and_expr
                  | exclusive_or_expr '^' and_expr {
                      current_exp = SageBuilder::buildBitXorOp(
                        (SgExpression*)($1),
                        (SgExpression*)($3)
                      );
                      $$ = current_exp;
                    }
                  | exclusive_or_expr LOGXOR and_expr {
                      current_exp = SageBuilder::buildBitXorOp(
                        (SgExpression*)($1),
                        (SgExpression*)($3)
                      );
                      $$ = current_exp;
                    }
                  ;

and_expr : equality_expr
         | and_expr '&' equality_expr {
             current_exp = SageBuilder::buildBitAndOp(
               (SgExpression*)($1),
               (SgExpression*)($3)
             );
             $$ = current_exp;
           }
         ;  

equality_expr : relational_expr
              | equality_expr EQ_OP2 relational_expr {
                  current_exp = SageBuilder::buildEqualityOp(
                    (SgExpression*)($1),
                    (SgExpression*)($3)
                  ); 
                  $$ = current_exp;
                }
              | equality_expr NE_OP2 relational_expr {
                  current_exp = SageBuilder::buildNotEqualOp(
                    (SgExpression*)($1),
                    (SgExpression*)($3)
                  ); 
                  $$ = current_exp;
                }
              ;
              
relational_expr : shift_expr
                | relational_expr '<' shift_expr { 
                    current_exp = SageBuilder::buildLessThanOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp; 
                  // std::cout<<"debug: buildLessThanOp():\n"<<current_exp->unparseToString()<<std::endl;
                  }
                | relational_expr '>' shift_expr {
                    current_exp = SageBuilder::buildGreaterThanOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp; 
                  }
                | relational_expr LE_OP2 shift_expr {
                    current_exp = SageBuilder::buildLessOrEqualOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    ); 
                    $$ = current_exp; 
                  }
                | relational_expr GE_OP2 shift_expr {
                    current_exp = SageBuilder::buildGreaterOrEqualOp(
                      (SgExpression*)($1),
                      (SgExpression*)($3)
                    );
                    $$ = current_exp; 
                  }
                ;

shift_expr : additive_expr
           | shift_expr SHRIGHT additive_expr {
               current_exp = SageBuilder::buildRshiftOp(
                 (SgExpression*)($1),
                 (SgExpression*)($3)
               ); 
               $$ = current_exp; 
             }
           | shift_expr SHLEFT additive_expr {
               current_exp = SageBuilder::buildLshiftOp(
                 (SgExpression*)($1),
                 (SgExpression*)($3)
               ); 
               $$ = current_exp; 
             }
           ;

additive_expr : multiplicative_expr
              | additive_expr '+' multiplicative_expr {
                  current_exp = SageBuilder::buildAddOp(
                    (SgExpression*)($1),
                    (SgExpression*)($3)
                  ); 
                  $$ = current_exp; 
                }
              | additive_expr '-' multiplicative_expr {
                  current_exp = SageBuilder::buildSubtractOp(
                    (SgExpression*)($1),
                    (SgExpression*)($3)
                  ); 
                  $$ = current_exp; 
                }
              ;

multiplicative_expr : unary_expr
                    | multiplicative_expr '*' unary_expr {
                        current_exp = SageBuilder::buildMultiplyOp(
                          (SgExpression*)($1),
                          (SgExpression*)($3)
                        ); 
                        $$ = current_exp; 
                      }
                    | multiplicative_expr '/' unary_expr {
                        current_exp = SageBuilder::buildDivideOp(
                          (SgExpression*)($1),
                          (SgExpression*)($3)
                        ); 
                        $$ = current_exp; 
                      }
                    | multiplicative_expr '%' unary_expr {
                        current_exp = SageBuilder::buildModOp(
                          (SgExpression*)($1),
                          (SgExpression*)($3)
                        ); 
                        $$ = current_exp; 
                      }
                    ;

primary_expr : ICONSTANT {
               char* end_ptr = NULL;
               long long parsed_value =
                   strtoll((const char*)($1), &end_ptr, 0);
               if (end_ptr != NULL && *end_ptr != '\0') {
                 parsed_value = strtoll((const char*)($1), NULL, 10);
               }
               SgLongLongIntVal* int_val =
                   SageBuilder::buildLongLongIntVal(parsed_value);
               int_val->set_valueString((const char*)($1));
               free(const_cast<char*>($1));
               current_exp = int_val;
               $$ = current_exp;
              }
             | HEXCONSTANT {
               SgLongLongIntVal* int_val =
                   SageBuilder::buildLongLongIntVal(strtoll((const char*)($1), NULL, 0));
               int_val->set_valueString((const char*)($1));
               free(const_cast<char*>($1));
               current_exp = int_val;
               $$ = current_exp;
              }
             | STRING_LITERAL {
               current_exp = SageBuilder::buildStringVal((const char*)($1));
               free(const_cast<char*>($1));
               $$ = current_exp;
              }
             | LOGOR {
               SgScopeStatement* scope = SageInterface::getScope(omp_directive_node);
               ROSE_ASSERT(scope != NULL);
               current_exp = SageBuilder::buildOpaqueVarRefExp(".or.", scope);
               $$ = current_exp;
              }
             | LOGAND {
               SgScopeStatement* scope = SageInterface::getScope(omp_directive_node);
               ROSE_ASSERT(scope != NULL);
               current_exp = SageBuilder::buildOpaqueVarRefExp(".and.", scope);
               $$ = current_exp;
              }
             | LOGXOR {
               SgScopeStatement* scope = SageInterface::getScope(omp_directive_node);
               ROSE_ASSERT(scope != NULL);
               current_exp = SageBuilder::buildOpaqueVarRefExp(".xor.", scope);
               $$ = current_exp;
              }
             | ID_EXPRESSION {
               SgScopeStatement* scope = SageInterface::getScope(omp_directive_node);
               ROSE_ASSERT(scope != NULL);
               if (SageInterface::is_Fortran_language()) {
                 SgVarRefExp* source_spelling_ref =
                     SageBuilder::buildDanglingVarRefExp(
                         SgName((const char*)($1)), scope);
                 if (SgVariableSymbol* symbol =
                         lookupVariableSymbolPreferTyped((const char*)($1), scope)) {
                   current_exp = SageBuilder::buildVarRefExp(symbol);
                   // Keep semantic symbol resolution while preserving original
                   // source spelling for Fortran directive unparsing.
                   current_exp->set_originalExpressionTree(source_spelling_ref);
                 } else {
                   current_exp = source_spelling_ref;
                 }
               } else if (SgVariableSymbol* symbol =
                              lookupVariableSymbolPreferTyped((const char*)($1), scope)) {
                 current_exp = SageBuilder::buildVarRefExp(symbol);
               } else {
                 current_exp = SageBuilder::buildOpaqueVarRefExp((const char*)($1), scope);
               }
               free(const_cast<char*>($1));
               $$ = current_exp;
              }
             | '(' expression ')' {
                 SgExpression* parenthesized = isSgExpression((SgNode*)($2));
                 if (parenthesized != NULL) {
                   parenthesized->set_need_paren(true);
                   current_exp = parenthesized;
                 }
                 $$ = current_exp;
               } 
             ;

unary_expr : postfix_expr {
             current_exp = (SgExpression*)($1);
             $$ = current_exp;
            }  
           |PLUSPLUS unary_expr {
              current_exp = SageBuilder::buildPlusPlusOp(
                (SgExpression*)($2),
                SgUnaryOp::prefix
              );
              $$ = current_exp;
            }
          | '*' unary_expr {
              current_exp = SageBuilder::buildPointerDerefExp((SgExpression*)($2));
              $$ = current_exp;
            }
          | MINUSMINUS unary_expr {
              current_exp = SageBuilder::buildMinusMinusOp(
                (SgExpression*)($2),
                SgUnaryOp::prefix
              );
              $$ = current_exp;
            }

           ;
/* Follow ANSI-C yacc grammar */                
argument_expression_list_opt
            : /* empty */ {
                $$ = SageBuilder::buildExprListExp_nfi();
              }
            | argument_expression_list {
                $$ = $1;
              }
            ;

argument_expression_list
            : assignment_expr {
                SgExprListExp* args = SageBuilder::buildExprListExp_nfi();
                args->append_expression((SgExpression*)($1));
                $$ = args;
              }
            | argument_expression_list ',' assignment_expr {
                SgExprListExp* args = isSgExprListExp((SgNode*)($1));
                ROSE_ASSERT(args != NULL);
                args->append_expression((SgExpression*)($3));
                $$ = args;
              }
            ;

parenthesized_argument_list_opt
            : /* empty */ {
                $$ = SageBuilder::buildExprListExp_nfi();
              }
            | parenthesized_argument_list {
                $$ = $1;
              }
            ;

parenthesized_argument_list
            : parenthesized_argument_item {
                SgExprListExp* args = SageBuilder::buildExprListExp_nfi();
                args->append_expression((SgExpression*)($1));
                $$ = args;
              }
            | parenthesized_argument_list ',' parenthesized_argument_item {
                SgExprListExp* args = isSgExprListExp((SgNode*)($1));
                ROSE_ASSERT(args != NULL);
                args->append_expression((SgExpression*)($3));
                $$ = args;
              }
            ;

parenthesized_argument_item
            : assignment_expr {
                $$ = $1;
              }
            | fortran_subscript_item {
                $$ = $1;
              }
            ;

fortran_subscript_item
            : expression ':' expression {
                $$ = buildOpenMPArraySectionSubscript((SgExpression*)($1),
                                                      (SgExpression*)($3));
              }
            | expression ':' expression ':' expression {
                $$ = buildOpenMPArraySectionSubscript((SgExpression*)($1),
                                                      (SgExpression*)($3),
                                                      (SgExpression*)($5));
              }
            | ':' expression {
                $$ = buildOpenMPArraySectionSubscript(
                    SageBuilder::buildNullExpression_nfi(),
                    (SgExpression*)($2));
              }
            | ':' expression ':' expression {
                $$ = buildOpenMPArraySectionSubscript(
                    SageBuilder::buildNullExpression_nfi(),
                    (SgExpression*)($2),
                    (SgExpression*)($4));
              }
            | expression ':' {
                $$ = buildOpenMPArraySectionSubscript(
                    (SgExpression*)($1),
                    SageBuilder::buildNullExpression_nfi());
              }
            | expression ':' ':' expression {
                $$ = buildOpenMPArraySectionSubscript(
                    (SgExpression*)($1),
                    SageBuilder::buildNullExpression_nfi(),
                    (SgExpression*)($4));
              }
            | ':' {
                $$ = buildOpenMPArraySectionSubscript(
                    SageBuilder::buildNullExpression_nfi(),
                    SageBuilder::buildNullExpression_nfi());
              }
            | ':' ':' expression {
                $$ = buildOpenMPArraySectionSubscript(
                    SageBuilder::buildNullExpression_nfi(),
                    SageBuilder::buildNullExpression_nfi(),
                    (SgExpression*)($3));
              }
            ;

postfix_expr:primary_expr {
               arraySection= false; 
                 current_exp = (SgExpression*)($1);
                 $$ = current_exp;
             }
            | postfix_expr '(' parenthesized_argument_list_opt ')' {
               current_exp = buildPostfixParenExpression(
                   (SgExpression*)($1), (SgExprListExp*)($3));
               $$ = current_exp;
             }
            |postfix_expr '[' expression ']' {
               arraySection= false; 
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), (SgExpression*)($3));
               $$ = current_exp;
             }
            | postfix_expr '[' expression ':' expression ']'
             {
               arraySection= true; // array section expression
               // postfix_expr should be ID_EXPRESSION
               if (array_symbol == NULL)
               {  
                 if (SgVarRefExp* vref = isSgVarRefExp((SgExpression*)($1))) {
                   array_symbol = isSgVariableSymbol(vref->get_symbol());
                 }
               }
               lower_exp = (SgExpression*)($3);
               length_exp = (SgExpression*)($5);
               if (array_symbol != NULL)
               {
                 SgType* t = array_symbol->get_type();
                 bool isPointer= (isSgPointerType(t) != NULL );
                 bool isArray= (isSgArrayType(t) != NULL);
                 if (!isPointer && ! isArray )
                 {
                   std::cerr<<"Error. ompparser.yy expects a pointer or array type."<<std::endl;
                   std::cerr<<"while seeing "<<t->class_name()<<std::endl;
                 }
               }
               assert (lower_exp && length_exp);
               SgSubscriptExpression* subscript =
                 buildOpenMPArraySectionSubscript(lower_exp, length_exp);
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), subscript);
               $$ = current_exp;
             }  
            | postfix_expr '[' expression ':' expression ':' expression ']'
             {
               arraySection= true; // array section expression with explicit stride
               if (array_symbol == NULL)
               {
                 if (SgVarRefExp* vref = isSgVarRefExp((SgExpression*)($1))) {
                   array_symbol = isSgVariableSymbol(vref->get_symbol());
                 }
               }
               lower_exp = (SgExpression*)($3);
               length_exp = (SgExpression*)($5);
               SgExpression* stride_exp = (SgExpression*)($7);
               assert (lower_exp && length_exp && stride_exp);
               SgSubscriptExpression* subscript =
                 buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), subscript);
               $$ = current_exp;
             }
            | postfix_expr '[' ':' expression ']'
             {
               arraySection= true; // array section expression with omitted lower bound
               if (array_symbol == NULL)
               {
                 if (SgVarRefExp* vref = isSgVarRefExp((SgExpression*)($1))) {
                   array_symbol = isSgVariableSymbol(vref->get_symbol());
                 }
               }
               lower_exp = SageBuilder::buildNullExpression_nfi();
               length_exp = (SgExpression*)($4);
               if (array_symbol != NULL)
               {
                 SgType* t = array_symbol->get_type();
                 bool isPointer= (isSgPointerType(t) != NULL );
                 bool isArray= (isSgArrayType(t) != NULL);
                 if (!isPointer && ! isArray )
                 {
                   std::cerr<<"Error. ompparser.yy expects a pointer or array type."<<std::endl;
                   std::cerr<<"while seeing "<<t->class_name()<<std::endl;
                 }
               }
               assert (lower_exp && length_exp);
               SgSubscriptExpression* subscript =
                 buildOpenMPArraySectionSubscript(lower_exp, length_exp);
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), subscript);
               $$ = current_exp;
             }
            | postfix_expr '[' ':' expression ':' expression ']'
             {
               arraySection= true; // array section expression with omitted lower bound
               if (array_symbol == NULL)
               {
                 if (SgVarRefExp* vref = isSgVarRefExp((SgExpression*)($1))) {
                   array_symbol = isSgVariableSymbol(vref->get_symbol());
                 }
               }
               lower_exp = SageBuilder::buildNullExpression_nfi();
               length_exp = (SgExpression*)($4);
               SgExpression* stride_exp = (SgExpression*)($6);
               assert (lower_exp && length_exp && stride_exp);
               SgSubscriptExpression* subscript =
                 buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), subscript);
               $$ = current_exp;
             }
            | postfix_expr '[' expression ':' ']'
             {
               arraySection= true; // array section expression with omitted length
               if (array_symbol == NULL)
               {
                 if (SgVarRefExp* vref = isSgVarRefExp((SgExpression*)($1))) {
                   array_symbol = isSgVariableSymbol(vref->get_symbol());
                 }
               }
               lower_exp = (SgExpression*)($3);
               length_exp = SageBuilder::buildNullExpression_nfi();
               assert (lower_exp && length_exp);
               SgSubscriptExpression* subscript =
                 buildOpenMPArraySectionSubscript(lower_exp, length_exp);
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), subscript);
               $$ = current_exp;
             }
            | postfix_expr '[' expression ':' ':' expression ']'
             {
               arraySection= true; // array section expression with omitted length and explicit stride
               if (array_symbol == NULL)
               {
                 if (SgVarRefExp* vref = isSgVarRefExp((SgExpression*)($1))) {
                   array_symbol = isSgVariableSymbol(vref->get_symbol());
                 }
               }
               lower_exp = (SgExpression*)($3);
               length_exp = SageBuilder::buildNullExpression_nfi();
               SgExpression* stride_exp = (SgExpression*)($6);
               assert (lower_exp && length_exp && stride_exp);
               SgSubscriptExpression* subscript =
                 buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
               current_exp = SageBuilder::buildPntrArrRefExp((SgExpression*)($1), subscript);
               $$ = current_exp;
             }
            | postfix_expr '.' ID_EXPRESSION {
                SgExpression* base = (SgExpression*)($1);
                SgVarRefExp* member = SageBuilder::buildOpaqueVarRefExp(
                    (const char*)($3), SageInterface::getScope(omp_directive_node));
                free(const_cast<char*>($3));
                current_exp = SageBuilder::buildDotExp(base, member);
                $$ = current_exp;
              }
             | postfix_expr PTR_TO ID_EXPRESSION {
                SgExpression* base = (SgExpression*)($1);
                SgVarRefExp* member = SageBuilder::buildOpaqueVarRefExp(
                    (const char*)($3), SageInterface::getScope(omp_directive_node));
                free(const_cast<char*>($3));
                current_exp = SageBuilder::buildArrowExp(base, member);
                $$ = current_exp;
             }
            | postfix_expr PLUSPLUS {
                  current_exp = SageBuilder::buildPlusPlusOp(
                    (SgExpression*)($1),
                    SgUnaryOp::postfix
                  ); 
                  $$ = current_exp; 
                }
             | postfix_expr MINUSMINUS {
                  current_exp = SageBuilder::buildMinusMinusOp(
                    (SgExpression*)($1),
                    SgUnaryOp::postfix
                  ); 
                  $$ = current_exp; 
             }
            ;

/* ----------------------end for parsing expressions ------------------*/

/*  in C
variable-list : identifier
              | variable-list , identifier 
*/

/* in C++ (we use the C++ version) */ 
variable_list : assignment_expr {
                if (!addOmpVariableExpr((SgExpression*)($1))) YYABORT;
              }
              | variable_list ',' assignment_expr {
                if (!addOmpVariableExpr((SgExpression*)($3))) YYABORT;
              }

%%
int yyerror(const char *s) {
    SgLocatedNode* lnode = isSgLocatedNode(omp_directive_node);
    assert (lnode);
    MLOG_ERROR_C("omp_exprparser",
                 "Error when parsing pragma at line %d: %s\n",
                 lnode->get_file_info()->get_line(),
                 orig_str ? orig_str : "");
    MLOG_ERROR_C("omp_exprparser", "%s\n", s ? s : "unknown parse error");
    ROSE_ABORT();
    return 0; // we want to the program to stop on error
}

void omp_exprparser_parser_init(SgNode* directive, const char* str) {
    orig_str = str;
    current_exp = NULL;
    array_symbol = NULL;
    lower_exp = NULL;
    length_exp = NULL;
    omp_exprparser_lexer_init(str);
    omp_directive_node = directive;
}

// Grab all explicit? variables declared within a common block and add them into the omp variable list
static void ofs_add_block_variables (const char* block_name)
{
  auto equals_case_insensitive = [](const std::string &left,
                                    const std::string &right) -> bool {
    if (left.size() != right.size()) {
      return false;
    }
    for (size_t i = 0; i < left.size(); ++i) {
      unsigned char lhs = static_cast<unsigned char>(left[i]);
      unsigned char rhs = static_cast<unsigned char>(right[i]);
      if (std::tolower(lhs) != std::tolower(rhs)) {
        return false;
      }
    }
    return true;
  };
  std::vector<SgCommonBlock*> block_vec = SageInterface::getSgNodeListFromMemoryPool<SgCommonBlock>();
  SgCommonBlockObject* found_block_object = NULL;
  for (std::vector<SgCommonBlock*>::const_iterator i = block_vec.begin();
       i!= block_vec.end();i++)
  {
    bool innerbreak = false;
    SgCommonBlock* c_block = *i;
    SgCommonBlockObjectPtrList & blockList = c_block->get_block_list();
    SgCommonBlockObjectPtrList::iterator i2 = blockList.begin();
    while (i2 != blockList.end())
    {
      std::string name = (*i2)->get_block_name();
      if (equals_case_insensitive(block_name ? block_name : "", name))
      {
        found_block_object = *i2;
        innerbreak = true;
        break;
      }
      i2++;
    }// end while block objects

    if (innerbreak)
      break;
  } // end for all blocks

  if (found_block_object == NULL)
  {
    MLOG_ERROR_C("omp_exprparser",
                 "cannot find a common block with a name of %s\n",
                 block_name ? block_name : "");
    ROSE_ABORT();
  }

  // add each variable within the block into ompattribute
  SgExprListExp * explistexp = found_block_object->get_variable_reference_list ();
  assert(explistexp != NULL);
  SgExpressionPtrList& explist = explistexp->get_expressions();

  Rose_STL_Container<SgExpression*>::const_iterator exp_iter = explist.begin();
  assert (explist.size()>0); // must have variable defined
  while (exp_iter !=explist.end())
  {
    SgVarRefExp * var_exp = isSgVarRefExp(*exp_iter);
    assert (var_exp!=NULL);
    SgVariableSymbol * symbol = isSgVariableSymbol(var_exp->get_symbol());
    assert (symbol!=NULL);
    SgInitializedName* sgvar = symbol->get_declaration();
    const char* var = sgvar->get_name().getString().c_str();
    if (sgvar != NULL) {
        symbol = isSgVariableSymbol(sgvar->get_symbol_from_symbol_table());
    };
    omp_variable_list.push_back(std::make_pair(var, sgvar));
    exp_iter++;
  }
}

static bool addOmpVariable(const char* var)  {

    // if the leading symbol is '/', it is a block name in Fortran
    if (var[0] == '/') {
        std::string block_name = std::string(var);
        block_name.pop_back();
        ofs_add_block_variables(block_name.c_str()+1);
        return true;
    }

    SgInitializedName* sgvar = NULL;
    SgVariableSymbol* symbol = NULL;
    SgScopeStatement* scope = NULL;
    SgFunctionDeclaration* func = NULL;

    if (omp_exprparser_look_forward != true) {
        scope = SageInterface::getScope(omp_directive_node);
    }
    else {
        SgStatement* cur_stmt = getEnclosingStatement(omp_directive_node);
        ROSE_ASSERT (isSgPragmaDeclaration(cur_stmt));

        // omp declare simd may show up several times before the impacted function declaration.
        func = findNextDeclareSimdFunction(cur_stmt);
        if (func == NULL) {
            std::string pragma_file;
            int pragma_line = 0;
            if (Sg_File_Info* fi = cur_stmt->get_file_info()) {
                pragma_file = fi->get_filenameString();
                pragma_line = fi->get_line();
            }

            SgStatement* nstmt = getNextStatement(cur_stmt);
            ROSE_ASSERT (nstmt); // must have next statement followed.
            while (nstmt != NULL) {
                if (SgFunctionDeclaration* cand = isSgFunctionDeclaration(nstmt)) {
                    if (!pragma_file.empty()) {
                        Sg_File_Info* stmt_fi = cand->get_file_info();
                        if (stmt_fi != NULL) {
                            if (stmt_fi->get_filenameString() != pragma_file ||
                                (pragma_line > 0 && stmt_fi->get_line() <= pragma_line)) {
                                nstmt = getNextStatement(nstmt);
                                continue;
                            }
                        }
                    }
                    func = cand;
                    break;
                }
                nstmt = getNextStatement (nstmt);
            }
        }
        ROSE_ASSERT (func);
        SgFunctionDefinition* def = func->get_definition();
        if (def == NULL) {
            if (SgFunctionDeclaration* def_decl =
                    isSgFunctionDeclaration(func->get_definingDeclaration())) {
                def = def_decl->get_definition();
            }
        }
        if (def != NULL) {
            scope = def->get_body();
        } else {
            scope = func->get_scope();
        }
    };

    ROSE_ASSERT(scope != NULL);
    if (func != NULL) {
        SgFunctionParameterList* params = func->get_parameterList();
        if (params != NULL) {
            const SgInitializedNamePtrList& args = params->get_args();
            for (SgInitializedName* arg : args) {
                if (arg != NULL &&
                    namesMatchInCurrentLanguage(arg->get_name().getString(), var)) {
                    sgvar = arg;
                    symbol = isSgVariableSymbol(arg->get_symbol_from_symbol_table());
                    break;
                }
            }
        }
    }
    if (sgvar == NULL) {
        symbol = lookupVariableSymbolPreferTyped(var, scope);
        if (symbol != NULL) {
            sgvar = symbol->get_declaration();
            if (sgvar != NULL) {
                symbol = isSgVariableSymbol(sgvar->get_symbol_from_symbol_table());
            }
        }
    }
    if (sgvar == NULL) {
        SgExpression* opaque_expr = SageBuilder::buildOpaqueVarRefExp(var, scope);
        omp_variable_list.push_back(std::make_pair(var, opaque_expr));
        array_symbol = NULL;
        return true;
    }
    omp_variable_list.push_back(std::make_pair(var, sgvar));
    array_symbol = symbol;
    return true;
}

static bool collectArraySectionMetadata(
    SgExpression* expr,
    SgVarRefExp*& base_var_ref,
    std::vector<std::pair<SgExpression*, SgExpression*> >& dimensions,
    bool& has_explicit_stride);

static bool appendArraySectionDimension(
    SgExpression* item,
    std::vector<std::pair<SgExpression*, SgExpression*> >& dimensions,
    bool& has_explicit_stride) {
    if (item == NULL) {
        return false;
    }

    if (SgSubscriptExpression* subscript = isSgSubscriptExpression(item)) {
        SgExpression* lower = subscript->get_lowerBound();
        SgExpression* length = subscript->get_upperBound();
        if (lower == NULL || length == NULL) {
            return false;
        }

        if (SgExpression* stride = subscript->get_stride()) {
            SgIntVal* int_stride = isSgIntVal(stride);
            if (int_stride == NULL || int_stride->get_value() != 1) {
                has_explicit_stride = true;
            }
        }

        dimensions.push_back(std::make_pair(lower, length));
        return true;
    }

    // Plain indices in Fortran mixed sections (e.g., a(i,1:n)) still occupy
    // one dimension. Preserve them as [index:1] metadata entries.
    dimensions.push_back(std::make_pair(item, SageBuilder::buildIntVal(1)));
    return true;
}

static bool collectArraySectionMetadata(
    SgExpression* expr,
    SgVarRefExp*& base_var_ref,
    std::vector<std::pair<SgExpression*, SgExpression*> >& dimensions,
    bool& has_explicit_stride) {
    if (expr == NULL) {
        return false;
    }

    if (SgCastExp* cast_exp = isSgCastExp(expr)) {
        return collectArraySectionMetadata(cast_exp->get_operand(),
                                           base_var_ref,
                                           dimensions,
                                           has_explicit_stride);
    }

    if (SgUnaryOp* unary_op = isSgUnaryOp(expr)) {
        return collectArraySectionMetadata(unary_op->get_operand(),
                                           base_var_ref,
                                           dimensions,
                                           has_explicit_stride);
    }

    if (SgPntrArrRefExp* array_ref = isSgPntrArrRefExp(expr)) {
        if (!collectArraySectionMetadata(array_ref->get_lhs_operand(),
                                         base_var_ref,
                                         dimensions,
                                         has_explicit_stride)) {
            return false;
        }

        SgExpression* rhs = array_ref->get_rhs_operand();
        if (isSgSubscriptExpression(rhs) != NULL) {
            return appendArraySectionDimension(rhs, dimensions,
                                               has_explicit_stride);
        }

        if (SgExprListExp* expr_list = isSgExprListExp(rhs)) {
            const SgExpressionPtrList& exprs = expr_list->get_expressions();
            for (SgExpression* item : exprs) {
                if (!appendArraySectionDimension(item,
                                                 dimensions,
                                                 has_explicit_stride)) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    if (SgVarRefExp* vref = isSgVarRefExp(expr)) {
        base_var_ref = vref;
        return true;
    }

    return false;
}

static bool addOmpVariableExpr(SgExpression* expr) {
    if (expr == NULL) {
        array_symbol = NULL;
        return false;
    }

    if (SgVarRefExp* vref = isSgVarRefExp(expr)) {
        if (SgVariableSymbol* sym = isSgVariableSymbol(vref->get_symbol())) {
            if (SageInterface::is_Fortran_language()) {
                // Keep the parsed expression node so Fortran clause entities
                // preserve original directive spelling (case/signature).
                std::string key = expr->unparseToString();
                if (key.empty()) {
                    key = sym->get_name().getString();
                }
                omp_variable_list.push_back(std::make_pair(key, expr));
                array_symbol = sym;
                return true;
            }
            std::string name = sym->get_name().getString();
            if (!name.empty() && addOmpVariable(name.c_str())) {
                array_symbol = NULL;
                return true;
            }
        }
    }

    if (isSgPntrArrRefExp(expr) != NULL) {
        SgVarRefExp* base_var_ref = NULL;
        std::vector<std::pair<SgExpression*, SgExpression*> > dimensions;
        bool has_explicit_stride = false;

        if (collectArraySectionMetadata(expr,
                                        base_var_ref,
                                        dimensions,
                                        has_explicit_stride) &&
            base_var_ref != NULL &&
            !dimensions.empty()) {
            SgVariableSymbol* sym = isSgVariableSymbol(base_var_ref->get_symbol());
            if (sym != NULL) {
                SgVariableSymbol* mapped_symbol = array_symbol;
                if (mapped_symbol == NULL) {
                    mapped_symbol = sym;
                }

                std::string key = expr->unparseToString();
                if (key.empty()) {
                    key = sym->get_name().getString();
                }
                omp_variable_list.push_back(std::make_pair(key, expr));
                // array_dimensions stores only (lower, length) pairs. Preserve
                // explicit-stride sections in expression form instead of
                // recording lossy metadata.
                if (mapped_symbol != NULL && !has_explicit_stride) {
                    array_dimensions[mapped_symbol] = dimensions;
                }
                array_symbol = NULL;
                return true;
            }
        }
    }

    if (SgFunctionRefExp* fref = isSgFunctionRefExp(expr)) {
        if (SgFunctionSymbol* sym = fref->get_symbol()) {
            std::string name = sym->get_name().getString();
            if (!name.empty()) {
                omp_variable_list.push_back(std::make_pair(name, expr));
                array_symbol = NULL;
                return true;
            }
        }
    }

    std::string key;
    if (expr->get_parent() != NULL) {
        key = expr->unparseToString();
    } else {
        key = expr->class_name();
    }
    omp_variable_list.push_back(std::make_pair(key, expr));
    array_symbol = NULL;
    return true;
}

SgExpression* parseArraySectionExpression(SgNode* directive, bool look_forward, const char* str) {
    orig_str = str;
    current_exp = NULL;
    array_symbol = NULL;
    lower_exp = NULL;
    length_exp = NULL;
    omp_exprparser_lexer_init(str);
    omp_directive_node = directive;
    omp_exprparser_look_forward = look_forward;
    omp_exprparser_parse();
    SgExpression* sg_expression = current_exp;

    return sg_expression;
}

SgExpression* parseExpression(SgNode* directive, bool look_forward, const char* str) {
    orig_str = str;
    current_exp = NULL;
    array_symbol = NULL;
    lower_exp = NULL;
    length_exp = NULL;
    omp_exprparser_lexer_init(str);
    omp_directive_node = directive;
    omp_exprparser_look_forward = look_forward;
    omp_exprparser_parse();
    SgExpression* sg_expression = current_exp;

    return sg_expression;
}
